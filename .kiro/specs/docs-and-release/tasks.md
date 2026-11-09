# Implementation Plan

- [x] 1. 基盤: サンプルのビルドオプションと文書用プログラムの登録
- [x] 1.1 `CLOPTS_BUILD_EXAMPLES` と文書用プログラムを登録する関数を用意し、`examples/` と `tests/docs/` をビルドツリーに組み込む
  - ルートの `CMakeLists.txt` に、トップレベルのときだけ既定で ON になる `CLOPTS_BUILD_EXAMPLES` を足す。`cmake/CloptsExamples.cmake` は `CLOPTS_BUILD_EXAMPLES` によらず常に include する（`tests/docs/` も同じ関数を使うため）。ON のときだけ `examples/` を加える。サンプルの登録が先に済むよう、この処理をテストのブロックより前に置く
  - 設計の DocProgramRegistry の契約どおりに、`clopts_add_doc_program(<name> SOURCES ... [ENV ...] [EXAMPLE])` を実装する。作るのは実行ファイル `clopts_doc_<name>`（`OUTPUT_NAME` は `<name>`）。`CLOpts::clopts` をリンクし、`clopts_apply_compiler_options` を付け、Windows の共有ビルドでは `$<TARGET_RUNTIME_DLLS>` をコピーする。グローバルプロパティ `CLOPTS_DOC_PROGRAMS`、`CLOPTS_DOC_PROGRAM_TARGET_<name>`、`CLOPTS_DOC_PROGRAM_ENV_<name>`、`CLOPTS_DOC_EXAMPLES` に記録する。名前の規則違反と重複は `FATAL_ERROR` にする
  - `examples/CMakeLists.txt` には、2 つのサンプルの登録（`clopts_add_doc_program(taskbook SOURCES taskbook/taskbook.c ENV TASKBOOK_FORMAT EXAMPLE)` と `clopts_add_doc_program(textcount SOURCES textcount/textcount.c ENV TEXTCOUNT_ENCODING EXAMPLE)`）を、この時点で書く。各ソースは、0 を返すだけの仮の `main` にしておく（3.1 と 3.2 は自分のソースだけを書き換えればよい）。`tests/docs/CMakeLists.txt` は、ラベル `docs` のテストをここで登録する旨の注釈と、README の最小サンプルの登録 `clopts_add_doc_program(hello SOURCES readme_minimal.c)` だけを持つ骨格として作る（`readme_minimal.c` は 0 を返すだけの仮の `main`。3.3 が書き換える）。そのうえで`tests/CMakeLists.txt` に `add_subdirectory(docs)` を足す
  - 完了条件: 静的・共有の両形式で、`CLOPTS_WERROR=ON` のもとで構成とビルドが通り、既存の `ctest` がすべて通る。`-DCLOPTS_BUILD_EXAMPLES=OFF` でも構成が通り、そのとき `clopts_doc_taskbook` が作られない。検証のために一時的に同じ名前を 2 回登録すると、構成が名前と呼び出し元を示して止まることを確かめてから、その一時的な重複登録（と、そのためだけに置いたファイル）を元に戻す。2 つのサンプルと `readme_minimal.c` の仮のソースは残す（3.1〜3.3 が書き換える）
  - _Requirements: 7.6, 7.7_

- [x] 2. 検査スクリプト
- [x] 2.1 (P) 文書の検査スクリプトの出力の照合（MODE=outputs）を、自己テスト用の入力とともに実装する
  - 設計の「照合用の印の書式」どおりに `clopts-check` の印を読む。属性（`exit`、`stream`、`env`）と、`--` より後ろのコマンドに分ける。印の直後のフェンスから期待出力を取り出す。フェンスの中にある印は対象にしない
  - 空の `WORK` で、空の標準入力を与えてプログラムを実行する。その際、`ENV_VARS` で登録された環境変数をすべて外し、`env=` で指定したものだけを設定する。選んだ出力先の内容と終了コードを、CRLF を LF にそろえてから期待出力と比べる
  - 失敗をすべて集め、`<文書>:<行>:` を前置して、設計の報告の形式で出す。対象は、出力の不一致（最初の違い、期待と実際の行、`WORK` に書き出したファイル）、終了コードの不一致、書式の誤りの各種、未登録のプログラムである。印が 0 件のときと、`REQUIRE` の名前に終了コード 0 の印か 0 以外の印が欠けているときも失敗にする
  - `tests/docs/fixtures/outputs/` に、`cmake` 自身をプログラムとして使う文書を用意する。内容は、一致する例、標準出力の不一致、標準エラーと終了コード、終了コードの不一致、書式の誤りの各種、未登録のプログラム、印が 0 件、`REQUIRE` の不足の各ケース
  - 完了条件: `cmake -DMODE=outputs ... -P tests/docs/check_docs.cmake` を直接呼ぶと、一致する例で照合した印の数を出して成功する。その他の各ケースでは、期待するファイル・行・理由を出して失敗する
  - _Boundary: DocsCheckScript_
  - _Requirements: 8.2, 8.3, 8.4, 8.5, 8.8_

