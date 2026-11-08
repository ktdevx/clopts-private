# Requirements Document

## Introduction

CLOpts は C99 で書かれた外部依存ゼロのコマンドライン引数パーサライブラリである。本 spec `project-foundation` は、ライブラリの機能を実装する前に必要な土台、すなわちビルド構成、テスト基盤、CI、配布（install / package config）、そしてすべての内部モジュールが依存する内部ユーティリティを整える。

参照元の Monde にはアプリケーション向けの同等の土台があるが、トップレベルでの CRT 種別の設定や無条件のテスト有効化など、他プロジェクトに取り込まれるライブラリにはそのまま使えない設定を含む。本 spec では「利用側のプロジェクトに取り込まれても利用側のビルド設定を汚さない」ことを第一の品質要件として、ライブラリ向けに作り直す。

成果物は、引数解析の機能を一切持たない「空のライブラリ」と、後続 spec がソースとテストを追加するだけで済む仕組みである。

**改訂（2026-11-08）**: 共有ライブラリを範囲に入れ、1.0.0 以降は共有ライブラリの ABI 後方互換を保証することになった（roadmap.md の Approach Decision）。これに伴い本 spec は、静的ライブラリと共有ライブラリのどちらでもビルドできること、公開する関数だけをシンボルとして出すこと、共有ライブラリの版を付けること、両方の形式の install と選択、両方の形式での CI を担う。書き込みコールバックは、構造体の値渡しと `FILE*` を受け取るライブラリ関数をやめる（`FILE*` を DLL の境界越しに渡さないため）。Requirement 1〜11 は初版の要件を改訂したもので、Requirement 12〜15 を追加した。

## Boundary Context

- **In scope**: ルートのビルド定義とヘルパ、`clopts` ターゲットと `CLOpts::clopts` エイリアス、静的／共有の選択、公開シンボルの印（export マクロ）とそれ以外のシンボルを出さない設定、共有ライブラリの版、静的版と共有版の並存 install と package config での選択、公開ヘッダの置き場（`include/clopts/`）、`src/` と `tests/` のディレクトリ構成、モジュール追加とテスト登録のヘルパ、テストフレームワーク Naos の固定取得、install / export と package config、ビルド構成の要件を自動検証するテスト（静的版と共有版の両方）、5 環境 × 2 形式の CI ワークフロー、書き込みコールバック型と `FILE*` 用の便宜コールバック（公開ヘッダ内で完結するもの）、内部ユーティリティ（可変長配列、サイズ計算のオーバーフロー検査）とその単体テスト、`.gitignore` の整備
- **Out of scope**: 引数解析の機能一切と公開 API のハンドル型（core-parser）、公開シンボル一覧との照合・公開ヘッダの静的検査・版の間の ABI 比較・ABI 方針の文書（abi-compat）、pkg-config ファイルの提供、同じビルドツリーで静的版と共有版を同時に作ること、ELF のシンボルバージョニング、ヘルプ・エラーの文面（help-and-messages）、Windows の argv と環境変数の取得（platform-argv）、README の本文・利用者向け文書・サンプルプログラム・リリースワークフロー・利用側を模した取り込み経路の利用テスト（docs-and-release）
- **Adjacent expectations**:
  - 後続 spec（core-parser、help-and-messages、platform-argv）は、本 spec が用意するモジュール追加・テスト登録の仕組みに従ってソースとテストを追加する。本 spec はそれらのモジュールの中身を持たない。
  - help-and-messages は、本 spec が定義する書き込みコールバック型と `FILE*` 用の便宜コールバックをそのまま使う。
  - core-parser、help-and-messages、platform-argv は、自分が追加する公開関数すべてに本 spec の公開シンボルの印を付ける。印のない関数は共有ライブラリから見えない。
  - abi-compat は、本 spec が用意する共有版のビルドと CI ジョブの上に、公開シンボル一覧の照合と ABI 比較を加える。本 spec は「印を付けた関数だけが出る仕組み」と「印のない内部関数が利用側から見えないこと」の検証までを行い、公開シンボルが一覧と過不足なく一致することの検証は行わない。
  - docs-and-release は本 spec の install 経路を前提に、利用側を模したプロジェクトでの利用テストとリリース手順を担う。本 spec では install 経路が動作することの検証までを行う。

