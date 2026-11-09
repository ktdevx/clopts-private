# Research & Design Decisions

## Summary
- **Feature**: `abi-compat`
- **Discovery Scope**: Extension（既存のビルド基盤・テスト登録・CI に、外側からの検査を足す）
- **Key Findings**:
  - 既存の `tests/cmake/run_fixture.cmake`（子プロジェクトを構成・ビルドして期待と比べる）と、`tests/platform/check_no_shell32.cmake`（`cmake -P` でソースを走査し、コメントを除いてから照合する）が、本 spec の検査の型としてそのまま使える。新しい実行基盤は要らない。
  - abidiff の終了コードはビットの和で、互換のない変更があるときはビット 4（ABI_CHANGE）とビット 8（ABI_INCOMPATIBLE_CHANGE）が両方立つ（値は 12）。判定は「ビット 8 が立っているか」で行い、値の一致では判定しない。ビット 1（ERROR）とビット 2（USAGE_ERROR）はツールの失敗として扱う。
  - CMake は、`MACHO_COMPATIBILITY_VERSION` / `MACHO_CURRENT_VERSION` が未設定なら、Mach-O の compatibility version に SOVERSION を、current version に VERSION を使う。project-foundation の現在の設定のままで、互換版は 1.x の間 `1.0.0`、現在版は `project(VERSION)` になる。

## Research Log

### 既存のテスト登録と `cmake -P` の型
- **Context**: 検査を CTest に載せる方法と、外部依存を増やさずに書く方法を確認する。
- **Sources Consulted**: `tests/CMakeLists.txt`、`tests/toolchain/CMakeLists.txt`、`tests/cmake/run_fixture.cmake`、`tests/platform/check_no_shell32.cmake`、`tests/platform/CMakeLists.txt`、`cmake/CloptsCompilerOptions.cmake`、`cmake/CloptsLibrary.cmake`
- **Findings**:
  - `cmake -P` で動く検査は `<module>.<name>` の名前（例: `platform.no_shell32`）でラベル付きで登録されている。
  - `run_fixture.cmake` は `EXPECT=build_ok` などで子プロジェクトをビルドし、失敗時に子の出力全体を表示する。`EXTRA_ARGS` で追加の `-D` を渡せる。Visual Studio 生成器には C コンパイラを渡さない。
  - `clopts_apply_compiler_options(<target>)` は C の警告設定（GCC/Clang の `-Wall -Wextra -Wpedantic -Werror=vla`、MSVC の `/W4 /utf-8 /Zc:preprocessor`、`CLOPTS_WERROR` で `-Werror` / `/WX`）を 1 か所にまとめている。子プロジェクトから include すれば、同じ警告設定で C の翻訳単位をコンパイルできる。
  - 共有ビルドでは `C_VISIBILITY_PRESET hidden`（Windows 以外）と `DEFINE_SYMBOL CLOPTS_BUILDING`、`SOVERSION` が設定済み。統合テストの子プロジェクトには既に C++ の翻訳単位があり、CI の 5 環境で C++ コンパイラが使えている（MinGW は msys2 の gcc パッケージに g++ が入る）。
- **Implications**: 照合とヘッダ検査は `cmake -P` スクリプト、コンパイル検査は `run_fixture.cmake` を使う子プロジェクトとして作る。C の警告設定は `clopts_apply_compiler_options` を再利用し、C++ の警告設定だけを子プロジェクトに書く。

### 公開ヘッダの書き方
- **Context**: ヘッダの静的検査が扱う構文の範囲を決める。
- **Sources Consulted**: `include/clopts/*.h`
- **Findings**:
  - 公開関数の宣言はすべて `CLOPTS_API <戻り値型> <名前>(...);` の形。引数が次の行に続く宣言がある（`help.h`、`parse.h`）。
  - 不透明型は `typedef struct <tag> <name>_t;`、列挙は `typedef enum <tag> { ... } <name>_t;`、関数ポインタ型は `typedef <型> (*<name>_fn)(...);`。
  - `writer.h` に `static inline int clopts_file_write(...) { ... }` の定義がある。`extern "C" {` は `#ifdef __cplusplus` の中にある。
  - `export.h` はマクロだけで、宣言を持たない。
- **Implications**: 走査器は「コメントと前処理行を除き、波括弧の深さを追いながら最上位の文に切る」だけで足りる。関数を返す関数や `extern` 変数のような形は今は使われていないので、検査の対象外として文書に書く（変数の export はシンボル照合が捕まえる）。
- 注記（2026-11-09）: この調査の時点では、steering の「公開ヘッダに構造体の定義を置かない」に従い、構造体・共用体の定義を違反として検査していた（要件 4.2・4.3）。この規則は「公開構造体は凍結する」に置き換わり、`include/clopts/alloc.h` に凍結した `clopts_allocator_t` を置いたので、ヘッダの検査は構造体・共用体の定義を判定しなくなった（要件 4.2・4.3 は廃止）。既存の公開構造体の配置の変化は、1.0.0 以降の abidiff の比較が検出する。

