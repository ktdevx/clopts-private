# Implementation Plan

- [x] 1. 基盤: ビルド骨格とテスト基盤
- [x] 1.1 ルートのビルド定義、コンパイラオプションヘルパ、モジュール追加ヘルパ、最初のモジュールを作り、空のライブラリがビルドできるようにする
  - CMake 3.21 以上を要求し、バージョンをプロジェクト定義の 1 か所にだけ書く。`TARGET clopts` が既にあれば何もせず戻るガードを先頭に置く
  - `CLOPTS_BUILD_TESTING` / `CLOPTS_INSTALL` / `CLOPTS_WERROR` の 3 オプションを、トップレベルのときだけ既定 ON で定義する
  - 静的ライブラリ `clopts` とエイリアス `CLOpts::clopts` を定義し、公開 include 基点を `include/`（install 後は `CMAKE_INSTALL_INCLUDEDIR`。そのためルートで `include(GNUInstallDirs)` を呼ぶ。キャッシュ変数の定義のみで、toolchain テストの検査対象外）、内部 include 基点を `src/` にする。傘ヘッダ `include/clopts/clopts.h` を置く（本タスクでは中身は空のガードのみ）
  - コンパイラオプションヘルパ: GCC / Clang / MinGW / MSVC の分岐、`CLOPTS_WERROR` による `-Werror` / `/WX` の切り替え、C99 の PUBLIC 機能宣言と拡張オフをターゲットプロパティで設定する。グローバル変数やディレクトリ単位の設定は一切置かない
  - モジュール追加ヘルパ: モジュール名とソース一覧を受けて `clopts` にソースを足す。別ターゲットは作らない。SOURCES が空なら構成エラー
  - 最初のモジュール `core` を登録し、サイズ計算のオーバーフロー検査（乗算・加算、オーバーフロー時は出力を変更しない、`out` の NULL は `assert`）を内部ヘッダと 1 つのソースで実装する。コーディング規約（Allman、波括弧、`/* */`、`clopts_core_` 接頭辞、ヘッダガード）に従う
  - `.gitignore` にビルドディレクトリ、CMake 生成物、`_deps/`、テスト出力、エディタ / OS の一時ファイルを追加する
  - 完了条件: ローカルの GCC と Clang で `cmake -S . -B build -DCLOPTS_BUILD_TESTING=OFF && cmake --build build` が警告なしで成功し、`libclopts.a` が生成される。`-DCLOPTS_WERROR=ON` でも同じ
  - _Boundary: RootCMake, CompilerOptions, ModulesHelper, PublicHeaders, CoreSize, GitIgnore_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 9.6, 9.8, 10.1, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

