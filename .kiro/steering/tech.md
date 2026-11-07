# Technology Stack

> 2026-11-07 時点、実装未着手。ビルド基盤は spec `project-foundation` で実装する。ここに書いた規約は Monde の規約を踏襲しつつ、「他プロジェクトに取り込まれるライブラリ」として必要な差分を加えたものである。

## Architecture

1 つの静的ライブラリ `clopts`（別名 `clopts::clopts`）の中を、責務ごとの内部モジュールに分ける。公開 API は `include/clopts/` のヘッダだけで、内部モジュールの境界は利用者から見えない。

```
clopts::clopts
  ├─ parser      宣言型・宣言検査・走査器・結果ストア・構造化エラー（中核）
  ├─ help        宣言と構造化エラーからの文面生成（書き込みコールバックへ）
  ├─ platform    Windows の UTF-8 argv と環境変数、他 OS ではパススルー
  └─ core        内部ユーティリティ（可変長配列、書き込みコールバック、FILE* ライタ）← 全モジュールが依存
```

- **文面は走査器に依存しない**: `help` は公開ヘッダの宣言型と構造化エラー型だけに依存し、`parser` の実装には依存しない。並行開発と単体テストのため。
- **プラットフォーム層は差し込み式**: `parser` の環境変数フォールバックは取得関数を差し替えられる。`platform` がなくても `getenv` 相当で動き、テストできる。
- **I/O はしない**: ライブラリは stdout / stderr に書かず `exit` しない。文面は書き込みコールバックへ、エラーは構造体で返す。`stdio` への依存は `FILE*` 便宜関数の翻訳単位に閉じ込める。

## Core Technologies

- **Language**: C99（MSVC で使えない機能は使わない。VLA と `_Complex` は使わない）
- **Build**: CMake 3.21 以上（`PROJECT_IS_TOP_LEVEL` を使う）
- **Test**: [Naos](https://github.com/vabelorg/naos)（CTest 経由で実行。テスト専用）
- **CI**: GitHub Actions
- **配布**: ソース配布のみ（GitHub のタグと Release）。事前ビルド済みバイナリは配らない（MSVC の CRT 種別の不一致を避けるため）

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
- `if(NOT TARGET clopts)` で二重定義を防ぎ、`clopts::clopts` を `add_subdirectory` 経路と `find_package` 経路の両方で提供する。
- MSVC の CRT 種別は利用側から継承する。install 経路ではビルド時の CRT をキャッシュ変数に記録して config で警告する（または文書化する）。

### コーディングスタイル（Monde と同じ）
- **Allman スタイル**: 関数定義、`if` / `else` / `for` / `while`、`struct` / `enum` の定義では `{` を次の行の先頭に置く。初期化子は対象外。
- **本体が 1 文でも波括弧を省略しない**。
- **複合条件では比較を個別に括弧で囲む**（例: `if ((i > 0) && (data[i - 1] == '\r'))`）。条件を折り返すときは演算子を次の行の先頭に置く。
- **コメント**: 1 行は `/* ... */`、複数行は先頭 `/*`、各行 ` * `、末尾 ` */`。`//` は使わない。
- **メモリ**: 確保は `malloc` / `calloc` / `realloc` / `free` を直接使い、サイズ計算のオーバーフローを検査する。結果オブジェクトは OOM 後も解放でき、解放は冪等にする。契約違反（NULL 渡しなど）は `assert` で捕まえ、戻り値では報告しない。
- **所有**: 宣言テーブルと argv は利用側が保持し、ライブラリは書き換えない。値の文字列は argv または宣言を指し、複製するのはリスト項目と環境変数の値だけ。

### メッセージと文書の言語
- コード内識別子、ヘルプ・エラー文面は英語。文面は LF 固定・決定的で、端末幅に依存しない。
- 文書（README、`docs/`）、spec、steering は日本語。

### Testing
- 内部モジュールごとに `tests/<module>/` に Naos の単体テストを持ち、CTest から実行する。ラベルは `<module>`。
- テストの宣言は架空のプログラム（Monde の `tool 1.2.3` 方式）だけを使い、実在のツールの宣言を持ち込まない。
- ヘルプ・エラー文面はバイト単位で比較する。エラー種別の列挙は全種別の文面を生成するテストで網羅する。
- Windows の分割規則は現行 MSVC CRT の既知の例（`"foo""bar"`、末尾のバックスラッシュ、先頭の空白、空の引数）を表駆動で検査する。
- 取り込み経路（`FetchContent` と install → `find_package`）は利用側を模したプロジェクトで CI で検証する。
- 文書中のサンプル出力はサンプルプログラムの実行結果と CTest で突き合わせる。
- Naos の不足や回避策は `docs/dev/naos-feedback.md` に記録する。Naos のリポジトリに Issue やコメントは作らない。

## Development Environment

### 対応環境（CI マトリクス）

| OS | コンパイラ |
| --- | --- |
| Windows | MSVC、MinGW-w64（msys2 ucrt64） |
| macOS | Apple Clang |
| Linux | GCC、Clang |

### Common Commands
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
- ライブラリだけをビルドする（Naos を取得しない）: `-DBUILD_TESTING=OFF`（または `-DCLOPTS_BUILD_TESTING=OFF`）
- 1 モジュールのテストだけ実行する: `ctest ... -L <module>`
- オフラインで構成する: `-DFETCHCONTENT_SOURCE_DIR_NAOS=<ローカルの Naos>`

## Key Technical Decisions

- **静的宣言テーブル + 構造化結果**（案A）: ビルダー API は宣言が手続きに散らばり、変数バインド方式は C で型安全を保てない。宣言・挙動・ヘルプが 1 か所で一致する利点を優先した。
- **値の参照は記述子ポインタ**: Monde の文字列キー探索は未宣言の名前を黙って NULL で返した。宣言テーブルの要素アドレスをキーにして打ち間違いを型で防ぐ。
- **エラーは構造体、文面は別 API**: 表示の書式と終了コードは利用側の決定事項。Monde では接頭辞を剥がす処理が必要になった。
- **出力先は書き込みコールバック**: `FILE*` 直書きはテストと GUI / ロガーへの転送を妨げ、呼び出し側バッファ方式は大きなヘルプで手間が増える。
- **Windows の argv は CRT 規則を自前実装**: `CommandLineToArgvW` は 2008 年以前の規則で動き、`main` の argv と食い違う。Rust std / Zig と同じく自前で分割する。副次的に shell32 依存が消える。
- **前方一致の省略は非対応**: 後からオプションを追加すると既存の省略形が壊れるため、clap など現代の実装にならって非対応とする。
- **Monde との API 互換は求めない**: Monde 側の置き換えは別 spec。今は「あるべき設計」を優先する。

---
_Document standards and patterns, not every dependency_