## Requirements

### Requirement 1: ライブラリターゲットと公開ヘッダの骨格

**Objective:** As a CLOpts の開発者, I want 機能を持たない空のライブラリターゲットと公開ヘッダの置き場が用意されていること, so that 後続 spec が最初からソースを追加するだけで開発を始められる

#### Acceptance Criteria
1. The ビルド構成 shall ライブラリターゲット `clopts` とそのエイリアス `CLOpts::clopts` を定義する。静的ライブラリにするか共有ライブラリにするかは Requirement 12 に従う。
2. The ビルド構成 shall 公開ヘッダの置き場として `include/clopts/` を持ち、傘ヘッダ `include/clopts/clopts.h` を提供する。
3. The ビルド構成 shall `include/` を `clopts` ターゲットの公開 include 基点とし、利用側が `#include <clopts/clopts.h>` で公開ヘッダを参照できるようにする。
4. The ビルド構成 shall 内部モジュールのソースを `src/<module>/` に置き、内部ヘッダの include 基点を `src/` としてライブラリ内部に限定する。
5. The ビルド構成 shall バージョン番号を記述する場所をルートのプロジェクト定義の 1 か所に限定する。
6. When 開発者がクリーンな状態で `cmake -S . -B build && cmake --build build && ctest --test-dir build` を実行する, the ビルド構成 shall 対応する 5 つの CI 環境（MSVC、MinGW-w64、Apple Clang、GCC、Clang）のすべてで、静的版と共有版のどちらを選んでも構成・ビルド・テストを成功させる。
7. The ビルド構成 shall CMake 3.21 以上を最低要件として宣言する。

### Requirement 2: サブプロジェクトとしての行儀

**Objective:** As a CLOpts を取り込む利用側の開発者, I want CLOpts を `add_subdirectory` や `FetchContent` で取り込んでも自分のビルド設定が変わらないこと, so that 自分のプロジェクトの CRT 種別・C 規格・警告・出力先を自分で決められる

#### Acceptance Criteria
1. The ビルド構成 shall `CMAKE_MSVC_RUNTIME_LIBRARY`、`CMAKE_C_STANDARD`、`CMAKE_C_EXTENSIONS`、`CMAKE_*_OUTPUT_DIRECTORY`、`BUILD_SHARED_LIBS`、`CMAKE_C_VISIBILITY_PRESET` のいずれも設定しない（静的／共有の選択とシンボルの公開範囲はターゲット単位で決める）。
2. The ビルド構成 shall ディレクトリ単位のコンパイルオプション・コンパイル定義・include ディレクトリの追加を行わず、すべての設定をターゲット単位で適用する。
3. The ビルド構成 shall C 規格の要件（C99）を `clopts` ターゲットの公開コンパイル機能として宣言し、言語拡張を無効にする。
4. The ビルド構成 shall 警告オプション、`-Werror` 相当、および `_CRT_SECURE_NO_WARNINGS` などの内部用コンパイル定義を `clopts` ターゲットと自身のテスト実行ファイルにのみ PRIVATE に適用し、リンクする利用側ターゲットに伝播させない。
5. When CLOpts がサブプロジェクトとして取り込まれる（トップレベルでない）, the ビルド構成 shall テストの有効化、Naos の取得、install 規則の定義、`-Werror` 相当の適用を既定で行わない。
6. While CLOpts がトップレベルでビルドされている, the ビルド構成 shall テスト、install、`-Werror` 相当を既定で有効にする。
7. The ビルド構成 shall テスト・install・`-Werror` の有効/無効を個別のオプション（`CLOPTS_BUILD_TESTING`、`CLOPTS_INSTALL`、`CLOPTS_WERROR`）で利用側が明示的に上書きできるようにする。
8. If 同一ビルド内で CLOpts が複数回取り込まれる, then the ビルド構成 shall 2 回目以降の取り込みでターゲットを再定義せず、構成エラーを起こさない。
9. The ビルド構成 shall MSVC の CRT 種別（静的 / 動的、デバッグ / リリース）を設定せず、利用側の設定を継承する。
10. The ビルド構成 shall テストでのみ使うテストフレームワーク Naos を、テストが有効な構成でのみ取得し、`clopts` ターゲットの依存関係に含めない。