- [x] 1.2 Naos の固定取得とテスト登録ヘルパを作り、最初の単体テストが CTest から走るようにする
  - Naos 取得ヘルパ: タグとコミット SHA を並べて 1 か所に置き、SHA で取得する。取得後に Naos の版ヘッダを読んでタグと照合し、不一致なら修正箇所を示して構成エラーにする。ローカルパス指定（`FETCHCONTENT_SOURCE_DIR_NAOS`）でも同じ照合を行う
  - ルートで `CLOPTS_BUILD_TESTING` が ON のときだけ `include(CTest)` を呼び、さらに `BUILD_TESTING` が ON のときだけ Naos を取得して `tests/` を追加する。`clopts` ターゲットは Naos に依存しない
  - テスト登録ヘルパ: モジュール名とソース一覧から `<module>_tests` を Naos のヘルパで作り、`CLOpts::clopts` と Naos をリンクし、内部ヘッダの include 基点を与え、コンパイラオプションヘルパを適用し、ラベル `<module>` を付ける。Linux で `m` が必要なら足す
  - `tests/core/` にテスト実行ファイルのエントリと、サイズ計算の単体テスト（0 × 任意、`SIZE_MAX × 1`、`(SIZE_MAX / 2 + 1) × 2`、加算の境界、通常値）を置く。Naos マクロの後ろに `;` を置かず、すべてのテストにアサーションを 1 つ以上置く
  - 完了条件: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` が成功し、`ctest -N` に `core_tests.size.<test>` 形式の名前が並び、`ctest -L core` で同じテストだけが走る。`-DBUILD_TESTING=OFF` では Naos が取得されない
  - _Boundary: NaosHelper, ModulesHelper, RootCMake, CoreTests_
  - _Requirements: 2.5, 2.6, 2.10, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 9.9_

- [x] 2. コア: 内部ユーティリティ
- [x] 2.1 可変長配列を単体テスト付きで実装する
  - 要素サイズ指定で初期化し、末尾追加で容量を 8 → 倍々に拡張する。ゼロ初期化の構造体を有効な空状態として扱う
  - 容量拡張前にサイズ計算の検査を通し、オーバーフローなら確保を試みずに失敗を返す。`realloc` 失敗時は既存の要素・長さ・容量を変えずに失敗を返す
  - 解放は冪等にし、解放後は要素サイズだけを残して再利用できる状態に戻す。契約違反（NULL、範囲外 index、要素サイズ 0 での追加）は `assert`
  - 単体テスト: 初期状態の不変条件、100 回追加して全要素一致、容量の境界、巨大な要素サイズで失敗経路に入り戻り値と状態不変を確認（成功した場合も不変条件を検査）、2 回解放、ゼロ初期化への解放、失敗後の解放
  - 完了条件: `ctest -L core` に `core_tests.vec.<test>` が加わりすべて通る。GCC / Clang の `-Wall -Wextra -Wpedantic -Werror -Werror=vla` で警告ゼロ
  - 2.2 と同じファイル（`src/core/CMakeLists.txt`、`core_internal.h`、`tests/core/CMakeLists.txt`、テストのエントリ）を編集するため並列にしない
  - _Boundary: CoreVec, CoreTests_
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.7, 9.8, 9.9, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

- [x] 2.2 書き込みコールバック型と `FILE*` ライタを公開ヘッダと内部ヘルパとして実装し、単体テストを付ける
  - 公開ヘッダ `include/clopts/writer.h` に、`write(ctx, data, len)` 形式で 0 = 成功 / 非 0 = 失敗を返すコールバック型、コールバックとコンテキストを組にしたライタ型、`FILE*` 向けライタを返す関数を宣言する。C++ から include できるようにし、傘ヘッダから include する
  - `FILE*` ライタの実装は `stdio` を include する唯一の翻訳単位に置き、`fwrite` の戻りが `len` 未満なら失敗を返す。`len == 0` では何も書かずに成功
  - 内部ヘルパ（ライタ経由でバイト列・NUL 終端文字列を書き、失敗をそのまま返す）を `stdio` 非依存の翻訳単位に置く。NULL は `assert`
  - 単体テスト: 一時ファイルに書いて読み戻しバイト比較、常に失敗するコールバックで非 0 が返る、メモリに蓄積する自作コールバックで順序を確認、`len == 0`、文字列ヘルパの長さ。一時ファイルの作成は 1 つの関数に閉じる
  - 完了条件: `ctest -L core` に `core_tests.writer.<test>` が加わりすべて通る。`src/core/` で `<stdio.h>` を include するファイルが `FILE*` ライタの翻訳単位だけであることを grep で確認できる
  - 2.1 と共有ファイルを編集するため 2.1 の後に順次実行する
  - _Boundary: PublicHeaders, CoreWriter, CoreTests_
  - _Requirements: 1.2, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 9.8, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

- [x] 3. 配布: install / export と package config
- [x] 3.1 install 規則、エクスポート、package config、version file を生成するヘルパを作る
  - ルートの `CMakeLists.txt` で `CLOPTS_INSTALL` が ON のときだけ install ヘルパを include して呼ぶ
  - `CLOPTS_INSTALL` が ON のときだけ、ライブラリ本体、`include/clopts/` 配下、エクスポートされたターゲット定義（名前空間 `CLOpts::`）、config、version file を標準の install 先に置く。`src/` 配下は install しない
  - version file は `SameMinorVersion` 互換で、アーキテクチャ非依存とは宣言しない
  - config テンプレートで、install 時の MSVC CRT 種別を記録し、利用側の設定と両方非空で不一致なら警告を出す。ベストエフォートである旨をコメントで明記する。ターゲット定義を include し、コンポーネント検査を呼ぶ
  - 完了条件: `cmake --install build --prefix <tmp>` の後、`<tmp>/lib/cmake/CLOpts/` に `CLOptsConfig.cmake`、`CLOptsConfigVersion.cmake`、`CLOptsTargets.cmake` があり、`<tmp>/include/clopts/clopts.h` と `writer.h` があり、`src/` 由来のヘッダがない。手元の最小プロジェクトで `find_package(CLOpts 0.1 CONFIG REQUIRED)` が成功し、`CLOpts::clopts` の `INTERFACE_COMPILE_FEATURES` が `c_std_99` を含む
  - _Boundary: InstallHelper, PackageConfig, RootCMake_
  - _Depends: 2.2_
  - _Requirements: 1.5, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_

- [x] 4. 検証: ビルド構成の自動テスト
- [x] 4.1 子 CMake 項目を構成・ビルド・install・実行して期待結果と照合する共有スクリプトを作る
  - 引数: フィクスチャのソース、作業ディレクトリ、期待（構成成功 / ビルド成功 / ビルド失敗 / 実行成功）、構成名、CLOpts のソース / ビルドディレクトリ、追加引数、ジェネレータとコンパイラ、install 経路モード
  - 作業ディレクトリは毎回削除して作り直す。親と同じジェネレータ・コンパイラ・（単一構成なら）ビルド種別・（MSVC なら）プラットフォームを子に渡す
  - 「ビルド失敗」期待では構成失敗とビルド失敗を区別し、構成失敗はテスト失敗にする。install 経路モードでは先に CLOpts のビルドディレクトリを一時 prefix に install し、`CMAKE_PREFIX_PATH` として子に渡す
  - 失敗時は子プロセスの出力を転記して `FATAL_ERROR`
  - `tests/CMakeLists.txt` に `add_subdirectory(toolchain)` と `add_subdirectory(integration)` を追加し、両ディレクトリに空の `CMakeLists.txt`（ラベルのコメントのみ）を置く。4.2 と 4.3 はそれぞれ自分のディレクトリだけを編集する
  - 完了条件: スクリプトを手で `cmake -P` 起動し、成功を期待するダミー項目で終了コード 0、失敗を期待する項目（存在しないソース）で「ビルド失敗を期待どおり検出」として終了コード 0、構成失敗で非 0 になる
  - _Boundary: RunFixture, ToolchainTests, IntegrationTests_
  - _Requirements: 5.7, 6.1, 6.2, 6.3, 6.4_

- [x] 4.2 (P) ビルド構成の否定テスト（グローバル状態なし、警告のエラー化、VLA 禁止）を CTest に登録する
  - `no_global_state` フィクスチャ: CLOpts を `add_subdirectory` した直後に、`CMAKE_MSVC_RUNTIME_LIBRARY` / `CMAKE_C_STANDARD` / `CMAKE_C_EXTENSIONS` / 3 つの `CMAKE_*_OUTPUT_DIRECTORY` / `BUILD_SHARED_LIBS` / `BUILD_TESTING` が未定義、ディレクトリプロパティ（COMPILE_OPTIONS、COMPILE_DEFINITIONS、INCLUDE_DIRECTORIES、LINK_LIBRARIES）が空、`clopts` の INTERFACE のオプションと定義が空で INTERFACE の機能が `c_std_99` のみ、`Naos::naos` が存在しないことを検査し、違反は構成エラー。CLOpts を 2 回 `add_subdirectory` しても構成エラーにならないことも同じフィクスチャで検査する
  - `warning` フィクスチャ: `CLOPTS_WERROR=ON` で CLOpts を取り込み、未使用変数と符号比較を含むソースにコンパイラオプションヘルパを当てる。ビルド失敗が期待値
  - `vla` フィクスチャ: 同上で VLA を含むソース。ビルド失敗が期待値
  - `tests/toolchain/` 配下だけを編集し、3 テストをラベル `toolchain` で登録する。フィクスチャは `CLOPTS_BUILD_TESTING=OFF` で CLOpts を取り込む
  - 完了条件: `ctest -L toolchain` で `toolchain.no_global_state`、`toolchain.warning_fails`、`toolchain.vla_fails` の 3 件が通る。コンパイラオプションヘルパから `-Werror` を一時的に外すと `warning_fails` が失敗に転じる
  - _Boundary: ToolchainTests_
  - _Depends: 4.1_
  - _Requirements: 2.1, 2.2, 2.4, 2.5, 2.8, 2.9, 3.2, 3.4, 3.7, 4.9, 6.1, 6.2, 6.5_

- [x] 4.3 (P) 利用側を模したプロジェクトで `add_subdirectory` 経路と install → `find_package` 経路を CTest に登録する
  - `consumer` フィクスチャ: モード変数で `add_subdirectory`（テスト・install・`-Werror` を OFF にして取り込む）と `find_package(CLOpts <現在の版> CONFIG REQUIRED)` を切り替え、`app` を `CLOpts::clopts` にリンクするだけでビルドする。`app` には警告フラグを付けない
  - 利用側ソース: 傘ヘッダを include し、`_CRT_SECURE_NO_WARNINGS` が定義されていれば `#error`、未使用引数を持つ `main` で `FILE*` ライタ経由で 1 行書き、終了コード 0
  - `tests/integration/` 配下だけを編集する。`integration.subdirectory`（実行成功期待）と `integration.install_find_package`（install 経路モード、実行成功期待。`CLOPTS_INSTALL` が ON のときのみ登録）をラベル `integration` で登録する
  - 完了条件: `ctest -L integration` で 2 件が通る。コンパイラオプションヘルパの `-Wall` を一時的に INTERFACE にすると `integration.subdirectory` が失敗に転じる
  - _Boundary: IntegrationTests_
  - _Depends: 3.1, 4.1_
  - _Requirements: 2.4, 4.9, 5.3, 5.4, 5.7, 6.3, 6.4, 6.5_