- [x] 2.2 文書の検査スクリプトに、ソースの照合・リンクの検査・API の網羅の検査を加える
  - MODE=sources: `clopts-source` の印の直後のブロックが、指定されたファイルとバイト単位で一致することを確かめる（CRLF の正規化だけを行う）。不一致では最初に違う行を、ファイルがなければその旨を報告する
  - MODE=links: `[t](target)` と `![a](target)` の相対リンクを集め、先が存在しないもの（`broken link`）と、`docs/dev/` を指すもの（`link to maintainer docs`）を報告する。外部 URL、`mailto:`、`#` だけのリンク、コードスパンとコードブロックの中は無視する
  - MODE=api_coverage: 公開シンボル一覧の名前（空行と `#` の注釈を除く）のうち、文書にバッククォート付きで現れないものを、すべて列挙して失敗する
  - 3 つのモードとも、対象が 0 件のときと、指定されたファイルがないときは失敗にする。自己テスト用の入力を `tests/docs/fixtures/sources/`、`links/`、`api_coverage/` に用意する
  - 完了条件: 3 つのモードを直接呼ぶと、正しい入力で成功し、不一致・ファイルなし・切れたリンク・`docs/dev/` へのリンク・不足する名前の各ケースで期待する報告を出して失敗する。コードスパンの中のリンクは報告されない
  - _Depends: 2.1_
  - _Requirements: 2.9, 6.2, 6.5, 8.6, 8.7, 8.8_

- [x] 2.3 (P) CHANGELOG の検査スクリプト（MODE=format と MODE=release）を、自己テスト用の入力とともに実装する
  - MODE=format: 設計の「CHANGELOG の書式」どおりかを確かめ、誤りを `CHANGELOG.md:<行>: <理由>` ですべて報告する。見るのは、最初の `## ` が `## [Unreleased]` であること、版の見出しと日付の形、新しい順で重複がないこと、`### ` が 7 種の分類だけであること、`互換性のない変更` が節の最初にあること
  - MODE=release: format の検査に加えて、タグが `vX.Y.Z` の形であること、`project(clopts VERSION X.Y.Z` がちょうど 1 つあってタグと一致すること、版の節があって空でないことを確かめる。通れば、節の本文を `OUTPUT` に書き、標準出力に `version X.Y.Z` を出す
  - `tests/docs/fixtures/changelog/` に、正しい形式、各種の書式の誤り、仮の `CMakeLists.txt`（タグと一致する版・一致しない版）、節がない・節が空のケース、抜き出した本文の期待値を用意する
  - 完了条件: 直接呼ぶと、正しい形式で成功し、各種の誤りで行番号付きの報告を出して失敗する。release は、タグと版の不一致・節がない・節が空で設計の文言を出して失敗し、正しい入力では期待値と同じ本文を書いて `version X.Y.Z` を出す
  - _Boundary: ChangelogCheckScript_
  - _Requirements: 10.2, 10.3, 10.4, 11.2, 11.3, 11.4_

- [x] 2.4 検査スクリプトの自己テストを CTest に登録する
  - `tests/docs/CMakeLists.txt` で、2.1〜2.3 の各ケースを `docs_selftest.<case>` として登録する（ラベル `docs`）。失敗を期待するケースは `PASS_REGULAR_EXPRESSION` で期待する報告に合わせ、ほかの理由の失敗が通らないようにする
  - 完了条件: `ctest -L docs` で、登録した自己テストがすべて通る。期待する報告を一時的に変えると、そのテストが失敗することを 1 件確かめてから戻す
  - _Requirements: 8.1_