### 各 OS の export の読み出し
- **Context**: 3.3、3.4 の正規化の規則を決める。
- **Sources Consulted**: binutils の `nm` / `objdump` のマニュアル、Apple の `nm(1)`、Microsoft の DUMPBIN /EXPORTS の説明、CMake の `CMAKE_NM` / `CMAKE_OBJDUMP` / `CMAKE_LINKER`
- **Findings**:
  - Linux: `nm -D --defined-only <so>` は動的シンボル表の定義済みシンボルを `<値> <型> <名前>` で出す。リンカが `_init`、`_fini` を、binutils の版によっては `_edata`、`_end`、`__bss_start` を加える。
  - macOS: `nm -gU <dylib>` は外部の定義済みシンボルを出し、C の名前には先頭に `_` が付く。
  - MSVC: `dumpbin /exports <dll>` は `ordinal hint RVA name` の表を出す。`link.exe /dump /exports` は同じ出力を返し、`CMAKE_LINKER` から辿れるので、開発者環境（`vcvars`）がなくても使える。`dumpbin.exe` は `link.exe` と同じディレクトリにある。
  - MinGW: `objdump -p <dll>` は `[Ordinal/Name Pointer] Table` の節に export 名を出す。列の並びは binutils の版で変わることがあるが、行の最後の語が名前である点は変わらない。`dllexport` が 1 つでもあれば、自動の全シンボル export は働かない。
  - CMake はこれらのツールを `CMAKE_NM`（GNU / Clang / AppleClang / MinGW）、`CMAKE_OBJDUMP`（GNU 系）、`CMAKE_LINKER`（MSVC）として見つけている。
- **Implications**: 出力の形式を `nm-elf` / `nm-macho` / `dumpbin` / `objdump-pe` の 4 つの形式名で区別し、形式ごとに「行から名前を取り出す規則」「取り除くシンボル」を持つ。ツールの出力を記録したファイルを入力にできるようにすれば、4 形式の正規化をどの OS でも自己テストできる。