- [ ] 5. CI
- [ ] 5.1 5 環境で構成・ビルド・テストを行う GitHub Actions ワークフローを作る
  - main への push と pull request で起動。`fail-fast: false` のマトリクス: Linux GCC、Linux Clang、macOS Apple Clang、Windows MSVC（Visual Studio ジェネレータ、`--config Release` / `-C Release`）、Windows MinGW-w64（MSYS2 UCRT64、Ninja）
  - 各ジョブで `-DCLOPTS_WERROR=ON` を付けて構成し、ビルドし、`ctest --output-on-failure` を実行する。Linux GCC ジョブでは追加で `-L core`、`-L toolchain`、`-L integration` の絞り込みを 1 回ずつ実行する
  - `permissions: contents: read`。リリース処理は含めない
  - 完了条件: ワークフローの YAML が妥当で、ブランチへ push した CI で 5 ジョブすべてが緑になる。1 ジョブを故意に失敗させても他ジョブは完了する
  - _Boundary: CIWorkflow_
  - _Requirements: 1.6, 7.1, 7.2, 7.3, 7.4, 7.5, 4.5_

- [ ] 6. 最終確認
- [ ] 6.1 spec 全体の受入条件を通しで確認し、規約違反と取りこぼしを直す
  - クリーンな状態から `cmake -S . -B build && cmake --build build && ctest --test-dir build` をローカルの GCC と Clang で実行し、全テスト（`core`、`toolchain`、`integration`）が通る
  - `ctest -N` のテスト名が `<module>_tests.<suite>.<test>` 形式、ラベルが `core` / `toolchain` / `integration` であることを確認する
  - `src/` と `tests/` の C ソースをコーディング規約（Allman、波括弧省略なし、複合条件の括弧、`//` 不使用、`clopts_core_` 接頭辞、ファイル名接頭辞、ヘッダガード形式）で目視と grep（`//`、`^\s*{` の位置、`CLOPTS_CORE_.*_H`）で点検し、違反を修正する
  - ルート `CMakeLists.txt` と `cmake/` に `set(CMAKE_` / `add_compile_options` / `add_definitions` / `include_directories` / `link_libraries` / 無条件の `include(CTest)` がないことを grep で確認する
  - 取得した Naos のソースが `git status` に現れないことを確認する
  - 完了条件: 上記がすべて満たされ、`git status` がクリーンで、要件 1〜11 の各受入条件に対応するテストまたは確認結果を列挙できる
  - _Requirements: 1.6, 2.1, 2.2, 4.5, 4.8, 10.2, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