- [x] 3. 文書用のプログラム
- [x] 3.1 (P) サブコマンドありのサンプル taskbook を作る
  - 設計の TaskbookExample どおりに宣言する。グローバルの `-v/--verbose`（カウント、上限 3）と `--color`（否定できるフラグ）を持ち、`add TITLE`（`--tag` リスト、`--priority` 既定値付き）、`list`（`--all`、`--format` 環境変数 `TASKBOOK_FORMAT` と既定値）、`done ID...` を持つ。各サブコマンドのハンドラを `clopts_result_run` で呼ぶ
  - 「サンプルの共通の規約」に従う。`clopts_argv_get` と `clopts_env_new` / `clopts_parse_with_env` を使い、状態ごとの出力先と終了コード（0 / 2 / 1）を守り、全経路で解放し、`<key>: <value> (<出所>)` の報告をする
  - 1.1 で登録済みの仮のソース `examples/taskbook/taskbook.c` を書き換える（`examples/CMakeLists.txt` は変えない）
  - 完了条件: 静的・共有の両形式で、`CLOPTS_WERROR=ON` のもとで警告なくビルドできる。`taskbook --help`、`taskbook add --help`、`taskbook --version` が終了コード 0 で標準出力に出す。未知のオプションと、サブコマンドの省略が終了コード 2 で標準エラーに出す。`add` の実行で、値と出所が報告される
  - _Boundary: TaskbookExample_
  - _Depends: 1.1_
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.8_

- [x] 3.2 (P) サブコマンドなしのサンプル textcount を作る
  - 設計の TextcountExample どおりに宣言する。`-l/--lines`・`-w/--words`、否定できる `--total`、カウント `-q/--quiet`、`-e/--encoding`（環境変数 `TEXTCOUNT_ENCODING`、既定値 `utf-8`）、`-x/--exclude`（区切り文字 `,` のリスト）、可変長の必須の `FILE` を持つ
  - 「サンプルの共通の規約」に従い、どのファイルをどの設定で数えるかを報告するだけにする（ファイルは読まない）
  - 1.1 で登録済みの仮のソース `examples/textcount/textcount.c` を書き換える（`examples/CMakeLists.txt` は変えない）
  - 完了条件: 静的・共有の両形式で、警告なくビルドできる。`textcount --help` と `--version` が終了コード 0 で出す。`FILE` の省略と未知のオプションが終了コード 2 で標準エラーに出す。`--`、`-`、`--no-total`、`-qqq`、環境変数と明示指定の優先順位を試した実行で、規則どおりの値と出所が報告される
  - _Boundary: TextcountExample_
  - _Depends: 1.1_
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.8_

- [x] 3.3 (P) README の最小サンプル hello を作り、テストのビルドに加える
  - 設計の ReadmeMinimal どおりに、オプション 1 つと位置引数 1 つを持つ `hello 1.0.0` を、README にそのまま載せられる短さで書く。中身は、宣言、`clopts_parse`、状態による分岐、値の参照、ヘルプとエラーの出力、解放
  - 1.1 で登録済みの仮のソース `tests/docs/readme_minimal.c` を書き換える（`tests/docs/CMakeLists.txt` は変えない）
  - 完了条件: テストを有効にした構成で、静的・共有の両形式で警告なくビルドできる。`hello --help` が終了コード 0 で、未知のオプションが終了コード 2 で、それぞれの出力先に出す
  - _Boundary: ReadmeMinimal_
  - _Depends: 1.1_
  - _Requirements: 1.4, 8.6_

- [x] 3.4 取り込まれたときのサンプルの既定を確かめるテストを加える
  - `tests/docs/fixtures/subproject/` に、CLOpts を `add_subdirectory` で取り込む利用側を作る。`EXPECT_EXAMPLES` が ON でなければ、`TARGET clopts_doc_taskbook` があるときに `FATAL_ERROR` にする。ON なら、ないときに `FATAL_ERROR` にする
  - これを `tests/cmake/run_fixture.cmake`（EXPECT=configure_ok）で起動する `docs.examples_subproject_default` と `docs.examples_subproject_on`（`-DCLOPTS_BUILD_EXAMPLES=ON -DEXPECT_EXAMPLES=ON`）を、`tests/docs/CMakeLists.txt` に登録する（ラベル `docs`）
  - 完了条件: 両方のテストが通る。fixture の判定を一時的に逆にすると、両方とも失敗することを確かめてから戻す
  - _Depends: 2.4_
  - _Requirements: 7.6, 7.7_

