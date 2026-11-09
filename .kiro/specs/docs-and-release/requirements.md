# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts を使おうとする開発者は、取り込み方（`FetchContent` / `add_subdirectory` / `find_package`、静的と共有の選び方）、ビルダー API での宣言の書き方、結果とエラーの読み方、ヘルプとエラーの表示の仕方、Windows での注意を、ソースを読まずに知りたい。保守者は、リリースの手順（版番号の付け方、SOVERSION、ABI 検査の基準、タグ、変更履歴）が決まっていないと、安定した版を利用者に示せない。

**現状**: `README.md` は `# CLOpts` の 1 行だけ。project-foundation が install/export（静的・共有の両方）を、core-parser / help-and-messages / platform-argv が API（ビルダー + 不透明ハンドル）を、abi-compat が ABI の方針と検査を用意したが、利用者向けの文書、サンプル、取り込み経路の統合テスト、リリース手順はない。Monde には `docs/`、`docs/dev/release.md`、`tests/docs/docs_check.cmake`、`.github/workflows/release.yml` があるが、アプリケーション向け（バイナリ配布）であり、ライブラリ向け（ソース配布）の文書はこれから書く。

**何を変えるか**: `README.md` に概要（C++ の同名プロジェクト Sirraide/clopts との区別を含む）、特徴、最小のサンプル、取り込み方 3 通り、静的と共有の選び方（`CLOPTS_BUILD_SHARED`、install 経路では prefix ごとに 1 つの形式）、ライセンスを書く。`docs/` に API リファレンス（ハンドルの種類と寿命、ビルダー関数、最初のエラーを記録する方式、結果とエラーのアクセサ、エラー種別、メモリの所有、解析規則の一覧: `--`、`-` 単体、並べ替え、`--no-x`、カウント、環境変数の優先順位）、Windows での注意（UTF-8 argv の取得、`wmain`、`activeCodePage`、DLL 名に版が入らないこと、PATH 上の DLL の衝突）、Naos の版更新手順を置き、abi-compat の ABI 方針文書への導線を張る。`examples/` にサブコマンドあり・なしの 2 サンプルを置き、CI でビルドし、文書中のサンプル出力を実行結果と CTest で突き合わせる（Monde の `monde-check` マーカー方式）。利用側プロジェクトを模した統合テストを、`FetchContent`（または `add_subdirectory`）と `install` → `find_package(CLOpts CONFIG)` の 2 経路 × 静的／共有で CI に通す。リリース手順（`docs/dev/release.md`。semver、pre-1.0 は minor で互換を壊してよく 1.0.0 から ABI 後方互換を保証、リリース前の ABI 検査の確認、SOVERSION を上げる判断手順）と、タグから GitHub Release を作りソースアーカイブと変更履歴を添えるワークフロー、CHANGELOG の形式を決める。文書は日本語が主で、コード中の識別子とヘルプ出力は英語。ライセンス表示は MIT、`The CLOpts Authors` のみ（Monde の著作権表示は併記せず、Naos はテスト専用なので THIRD_PARTY_NOTICES に載せない）。

**範囲外**: ライブラリの機能追加・変更、API の設計と挙動（core-parser、help-and-messages、platform-argv）、ABI の方針文書と自動検査そのもの（abi-compat）、ビルド基盤そのもの（project-foundation）、パッケージマネージャ（vcpkg / Conan）への登録、単一ファイル化、事前ビルド済みバイナリ、Monde 側の置き換え手順。

詳細は同ディレクトリの `brief.md` を参照。

## Introduction

docs-and-release は、CLOpts を使う開発者がソースを読まずに取り込み・宣言・解析・表示を行えるように利用者向けの文書とサンプルを整え、保守者が安定した版を示せるようにリリースの手順と仕組みを決める。文書は README と `docs/` の章（API リファレンス、解析規則、取り込み手順、Windows での注意）からなり、サブコマンドあり・なしの 2 つのサンプルプログラムを伴う。文書中のサンプル出力とコードは、ビルドしたサンプルの実行結果や公開 API と CTest で自動で突き合わせ、手で書いた記述が古くなることを防ぐ。リリースはソース配布だけとし、手順書、変更履歴の形式、タグから GitHub Release を作るワークフローを用意する。

