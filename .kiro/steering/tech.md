# Technology Stack

> 2026-11-08 同期。project-foundation と core-parser は初版の方式（`static const` の宣言テーブル、静的ライブラリのみ）で実装済みだが、2026-11-08 の改訂（roadmap.md の Approach Decision）で、共有ライブラリへの対応と、ビルダー API + 不透明ハンドルへの作り直しが決まった。本ファイルは改訂後の目指す姿を正とする。既存のコードとの差は、roadmap.md の「Existing Spec Updates」で解消する。ここに書いた規約は、Monde の規約を踏襲したうえで、「他のプロジェクトに取り込まれるライブラリ」と「ABI を保つ共有ライブラリ」として必要な差分を加えたものである。

## Architecture

1 つのライブラリ `clopts`（別名 `CLOpts::clopts`。静的か共有かはビルド時に選ぶ）の中を、責務ごとの内部モジュールに分ける。公開 API は `include/clopts/` のヘッダだけで、内部モジュールの境界は利用者から見えない。

```
CLOpts::clopts
  ├─ parser      ビルダーと宣言モデル・宣言検査・走査器・結果ストア・構造化エラー（中核）
  ├─ help        宣言と構造化エラーからの文面生成（書き込みコールバックへ）
  ├─ platform    Windows の UTF-8 argv と環境変数、他 OS ではパススルー
  └─ core        内部ユーティリティ（可変長配列、書き込みコールバック）← 全モジュールが依存
```

- **文面は走査器に依存しない**: `help` は、`parser` が用意する読み取り専用の宣言モデル（内部ヘッダ）と結果のアクセサだけに依存し、走査器の実装には依存しない。並行開発と単体テストのため。
- **プラットフォーム層は差し込み式**: `parser` の環境変数フォールバックは取得関数を差し替えられる。`platform` がなくても `getenv` 相当で動き、テストできる。
- **I/O はしない**: ライブラリは stdout / stderr に書かず `exit` しない。文面は書き込みコールバックへ、エラーは構造体で返す。ライブラリ本体は `stdio` に依存しない。`FILE*` 向けのコールバックは公開ヘッダの `static inline` として、利用側の CRT でコンパイルされる。

## Core Technologies