- [x] 4. 利用者向けの文書
- [x] 4.1 (P) API リファレンスの章を書く
  - 設計の ApiReferenceDoc の節の構成どおりに書く。全体の流れ、ハンドルと寿命の表、宣言、解析と状態の表、結果とエラーのアクセサ、使い方エラー 7 種と宣言エラー 15 種の表、文面の出力、メモリの所有と寿命、プラットフォームの関数の一覧
  - 内容は公開ヘッダの注釈と矛盾させない。各公開関数を少なくとも 1 回、バッククォート付きで書く。taskbook の使い方エラーの出力を `clopts-check` の印付きで載せる
  - 完了条件: `check_docs.cmake` の api_coverage を `abi/clopts.symbols` と `docs/api.md` で直接呼ぶと成功する。outputs をビルドした taskbook で直接呼ぶと、この章の印がすべて一致する
  - _Boundary: ApiReferenceDoc_
  - _Depends: 2.2, 3.1_
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 6.3, 6.4, 8.3_

- [x] 4.2 (P) 解析規則の章を書く
  - 設計の ParsingDoc どおりに、3.1〜3.8 の規則ごとに節を設け（各節に実行例を置く）、textcount（グローバルオプションと並べ替えは taskbook）の実行例を `clopts-check` の印付きで載せる。優先順位の例では `env=` を使う
  - textcount と taskbook のヘルプ出力を印付きで載せる。textcount については、終了コード 0 の印のほかに、終了コードが 0 でない使い方エラーの印（3.4 の前方一致を試した例など）を少なくとも 1 つ載せる
  - 完了条件: outputs をビルドした taskbook と textcount で直接呼ぶと、この章の印がすべて一致し、`REQUIRE=textcount` を渡すと、終了コード 0 と 0 以外の両方の印があることで通る。各規則（3.1〜3.8）に少なくとも 1 つの説明があり、3.1〜3.8 のすべてに実行例がある（3.8 は、数値を期待しそうなオプションに数値でない値を渡しても、そのまま文字列として報告される例）
  - _Boundary: ParsingDoc_
  - _Depends: 2.1, 3.1, 3.2_
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 6.3, 8.3_

- [x] 4.3 (P) 取り込み手順の章を書く
  - 設計の IntegrationDoc どおりに書く。3 通りの手順（FetchContent は版のタグを指定）、CMake オプションの表と親からの上書き、形式の選択（1 つの prefix に 1 つの形式、prefix を分ける手順、同じ prefix への上書き）、静的と共有の目安、MSVC の CRT、CI で検査している経路
  - 記述例のターゲット名と `find_package` の呼び方を、`tests/integration/fixtures/consumer`、`cmake/CLOptsConfig.cmake.in` の挙動と突き合わせる
  - 完了条件: 記述例と統合テストの fixture の対応（経路ごとに、どの fixture の何行と同じ形か）を確かめた結果を示せる。links を直接呼ぶと、この章のリンク（`docs/abi.md` を含む）が解決する
  - _Boundary: IntegrationDoc_
  - _Depends: 2.2_
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 6.3, 6.4_

- [x] 4.4 (P) Windows での注意の章を書く
  - 設計の WindowsDoc どおりに書く。UTF-8 の argv の取得と解放の順序、`wmain` と `clopts_argv_from_wide`、環境変数の UTF-8 での取得と差し込み、`activeCodePage` との関係、DLL 名と PATH の衝突（`docs/abi.md` へのリンク）、コンソールのコードページ
  - 完了条件: 内容が `include/clopts/platform.h` の注釈（所有、寿命、他の OS でのパススルー）と矛盾しない。links を直接呼ぶと、この章のリンクが解決する
  - _Boundary: WindowsDoc_
  - _Depends: 2.2_
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 6.3, 6.4_