本 spec はライブラリの挙動を変えない。文書は既存の spec（core-parser、help-and-messages、platform-argv、project-foundation、abi-compat）が実装した挙動を、利用者から見える形で説明する。

本文書の主語は次のとおり。
- the README: リポジトリ直下の `README.md`。
- the user documentation: `docs/` 直下の利用者向けの章。
- the examples: `examples/` のサンプルプログラム。
- the docs check: 文書とサンプル・公開 API の整合を検査する CTest のテスト（ラベル `docs`）。
- the release guide: 保守者向けのリリース手順書 `docs/dev/release.md`。
- the changelog: リポジトリ直下の変更履歴 `CHANGELOG.md`。
- the release workflow: 版のタグを受けて GitHub Release を作る CI のワークフロー。

## Boundary Context

- **In scope**: README（概要、特徴、最小のサンプル、取り込み方 3 通り、静的と共有の選び方、ライセンス）、`docs/` の利用者向けの章（API リファレンス、解析規則、取り込み手順、Windows での注意）、サブコマンドあり・なしの 2 つのサンプルプログラムとその CI でのビルド、文書とサンプル・公開 API の整合検査、リリース手順書（版番号の規則、リリース前の確認、SOVERSION を上げる判断、Naos の版更新手順）、変更履歴の形式、タグから GitHub Release を作るワークフロー。
- **Out of scope**: ライブラリの機能追加・挙動の変更、英語版の README・文書（文書は日本語だけ）、取り込み経路の統合テストの追加（FetchContent 経路の新しいテストを含む。既存の統合テストを使う）、v0.1.0 を含む実際のリリースの発行（タグを打つのは本 spec の完了後に保守者が手順書に沿って行う）、パッケージマネージャ（vcpkg / Conan）への登録、単一ファイル化、事前ビルド済みバイナリの配布、Monde 側の置き換え手順、ABI 方針の文書（`docs/abi.md`）と ABI の検査そのもの。
- **Adjacent expectations**:
  - core-parser、help-and-messages、platform-argv が公開 API とその挙動、ヘルプ・エラー文面を提供する。本 spec の文書はそれらの挙動を説明するだけで、文書と挙動が食い違うときは挙動に合わせて文書を直す。
  - project-foundation が install / export、静的／共有の選択（`CLOPTS_BUILD_SHARED`、install した形式を `find_package(CLOpts CONFIG)` で提供し `CLOpts_LIBRARY_TYPE` で知らせること）と、`add_subdirectory` 経路と install → `find_package` 経路の統合テスト（静的版・共有版の CI ジョブ）を提供する。本 spec はこれらを変えず、取り込み手順の記述をこれらに合わせる。
  - abi-compat が ABI 方針の文書（`docs/abi.md`）、公開シンボル一覧、ABI の検査（`docs/dev/abi-checks.md`）を提供する。本 spec の文書とリリース手順書はそれらへ導線を張り、内容を重複して書かない。
  - 将来の機能追加の spec は、本 spec が定める変更履歴の形式と、文書の更新の規約に従う。

## Requirements

### Requirement 1: README
**Objective:** As a CLOpts を使うか検討している開発者, I want README だけで CLOpts が何で、どう取り込み、どう書くのかを把握したい, so that 採用を判断し、最初のプログラムをすぐに書き始められる