## Implementation Notes
- Naos v0.4.0 の版は `include/naos/version.h` の `NAOS_VERSION_MAJOR/MINOR/PATCH` の 3 つの整数マクロで表される（文字列マクロではない）。`NAOS_DECLARE_SUITE(x);` は宣言なので `;` が必要、`NAOS_TEST` / `NAOS_SUITE` / `NAOS_MAIN` の後ろには付けない。
- オフライン検証には `-DFETCHCONTENT_SOURCE_DIR_NAOS=<絶対パス>` を使う（相対パスは早期の版照合で version.h が見つからず構成エラーになる）。glibc + GCC では `m` のリンクは不要だったが、design に従い CI で確認するまで残している。
- 可変長配列の `realloc` 失敗経路はフックを置かない方針のため直接テストできない。巨大な要素サイズ（`SIZE_MAX/4`）は確保前のオーバーフロー検査で失敗する。
- `run_fixture.cmake` は子に `FIXTURE_BIN_DIR`（`<WORK_DIR>/bin`）を渡す。`run_ok` のフィクスチャは実行ファイルの `RUNTIME_OUTPUT_DIRECTORY` をこれにする（`CMAKE_RUNTIME_OUTPUT_DIRECTORY` は no_global_state の検査対象なので使わない）。利用側のモードや期待版は `EXTRA_ARGS` で渡す。「ビルド失敗」の検証には存在しないソースではなく存在しないヘッダを使う（ソースの欠落は構成時に失敗する）。
- `toolchain.vla_fails` は `CLOPTS_WERROR=OFF` で取り込み、失敗が `-Werror=vla` そのものから来ることを確かめる（要件 3.7 は WERROR に依存しない）。`no_global_state` はルートのディレクトリスコープの変数も検出できるよう、CLOpts が追加したディレクトリを再帰的に検査する。
- `-Wall` 単独の INTERFACE 漏れは利用側に `-Werror` がないためコンパイルでは検出できない（未使用引数で落ちるのは `-Wall -Wextra -Werror` がそろって漏れたときだけ）。そのため consumer フィクスチャは構成時に `CLOpts::clopts` の `INTERFACE_COMPILE_OPTIONS` に警告フラグがないこと、`INTERFACE_COMPILE_DEFINITIONS` に `_CRT_SECURE_NO_WARNINGS` がないことも検査する。design.md の IntegrationTests の記述はこの検査を反映していない。
