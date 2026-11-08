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

- [x] 5. CI
- [x] 5.1 5 環境で構成・ビルド・テストを行う GitHub Actions ワークフローを作る
  - main への push と pull request で起動。`fail-fast: false` のマトリクス: Linux GCC、Linux Clang、macOS Apple Clang、Windows MSVC（Visual Studio ジェネレータ、`--config Release` / `-C Release`）、Windows MinGW-w64（MSYS2 UCRT64、Ninja）
  - 各ジョブで `-DCLOPTS_WERROR=ON` を付けて構成し、ビルドし、`ctest --output-on-failure` を実行する。Linux GCC ジョブでは追加で `-L core`、`-L toolchain`、`-L integration` の絞り込みを 1 回ずつ実行する
  - `permissions: contents: read`。リリース処理は含めない
  - 完了条件: ワークフローの YAML が妥当で、ブランチへ push した CI で 5 ジョブすべてが緑になる。1 ジョブを故意に失敗させても他ジョブは完了する
  - _Boundary: CIWorkflow_
  - _Requirements: 1.6, 7.1, 7.2, 7.3, 7.4, 7.5, 4.5_

- [x] 6. 最終確認
- [x] 6.1 spec 全体の受入条件を通しで確認し、規約違反と取りこぼしを直す
  - クリーンな状態から `cmake -S . -B build && cmake --build build && ctest --test-dir build` をローカルの GCC と Clang で実行し、全テスト（`core`、`toolchain`、`integration`）が通る
  - `ctest -N` のテスト名が `<module>_tests.<suite>.<test>` 形式、ラベルが `core` / `toolchain` / `integration` であることを確認する
  - `src/` と `tests/` の C ソースをコーディング規約（Allman、波括弧省略なし、複合条件の括弧、`//` 不使用、`clopts_core_` 接頭辞、ファイル名接頭辞、ヘッダガード形式）で目視と grep（`//`、`^\s*{` の位置、`CLOPTS_CORE_.*_H`）で点検し、違反を修正する
  - ルート `CMakeLists.txt` と `cmake/` に `set(CMAKE_` / `add_compile_options` / `add_definitions` / `include_directories` / `link_libraries` / 無条件の `include(CTest)` がないことを grep で確認する
  - 取得した Naos のソースが `git status` に現れないことを確認する
  - 完了条件: 上記がすべて満たされ、`git status` がクリーンで、要件 1〜11 の各受入条件に対応するテストまたは確認結果を列挙できる
  - _Requirements: 1.6, 2.1, 2.2, 4.5, 4.8, 10.2, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

## 改訂（2026-11-08: 共有ライブラリ化と ABI 互換）

> 1〜6 は初版の実装（完了済み）。以下は改訂後の requirements.md / design.md に合わせた差分の実装である。初版から変えない部品（CompilerOptions、NaosHelper、CoreVec、CoreSize、GitIgnore、warning / vla の fixture）には触れない。

- [x] 7. 基盤: 形式の選択と公開シンボルの印
- [x] 7.1 ライブラリ作成ヘルパを新設し、静的版と共有版を選べるようにする
  - 静的に固定していたライブラリの定義をルートから外し、新しいヘルパで作る。形式は新しいオプションで選ぶ。既定値は構成した時点の `BUILD_SHARED_LIBS` で、未定義なら静的。親が通常変数で指定した値が優先される
  - 版から SOVERSION と package の互換規則を計算する小関数を置く。0.x は `0.<minor>` と minor 一致、1.0 以上は major と major 一致。計算結果を呼び出し元に返す
  - 静的版には位置独立コード、静的版を示す定義（ライブラリ自身と利用側の両方へ伝わる形）を付ける。MSVC では静的ライブラリの出力名を共有版のインポートライブラリと別にする
  - 共有版には「ライブラリ自身のビルド中」を示す定義、Windows 以外では既定で非公開になる可視性、版と SOVERSION を付ける
  - 形式によらず別名 `CLOpts::clopts` を定義する。グローバル変数やディレクトリ単位の設定は増やさない
  - 静的版の定義が利用側へ伝わるようになるので、グローバル状態なしの fixture のうち `clopts` の利用側へ伝わる定義の検査だけを「静的版なら静的版の定義だけ、共有版なら空」に直す（検査対象の変数の追加や形式の受け渡しは 10.2）。既存の toolchain テストを赤にしないための最小の変更である
  - 完了条件: ローカルの GCC で、既定の構成では静的版のままビルドとテスト（`toolchain.no_global_state` を含む）が通る。`-DCLOPTS_BUILD_SHARED=ON -DCLOPTS_BUILD_TESTING=OFF` では `libclopts.so.0.1.0` と版付きの名前のリンクができ、`nm -D --defined-only` に `clopts_core_` で始まるシンボルが現れない
  - _Boundary: RootCMake, LibraryHelper, ToolchainTests_
  - _Requirements: 1.1, 1.5, 2.1, 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 14.2, 15.1, 15.2, 15.3, 15.4, 15.6_