#### Acceptance Criteria
1. The README shall CLOpts が C99 で書かれた外部依存ゼロのコマンドライン引数パーサライブラリであることを冒頭で示す。
2. The README shall C++ の同名プロジェクト Sirraide/clopts とは別のプロジェクトであることを示す。
3. The README shall 主な特徴として、少なくとも宣言からの解析とヘルプ文面の生成、構造化エラー、ライブラリが入出力も `exit` もしないこと、Windows での UTF-8 の引数、静的／共有の選択と 1.0.0 以降の ABI 後方互換を挙げる。
4. The README shall プログラムを宣言し、解析し、結果を読み、エラーまたはヘルプを表示し、後始末するまでの一通りを含む最小のサンプルコードを載せる。
5. The README shall 取り込み方として、`add_subdirectory`、`FetchContent`、install した後の `find_package(CLOpts CONFIG)` の 3 通りを、それぞれ利用側の CMake の記述例とともに示し、どの経路でも `CLOpts::clopts` にリンクすることを示す。
6. The README shall 静的と共有の選び方として、`add_subdirectory` / `FetchContent` 経路では `CLOPTS_BUILD_SHARED`（既定は `BUILD_SHARED_LIBS`）で、install → `find_package` 経路では install した形式がそのまま使われ、両方を使うときは prefix を分けて `CMAKE_PREFIX_PATH` で選ぶことを示す。
7. The README shall 対応環境（CMake の最低版、C の規格、CI で検査しているコンパイラと OS）を示す。
8. The README shall 詳細を `docs/` の各章に委ね、各章への相対リンクを持つ。
9. The README shall ライセンス欄に、MIT License であることと著作権者が The CLOpts Authors であることだけを示し、Monde への言及や Monde の著作権表示を含めない。

### Requirement 2: API リファレンス
**Objective:** As a CLOpts で CLI ツールを書く開発者, I want 公開 API の型、関数、寿命、所有、エラーの扱いを 1 か所で確認したい, so that ヘッダや実装を読まずに正しく API を呼べる

#### Acceptance Criteria
1. The user documentation shall API リファレンスの章で、公開されている不透明ハンドルの種類ごとに、何を表すか、どの関数で作られ、どの関数で解放され、どの時点まで有効か（他のハンドルへの依存を含む）を示す。
2. The user documentation shall ビルダー関数（プログラム、サブコマンド、オプション、位置引数の宣言と属性の設定）を一覧にし、それぞれの引数と戻り値の意味を示す。
3. The user documentation shall ビルダーの失敗の扱いとして、最初のエラーだけが記録され、以降のビルダー呼び出しは何もしないこと、NULL のハンドルを渡しても安全であること、記録されたエラーが解析時または宣言の検査時にまとめて返ることを示す。
4. The user documentation shall 解析の呼び出し方と、解析の状態（成功、ヘルプやバージョンの要求、使い方エラー、宣言エラー、メモリ不足など、公開ヘッダにある状態のすべて）の意味と、利用側がそれぞれでとるべき処理を示す。
5. The user documentation shall 結果のアクセサ（値、出現回数、値の出所、位置引数、選ばれたサブコマンド）とエラーのアクセサ（種別、該当コマンド、該当オプション、該当トークン、argv の添字）を示す。
6. The user documentation shall 公開ヘッダにあるエラー種別のすべてについて、どの入力で起きるかを示す。
7. The user documentation shall メモリの所有として、ビルダーに渡した文字列はライブラリが複製すること、ライブラリが確保したものはライブラリの関数で解放すること、argv をライブラリが書き換えないこと、結果から得た文字列がいつまで有効か（argv を借用するかどうかを含む）を示す。
8. The user documentation shall ヘルプ・使い方・バージョン・エラーの文面を書き込みコールバックへ出す関数と、`FILE*` へ出すための公開ヘッダの便宜関数の使い方を示す。
9. The user documentation shall 公開シンボル一覧にあるすべての公開関数を、API リファレンスのいずれかの箇所で名前を挙げて説明する。

### Requirement 3: 解析規則
**Objective:** As a CLOpts で CLI ツールを書く開発者, I want 引数がどの規則で解釈されるかを一覧で確認したい, so that 自分のツールの利用者に挙動を説明でき、想定外の解釈に驚かない

#### Acceptance Criteria
1. The user documentation shall 解析規則の章で、長いオプション（`--name`、`--name=value`）と短いオプション（`-o v`、`-ov`、まとめ書き `-abc`）の書き方を示す。
2. The user documentation shall `--` 以降がすべて位置引数になることと、`-` 単体が位置引数であることを示す。
3. The user documentation shall サブコマンド名より前にはグローバルオプションだけを置けることと、サブコマンド以降はオプションと位置引数を自由に並べ替えられることを示す。
4. The user documentation shall 長いオプションの前方一致による省略を行わないことと、大文字小文字を区別することを示す。
5. The user documentation shall 繰り返し指定の扱い（単一値は後勝ち、リスト値は連結、フラグは重複を許す、カウントは上限でクランプする）と、`--no-<name>` による否定とカウントフラグ（`-vvv`）の挙動を示す。
6. The user documentation shall 値の優先順位が明示指定 > 環境変数 > 既定値であることと、値ごとに出所を判別できることを示す。
7. The user documentation shall `--help`、`-h`、`help <cmd>`、`--version` の要求が他の使い方エラーに優先することと、最初に見つけた使い方エラーだけが報告されることを示す。
8. The user documentation shall 値は文字列として返り、型変換と妥当性検査は利用側が行うことを示す。