### Requirement 3: コンパイラオプションと警告方針

**Objective:** As a CLOpts の開発者, I want 対応するすべてのコンパイラで同じ厳しさの警告がエラーとして扱われること, so that 移植性の問題を CI の段階で検出できる

#### Acceptance Criteria
1. While GCC または Clang（Apple Clang、MinGW-w64 を含む）でビルドしている, the ビルド構成 shall `clopts` ターゲットとテスト実行ファイルに `-Wall -Wextra -Wpedantic -Werror=vla` を適用する。
2. While GCC または Clang でビルドしていて `-Werror` 相当が有効である, the ビルド構成 shall 警告をエラーとして扱う。
3. While MSVC でビルドしている, the ビルド構成 shall `clopts` ターゲットとテスト実行ファイルに `/std:c11 /W4 /utf-8 /Zc:preprocessor` を適用し、`_CRT_SECURE_NO_WARNINGS` を PRIVATE に定義する。
4. While MSVC でビルドしていて `-Werror` 相当が有効である, the ビルド構成 shall `/WX` を適用して警告をエラーとして扱う。
5. While MinGW-w64 でビルドしている, the ビルド構成 shall `clopts` ターゲットとテスト実行ファイルに `__USE_MINGW_ANSI_STDIO=1` を PRIVATE に定義する。
6. The ビルド構成 shall コンパイラオプションの適用を 1 つのヘルパに集約し、ライブラリとテスト実行ファイルの双方に同じヘルパで適用する。
7. If `clopts` ターゲットまたはテスト実行ファイルのソースに可変長配列（VLA）が含まれる, then the ビルド構成 shall GCC / Clang でのビルドをエラーで失敗させる。

### Requirement 4: モジュール追加とテスト登録のヘルパ

**Objective:** As a 後続 spec を実装する開発者, I want ソースとテストを決まった場所に置いて 1 行ずつ登録するだけで済むこと, so that ビルド構成の詳細を知らなくてもモジュールを追加できる

#### Acceptance Criteria
1. The ビルド構成 shall 内部モジュール `<module>` のソースを `clopts` ターゲットに追加するヘルパを提供し、別個のライブラリターゲットを作らない。
2. When 開発者がモジュール追加ヘルパを `src/<module>/` 配下のソース一覧とともに呼び出す, the ビルド構成 shall それらのソースを `clopts` ターゲットに組み込む。
3. The ビルド構成 shall `tests/<module>/` の Naos テストを実行ファイル `<module>_tests` としてビルドし CTest に登録するヘルパを提供する。
4. When 開発者がテスト登録ヘルパを呼び出す, the ビルド構成 shall 生成するテスト実行ファイルに `CLOpts::clopts` と Naos をリンクし、Requirement 3 のコンパイラオプションを適用し、CTest のラベル `<module>` を付与する。
5. When 開発者が `ctest --test-dir build -L <module>` を実行する, the テスト基盤 shall 該当モジュールのテストだけを実行する。
6. The ビルド構成 shall Naos の取得版（タグ `v0.4.0` とコミット SHA `09fde7172c4b63d416c32e98499cb0c0306d3dea`）を 1 か所で固定し、タグとコミット SHA の両方で検証する。
7. Where 開発者が取得済みの Naos のローカルパスを指定する, the ビルド構成 shall ネットワークにアクセスせずそのローカルパスの Naos を使ってテストを構成する。
8. The ビルド構成 shall CTest のテスト名を `<module>_tests.<suite>.<test>` の形式で登録する。
9. The ビルド構成 shall `src/` に対応しないテストディレクトリとして `tests/toolchain/` と `tests/integration/` を本 spec の範囲で用意し、それぞれラベル `toolchain`、`integration` で登録する。
10. While 共有版を Windows でビルドしている, the テスト基盤 shall 利用側の PATH などの環境を変えずに、テスト実行ファイルが実行時に必要な DLL を見つけられるようにする。
11. While 共有版をビルドしている, the テスト基盤 shall 内部モジュールの単体テストが、共有ライブラリから出していない内部関数も呼び出してテストできるようにする（内部関数をテストのために公開シンボルへ加えない）。

### Requirement 5: install / export と package config

