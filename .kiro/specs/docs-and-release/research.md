# 調査と設計判断の記録

## Summary
- **Feature**: `docs-and-release`
- **Discovery Scope**: Extension（既存のライブラリ、ビルド基盤、テスト基盤、CI に、文書・サンプル・検査・リリースの仕組みを足す）
- **Key Findings**:
  - 文書が説明する公開 API は 6 つの公開ヘッダ（`decl.h`、`parse.h`、`help.h`、`platform.h`、`writer.h`、`export.h`）にまとまっていて、公開シンボル一覧 `abi/clopts.symbols` には 65 個の関数がある。ヘッダの注釈に、寿命・所有・NULL の扱い・スレッドの規約がすでに英語で書かれている。そのため API リファレンスは新しい規約を作らず、それらを日本語で体系立てて並べ直す文書になる。
  - CTest から `cmake -P` のスクリプトを起動し、自己テストを `PASS_REGULAR_EXPRESSION` で確かめる検査の型は `tests/abi/` に確立している。CI のワークフローから `tests/` 配下のスクリプトを直接呼ぶ前例も、`abi-compare` ジョブ（`tests/abi/run_abidiff.cmake`）にある。文書の検査とリリースの検査はこの型に合わせる。
  - Monde の `docs_check.cmake` は、印（`<!-- monde-check: help -->`）の直後にあるコードブロックを `--help` の出力と比べる。印に引数・終了コード・出力先の情報はない。CLOpts の文書では解析規則の例やエラー出力も照合したいので、印に引数・終了コード・出力先・環境変数を書けるように一般化する。

## Research Log

### 公開 API の範囲と既存の注釈
- **Context**: API リファレンス（要件 2）と整合検査（8.7）が対象とする公開関数の範囲を確かめる。
- **Sources Consulted**: `include/clopts/*.h`、`abi/clopts.symbols`、`docs/abi.md`
- **Findings**:
  - ハンドルは `clopts_program_t`、`clopts_command_t`（root を含む）、`clopts_option_t`、`clopts_positional_t`、`clopts_result_t`、`clopts_argv_t`、`clopts_env_t` の 7 種類ある。前の 4 つは program が所有し、`clopts_program_free` まで有効である。
  - 状態の列挙 `clopts_status_t` は 7 値（OK、HELP、VERSION、NO_COMMAND、USAGE_ERROR、DECL_ERROR、OUT_OF_MEMORY）。使い方エラーは 7 種、宣言エラーは 15 種、値の出所は 4 値ある。ほかに `clopts_write_status_t`、`clopts_argv_status_t`、`clopts_env_status_t`、`clopts_env_error_t` がある。
  - 結果から得る文字列は、結果を解放するまで、かつ program と argv が生きている間だけ有効である（位置引数は argv の要素そのものを返す）。`clopts_result_free` は program の解放の前後どちらで呼んでもよい。
  - `clopts_file_write` は `writer.h` の `static inline` 関数なので、公開シンボル一覧には載っていない。`clopts_argv_from_wide` は Windows だけで宣言される。
  - `docs/abi.md` は abi-compat が所有する利用者向けの章で、Windows の DLL 名、SOVERSION、互換を保つ変更と壊す変更をすでに説明している。この章から他の文書へのリンクはない。
- **Implications**: API リファレンスは公開ヘッダの章立て（宣言、解析と結果、文面、プラットフォーム）に沿って並べる。整合検査は公開シンボル一覧を基準にし、`static inline` の便宜関数は検査の対象外とする（文書には載せる）。Windows の DLL の話は `docs/abi.md` へのリンクで済ませ、重複して書かない。