### Requirement 4: 取り込み手順
**Objective:** As a CLOpts を自分のプロジェクトに組み込む開発者, I want 取り込み方と静的／共有の選び方を、自分の状況に合わせて選べるだけの詳しさで知りたい, so that ビルドの設定で迷わず、利用側の設定を壊さずに取り込める

#### Acceptance Criteria
1. The user documentation shall 取り込み手順の章で、`add_subdirectory`、`FetchContent`、install → `find_package(CLOpts CONFIG)` の 3 通りの手順を、利用側の CMake の記述例とともに示す。
2. The user documentation shall FetchContent の記述例で、版のタグを指定して取得する書き方を示す。
3. The user documentation shall CLOpts のオプション（`CLOPTS_BUILD_SHARED`、`CLOPTS_BUILD_TESTING`、`CLOPTS_BUILD_EXAMPLES`、`CLOPTS_INSTALL`、`CLOPTS_WERROR`）の意味と既定値（トップレベルかどうかで変わるものを含む）と、親プロジェクトから上書きする方法を示す。
4. The user documentation shall install 経路では 1 つの prefix に 1 つの形式が入り、`find_package` は install した形式を提供すること（`CLOpts_LIBRARY_TYPE` で確かめられること）と、両方の形式を使うときは別々の prefix に install すること、同じ prefix に別の形式を install したときに何が起きるかを示す。
5. The user documentation shall 静的と共有の選び方の目安（共有版の利点、Windows で DLL を実行ファイルから見える場所に置く必要があること、1.0.0 より前は minor ごとに ABI が変わること）を示し、ABI 方針の文書（`docs/abi.md`）への相対リンクを持つ。
6. The user documentation shall MSVC の CRT 種別は利用側の設定を継承することと、install 経路でビルド時と利用時の CRT 種別が異なるときの扱いを示す。
7. The user documentation shall 取り込み手順に載せる記述例の形（ターゲット名、`find_package` の呼び方、形式の選び方）を、既存の統合テストの利用側プロジェクトが使う形と食い違わないようにし、どの経路が CI で静的版・共有版について検査されているかを示す。

### Requirement 5: Windows での注意
**Objective:** As a Windows でも動く CLI ツールを書く開発者, I want Windows 特有の注意点をまとめて知りたい, so that 非 ASCII の引数や DLL の扱いで実行時に困らない

#### Acceptance Criteria
1. The user documentation shall Windows での注意の章で、`main` の argv ではなく CLOpts の関数で UTF-8 の argv を取得する方法と、その argv の所有と解放を示す。
2. The user documentation shall `wmain` を使う利用者向けに、`wargv` を UTF-8 の argv に変換する方法を示す。
3. The user documentation shall 環境変数を UTF-8 で取得する方法と、それを環境変数フォールバックに差し込む方法を示す。
4. The user documentation shall 実行ファイルのマニフェストで `activeCodePage` を UTF-8 にする方法との関係（CLOpts の関数を使えばマニフェストがなくても UTF-8 の引数を得られること）を示す。
5. The user documentation shall 共有版の DLL のファイル名に版が入らないことと、PATH 上の別の版の DLL が読み込まれる衝突の危険と、その避け方（実行ファイルの横に DLL を置くこと、静的版を使うこと）を示し、ABI 方針の文書への相対リンクを持つ。
6. The user documentation shall 標準出力・標準エラーへ UTF-8 の文面を出すときの注意（コンソールのコードページ）を示す。

### Requirement 6: 文書の構成と表記
**Objective:** As a CLOpts の利用者と保守者, I want 文書の置き場所、言語、相互の導線が一貫していてほしい, so that 必要な記述にたどり着け、文書の保守の範囲が明確になる