**Objective:** As a CLOpts をシステムにインストールして使う利用側の開発者, I want `find_package(CLOpts CONFIG)` で見つけて `CLOpts::clopts` にリンクするだけで使えること, so that 取り込み方法によらず同じリンク手順で済む

#### Acceptance Criteria
1. While install が有効である, the ビルド構成 shall ライブラリ本体（共有版では実行時に読み込まれるファイル、Windows ではリンク用のインポートライブラリを含む）、`include/clopts/` 配下の公開ヘッダ、エクスポートされたターゲット定義、package config ファイル、バージョンファイルを install する。
2. The ビルド構成 shall 内部モジュールのヘッダ（`src/` 配下）を install しない。
3. When 利用側が install 済みの CLOpts を `find_package(CLOpts CONFIG)` で検索する, the package config shall インポートターゲット `CLOpts::clopts` を提供し、公開ヘッダの include パスと C99 の要件を利用側に伝える。
4. When 利用側が `find_package(CLOpts <version> CONFIG)` でバージョンを指定する, the バージョンファイル shall インストール済みの版との互換性を判定し、非互換の版では検索を失敗させる。互換とみなすのは、要求した版以上で、かつ 1.0.0 より前では major と minor が一致する版、1.0.0 以上では major が一致する版とする（Requirement 15 の互換性の版と同じ規則）。
5. The バージョンファイル shall アーキテクチャ非依存とは宣言せず、異なるアーキテクチャ向けのインストールを互換とみなさない。
6. While 静的版を install している, the ビルド構成 shall install 時に使われた MSVC の CRT 種別を記録し、利用側が異なる CRT 種別で `find_package` した場合に警告を出すか、または package config の文書でその制約を明記する。
7. When 開発者が install を実行してから、install 先を参照する利用側を模した最小プロジェクトを構成・ビルドする, the テスト基盤 shall `find_package(CLOpts CONFIG)` と `target_link_libraries(app PRIVATE CLOpts::clopts)` だけで利用側がビルドでき、実行できることを、そのビルドツリーの形式（静的版または共有版）について CTest のテストとして検証する。

### Requirement 6: ビルド構成の自動検証

**Objective:** As a CLOpts の保守者, I want ビルド構成の要件がテストとして検証されること, so that 後から加えた変更が利用側を汚す設定を持ち込んだときに CI で気づける

#### Acceptance Criteria
1. The テスト基盤 shall `tests/toolchain/` に、Requirement 2.1 に挙げたグローバル変数が CLOpts の構成によって設定されていないことを検証するテストを持つ。
2. The テスト基盤 shall `tests/toolchain/` に、警告がエラー化されることと VLA がビルドエラーになることを、意図的に警告・VLA を含むソースのビルドが失敗することで検証するテストを持つ。
3. The テスト基盤 shall `tests/integration/` に、CLOpts を `add_subdirectory` で取り込んだ利用側を模した最小プロジェクトが、`target_link_libraries(app PRIVATE CLOpts::clopts)` だけでビルドでき、かつ利用側に CLOpts の警告オプションや内部コンパイル定義が伝播していないことを検証するテストを持つ。
4. The テスト基盤 shall `tests/integration/` に、Requirement 5.7 の install 経路の検証を持つ。
5. When `ctest --test-dir build` を実行する, the テスト基盤 shall 上記の検証テストをすべて含めて実行する。
6. The テスト基盤 shall `tests/integration/` の `add_subdirectory` 経路の検証を、そのビルドツリーの形式（静的版または共有版）で行い、利用側を模したプロジェクトが実行時まで成功することを確認する。
7. The テスト基盤 shall 静的版と共有版を同じ prefix に install した状態で、package config が Requirement 13 の規則どおりに形式を選ぶこと（形式の指定、指定のないときの既定、両方の指定の拒否、入っていない形式の指定での失敗）を検証するテストを持つ。
8. The テスト基盤 shall 共有版で、公開シンボルの印を付けていない内部関数を利用側から参照するとリンクに失敗すること（内部関数が共有ライブラリから見えないこと）を検証するテストを持つ。

### Requirement 7: CI ワークフロー

**Objective:** As a CLOpts の保守者, I want main への push と pull request で 5 環境のビルドとテストが自動実行されること, so that 対応環境のいずれかで壊れた変更を取り込む前に検出できる