### 既存のビルド基盤とテスト基盤
- **Context**: サンプルと検査をどこにどう組み込むかを決める。
- **Sources Consulted**: `CMakeLists.txt`、`cmake/Clopts*.cmake`、`tests/CMakeLists.txt`、`tests/abi/CMakeLists.txt`、`tests/integration/`、`.github/workflows/ci.yml`
- **Findings**:
  - `CLOPTS_BUILD_EXAMPLES` はまだない。トップレベルを既定とするオプションは `CLOPTS_BUILD_TESTING`、`CLOPTS_INSTALL`、`CLOPTS_WERROR` の 3 つで、いずれも `option(... ${PROJECT_IS_TOP_LEVEL})` で定義されている。
  - `clopts_apply_compiler_options(<target>)` は、C99 の指定と警告の設定（`CLOPTS_WERROR` のときはエラー化）をターゲット単位で付ける。
  - Windows の共有ビルドで実行ファイルの横に DLL を置く方法として、統合テストの fixture が `$<TARGET_RUNTIME_DLLS:app>` を POST_BUILD でコピーしている。
  - CI は 5 環境 × 静的／共有の 10 ジョブで、トップレベルとして構成して `ctest` を全件実行する。タグでは起動しない（`on.push.branches: [main]` と `pull_request`）。
  - 統合テストは `add_subdirectory` 経路（`integration.subdirectory`）と install → `find_package` 経路（`integration.install_find_package`）を静的版・共有版の両方で検査する。`integration.package_selection` は `COMPONENTS static|shared`、`CLOPTS_SHARED_LIBS`、`BUILD_SHARED_LIBS` による形式の選択を 8 通り検査する。
  - package config は、静的版の install 時の MSVC の CRT 種別を記録し、利用側の CRT 種別と違えば警告する（`cmake/CLOptsConfig.cmake.in`、`CLOptsStaticRuntime.cmake.in`）。
- **Implications**: サンプルはトップレベルでは既定でビルドされるので、`ci.yml` を変えなくても CI の全環境でサンプルのビルドと `docs` ラベルのテストが走る（7.8、8.1、11.8）。取り込み手順の記述例は統合テストの fixture と同じ形にそろえる（4.7）。

### Monde の文書検査とリリースの仕組み
- **Context**: 参考として示された Monde の方式を、どこまで踏襲するかを決める。
- **Sources Consulted**: `ref/monde/tests/docs/docs_check.cmake`、`ref/monde/tests/docs/CMakeLists.txt`、`ref/monde/.github/workflows/release.yml`、`ref/monde/docs/dev/release.md`、`ref/monde/tools/release/check_version.cmake`、`ref/monde/docs/dev/naos-feedback.md`
- **Findings**:
  - docs_check は MODE ごとに CTest のテストを分けている（help、quickstart、links、notices）。失敗はすべて集めてから `FATAL_ERROR` で報告し、各行の先頭に `<相対パス>:<行>:` を付ける。比較の前の正規化は CRLF → LF だけ。不一致のときは expected / actual のファイルを書き出し、最初に違う行を示す。照合の対象が 0 件なら失敗する。
  - リンクの検査は README と `docs/*.md`（再帰しない）が対象で、外部 URL・`mailto:`・アンカーだけのリンクを飛ばし、コードブロックとコードスパンの中を無視する。
  - release.yml はタグ `v[0-9]+.[0-9]+.[0-9]+` で起動し、`check_version.cmake` で `project(VERSION)` とタグを比べてから、`gh release create --verify-tag` で Release を作る。タグはスクリプトに埋め込まず、環境変数で渡す。権限は既定を `contents: read` にし、Release を作るジョブだけ `contents: write` にしている。
  - Monde には CHANGELOG がなく、Release の本文は手で書く。Naos の版更新手順も文書にない。naos-feedback.md は要望を記録する書式を定めている。
- **Implications**: 失敗をすべて集めてから報告する方式、`<ファイル>:<行>:` の書式、CRLF の正規化、0 件で失敗すること、リンク検査の規則、タグの受け渡し方、権限の分け方は踏襲する。印の書式は一般化する（下の判断を参照）。Release の本文は CHANGELOG から自動で抜き出す（要件 11.4）。Naos の手順と naos-feedback.md は本 spec で用意する。

### ソースアーカイブと submodule
- **Context**: 11.6（`ref/` を含めない）と 11.7（ネットワークなしでビルドできる）の実現方法を決める。
- **Sources Consulted**: git-archive のマニュアル（`export-ignore` 属性、submodule の扱い）、CMake の FetchContent のマニュアル（`FETCHCONTENT_FULLY_DISCONNECTED`）、GitHub の自動生成ソースアーカイブの挙動
- **Findings**:
  - `git archive` は submodule の中身を含めない。ただし gitlink のパス（空のディレクトリ）と `.gitmodules` は残る。`.gitattributes` の `export-ignore` を付けたパスはアーカイブから除かれる。GitHub が Release に自動で付ける「Source code」アーカイブも `git archive` 相当で作られ、`export-ignore` に従う。
  - `FETCHCONTENT_FULLY_DISCONNECTED=ON` を付けると FetchContent はダウンロードも更新もしない。Naos の取得は `BUILD_TESTING` が ON のときだけ行われるので、テストを無効にした構成はネットワークを使わない。これを ON にしたうえで構成が通れば、取得に頼っていないことを確かめられる。