- [x] 4.5 README と `docs/` の入口を書く
  - 設計の ReadmeDoc どおりに、README を全面的に書き直す。概要（Sirraide/clopts との区別）、特徴、最小サンプル（`clopts-source` の印と `hello --help` の `clopts-check` の印）、取り込み方 3 通りと形式の選び方、対応環境、文書への導線、ライセンス（MIT と The CLOpts Authors だけ。Monde には触れない）
  - `docs/README.md` に、章の表（api、parsing、integration、windows、abi）と読む順序と README への導線を置く
  - 完了条件: sources と links を直接呼ぶと成功する（README の最小サンプルが `tests/docs/readme_minimal.c` と一致し、README と `docs/*.md` の相対リンクがすべて解決し、`docs/dev/` を指さない）。README の取り込み方の記述例が 4.3 の章と同じ形である
  - _Depends: 2.2, 3.3, 4.1, 4.2, 4.3, 4.4_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 6.1, 6.2, 6.3, 6.4_

- [x] 5. リリース
- [x] 5.1 (P) CHANGELOG を作る
  - 設計の Changelog どおりに、冒頭の説明（semver と書式の説明）と `## [Unreleased]` の節を書く。`### 追加` には、初版で入る機能（宣言と解析、文面、Windows の UTF-8、静的／共有と ABI の検査、文書とサンプル）を列挙する
  - 完了条件: `check_changelog.cmake` の format を直接呼ぶと、`changelog: 0 release section(s)` を出して成功する
  - _Boundary: Changelog_
  - _Depends: 2.3_
  - _Requirements: 10.1, 10.2, 10.3, 10.4_

- [x] 5.2 (P) リリース手順書と Naos への要望の記録先を書く
  - 設計の ReleaseGuide の節の構成どおりに、`docs/dev/release.md` を書く。関係するファイル、版番号の規則、SOVERSION の判断、リリースの手順（実行するコマンド付き）、ワークフローの動きとソース配布の理由、失敗したときの戻し方、文書と CHANGELOG の更新の規約、Naos の更新手順、ワークフローを試す方法
  - `docs/dev/naos-feedback.md` を、記録の書式と追記だけを行う規約を持つ、記録 0 件のファイルとして作る
  - 完了条件: 手順書に書いたコマンドのうち、`check_changelog.cmake` の release（`CHANGELOG=` で仮の CHANGELOG を渡し、`TAG=v0.1.0`）と `ctest -L abi` が、この時点のリポジトリで書いたとおりに動く（`ctest -L docs` の確認は 6.1 で行う）。SOVERSION の規則は `docs/abi.md` を参照するだけで、書き写していない
  - _Boundary: ReleaseGuide, NaosFeedback_
  - _Depends: 2.3_
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 10.5_

- [x] 5.3 (P) リリースのワークフローとアーカイブから除くパスを用意する
  - `.gitattributes` に `ref export-ignore` と `.gitmodules export-ignore` を書く
  - 設計の ReleaseWorkflow の契約どおりに `.github/workflows/release.yml` を書く。起動はタグ `v[0-9]+.[0-9]+.[0-9]+`、権限はジョブ単位の `contents: write`、タグは環境変数で渡す。手順は、タグの形式、CHANGELOG とタグと版の検査、`git archive` による tar.gz と zip、展開して `ref` と `.gitmodules` がないことの確認、`-DBUILD_TESTING=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON` での構成とビルド、`SHA256SUMS`、`gh release create --verify-tag --notes-file`。`ci.yml` は変えない
  - 完了条件: ワークフローの手順 3〜6 を、手元で同じコマンドで再現する。手順 3 は、`## [0.1.0] - <日付>` の節を持つ仮の CHANGELOG を `CHANGELOG=` で渡し、`TAG=v0.1.0` で実行する。手順 4 は、未コミットの `.gitattributes` が効くよう `git archive --worktree-attributes HEAD` で作る（ワークフローでは、コミット済みの `.gitattributes` がタグの木から読まれる）。そのうえで、アーカイブに `ref/` と `.gitmodules` がないこと、展開したものがオフラインの構成でビルドできることを示す。リモートに仮のタグを push して全体を試すのは、ユーザーの承認を得た場合だけ行う。承認がなければ、未実施であることと、試す手順を記録する
  - _Boundary: ReleaseWorkflow, GitAttributes_
  - _Depends: 2.3_
  - _Requirements: 11.1, 11.4, 11.5, 11.6, 11.7, 11.8_