#### Acceptance Criteria
1. When main ブランチへの push または pull request が発生する, the CI ワークフロー shall MSVC、MinGW-w64（msys2 ucrt64）、Apple Clang、GCC、Clang の 5 環境それぞれで、静的版と共有版の両方について構成・ビルド・テストを実行する。
2. The CI ワークフロー shall 各環境で `-Werror` 相当を有効にしてビルドする。
3. If いずれかの環境・形式で構成・ビルド・テストのいずれかが失敗する, then the CI ワークフロー shall そのジョブを失敗として報告し、他のジョブは中断せず完了させる。
4. When テストが失敗する, the CI ワークフロー shall 失敗したテストの出力をジョブログに含める。
5. The CI ワークフロー shall リリースの処理（タグの検査、GitHub Release の作成）を含まない。
6. The CI ワークフロー shall 少なくとも 1 つの環境で、静的版と共有版を同じ prefix に install して Requirement 6.7 の形式選択の検証を実行する。

### Requirement 8: 書き込みコールバックと `FILE*` 用の便宜コールバック

**Objective:** As a 後続 spec（help-and-messages）を実装する開発者と、その文面を表示する利用側の開発者, I want 出力先を抽象化した書き込みコールバック型と、`FILE*` へ書くための既製のコールバックがあること, so that ライブラリが stdout / stderr に直接書かずに文面を生成でき、共有ライブラリと利用側で C ランタイムが違っても `FILE*` を安全に使える

#### Acceptance Criteria
1. The 公開ヘッダ shall `write(ctx, data, len)` の形で任意の出力先へバイト列を書き込み、成功か失敗かを返すコールバック型を定義する。
2. The 公開ヘッダ shall コールバックと利用側コンテキストを組にした構造体を定義せず、ライブラリの関数はコールバックとコンテキストを別々の引数として受け取る形を前提にする。
3. The 公開ヘッダ shall `FILE*` をコンテキストとして渡せば任意の `FILE*` へ書き込める既製のコールバックを、ライブラリ本体に含めず、利用側の翻訳単位でコンパイルされる形で提供する。
4. When 既製の `FILE*` 用コールバックに長さ `len` のバイト列を渡す, the 既製の `FILE*` 用コールバック shall その `len` バイトを順序どおり指定の `FILE*` に書き込み、書き込めなかったときは失敗を返す。
5. If 書き込みコールバックが失敗を報告する, then the 書き込みユーティリティ shall その失敗を呼び出し元に伝え、以降の書き込みを継続するか中断するかを呼び出し元が判断できるようにする。
6. The ライブラリ本体 shall `stdio` に依存せず、`FILE*` を引数に取る関数を公開しない。
7. When 公開ヘッダを include した翻訳単位が既製の `FILE*` 用コールバックを使わない, the ビルド構成 shall Requirement 3 の警告設定（`-Werror` 相当を含む）のもとで、未使用に関する警告を出さずにコンパイルを成功させる。
8. The テスト基盤 shall 書き込みコールバック型と既製の `FILE*` 用コールバックの動作（全バイトの書き込み、長さ 0、書き込み失敗の報告）を `tests/core/` の単体テストで検証する。

### Requirement 9: 内部ユーティリティ — 可変長配列とサイズ計算

**Objective:** As a 後続 spec を実装する開発者, I want 要素の追加に応じて伸びる配列と安全なサイズ計算があること, so that 各モジュールで同じメモリ管理の誤りを繰り返さずに済む

#### Acceptance Criteria
1. The core モジュール shall 要素サイズを指定して初期化し、末尾への追加で自動的に容量を拡張する可変長配列を提供する。
2. When 可変長配列に要素を追加して容量が不足する, the 可変長配列 shall 既存の要素を保ったまま容量を拡張する。
3. If メモリ確保に失敗する, then the 可変長配列 shall 失敗を戻り値で報告し、既存の要素と内部状態を破壊しない。
4. The 可変長配列 shall 解放操作を冪等にし、未初期化または解放済みの配列に対する再度の解放が安全であるようにする。
5. If メモリ確保が失敗した後に解放操作を行う, then the 可変長配列 shall 確保済みのメモリをすべて解放する。
6. The core モジュール shall 要素数と要素サイズの乗算など、サイズ計算のオーバーフローを検査する手段を提供し、オーバーフローする計算を失敗として報告する。
7. If サイズ計算がオーバーフローする, then the 可変長配列 shall メモリ確保を試みずに失敗を報告する。
8. The core モジュール shall 契約違反（NULL の配列への操作など）を `assert` で検出し、戻り値では報告しない。
9. The core モジュール shall 可変長配列とサイズ計算の動作（拡張、オーバーフロー、確保失敗時の状態、解放の冪等性）を `tests/core/` の単体テストで検証する。