- [x] 7.2 共有版でも内部関数を単体テストできるよう、テスト用の静的コピーとテスト登録の切り替えを入れる
  - 共有版でテストが有効なときだけ、同じソースからテスト専用の静的ライブラリを作る（全体ビルドからは外し、install も export もしない）。include の基点、コンパイラオプション、静的版の定義は本体と同じにする
  - モジュール追加ヘルパは、テスト専用のライブラリがあれば同じソースをそちらにも足す。テスト登録ヘルパは、静的版では本体に、共有版ではテスト専用のライブラリにリンクする
  - 完了条件: 共有版（`-DCLOPTS_BUILD_SHARED=ON`）で `ctest -L core` と `ctest -L parser` が通り、テスト実行ファイルが共有ライブラリに依存していない（`ldd` に `libclopts` が現れない）。静的版の挙動は変わらない
  - _Boundary: LibraryHelper, ModulesHelper_
  - _Requirements: 1.4, 4.1, 4.2, 4.3, 4.4, 4.10, 4.11_

- [x] 7.3 (P) 公開シンボルの印の公開ヘッダを作り、解析 API の公開関数に暫定で印を付ける
  - 印は静的版では空、共有版のビルド中は「出す」、共有版の利用側では「取り込む」に切り替わる。判定は静的版の定義、Windows、GCC 4 以上の順で、MinGW で可視性の属性を使わない
  - 解析 API の公開ヘッダには、印のヘッダの include と、13 個の公開関数の宣言の先頭への印だけを足す（型・関数・コメントは変えない。core-parser の作り直しまでの暫定）
  - 完了条件: 共有版の `nm -D --defined-only libclopts.so` に `clopts_` で始まるシンボルがちょうど 13 個（解析 API の公開関数）あり、`clopts_core_` / `clopts_parser_` で始まるものがない。静的版（テスト込み）と共有版（`-DCLOPTS_BUILD_TESTING=OFF`。共有版でテストを有効にするには 7.2 が必要）が `-Werror` で警告なくビルドできる
  - 7.2 と触るファイルが重ならない（7.2 は CMake のヘルパ、本タスクは公開ヘッダ）
  - _Depends: 7.1_
  - _Boundary: PublicHeaders_
  - _Requirements: 14.1, 14.3, 14.4, 14.5, 14.6_

- [x] 8. コア: 書き込みコールバックを構造体と `FILE*` の受け渡しなしの形に作り直す
  - 公開ヘッダからコールバックとコンテキストを組にした構造体と、`FILE*` を受け取るライブラリ関数を外し、`FILE*` をコンテキストに渡す既製のコールバックを公開ヘッダ内の `static inline` として置く（書き込めなければ失敗、長さ 0 は何もせず成功）
  - 内部の書き込みヘルパをコールバックとコンテキストを別々に受け取る形に変え、`stdio` を使う翻訳単位を削除する
  - 書き込みの単体テストを書き直す（既製のコールバックの全バイト書き込みと読み戻し、長さ 0、読み取り専用の `FILE*` への書き込み失敗、内部ヘルパの順序・長さ・失敗の伝播）
  - integration の利用側 fixture の `main.c` を、新しい既製のコールバックで 1 行書く形に直す（削除した関数を使っているため。fixture の他の変更は 10.3）
  - 完了条件: 静的版と共有版の両方で `ctest -L core` と `ctest -L integration` が通り、`src/core/` に `<stdio.h>` の include がない。公開ヘッダに構造体の定義がない
  - _Boundary: PublicHeaders, CoreWriter, CoreTests, IntegrationTests（利用側 fixture の `main.c` の最小の追従のみ）_
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 9.8, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 14.5_