#### Acceptance Criteria
1. The user documentation shall `docs/` 直下に、利用者向けの章として API リファレンス、解析規則、取り込み手順、Windows での注意を置き、章の一覧とそれぞれの内容を示す入口を持つ。
2. The user documentation shall 章の間を相対リンクで結び、ABI 方針の文書（`docs/abi.md`）へ導線を張り、保守者向けの文書（`docs/dev/`）へはリンクしない。
3. The README and the user documentation shall 日本語で書き、識別子、コマンド、ヘルプ・エラーの文面は英語のまま示す。
4. The README and the user documentation shall ABI 方針の内容（SOVERSION の規則、互換を保つ変更と壊す変更）を重複して書かず、ABI 方針の文書を参照する。
5. If README または利用者向けの章の相対リンクの先が存在しないとき, the docs check shall 失敗し、リンク元のファイルとリンク先を報告する。

### Requirement 7: サンプルプログラム
**Objective:** As a CLOpts を初めて使う開発者, I want そのままビルドして動かせる代表的なサンプルを見たい, so that 自分のツールの雛形にできる

#### Acceptance Criteria
1. The examples shall サブコマンドを持つ架空のツールと、サブコマンドを持たない架空のフィルタ型ツール（`tool [options] FILE...` の形）の 2 つのプログラムからなる。
2. The examples shall 2 つを合わせて、グローバルオプション、サブコマンドごとのオプション、位置引数、フラグ、カウントフラグ、単一値、リスト値、`--no-<name>` による否定、既定値、環境変数フォールバック、ヘルプとバージョンの表示、使い方エラーの表示と終了コードの決定を示す。
3. The examples shall Windows でも UTF-8 の argv を使って引数を受け取る。
4. The examples shall ライブラリが確保したものをすべて解放してから終了する。
5. The examples shall 実在のツールの宣言を使わず、架空のプログラム名と版で宣言する。
6. Where CLOpts がトップレベルのプロジェクトとしてビルドされているとき, the examples shall 既定でビルドされ、`CLOPTS_BUILD_EXAMPLES` を OFF にするとビルドされない。
7. Where CLOpts が他のプロジェクトに取り込まれているとき, the examples shall 既定でビルドされない。
8. The examples shall CI のすべての環境（静的版・共有版の両方）でビルドされ、プロジェクトと同じ警告の設定で警告なくビルドできる。

### Requirement 8: 文書とサンプル・公開 API の整合検査
**Objective:** As a CLOpts の保守者, I want 文書に載せたサンプル出力やコードが実際の挙動と食い違ったら自動で気づきたい, so that ライブラリや文面を変えたときに文書が黙って古くならない

#### Acceptance Criteria
1. The docs check shall CTest のテスト（ラベル `docs`）として登録され、サンプルプログラムがビルドされる構成で CI のすべての環境で動く。
2. The docs check shall README と利用者向けの章のうち照合用の印を付けたサンプル出力について、印が指すサンプルプログラムを印が指す引数で実行し、その出力と終了コードが文書の記載とバイト単位で（改行の違いを除いて）一致することを検査する。
3. The README and the user documentation shall 2 つのサンプルプログラムのそれぞれについて、少なくともヘルプの出力と使い方エラーの出力を照合用の印付きで載せる。
4. If 照合用の印付きのサンプル出力が実行結果と一致しないとき, the docs check shall 失敗し、文書のファイル名、該当箇所、期待した出力と実際の出力の違いを報告する。
5. If 照合用の印が存在しないサンプルプログラムを指すか、印の書式が誤っているとき, the docs check shall 失敗し、該当するファイルと印を報告する。
6. The docs check shall README の最小のサンプルコードが、CI でビルドされるコードと一致することを検査する。
7. If 公開シンボル一覧にある公開関数のうち API リファレンスで名前が挙がっていないものがあるとき, the docs check shall 失敗し、該当する関数名をすべて報告する。
8. If 照合の対象が 1 つも見つからないとき, the docs check shall 成功扱いにせず失敗する。