### Requirement 10: リポジトリ衛生

**Objective:** As a CLOpts の保守者, I want ビルド生成物や取得した依存のソースがリポジトリに混入しないこと, so that リポジトリを清潔に保てる

#### Acceptance Criteria
1. The `.gitignore` shall ビルドディレクトリ、CMake の生成物、テストの出力、主要エディタ / OS の一時ファイルを除外する。
2. The リポジトリ shall FetchContent で取得した Naos のソースをバージョン管理に含めない。

### Requirement 11: コーディング規約への準拠

**Objective:** As a CLOpts の保守者, I want 本 spec で書かれる C ソースが Monde と同じスタイルに従うこと, so that 後続 spec のコードと見た目が揃い、レビューの負担が減る

#### Acceptance Criteria
1. The core モジュールのソース shall Allman スタイル（関数定義、`if` / `else` / `for` / `while`、`struct` / `enum` の定義で `{` を次行の先頭に置く）に従う。
2. The core モジュールのソース shall 本体が 1 文でも波括弧を省略しない。
3. The core モジュールのソース shall 複合条件で各比較を個別に括弧で囲む。
4. The core モジュールのソース shall コメントに `/* ... */` 形式のみを使い、`//` を使わない。
5. The core モジュールのソース shall 公開されない共有シンボルに `clopts_core_` の接頭辞を付け、翻訳単位内で閉じるものは `static` にする。
6. The core モジュールのソース shall ファイル名に `core_` の接頭辞を付けた小文字の snake_case を使い、内部ヘッダのガードを `CLOPTS_CORE_<FILE>_H` の形式にする。

### Requirement 12: 静的ライブラリと共有ライブラリの選択

**Objective:** As a CLOpts を取り込む利用側の開発者, I want CLOpts を静的ライブラリにするか共有ライブラリにするかを、CMake の標準的な慣習と CLOpts 専用のオプションの両方で選べること, so that 自分のプロジェクト全体の方針に合わせつつ、CLOpts だけを別の形式にすることもできる

#### Acceptance Criteria
1. The ビルド構成 shall `clopts` ターゲットを静的ライブラリまたは共有ライブラリのどちらか一方としてビルドし、1 つのビルドツリーで両方を同時には作らない。
2. The ビルド構成 shall 形式を選ぶオプション `CLOPTS_BUILD_SHARED` を提供し、その既定値を、構成した時点の `BUILD_SHARED_LIBS` の値とする（未設定なら静的版）。
3. When 利用側が `CLOPTS_BUILD_SHARED` をキャッシュ変数または（CLOpts を取り込む前に設定した）通常変数で指定する, the ビルド構成 shall `BUILD_SHARED_LIBS` の値にかかわらずその指定に従う。
4. While 静的版をビルドしている, the ビルド構成 shall 利用側が自分の共有ライブラリに CLOpts を静的にリンクできるよう、位置独立コードとしてコンパイルする。
5. While 静的版をビルドしている, the ビルド構成 shall 「静的版を使っている」ことを示すコンパイル定義を、`clopts` ターゲットとそれにリンクする利用側の両方に自動的に伝える。
6. While MSVC で静的版をビルドしている, the ビルド構成 shall 静的ライブラリのファイル名を、共有版のインポートライブラリのファイル名と異なるものにし、両方を同じディレクトリに install しても上書きし合わないようにする。
7. The ビルド構成 shall 形式によらず、利用側がリンクに使う名前を `CLOpts::clopts` の 1 つにする。

### Requirement 13: package config での形式の選択

**Objective:** As a CLOpts をシステムにインストールして使う利用側の開発者, I want 静的版と共有版が同じ prefix に並んで入っていても、`find_package` でどちらを使うかを選べること, so that 1 つのインストール先から用途に応じた形式を使い分けられる