- **Implications**: `.gitattributes` で `ref` と `.gitmodules` に `export-ignore` を付ける。ワークフローは、展開したアーカイブに `ref/` がないことを確かめてから、`-DBUILD_TESTING=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON` で構成・ビルドする。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| A: Monde と同じ固定の印 | `help` / `help <cmd>` の印だけを扱い、引数は印の種類で決まる | 実装が小さい | エラー出力・解析規則の例・環境変数の例を照合できず、要件 8.3 と解析規則の例が手書きになる | 退けた |
| B: 一般化した印（採用） | 印にプログラム名・引数・終了コード・出力先・環境変数を書く | ヘルプ、エラー、解析規則の例を 1 つの仕組みで照合できる | 印の書式を少し覚える必要がある | 書式は release.md と自己テストで固定する |
| C: 文書から生成 | サンプルを実行して文書の該当部分を書き換えるスクリプト | 手で写す手間がない | 文書の変更が自動の書き換えに頼る。CI で差分の有無を見る仕組みが別に要る | 退けた（照合で十分） |

## Design Decisions

### Decision: 印の書式を一般化する
- **Context**: 要件 8.2〜8.5 は、ヘルプだけでなく使い方エラーの出力（標準エラー、終了コードが 0 以外）も照合することを求めている。解析規則の章でも、規則ごとの実行例を照合したい。
- **Alternatives Considered**:
  1. Monde と同じく `help` の種類だけの印にする。
  2. 印に属性と実行するコマンドを書く。
- **Selected Approach**: `<!-- clopts-check: [exit=<n>] [stream=stdout|stderr] [env=<NAME>=<VALUE>]... -- <program> [<arg>...] -->` とする。`--` より後ろは `separate_arguments(UNIX_COMMAND)` で分割するので、引用符が使える。印の次の行から始まるコードブロックを、期待する出力とする。
- **Rationale**: ヘルプ、エラー、解析規則の例を 1 つの書式で扱える。`--` で属性とコマンドを分けるので、属性の名前と引数がぶつからない。
- **Trade-offs**: Monde の印より長くなる。書式の誤りは検査で必ず報告する（8.5）。
- **Follow-up**: 書式の誤りの種類ごとに自己テストを持つ。

### Decision: 検査とリリース用のスクリプトを `tests/docs/` に置く
- **Context**: CHANGELOG の形式の検査とリリースノートの抜き出しは、CTest からもリリースのワークフローからも使う。structure.md は、`src/` に対応しないテストディレクトリを 4 つ（toolchain、integration、docs、abi）に限っている。
- **Alternatives Considered**:
  1. Monde と同じく `tools/release/` を新しく作る（steering の更新が要る）。
  2. `tests/docs/` に置き、CTest とワークフローの両方から呼ぶ（`tests/abi/run_abidiff.cmake` の前例と同じ）。
- **Selected Approach**: 2。CHANGELOG は文書なので、その形式の検査は `docs` ラベルの範囲に入る。タグと版の照合は、リリースノートを抜き出すモードの前提条件としてまとめる。
- **Rationale**: steering のディレクトリ規則を変えずに済み、既存の前例に沿う。
- **Trade-offs**: リリースのワークフローが `tests/` 配下のスクリプトに依存する。このスクリプトはライブラリのビルドを必要としない `cmake -P` なので、テストのビルドなしで呼べる。

### Decision: サンプルは解析結果を報告するだけにする
- **Context**: 文書のサンプル出力は CI の全 OS で決定的でなければならない（8.2）。
- **Alternatives Considered**:
  1. 実際にファイルを読む・状態を保存するツールにする。
  2. 解析した結果（選ばれたコマンド、各値とその出所、位置引数）を標準出力に報告するだけにする。
- **Selected Approach**: 2。taskbook はサブコマンドごとのハンドラ（`clopts_result_run`）で「何をするか」を報告し、textcount は「どのファイルを何の設定で数えるか」を報告する。
- **Rationale**: ファイル・時刻・OS に依存しない出力になり、解析規則の例（`--`、並べ替え、`--no-x`、カウント、優先順位）をそのまま照合できる。
- **Trade-offs**: 実用的な処理は含まない。雛形としての価値は、宣言・解析・表示・終了コード・後始末の流れにある。