- [x] 6. 統合と検証
- [x] 6.1 実際の文書の検査を CTest に登録し、全体を通す
  - `tests/docs/CMakeLists.txt` に、設計の DocsTests の表どおりに `docs.outputs`（`CLOPTS_BUILD_EXAMPLES` が ON のときだけ）、`docs.sources`、`docs.links`、`docs.api_coverage`、`docs.changelog` を登録する。outputs の `PROGRAMS`、`ENV_VARS`、`REQUIRE` は、グローバルプロパティから `$<TARGET_FILE:...>` で組み立てる。`docs/*.md` は `CONFIGURE_DEPENDS` 付きの glob で集め、名前の順に並べる
  - 完了条件: Linux の静的・共有の両形式で、`CLOPTS_WERROR=ON` のもとで構成・ビルドし、`ctest -L docs` がすべて通る。`docs.outputs` は、taskbook と textcount の両方に終了コード 0 と 0 以外の印があることを含めて通る。`-DCLOPTS_BUILD_EXAMPLES=OFF` では `docs.outputs` が登録されず、他の `docs.*` は通る
  - _Depends: 3.4, 4.5, 5.1_
  - _Requirements: 2.9, 6.5, 8.1, 8.2, 8.3, 8.6, 8.7, 10.1_

- [x] 6.2 全体の回帰と CI での確認
  - 静的・共有の両形式で、テストを含む全体を構成・ビルドし、`ctest` の全件（既存の core / parser / help / platform / toolchain / integration / abi と新しい docs）が通ることを確かめる
  - CI は main への push と pull request でしか動かないので、ユーザーの承認を得て pull request を作り、CI の 10 ジョブと `abi-compare` の結果を確かめる。Windows（MSVC、MinGW）と macOS で、サンプルのビルドと `docs.outputs` が通ることを確かめ、失敗があれば原因を直す
  - 完了条件: 手元の全件の結果を示せる。承認を得て CI を動かした場合は、全ジョブが成功した実行の記録（実行の識別子とジョブごとの結果）を示せる。承認がなければ、CI での確認が未実施であることと、その手順を記録する
  - _Requirements: 7.8, 8.1, 11.8_