### abidiff の終了コードとヘッダの指定
- **Context**: 6.5〜6.8 の判定規則を決める。
- **Sources Consulted**: [abidiff(1)](https://man.archlinux.org/man/abidiff.1.en)、[libabigail ABI changes](https://sourceware.org/libabigail/manual/abi-changes.html)、[abicompat(1)](https://man7.org/linux/man-pages/man1/abicompat.1.html)
- **Findings**:
  - 終了コードはビット集合: 1 = ERROR、2 = USAGE_ERROR、4 = ABI_CHANGE、8 = ABI_INCOMPATIBLE_CHANGE。互換のない変更があるときは 4 と 8 が両方立つ。
  - `--headers-dir1` / `--headers-dir2` を指定すると、公開ヘッダに定義されていない型の変更を報告から外す。`--drop-private-types` を足すと、公開ヘッダにない型を比較の対象からも外す。
  - 関数の削除は互換のない変更（ビット 8）、関数の追加は ABI_CHANGE（ビット 4）だけになる。
  - libabigail の版が変わると ABI の記述ファイル（XML）の形式が変わり得る。
- **Implications**: 判定は「ビット 1 か 2 → ツールの失敗」「ビット 8 → 互換のない変更」「それ以外（0 か 4）→ 成功」。基準は XML を保存せず、毎回タグからビルドする。

### macOS の互換版と現在版
- **Context**: 1.5 の方針を決める（roadmap で「minor ごとに `MACHO_COMPATIBILITY_VERSION` を上げるか」を本 spec で決めることになっている）。
- **Sources Consulted**: [CMake MACHO_COMPATIBILITY_VERSION](https://cmake.org/cmake/help/v4.4/_sources/prop_tgt/MACHO_COMPATIBILITY_VERSION.rst.txt)、[CMake MACHO_CURRENT_VERSION](https://cmake.org/cmake/help/v4.0/prop_tgt/MACHO_CURRENT_VERSION.html)、[CMake SOVERSION](https://cmake.org/cmake/help/v3.29/prop_tgt/SOVERSION.html)
- **Findings**:
  - 未設定なら compatibility version は SOVERSION、current version は VERSION から作られる。ファイル名と install name は SOVERSION のまま。
  - dyld は、利用側に記録された compatibility version より current version が小さいライブラリを読み込まない。
  - minor ごとに compatibility version を上げると、新しい minor に対してビルドした利用側を古い minor のライブラリで動かしたとき、関数の不足ではなく読み込みの時点で明確に失敗する。これは前方互換の保護にあたる。
- **Implications**: 前方互換は本 spec の範囲外で、設定は project-foundation の所有なので、CMake の既定（互換版 = SOVERSION、現在版 = VERSION）をそのまま方針として採り、文書に書く。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| `cmake -P` スクリプト + 子プロジェクト | 照合・静的検査は `cmake -P`、コンパイル検査は `run_fixture.cmake` | 外部依存なし。既存の型と同じで、全 OS で動く | CMake の文字列処理は C の構文解析には弱い | 採用。扱う構文を限定して文書化する |
| Python スクリプト | 照合と走査を Python で書く | 構文解析が書きやすい | Windows の CI に Python が要る。外部依存が増える | 不採用（制約に反する） |
| コンパイラの前処理出力を走査 | `-E` の出力から宣言を抜く | 前処理後の正確な宣言が得られる | `CLOPTS_API` が展開されて消え、付け漏れを判定できない | 不採用 |
| abidw の XML をリポジトリに置く | 基準の記述ファイルを版管理する | CI でタグをビルドしなくてよい | libabigail の版が変わると読めなくなる | 不採用（brief の判断どおり） |

## Design Decisions

### Decision: 公開シンボル一覧の置き場所を `abi/clopts.symbols` にする
- **Context**: 2.1。一覧は検査の入力であると同時に、公開 API の契約そのもの。
- **Alternatives Considered**:
  1. `tests/abi/clopts.symbols` — テスト資材と並べる
  2. `abi/clopts.symbols` — リポジトリ直下の専用ディレクトリ
- **Selected Approach**: 2。
- **Rationale**: 公開関数を足す spec（help-and-messages、platform-argv）が更新する契約であり、テストの内部資材ではない。差分がレビューで目立つ。
- **Trade-offs**: 直下のディレクトリが 1 つ増える。install はしない。
- **Follow-up**: structure.md の `tests/abi/` の項に場所を追記する。

### Decision: 自己テストのために、ツールの出力を記録したファイルを照合の入力にできるようにする
- **Context**: 3.4 の正規化は 4 形式あり、各 OS の CI でしか本物の出力を得られない。
- **Selected Approach**: 照合スクリプトは、ツールを実行する代わりに `DUMP_FILE` を読む経路を持つ。記録した出力（`tests/abi/fixtures/exports/`）で、全形式の正規化・不足・余分の報告をどの OS でも検査する。
- **Rationale**: ある OS の正規化を変えたとき、別の OS の CI を待たずに誤りが分かる。
- **Trade-offs**: 記録した出力がツールの版の変化に追従しない。本物の出力は各 OS の `abi.exports` が検査するので、両者で補い合う。

### Decision: C++ のリンケージを、一覧の全シンボルを C++ から参照してリンクすることで確かめる
- **Context**: 5.3。
- **Alternatives Considered**:
  1. C++ の翻訳単位で `extern "C"` の再宣言を生成し、リンケージの食い違いをコンパイルエラーにする — 宣言の再生成が要る
  2. C++ の実行ファイルから一覧の全シンボルのアドレスを取り、`CLOpts::clopts` にリンクする — 名前修飾を受けていればリンクで失敗する
- **Selected Approach**: 2。
- **Rationale**: 宣言を再生成せずに済み、一覧に載ったのにヘッダにない関数もコンパイルで分かる。
- **Trade-offs**: コンパイル検査が公開シンボル一覧に依存する。一覧は本 spec の所有なので、境界の外への依存は増えない。

### Decision: C++ コンパイラがないときは、テストを `DISABLED` で登録する
- **Context**: 5.6、5.8。
- **Selected Approach**: `tests/abi/CMakeLists.txt` で `check_language(CXX)` を使い、見つからなければ `abi.headers_cxx` を `DISABLED` にする。CLOpts 本体の `project()` は `LANGUAGES C` のまま変えない。
- **Rationale**: CTest は `DISABLED` のテストを「Not Run (Disabled)」として表示し、失敗に数えない。C++ を有効にするのはテストのときの子プロジェクトだけなので、利用側には C++ を要求しない。

### Decision: abidiff の比較を 1 つの `cmake -P` スクリプトにまとめ、CI のジョブはそれを呼ぶだけにする
- **Context**: 6.1〜6.9。
- **Selected Approach**: `tests/abi/run_abidiff.cmake` が、版の読み取り、基準のタグの選択、飛ばす判定、両版のビルドと install、abidiff の実行、終了コードの判定を行う。基準の選択と終了コードの判定は関数に分け、`MODE=self_test` で表駆動の自己テスト（`abi.compare_selftest`、全 OS）を行う。
- **Rationale**: 1.0.0 まではジョブが常に飛ばすので、判定の規則を CI の実行で確かめられない。自己テストで規則を先に固める。手元の Linux でも同じスクリプトで再現できる。
- **Trade-offs**: 両版のビルドと abidiff の呼び出しそのものは、1.0.0 の最初のリリース後まで CI で実行されない（Risks を参照）。

### Decision: macOS の互換版と現在版は CMake の既定に従い、minor ごとに上げない
- **Context**: 1.5。
- **Alternatives Considered**:
  1. minor ごとに `MACHO_COMPATIBILITY_VERSION` を `<major>.<minor>.0` にする — 前方互換の保護
  2. 既定（互換版 = SOVERSION、現在版 = VERSION）のまま
- **Selected Approach**: 2。
- **Rationale**: 前方互換は保証の範囲外。Linux と Windows には同等の仕組みがなく、macOS だけで挙動を変えると方針の説明が複雑になる。設定を変えると project-foundation の所有物に手を入れることになる。
- **Trade-offs**: 新しい minor に対してビルドした利用側を古い minor のライブラリで動かすと、読み込み時ではなく、足りない関数を解決する時点で失敗する。文書にこの挙動を書く。
- **Follow-up**: 前方互換を保証範囲に入れる場合は、本 spec と project-foundation を見直す（Revalidation Trigger）。

## Design Synthesis

- **Generalization**: シンボル照合の 4 つの OS は「ツールの出力 → 名前の集合への正規化 → 一覧との集合の差」という 1 つの流れの変種である。形式名ごとの抽出規則と除外リストだけを差し替え、比較と報告は共通にする。ヘッダの検査も、「静的検査」と「C / C++ のコンパイル検査」の 2 つに分け、後者は言語を引数にした 1 つの子プロジェクトにする。
- **Build vs. Adopt**: ABI の比較は libabigail（abidiff）を採用する。シンボルの読み出しは各 OS の標準ツールを採用する。ヘッダの構文検査は、該当する既存の道具（`ctags`、`clang` の AST 出力など）が全 OS に揃っておらず外部依存になるため、扱う構文を限定した走査器を CMake で書く。
- **Simplification**: シンボル一覧の「並び順の強制」や「ヘッダの宣言との自動突き合わせ」は要件にないので入れない（一覧とヘッダの食い違いは、export の照合と C++ のリンクで間接的に捕まる）。ELF のバージョンスクリプトは範囲外。abidiff の基準の選択は「同じ major の最新タグ」だけにし、ブランチや手動の指定は持たない。

## Risks & Mitigations
- MinGW の `objdump -p` の列の形式が binutils の版で変わる — 行の最後の語を名前とする規則にし、記録した出力で自己テストする。CI の `abi.exports` が本物の出力で確かめる。
- Linux のリンカが加えるシンボルの顔ぶれが binutils の版で変わる — 除外リストは「あれば除く」集合にし、無くても失敗しない。未知のシンボルが出たら、報告に出た名前を見て除外リストに理由付きで足す。
- abidiff の比較が 1.0.0 まで CI で実行されない — 判定の規則は自己テストで固める。1.0.0 のリリース手順（docs-and-release）で、最初のパッチリリースの前に手元でスクリプトを一度実行することを求める。
- ヘッダの走査器が想定外の構文（関数を返す関数、`extern` 変数、マクロで包んだ宣言）を見落とす — 扱う構文を文書と走査器の注釈に書き、扱えない形が現れたら走査器を広げる。export の照合が最後の網になる。
- `check_language(CXX)` がトップレベルのキャッシュに `CMAKE_CXX_COMPILER` を残す — テストを有効にしたトップレベルのビルドでだけ実行されるので、サブプロジェクトとして取り込む利用側には影響しない。

## References
- [abidiff(1)](https://man.archlinux.org/man/abidiff.1.en) — 終了コードのビット、`--headers-dir1/2`、`--drop-private-types`
- [libabigail: ABI changes](https://sourceware.org/libabigail/manual/abi-changes.html) — 互換のない変更の分類
- [abicompat(1)](https://man7.org/linux/man-pages/man1/abicompat.1.html) — ABI_CHANGE だけのときの意味
- [CMake MACHO_COMPATIBILITY_VERSION](https://cmake.org/cmake/help/v4.4/_sources/prop_tgt/MACHO_COMPATIBILITY_VERSION.rst.txt) / [MACHO_CURRENT_VERSION](https://cmake.org/cmake/help/v4.0/prop_tgt/MACHO_CURRENT_VERSION.html) / [SOVERSION](https://cmake.org/cmake/help/v3.29/prop_tgt/SOVERSION.html) — Mach-O の版の既定値