### Requirement 9: 版番号の規則とリリース手順
**Objective:** As a CLOpts の保守者, I want 版番号の付け方とリリースの手順が文書で決まっていてほしい, so that 誰がリリースしても同じ確認を経て、利用者に安定した版を示せる

#### Acceptance Criteria
1. The release guide shall 版番号が semver に従うことと、版番号を書く場所がルートの `project(VERSION)` の 1 か所だけであることを示す。
2. The release guide shall 1.0.0 より前は minor を上げるときに API / ABI の互換を壊してよいことと、1.0.0 から共有ライブラリの ABI 後方互換を保証することを示し、SOVERSION の規則は ABI 方針の文書に従うことを示す。
3. The release guide shall リリースの手順を、版番号と変更履歴の更新、main の CI がすべて成功していることの確認、タグの作成と push、ワークフローが作った Release の確認の順に、実行するコマンドとともに示す。
4. The release guide shall リリース前の ABI の確認として、公開シンボル一覧と export されたシンボルの照合が成功していること、1.0.0 以降は直近のリリースとの ABI 比較が成功していることを確かめる手順を示し、ABI の検査の説明（`docs/dev/abi-checks.md`）を参照する。
5. The release guide shall SOVERSION を上げるかどうかの判断手順として、前回のリリースからの変更に互換を壊す変更が含まれるかを確かめ、含まれるときは major（0.x の間は minor）を上げることを示す。
6. The release guide shall 互換を壊す変更を含むリリースで、変更履歴にその旨と利用側に必要な対応を書くことを示す。
7. The release guide shall リリースの配布物がソースだけであり、事前ビルド済みのバイナリを配らないことと、その理由を示す。
8. The release guide shall Naos の版を更新する手順として、更新する場所（版の指定が 1 か所に集まっていること）、タグとコミットの両方を更新すること、更新後に CI のすべての環境でテストが通ることを確かめること、Naos の不足や回避策の記録先を示す。
9. The release guide shall リリースのワークフローが失敗したときの戻し方（タグを消し、原因を直してから打ち直すことを含む）を示す。

### Requirement 10: 変更履歴の形式
**Objective:** As a CLOpts の利用者と保守者, I want 版ごとの変更を決まった形式で読めるようにしたい, so that 利用者は版を上げるときの影響を判断でき、保守者はリリースのたびに書き方で迷わない

#### Acceptance Criteria
1. The changelog shall リポジトリ直下に置き、日本語で書く。
2. The changelog shall 未リリースの変更を集める節を先頭に持ち、その下に版ごとの節を新しい順に並べる。
3. The changelog shall 版ごとの節の見出しに版番号とリリース日を含める。
4. The changelog shall 各節の変更を、追加、変更、削除、修正などの決まった分類に分け、互換を壊す変更は分類と別に目立つ形で示す。
5. The release guide shall 機能を追加・変更する変更は、同じ変更の中で未リリースの節に項目を足すことを示す。

### Requirement 11: リリースのワークフロー
**Objective:** As a CLOpts の保守者, I want 版のタグを push するだけで、検査を経た GitHub Release が作られてほしい, so that リリースの作業が手作業の誤りに左右されない

#### Acceptance Criteria
1. When `vX.Y.Z` の形のタグが push されたとき, the release workflow shall 起動する。
2. If タグの版番号がそのコミットの `project(VERSION)` と一致しないとき, the release workflow shall Release を作らずに失敗し、タグの版とプロジェクトの版を報告する。
3. If 変更履歴にタグの版の節がないとき, the release workflow shall Release を作らずに失敗し、見つからなかった版を報告する。
4. When 版番号と変更履歴の検査が通ったとき, the release workflow shall タグの版を名前とする GitHub Release を作り、変更履歴のその版の節を本文にし、ソースアーカイブを添える。
5. The release workflow shall 事前ビルド済みのバイナリを Release に添えない。
6. The release workflow shall ソースアーカイブに参照用の submodule（`ref/`）の中身を含めない。
7. The release workflow shall ソースアーカイブを展開したものが、ネットワークに接続せずに、テストを無効にした構成でライブラリをビルドできることを確かめてから Release を作る。
8. The release workflow shall 通常の CI のワークフローを変更せず、それと独立に動く。
