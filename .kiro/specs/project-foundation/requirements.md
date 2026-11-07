# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts を開発する開発者、および後続の 4 spec（core-parser、help-and-messages、platform-argv、docs-and-release）を実装するエージェント。ライブラリのコードを書き始める前に、ビルド・テスト・CI・配布の土台を必要としている。

**現状**: リポジトリには `LICENSE`（MIT、The CLOpts Authors）、1 行の `README.md`、`.gitignore`、Kiro の skill とテンプレートしかなく、CMake 構成・ソース・テスト・CI は存在しない。参照元の Monde には相当する土台（`cmake/Monde*.cmake`、`cmake/MondeNaos.cmake`、`.github/workflows/ci.yml`）があるが、トップレベルでの `CMAKE_MSVC_RUNTIME_LIBRARY` 設定や無条件の `include(CTest)` などアプリケーション向けの設定を含むため、`add_subdirectory` / `FetchContent` で取り込まれるライブラリにはそのまま使えない。

**何を変えるか**: Monde の CMake ヘルパを参考にライブラリ向けに作り直し、以下を用意する。
- `cmake -S . -B build && cmake --build build && ctest --test-dir build` が 5 つの CI 環境（MSVC / MinGW-w64 / Apple Clang / GCC / Clang）すべてで通る空のライブラリ（`clopts` ターゲットと `clopts::clopts` エイリアス、公開ヘッダの置き場 `include/clopts/`）。
- 利用側が `add_subdirectory`、`FetchContent_MakeAvailable`、`find_package(clopts CONFIG)` のいずれでも `target_link_libraries(app PRIVATE clopts::clopts)` だけで使え、利用側のビルド設定（CRT 種別、C 規格、警告、出力ディレクトリ、`BUILD_SHARED_LIBS`）を汚さないこと。設定はすべてターゲット単位とし、テスト・install・`-Werror` はトップレベル時またはオプション（`CLOPTS_BUILD_TESTING` 等）で囲む。`if(NOT TARGET clopts)` で二重定義を防ぐ。
- install/export（`install(EXPORT ... NAMESPACE clopts::)`、`write_basic_package_version_file`、`ARCH_INDEPENDENT` なし）。MSVC の CRT 種別は利用側から継承し、install 経路ではビルド時の CRT を記録して config で警告または文書化する。
- 後続 spec が「モジュールのソースを追加し、`tests/<module>/` に Naos のテストを追加する」だけで済むモジュール追加・テスト登録の CMake ヘルパ。Naos は v0.4.0 とコミット SHA `09fde7172c4b63d416c32e98499cb0c0306d3dea` で固定し、テスト有効時のみ取得、版の管理は 1 か所。テスト実行ファイルにも CLOpts のコンパイラオプションを適用する。
- 内部ユーティリティ: 書き込みコールバック型（`write(ctx, data, len)`）と `FILE*` 向け既製ライタ、可変長配列、必要に応じてサイズ計算のオーバーフロー検査。いずれも単体テスト付き。
- 5 環境でビルドとテストを行う CI ワークフロー、ビルド構成の要件（グローバル設定を置かないこと、install 経路が動くこと）を CTest で自動検証するテスト、LICENSE への Monde 由来コードの著作権表示（`Copyright (c) 2026 The Monde Authors`）の併記、`.gitignore` の整備。

**制約**: C99、外部依存ゼロ、CMake 3.21 以上。GCC / Clang は `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、拡張機能オフ、MinGW では `__USE_MINGW_ANSI_STDIO=1`。MSVC は `/std:c11`（C99 として使用）、`/W4 /WX /utf-8 /Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を PRIVATE 定義。コーディングスタイルは Monde と同じ（Allman、波括弧省略なし、複合条件の括弧、`//` 不使用）。

**対象外**: 引数解析の機能、ヘルプ文面、Windows argv、README の本文、リリースワークフロー。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
