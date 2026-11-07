# Requirements Document

## Introduction

CLOpts は C99 で書かれた外部依存ゼロのコマンドライン引数パーサライブラリである。本 spec `project-foundation` は、ライブラリの機能を実装する前に必要な土台、すなわちビルド構成、テスト基盤、CI、配布（install / package config）、そしてすべての内部モジュールが依存する内部ユーティリティを整える。

参照元の Monde にはアプリケーション向けの同等の土台があるが、トップレベルでの CRT 種別の設定や無条件のテスト有効化など、他プロジェクトに取り込まれるライブラリにはそのまま使えない設定を含む。本 spec では「利用側のプロジェクトに取り込まれても利用側のビルド設定を汚さない」ことを第一の品質要件として、ライブラリ向けに作り直す。

成果物は、引数解析の機能を一切持たない「空のライブラリ」と、後続 spec がソースとテストを追加するだけで済む仕組みである。

## Boundary Context

- **In scope**: ルートのビルド定義とヘルパ、`clopts` ターゲットと `CLOpts::clopts` エイリアス、公開ヘッダの置き場（`include/clopts/`）、`src/` と `tests/` のディレクトリ構成、モジュール追加とテスト登録のヘルパ、テストフレームワーク Naos の固定取得、install / export と package config、ビルド構成の要件を自動検証するテスト、5 環境の CI ワークフロー、内部ユーティリティ（書き込みコールバック型、`FILE*` ライタ、可変長配列、サイズ計算のオーバーフロー検査）とその単体テスト、`.gitignore` の整備
- **Out of scope**: 引数解析の機能一切と公開 API の宣言型（core-parser）、ヘルプ・エラーの文面（help-and-messages）、Windows の argv と環境変数の取得（platform-argv）、README の本文・利用者向け文書・サンプルプログラム・リリースワークフロー・利用側を模した取り込み経路の利用テスト（docs-and-release）
- **Adjacent expectations**:
  - 後続 spec（core-parser、help-and-messages、platform-argv）は、本 spec が用意するモジュール追加・テスト登録の仕組みに従ってソースとテストを追加する。本 spec はそれらのモジュールの中身を持たない。
  - help-and-messages は本 spec が定義する書き込みコールバック型と `FILE*` ライタをそのまま使う。
  - docs-and-release は本 spec の install 経路を前提に、利用側を模したプロジェクトでの利用テストとリリース手順を担う。本 spec では install 経路が動作することの検証までを行う。

## Requirements

### Requirement 1: ライブラリターゲットと公開ヘッダの骨格

**Objective:** As a CLOpts の開発者, I want 機能を持たない空のライブラリターゲットと公開ヘッダの置き場が用意されていること, so that 後続 spec が最初からソースを追加するだけで開発を始められる

#### Acceptance Criteria
1. The ビルド構成 shall 静的ライブラリターゲット `clopts` とそのエイリアス `CLOpts::clopts` を定義する。
2. The ビルド構成 shall 公開ヘッダの置き場として `include/clopts/` を持ち、傘ヘッダ `include/clopts/clopts.h` を提供する。
3. The ビルド構成 shall `include/` を `clopts` ターゲットの公開 include 基点とし、利用側が `#include <clopts/clopts.h>` で公開ヘッダを参照できるようにする。
4. The ビルド構成 shall 内部モジュールのソースを `src/<module>/` に置き、内部ヘッダの include 基点を `src/` としてライブラリ内部に限定する。
5. The ビルド構成 shall バージョン番号を記述する場所をルートのプロジェクト定義の 1 か所に限定する。
6. When 開発者がクリーンな状態で `cmake -S . -B build && cmake --build build && ctest --test-dir build` を実行する, the ビルド構成 shall 対応する 5 つの CI 環境（MSVC、MinGW-w64、Apple Clang、GCC、Clang）のすべてで構成・ビルド・テストを成功させる。
7. The ビルド構成 shall CMake 3.21 以上を最低要件として宣言する。

### Requirement 2: サブプロジェクトとしての行儀

**Objective:** As a CLOpts を取り込む利用側の開発者, I want CLOpts を `add_subdirectory` や `FetchContent` で取り込んでも自分のビルド設定が変わらないこと, so that 自分のプロジェクトの CRT 種別・C 規格・警告・出力先を自分で決められる

#### Acceptance Criteria
1. The ビルド構成 shall `CMAKE_MSVC_RUNTIME_LIBRARY`、`CMAKE_C_STANDARD`、`CMAKE_C_EXTENSIONS`、`CMAKE_*_OUTPUT_DIRECTORY`、`BUILD_SHARED_LIBS` の既定値のいずれも設定しない。
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

### Requirement 5: install / export と package config

**Objective:** As a CLOpts をシステムにインストールして使う利用側の開発者, I want `find_package(CLOpts CONFIG)` で見つけて `CLOpts::clopts` にリンクするだけで使えること, so that 取り込み方法によらず同じリンク手順で済む