- [x] 9. 配布: 形式ごとの install / export と、package config での形式の選択
  - ライブラリの install に、共有版の実行時ファイル（Windows では `bin` の DLL）、Linux / macOS の版付きファイルとリンク用の名前、Windows のインポートライブラリの行き先を足す
  - ターゲット定義のファイルを形式ごとの名前（静的版用と共有版用）で install し、同じ prefix に両方を入れても上書きし合わないようにする。静的版だけが、ビルド時の MSVC の CRT 種別を記録する断片を生成して install する。version file の互換規則は 7.1 の計算結果から取る（`ARCH_INDEPENDENT` は付けない）
  - package config を作り直す: コンポーネント `static` / `shared` の指定、指定なしのときの `CLOPTS_SHARED_LIBS` → `BUILD_SHARED_LIBS` → 入っている方の順の既定、両方の指定・未知のコンポーネント・入っていない形式の指定での失敗（理由を示すメッセージ）、`CLOpts_LIBRARY_TYPE`、静的版を選んだときだけの CRT の警告、2 回目の `find_package` で別の形式を求められたときの失敗。config の内容は形式によらず同じにする
  - install のファイル名と config を同じタスクで変えるのは、途中で install 経路のテストを赤にしないためである
  - 完了条件: 既存の `integration.install_find_package` が静的版で通る。静的版と共有版を同じ prefix に install すると、両方のターゲット定義ファイル、共有版の `libclopts.so.0.1.0` / `libclopts.so.0.1` / `libclopts.so`、静的版の `libclopts.a` と CRT 記録の断片が並び、config と version file は 1 つずつになる。その prefix に対して手元で `COMPONENTS static` / `COMPONENTS shared` / 指定なしの 3 通りが期待どおりの形式を返し、`COMPONENTS static shared` と「静的版を読み込んだ後に `COMPONENTS shared` で 2 回目の `find_package`」が理由つきで失敗する
  - _Boundary: InstallHelper, PackageConfig_
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 12.7, 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 15.5_

- [ ] 10. 検証: ビルド構成の自動テストを両方の形式に広げる
- [x] 10.1 fixture を動かす共有スクリプトに、構成失敗の期待、失敗理由の照合、DLL などの除外を足す
  - 期待値に「構成が失敗する」を足す。失敗を期待するケースで、子プロセスの出力が指定の正規表現に一致しなければテスト失敗にする
  - 実行するファイルを探すとき、共有ライブラリ（`.dll`、`.so`、版付きの `.so`、`.dylib`）を候補から外す
  - 完了条件: 既存の toolchain / integration のテストが静的版で変わらず通り、手元で、存在しないシンボルへの参照を持つ一時 fixture に `build_fail` と合わない正規表現を与えるとテストが失敗する
  - _Boundary: RunFixture_
  - _Requirements: 6.5_

- [x] 10.2 (P) toolchain の検証を、グローバル状態の検査の更新と形式の決定のテストで広げる
  - グローバル状態なしの検査に、可視性の既定と位置独立コードの既定の変数を足し、利用側へ伝わる定義の検査は 7.1 で直したものをそのまま使い、ビルドツリーの形式を fixture に渡して共有版のケースも実際に検査されるようにする
  - 形式の決定の fixture を新設し、既定・`BUILD_SHARED_LIBS` だけ・`BUILD_SHARED_LIBS` と CLOpts 専用の指定の上書き、の 3 ケースを登録する。型と形式ごとのプロパティ（位置独立コード、静的版の定義、MSVC の出力名、共有版の版・SOVERSION・ビルド中の定義・可視性）を構成時に検査し、SOVERSION の計算は現在の版と仮の 1.x 版の両方で検査する。形式の決定の fixture には親のビルドツリーの形式を渡さない（キャッシュに入ると 3 ケースの意味が変わるため。形式を渡すのはグローバル状態なしの fixture だけ）
  - 完了条件: 静的版と共有版の両方で `ctest -L toolchain` が通り、`toolchain.library_type.default` / `.build_shared_libs` / `.override` が並ぶ
  - _Depends: 7.1_
  - _Boundary: ToolchainTests_
  - _Requirements: 2.1, 2.2, 2.9, 4.9, 6.1, 6.2, 12.2, 12.3, 12.4, 12.5, 12.6, 15.1, 15.2, 15.3, 15.4_