### Decision: README の最小サンプルを、テストでビルドするソースと照合する
- **Context**: 8.6 は README の最小サンプルとビルドされるコードが一致することを求め、7.1 はサンプルを 2 つに限っている。
- **Selected Approach**: 最小サンプルのソースを `tests/docs/readme_minimal.c` に置いてテストのビルドでコンパイルし（プログラム名 `hello`）、README の印 `<!-- clopts-source: tests/docs/readme_minimal.c -->` の直後のコードブロックがこのファイルとバイト単位で一致することを検査する。出力の照合にも使えるよう、印の対象プログラムとして登録する。
- **Rationale**: サンプルの数を増やさずに、README のコードがコンパイルできることを保証できる。
- **Trade-offs**: README のコードを直すときは、2 か所（ファイルと README）を同時に直す必要がある。検査が食い違いを報告する。

### Decision: 文書に使うプログラムの登録を CMake の関数で一元化する
- **Context**: 出力の照合は、印のプログラム名から実行ファイルのパスと、隔離すべき環境変数を知る必要がある。Windows の共有ビルドでは、実行ファイルの横に DLL が要る。
- **Selected Approach**: `cmake/CloptsExamples.cmake` に `clopts_add_doc_program(<name> SOURCES ... [ENV <var>...] [EXAMPLE])` を置く。この関数は実行ファイルを作り、コンパイラオプションを付け、Windows の共有ビルドでは DLL をコピーし、名前・ターゲット・環境変数をグローバルプロパティに記録する。`tests/docs/` はこのプロパティから照合のテストの引数を組み立てる。
- **Rationale**: サンプルを足すときに変える場所が 1 か所で済む。印の解決、環境変数の隔離、8.3 の「各サンプルについて最低限の印がある」の検査が、同じ情報源を使う。

### Decision: 単純化
- CI の設定は変えない。サンプルはトップレベルで既定でビルドされ、`ctest` の全件実行に `docs` ラベルが含まれる。
- 取り込み経路の統合テストは足さない（ユーザーの判断）。
- 英語版の文書は作らない。
- アーカイブから除くのは `ref` と `.gitmodules` だけにする。`.kiro/` や `.claude/` は害がないので残す。
- Release を再実行するための `workflow_dispatch` は設けない。失敗したら、タグと Release を消して打ち直す手順を release.md に書く。

## Risks & Mitigations
- 公開リポジトリの URL が決まっていない（現在は `ktdevx/clopts-private`）。FetchContent の記述例に URL を書く必要がある — 記述例の URL は README と取り込み手順の 2 か所だけに置き、release.md のリリース前の確認項目に「URL が公開先と一致すること」を入れる。
- Windows のコンソールは標準出力の `\n` を `\r\n` に変える — 比較の前に両側の CRLF を LF にそろえる。
- 利用者の環境変数がサンプルの出力に混ざる — 照合するときは、登録された環境変数をすべて外してから、印で指定したものだけを設定する。
- 公開関数を足したときに API リファレンスの更新を忘れる — 8.7 の網羅検査で CI が失敗する。
- release.yml は CI では実行されない — タグと版の照合、CHANGELOG の検査と抜き出しは、CTest の自己テストで確かめる。ワークフロー全体は、release.md に書いた、フォークや仮のタグで試す手順で確かめる。

## References
- `ref/monde/tests/docs/docs_check.cmake` — 文書と出力の照合、リンクの検査の参考
- `ref/monde/.github/workflows/release.yml`、`ref/monde/tools/release/check_version.cmake` — タグと版の照合、gh CLI による Release の作成の参考
- [git-archive](https://git-scm.com/docs/git-archive)、[gitattributes: export-ignore](https://git-scm.com/docs/gitattributes#_creating_an_archive) — アーカイブから除くパス
- [CMake FetchContent: FETCHCONTENT_FULLY_DISCONNECTED](https://cmake.org/cmake/help/latest/module/FetchContent.html) — オフラインでの構成の確認
- [Keep a Changelog 1.1.0](https://keepachangelog.com/ja/1.1.0/) — 変更履歴の形式の参考（分類と未リリースの節）
- [gh release create](https://cli.github.com/manual/gh_release_create) — `--verify-tag`、`--notes-file`