#### Acceptance Criteria
1. While install が有効である, the ビルド構成 shall ライブラリ本体、`include/clopts/` 配下の公開ヘッダ、エクスポートされたターゲット定義、package config ファイル、バージョンファイルを install する。
2. The ビルド構成 shall 内部モジュールのヘッダ（`src/` 配下）を install しない。
3. When 利用側が install 済みの CLOpts を `find_package(CLOpts CONFIG)` で検索する, the package config shall インポートターゲット `CLOpts::clopts` を提供し、公開ヘッダの include パスと C99 の要件を利用側に伝える。
4. When 利用側が `find_package(CLOpts <version> CONFIG)` でバージョンを指定する, the バージョンファイル shall インストール済みの版との互換性を判定し、非互換の版では検索を失敗させる。
5. The バージョンファイル shall アーキテクチャ非依存とは宣言せず、異なるアーキテクチャ向けのインストールを互換とみなさない。
6. The ビルド構成 shall install 時に使われた MSVC の CRT 種別を記録し、利用側が異なる CRT 種別で `find_package` した場合に警告を出すか、または package config の文書でその制約を明記する。
7. When 開発者が install を実行してから、install 先を参照する利用側を模した最小プロジェクトを構成・ビルドする, the テスト基盤 shall `find_package(CLOpts CONFIG)` と `target_link_libraries(app PRIVATE CLOpts::clopts)` だけで利用側がビルドできることを CTest のテストとして検証する。

### Requirement 6: ビルド構成の自動検証

**Objective:** As a CLOpts の保守者, I want ビルド構成の要件がテストとして検証されること, so that 後から加えた変更が利用側を汚す設定を持ち込んだときに CI で気づける

#### Acceptance Criteria
1. The テスト基盤 shall `tests/toolchain/` に、Requirement 2.1 に挙げたグローバル変数が CLOpts の構成によって設定されていないことを検証するテストを持つ。
2. The テスト基盤 shall `tests/toolchain/` に、警告がエラー化されることと VLA がビルドエラーになることを、意図的に警告・VLA を含むソースのビルドが失敗することで検証するテストを持つ。
3. The テスト基盤 shall `tests/integration/` に、CLOpts を `add_subdirectory` で取り込んだ利用側を模した最小プロジェクトが、`target_link_libraries(app PRIVATE CLOpts::clopts)` だけでビルドでき、かつ利用側に CLOpts の警告オプションや内部コンパイル定義が伝播していないことを検証するテストを持つ。
4. The テスト基盤 shall `tests/integration/` に、Requirement 5.7 の install 経路の検証を持つ。
5. When `ctest --test-dir build` を実行する, the テスト基盤 shall 上記の検証テストをすべて含めて実行する。

### Requirement 7: CI ワークフロー

**Objective:** As a CLOpts の保守者, I want main への push と pull request で 5 環境のビルドとテストが自動実行されること, so that 対応環境のいずれかで壊れた変更を取り込む前に検出できる

#### Acceptance Criteria
1. When main ブランチへの push または pull request が発生する, the CI ワークフロー shall MSVC、MinGW-w64（msys2 ucrt64）、Apple Clang、GCC、Clang の 5 環境それぞれで構成・ビルド・テストを実行する。
2. The CI ワークフロー shall 各環境で `-Werror` 相当を有効にしてビルドする。
3. If いずれかの環境で構成・ビルド・テストのいずれかが失敗する, then the CI ワークフロー shall その環境のジョブを失敗として報告し、他の環境のジョブは中断せず完了させる。
4. When テストが失敗する, the CI ワークフロー shall 失敗したテストの出力をジョブログに含める。
5. The CI ワークフロー shall リリースの処理（タグの検査、GitHub Release の作成）を含まない。

### Requirement 8: 内部ユーティリティ — 書き込みコールバック

**Objective:** As a 後続 spec（help-and-messages）を実装する開発者, I want 出力先を抽象化した書き込みコールバック型と `FILE*` 向けの既製ライタがあること, so that ライブラリが stdout / stderr に直接書かずに文面を生成できる

#### Acceptance Criteria
1. The core モジュール shall `write(ctx, data, len)` の形で任意の出力先へバイト列を書き込むコールバック型を定義する。
2. The core モジュール shall コールバック関数ポインタと利用側コンテキストを組にしたライタ型を定義する。
3. The core モジュール shall 任意の `FILE*` へ書き込む既製ライタを構築する手段を提供する。
4. When `FILE*` ライタに長さ `len` のバイト列を書き込む, the `FILE*` ライタ shall その `len` バイトを順序どおり指定の `FILE*` に書き込む。
5. If 書き込みコールバックが失敗を報告する, then the 書き込みユーティリティ shall その失敗を呼び出し元に伝え、以降の書き込みを継続するか中断するかを呼び出し元が判断できるようにする。
6. The core モジュール shall `stdio` への依存を `FILE*` ライタの翻訳単位に閉じ込め、他のユーティリティから `stdio` に依存しない。
7. The core モジュール shall 書き込みコールバック型と `FILE*` ライタの動作を `tests/core/` の単体テストで検証する。

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