- [x] 10.3 (P) integration の利用側テストを両方の形式と C++ に広げ、内部関数が見えないことのテストを足す
  - ビルドツリーの形式を利用側 fixture に渡し、`CLOpts::clopts` の型が期待と一致することを構成時に確かめる。install 経路では、ビルドツリーの形式を選ぶよう指定して `find_package` する
  - 利用側 fixture の `main.c` は 8 で新しい既製のコールバックに移した状態から始める
  - 利用側 fixture で C++ を有効にし、公開の傘ヘッダを include して既製のコールバックを使う C++ の翻訳単位を足す
  - Windows の共有版では、ビルド後に実行時に必要な DLL を実行ファイルの横へコピーする
  - 内部関数（サイズ計算の乗算）を自分で宣言して呼ぶ fixture を新設し、共有版のときだけ「ビルド失敗、出力にその関数名を含む」を期待するテストとして登録する
  - 完了条件: 静的版と共有版の両方で `integration.subdirectory` と `integration.install_find_package` が実行まで通り、共有版では `integration.internal_hidden` も通る
  - _Depends: 7.3, 8, 9, 10.1_
  - _Boundary: IntegrationTests_
  - _Requirements: 2.4, 4.10, 5.7, 6.3, 6.4, 6.6, 6.8, 14.2, 14.3, 14.6_

- [ ] 10.4 静的版と共有版を並べて install し、`find_package` の選択を検証するテストを足す
  - 2 つの形式を構成・ビルドして同じ prefix（と静的版だけの prefix）に install し、選択用の fixture を設計の 8 ケースで構成だけ行って、選ばれた形式または失敗とその理由を照合するスクリプトを作る。MSVC では 2 つの `.lib` が並ぶことも確かめる
  - install が有効なときだけ `integration.package_selection` として登録する
  - 完了条件: 静的版と共有版のどちらのビルドツリーからでも `integration.package_selection` が通り、失敗したケースはケース名と子プロセスの出力を表示する
  - 10.3 と同じ `tests/integration/CMakeLists.txt` を編集するため、10.3 の後に行う
  - _Depends: 9, 10.1, 10.3_
  - _Boundary: PackageSelectionTest_
  - _Requirements: 6.7, 7.6, 12.6, 13.2, 13.3, 13.4, 13.5_

- [ ] 11. CI を 5 環境 × 2 形式に広げる
  - マトリクスを toolchain と形式（静的・共有）の組み合わせにし、構成に形式の指定を渡す。ジョブ名に両方を含める。失敗は他のジョブを止めない
  - MSYS2 の準備で C++ コンパイラが入ることを確かめる。ラベル別の再実行は linux-gcc の両方の形式で行う
  - 完了条件: push した PR で 10 ジョブが走り、すべて `-Werror` 付きで構成・ビルド・テスト（`integration.package_selection` を含む）に成功する
  - _Requirements: 1.6, 4.5, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 14.4_

- [ ] 12. 最終確認: 改訂後の受入条件を通しで確認する
  - クリーンな状態から、ローカルの GCC と Clang で静的版と共有版をそれぞれ構成・ビルド・テストし、全テストが通る
  - `ctest -N` のテスト名とラベル（`core` / `parser` / `toolchain` / `integration`）、共有版の公開シンボル（7.3 の条件）、ルートと `cmake/` にグローバル設定がないこと（grep）、`git status` がクリーンであることを確認する
  - 初版から変えていない要件（コンパイラ別のフラグ、Naos の固定、可変長配列、`.gitignore`、コーディング規約）が退行していないことを、既存のテストと grep で確認する
  - 完了条件: 上記がすべて満たされ、要件 1〜15 の各受入条件に対応するテストまたは確認結果を列挙できる
  - _Requirements: 1.2, 1.3, 1.6, 1.7, 2.3, 2.5, 2.6, 2.7, 2.8, 2.10, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 4.5, 4.6, 4.7, 4.8, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.9, 10.1, 10.2, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