#### Acceptance Criteria
1. While install が有効である, the ビルド構成 shall 静的版と共有版のターゲット定義を別々のファイルとして install し、別々のビルドツリーから同じ prefix に install しても互いに上書きしないようにする。
2. When 利用側が `find_package(CLOpts CONFIG COMPONENTS static)` または `COMPONENTS shared` を指定する, the package config shall 指定された形式の `CLOpts::clopts` を提供する。
3. When 利用側が形式を指定しない, the package config shall `CLOPTS_SHARED_LIBS` が定義されていればその値、なければ `BUILD_SHARED_LIBS` の値に従って形式を選び、選んだ形式が入っていなければ入っている方の形式を提供する。
4. If 利用側が `static` と `shared` の両方を指定する, then the package config shall 検索を失敗させ、両方は同時に使えないことを示すメッセージを出す。
5. If 利用側が指定した形式が install されていない, then the package config shall 検索を失敗させ（`CLOpts_FOUND` を偽にし）、どの形式が見つからなかったかを示すメッセージを出す。
6. The package config shall どちらの形式を提供したかを利用側が CMake の変数で確認できるようにする。

### Requirement 14: 公開シンボルの制御

**Objective:** As a CLOpts の保守者と共有版を使う利用側の開発者, I want 公開 API の関数だけが共有ライブラリのシンボルとして出て、内部の関数は出ないこと, so that 内部関数に依存した利用側がライブラリの更新で壊れることを防ぎ、ABI として約束する範囲を公開 API に限定できる

#### Acceptance Criteria
1. The 公開ヘッダ shall 公開関数に付ける印（export マクロ）を定義し、その印は共有版のビルド時には「出す」、共有版を使う利用側では「取り込む」、静的版ではライブラリ側でも利用側でも「何もしない」を意味するように、利用側の追加の設定なしで切り替わる。
2. While 共有版をビルドしている, the ビルド構成 shall 印を付けた関数だけを共有ライブラリのシンボルとして出し、印のない関数（`clopts_core_` などの内部関数を含む）は出さない。
3. When 共有版を Windows（MSVC または MinGW-w64）で使う利用側が公開ヘッダを include する, the 公開ヘッダ shall 利用側の追加の設定なしで、DLL からの取り込みとして公開関数を宣言する。
4. The 公開ヘッダ shall 対応する 5 環境のいずれでも、静的版・共有版のどちらでも、Requirement 3 の警告設定（`-Werror` 相当を含む）のもとで警告を出さずにコンパイルできる。
5. The 公開ヘッダ shall 構造体・共用体の定義を持たない（本 spec が所有する公開ヘッダについて）。
6. The 公開ヘッダ shall C と C++ の両方の利用側から include でき、C++ の利用側からも C のリンケージで公開関数を参照できるようにする。

### Requirement 15: 共有ライブラリの版

**Objective:** As a CLOpts を共有ライブラリとして配る保守者と、それを使う利用側, I want 共有ライブラリのファイルに、互換性の範囲を表す版が付いていること, so that 互換性のない版を取り違えて読み込むことを OS の仕組みで防げる

#### Acceptance Criteria
1. While 共有版をビルドしている, the ビルド構成 shall ライブラリの版（ルートのプロジェクト定義の版）を共有ライブラリのファイルの版として付ける。
2. While ライブラリの版が 1.0.0 より前である, the ビルド構成 shall 共有ライブラリの互換性の版（SOVERSION）を `0.<minor>` とし、minor が違う版を互換とみなさないようにする。
3. While ライブラリの版が 1.0.0 以上である, the ビルド構成 shall 共有ライブラリの互換性の版を major 番号とする。
4. The ビルド構成 shall 互換性の版をルートのプロジェクト定義の版から導き、別の場所に手で書かない。
5. When Linux または macOS で共有版を install する, the ビルド構成 shall 互換性の版を名前に含むファイルと、リンク用の版のない名前を、その OS の慣習どおりに install する。
6. Where Windows で共有版をビルドする, the ビルド構成 shall DLL のファイル名に版を含めない（Windows の慣習。版の違う DLL の衝突への注意は docs-and-release が文書にする）。