- **Language**: C99（MSVC で使えない機能は使わない。VLA と `_Complex` は使わない）
- **Build**: CMake 3.21 以上（`PROJECT_IS_TOP_LEVEL` を使う）
- **Test**: [Naos](https://github.com/vabelorg/naos)（CTest 経由で実行。テスト専用）
- **CI**: GitHub Actions
- **配布**: ソース配布のみ（GitHub のタグと Release）。事前ビルド済みバイナリは配らない（MSVC の CRT 種別の不一致を避けるため）。静的ライブラリにも共有ライブラリにもビルドできる

## Key Libraries

- **実行時の外部依存はゼロ**。C 標準ライブラリと、Windows では kernel32 の Win32 API（`GetCommandLineW`、`WideCharToMultiByte`、`GetEnvironmentVariableW`）のみ。shell32（`CommandLineToArgvW`）には依存しない。
- **Naos はテスト専用**。pre-1.0 で変化が速いため、FetchContent でタグ v0.4.0 とコミット SHA の両方で固定し、`BUILD_TESTING` が ON のときだけ取得する。版の更新は `cmake/CloptsNaos.cmake` の 1 か所で行う。配布物に含まれないので THIRD_PARTY_NOTICES には載せない。
- **第三者の素材**: 現時点ではなし。将来取り込む場合は Monde と同じく許容型ライセンスに限り、`THIRD_PARTY_NOTICES` と `licenses/<ID>.txt` に登録する。

## Development Standards

### 言語機能と移植性
- GCC / Clang: `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、言語拡張オフ。MinGW では `__USE_MINGW_ANSI_STDIO=1`。
- MSVC: `/std:c11`（C99 として使う）、`/W4 /WX /utf-8 /Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を PRIVATE 定義。
- `wchar_t` の幅は環境依存（Windows は 16 bit）。UTF-16 を扱うコードは Windows 専用の翻訳単位に閉じ込め、分割規則そのものは入力を配列として受ける純粋関数にしてどの OS でもテストする。

### サブプロジェクトとして行儀よくする（ライブラリ固有）
- グローバルに設定しない: `CMAKE_MSVC_RUNTIME_LIBRARY`、`CMAKE_C_STANDARD` / `CMAKE_C_EXTENSIONS`、`CMAKE_*_OUTPUT_DIRECTORY`、`BUILD_SHARED_LIBS` の既定、ディレクトリ単位の `add_compile_options`、無条件の `include(CTest)` / `enable_testing()`。
- すべてターゲット単位: `target_compile_features(clopts PUBLIC c_std_99)`、`target_compile_options(... PRIVATE ...)`。
- テスト・サンプル・install・`-Werror` は `PROJECT_IS_TOP_LEVEL` を既定とするオプション（`CLOPTS_BUILD_TESTING`、`CLOPTS_BUILD_EXAMPLES`、`CLOPTS_INSTALL`、`CLOPTS_WERROR`）で囲む。
- 静的／共有は `CLOPTS_BUILD_SHARED`（既定は `BUILD_SHARED_LIBS`）で選び、`add_library` に型を明示的に渡す。親プロジェクトは通常変数で上書きできる（CMP0077）。
- `if(NOT TARGET clopts)` で二重定義を防ぎ、`CLOpts::clopts` を `add_subdirectory` 経路と `find_package` 経路の両方で提供する。
- MSVC の CRT 種別は利用側から継承する。install 経路ではビルド時の CRT をキャッシュ変数に記録して config で警告する（または文書化する）。

### 共有ライブラリと ABI（ライブラリ固有）
- **公開ヘッダに構造体の定義を置かない**: 公開型は不透明型の前方宣言（`typedef struct clopts_program clopts_program_t;`）、列挙、関数ポインタ型だけにする。構造体を値で受け渡さない。
- **属性の追加は関数の追加**: 宣言はビルダー関数で組み立てる。新しい属性は設定関数を足して表す。列挙は末尾に追加するだけにし、既存の値は変えない。
- **ビルダーの失敗**: 最初のエラー（宣言エラー、OOM）を program に記録し、以降の呼び出しは何もしない。NULL のハンドルを渡しても安全にする。誤りは解析時（または検査関数）にまとめて返す。
- **export**: 公開関数にはすべて `CLOPTS_API`（`clopts/export.h`）を付ける。判定の順は `CLOPTS_STATIC` → `_WIN32 || __CYGWIN__`（`dllexport` / `dllimport`）→ `__GNUC__ >= 4`（visibility）。`C_VISIBILITY_PRESET hidden` で、それ以外のシンボルは出さない。
- **CRT の境界**: ライブラリが確保したものはライブラリの関数で解放する。利用側で確保したメモリをライブラリが `free` しない。`FILE*` をライブラリ関数の引数にしない。コールバックが返す文字列は借用で、必要ならライブラリが複製する。
- **版**: `project(VERSION)` が唯一の版番号。SOVERSION は 0.x の間は `0.<minor>`、1.0.0 から `1`。0.x の間は minor ごとに API / ABI を壊してよい。
- **検査**: 公開シンボル一覧との照合（共有ビルド、全 OS）と、Linux での abidiff（1.0.0 以降）で ABI の変化を CI で検出する。

### コーディングスタイル（Monde と同じ）
- **Allman スタイル**: 関数定義、`if` / `else` / `for` / `while`、`struct` / `enum` の定義では `{` を次の行の先頭に置く。初期化子は対象外。
- **本体が 1 文でも波括弧を省略しない**。
- **複合条件では比較を個別に括弧で囲む**（例: `if ((i > 0) && (data[i - 1] == '\r'))`）。条件を折り返すときは演算子を次の行の先頭に置く。
- **コメント**: 1 行は `/* ... */`、複数行は先頭 `/*`、各行 ` * `、末尾 ` */`。`//` は使わない。
- **メモリ**: 確保は `malloc` / `calloc` / `realloc` / `free` を直接使い、サイズ計算のオーバーフローを検査する。結果オブジェクトは OOM 後も解放でき、解放は冪等にする。契約違反（NULL 渡しなど）は `assert` で捕まえ、戻り値では報告しない。
- **所有**: ビルダーに渡した文字列はライブラリが複製し、宣言モデルはライブラリが所有する。argv は利用側が保持し、ライブラリは書き換えない（解析結果の中の値が argv を借用するかどうかは core-parser の設計で決め、寿命を API リファレンスに書く）。

### メッセージと文書の言語
- コード内識別子、ヘルプ・エラー文面は英語。文面は LF 固定・決定的で、端末幅に依存しない。
- 文書（README、`docs/`）、spec、steering は日本語。

### Testing
- 内部モジュールごとに `tests/<module>/` に Naos の単体テストを持ち、CTest から実行する。ラベルは `<module>`。
- テストの宣言は架空のプログラム（Monde の `tool 1.2.3` 方式）だけを使い、実在のツールの宣言を持ち込まない。
- ヘルプ・エラー文面はバイト単位で比較する。エラー種別の列挙は全種別の文面を生成するテストで網羅する。
- Windows の分割規則は現行 MSVC CRT の既知の例（`"foo""bar"`、末尾のバックスラッシュ、先頭の空白、空の引数）を表駆動で検査する。
- 取り込み経路（`FetchContent` と install → `find_package`）は、利用側を模したプロジェクトで、静的版と共有版のそれぞれについて CI で検証する。
- 共有ビルドでは、export されたシンボルが公開シンボル一覧と過不足なく一致することを検査する。Windows では、テスト実行ファイルの横に DLL を置く（`$<TARGET_RUNTIME_DLLS>`）。
- 文書中のサンプル出力はサンプルプログラムの実行結果と CTest で突き合わせる。
- Naos の不足や回避策は `docs/dev/naos-feedback.md` に記録する。Naos のリポジトリに Issue やコメントは作らない。

## Development Environment

### 対応環境（CI マトリクス）

| OS | コンパイラ |
| --- | --- |
| Windows | MSVC、MinGW-w64（msys2 ucrt64） |
| macOS | Apple Clang |
| Linux | GCC、Clang |

各環境で静的版と共有版の両方をビルドしてテストする。

### Common Commands
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
- ライブラリだけをビルドする（Naos を取得しない）: `-DBUILD_TESTING=OFF`（または `-DCLOPTS_BUILD_TESTING=OFF`）
- 1 モジュールのテストだけ実行する: `ctest ... -L <module>`
- 共有ライブラリとしてビルドする: `-DCLOPTS_BUILD_SHARED=ON`（または `-DBUILD_SHARED_LIBS=ON`）
- オフラインで構成する: `-DFETCHCONTENT_SOURCE_DIR_NAOS=<ローカルの Naos>`

## Key Technical Decisions

- **ビルダー API + 不透明ハンドル**（案B、2026-11-08 に案A「静的宣言テーブル」から変更）: 共有ライブラリの ABI 互換を保つには、利用側のバイナリに構造体の配置や配列の刻み幅が焼き込まれてはいけない。ビルダーなら属性の追加が関数の追加で済み、要素数や番兵も要らない。版番号付きの構造体（libgit2 型）やタグ付き属性列は、使いやすさと保守費用が見合わないので退けた。
- **値の参照はオプションハンドル**: Monde の文字列キー探索は、宣言していない名前を黙って NULL で返した。宣言時に返るハンドルをキーにして、打ち間違いを型で防ぐ。
- **エラーは構造化（アクセサで読む）、文面は別 API**: 表示の書式と終了コードは利用側の決定事項。Monde では接頭辞を剥がす処理が必要になった。
- **出力先は書き込みコールバック**: `FILE*` に直接書くと、テストや GUI / ロガーへの転送がしにくい。呼び出し側がバッファを用意する方式は、大きなヘルプで手間が増える。`FILE*` 用のコールバックは、DLL の境界を越えないよう公開ヘッダの `static inline` で提供する。
- **Windows の argv は CRT 規則を自前実装**: `CommandLineToArgvW` は 2008 年以前の規則で動き、`main` の argv と食い違う。Rust std / Zig と同じく自前で分割する。副次的に shell32 依存が消える。
- **前方一致の省略は非対応**: 後からオプションを追加すると既存の省略形が壊れるため、clap など現代の実装にならって非対応とする。
- **Monde との API 互換は求めない**: Monde 側の置き換えは別 spec。今は「あるべき設計」を優先する。

---
_Document standards and patterns, not every dependency_