## Implementation Notes

> 以下は初版（タスク 1〜6）の実装メモ。改訂（タスク 7〜12）でも有効なものは引き継ぐ。`clopts_writer_file` と `core_writer_file.c` に関する記述は、タスク 8 で無効になる。
- Naos v0.4.0 の版は `include/naos/version.h` の `NAOS_VERSION_MAJOR/MINOR/PATCH` の 3 つの整数マクロで表される（文字列マクロではない）。`NAOS_DECLARE_SUITE(x);` は宣言なので `;` が必要、`NAOS_TEST` / `NAOS_SUITE` / `NAOS_MAIN` の後ろには付けない。
- オフライン検証には `-DFETCHCONTENT_SOURCE_DIR_NAOS=<絶対パス>` を使う（相対パスは早期の版照合で version.h が見つからず構成エラーになる）。glibc + GCC では `m` のリンクは不要だったが、design に従い CI で確認するまで残している。
- 可変長配列の `realloc` 失敗経路はフックを置かない方針のため直接テストできない。巨大な要素サイズ（`SIZE_MAX/4`）は確保前のオーバーフロー検査で失敗する。
- `run_fixture.cmake` は子に `FIXTURE_BIN_DIR`（`<WORK_DIR>/bin`）を渡す。`run_ok` のフィクスチャは実行ファイルの `RUNTIME_OUTPUT_DIRECTORY` をこれにする（`CMAKE_RUNTIME_OUTPUT_DIRECTORY` は no_global_state の検査対象なので使わない）。利用側のモードや期待版は `EXTRA_ARGS` で渡す。「ビルド失敗」の検証には存在しないソースではなく存在しないヘッダを使う（ソースの欠落は構成時に失敗する）。
- `toolchain.vla_fails` は `CLOPTS_WERROR=OFF` で取り込み、失敗が `-Werror=vla` そのものから来ることを確かめる（要件 3.7 は WERROR に依存しない）。`no_global_state` はルートのディレクトリスコープの変数も検出できるよう、CLOpts が追加したディレクトリを再帰的に検査する。
- `-Wall` 単独の INTERFACE 漏れは利用側に `-Werror` がないためコンパイルでは検出できない（未使用引数で落ちるのは `-Wall -Wextra -Werror` がそろって漏れたときだけ）。そのため consumer フィクスチャは構成時に `CLOpts::clopts` の `INTERFACE_COMPILE_OPTIONS` に警告フラグがないこと、`INTERFACE_COMPILE_DEFINITIONS` に `_CRT_SECURE_NO_WARNINGS` がないことも検査する。design.md の IntegrationTests の記述はこの検査を反映していない。
- MSVC の `/W4 /WX` では `(char)0xFF` のような定数の切り捨てキャストが C4310 でエラーになる。バイト値は `'\xFF'` のような文字リテラルで書く。CI の 5 環境（windows-latest は Visual Studio 18 2026）で全 31 テストが通ることを確認済み。
- （改訂 9）package config は、形式を指定しない 2 回目の `find_package` では既に読み込まれた形式をそのまま使う（失敗するのは明示のコンポーネントが異なるときだけ）。二重読み込みの判定は `TARGET CLOpts::clopts` の有無なので、兄弟ディレクトリで別々の形式を読み込むと両方成功する（IMPORTED ターゲットはディレクトリスコープ）。`OPTIONAL_COMPONENTS static|shared` も明示の要求として扱う。
- （改訂 10.1）`run_fixture.cmake` の `FAIL_REGEX` は CMake の `MATCHES` で照合する（`\d` などは使えない）。stdout と stderr をまとめた出力が対象。`FAIL_REGEX` は `build_fail` / `configure_fail` 以外と組み合わせると構成エラーになる。
- （改訂 10.3）`run_fixture.cmake` は C コンパイラだけを子に渡すので、consumer fixture の C++ は既定の C++ コンパイラ（Clang の構成でも g++）でビルドされる。`consumer_cpp.cpp` は `&clopts_parse` を volatile ポインタに入れて、C++ からの C リンケージ（と Windows の dllimport）を実際にリンクで検査する。