## Implementation Notes
- 1.1: `clopts_add_doc_program` は重複時に両方の呼び出し元を示すため、設計にない内部用のグローバルプロパティ `CLOPTS_DOC_PROGRAM_FILE_<name>` を持つ（他のコンポーネントは読まない）。ビルド成果物は `build/examples/taskbook`、`build/tests/docs/hello` に出る。
- 2.1: `check_docs.cmake` は印の環境を `cmake -E env` ではなくスクリプト内の `set/unset(ENV{})` で隔離する（登録外の変数は元の値に戻す）。値が空の `env=NAME=` と `;` を含む印は書式の誤り。理由の文字列に生の `;` を入れるとリストが割れるので、`_docs_semicolon` の代替文字を使い、最後の報告で戻す。自己テストの match と stderr_mismatch は `cmake -E cat missing.txt` の文面に依存する。
- 2.2: links モードはフェンスを 0〜3 文字の字下げでしか認識しない。文書（4.x）のコードブロックは行頭（字下げ 3 文字以内）に置くこと。参照形式のリンク `[t]: x` は検査されない。api_coverage は `SYMBOLS=` で一覧を受け取り、不足を `<一覧>:<行>:` で報告する。
- 2.3: `check_changelog.cmake` の release は `SOURCE_DIR`（`${SOURCE_DIR}/CMakeLists.txt` を読む）と `CHANGELOG`（既定 `${SOURCE_DIR}/CHANGELOG.md`）を受け取り、成功時は標準出力にちょうど `version X.Y.Z` の 1 行だけを出す。「空の節」は空行だけの節を指す（`###` の見出しだけの節は空とみなさない）。フェンスは認識しないので、CHANGELOG のコードブロックに `## ` の行を書かないこと。コメントアウトした `project(clopts VERSION` は数えない。先頭が 0 の版（`v01.2.3`）は拒否する。
- 2.4: 自己テストは `docs_selftest.<mode>.<case>`（61 件）。期待行はヘルパーが正規表現にエスケープして `(^|\n) *<行>\r?\n` で順に結ぶ。release の成功ケースは `tests/docs/run_release_selftest.cmake` が抜き出した本文を期待値と比べる。
- 3.1: サンプルの報告の書式は、1 行目 `command: <name>`、続いてそのコマンドの値、最後にグローバル（`verbose`、`color`）。オプションは `key: value (explicit|env|default)`、値を得なかったオプション（出所 NONE）と位置引数には出所を付けない。リストは `", "` で結ぶ。taskbook の版は 1.2.3。空の `TASKBOOK_FORMAT=` は `format:  (env)`（空白 2 つ）になるので文書の例に使わない。
- 3.2: textcount（0.4.0）の報告は `files:` から始まり、宣言順に続く。`-q` に上限はなく、`--total` に既定値はない。3.5 の上限の例と 3.7 の `help <cmd>` は taskbook（`-vvvv`）で示す。3.8 の数値でない値の例には、数値らしい引数を持つ taskbook（`done abc` など）を使う。
- 3.3: 共通の規約は README の最小サンプルにも適用される（例外は環境変数の行だけ）。hello も `clopts_argv_get` を使い、到達しない NO_COMMAND の分岐（終了コード 2）を持つ。ヘルプと使い方の案内には `clopts_result_command` / `clopts_result_error_command` を渡す。ファイルは 78 行で、README には 1 バイトも変えずに載せる。
- 4.1: api_coverage は `` `name` `` か `` `name()` `` だけを数えるので、引数付きで書くときは `` `name`（`args`）`` の形にする。docs/api.md は parsing.md と windows.md へリンクしているので、links の検査は 4.2 と 4.4 が済むまで通らない。
- 4.3: docs/integration.md の FetchContent の例は、設計の ReadmeDoc どおり `ktdevx/clopts-private.git` と `GIT_TAG v0.1.0` を使う（タグはリリースまで存在しない）。Windows の DLL のコピーは共有版のときだけ行う（静的版では `$<TARGET_RUNTIME_DLLS>` が空になり、`copy_if_different` が失敗する）。README の記述例もこの形に合わせること。
- 5.2: リリース手順書は、リリースのワークフローを「タイトル `CLOpts X.Y.Z`、添付は `clopts-X.Y.Z.tar.gz`、`clopts-X.Y.Z.zip`、`SHA256SUMS`、ノートは `$RUNNER_TEMP` に書く」と記述している。5.3 の `release.yml` はこれに合わせる。手順書の FetchContent の URL（`ktdevx/clopts-private.git`）は、公開先が決まったら README と docs/integration.md と一緒に直す。
- 5.3: リリースのワークフローの手順 3〜6 は手元で再現した（`git archive --worktree-attributes`、ネットワークなしの構成とビルド、`SHA256SUMS` の検証）。リモートに仮のタグを push して全体を試すことは、ユーザーの承認がないので未実施。試す手順は docs/dev/release.md の「リリースのワークフローを試す方法」にある。`git archive` は同名のブランチとの曖昧さを避けるため `refs/tags/$TAG` を指定する。
- 6.1: 実際の文書の検査（`docs.outputs`、`docs.sources`、`docs.links`、`docs.api_coverage`、`docs.changelog`）は、リストを生の `;` を含む引用付きの `-D` 引数で渡す（`cmake_parse_arguments(PARSE_ARGV)` 経由なのでエスケープは不要）。利用者向けの文書は README.md と `docs/*.md`（再帰しないので `docs/dev/` は除かれる）。
- 6.2: 手元（Linux）では、`CLOPTS_WERROR=ON` のもとで静的版 694 件・共有版 696 件の ctest がすべて通った（`-L docs` は 68 件）。ユーザーの承認を得て PR #15 を作り、CI の実行 42543436516（head d3187ea）で 10 ジョブ（linux-gcc/clang、macos-appleclang、windows-msvc/mingw の各静的・共有）と `abi-compare` がすべて成功した。windows-msvc-shared では 733 件が通り、`docs` ラベルの 68 件（`docs.outputs` を含む）も通った。
