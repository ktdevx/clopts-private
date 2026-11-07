# Brief: project-foundation

## Problem
CLOpts を開発する開発者（および後続の 4 spec を実装するエージェント）は、ライブラリのコードを書き始める前に、ビルド・テスト・CI・配布の土台を必要とする。Monde には相当する土台（`cmake/Monde*.cmake`、`cmake/MondeNaos.cmake`、`.github/workflows/ci.yml`）があるが、アプリケーション向けの設定（トップレベルで `CMAKE_MSVC_RUNTIME_LIBRARY` を設定する、`include(CTest)` を無条件に行う、など）が含まれており、他プロジェクトに `add_subdirectory` / `FetchContent` で取り込まれるライブラリにはそのまま使えない。

## Current State
リポジトリには `LICENSE`（MIT、The CLOpts Authors）、1 行の `README.md`、`.gitignore`、Kiro の skill とテンプレートだけがあり、CMake 構成・ソース・テスト・CI は存在しない。参照元の Monde は C99、CMake 3.21+、Naos v0.4.0（FetchContent）、CI マトリクス MSVC / MinGW-w64 / Apple Clang / GCC / Clang で、警告は `cmake/MondeCompilerOptions.cmake` でターゲット単位に適用している。

## Desired Outcome
- `cmake -S . -B build && cmake --build build && ctest --test-dir build` が 5 つの CI 環境すべてで通る空のライブラリ（`clopts` ターゲットと `CLOpts::clopts` エイリアス）が存在する。
- 利用側が `add_subdirectory(clopts)`、`FetchContent_MakeAvailable(clopts)`、`find_package(CLOpts CONFIG)` のいずれでも `target_link_libraries(app PRIVATE CLOpts::clopts)` だけで使え、利用側のビルド設定（CRT 種別、C 規格、警告）を汚さない。
- 後続 spec が「モジュールのソースを追加し、`tests/<module>/` に Naos のテストを追加する」だけで済む仕組み（モジュール追加とテスト登録の CMake ヘルパ）がある。
- 書き込みコールバック型（`clopts_writer_t` 相当: `write(ctx, data, len)`）と `FILE*` 向けの既製ライタ、可変長配列などの内部ユーティリティが、単体テスト付きで用意されている。

## Approach
Monde の `cmake/MondeCompilerOptions.cmake` / `MondeTargets.cmake` / `MondeNaos.cmake` を参考に、ライブラリ向けに作り直す。すべての設定をターゲット単位（`target_compile_features(clopts PUBLIC c_std_99)`、`target_compile_options(... PRIVATE ...)`）にし、テスト・install・`-Werror` は `if(PROJECT_IS_TOP_LEVEL)`（CMake 3.21）または `CLOPTS_BUILD_TESTING` 等のオプションで囲む。install では `install(EXPORT ... NAMESPACE clopts::)` と `write_basic_package_version_file`（静的ライブラリなので `ARCH_INDEPENDENT` は付けない）で package config を出す。MSVC の CRT 種別は設定せず利用側から継承し、install 経路ではビルド時の CRT をキャッシュ変数で記録して config で警告する（または文書化する）。Naos はタグ v0.4.0 とコミット SHA（`09fde7172c4b63d416c32e98499cb0c0306d3dea`）の両方で固定し、`BUILD_TESTING` が ON のときだけ取得する。

## Scope
- **In**: ルート `CMakeLists.txt`、`cmake/` のヘルパ（コンパイラオプション、モジュール追加、テスト登録、Naos 取得、install/export）、`src/` と `tests/` のディレクトリ構成、空の `clopts` ターゲットと公開ヘッダの置き場（`include/clopts/`）、内部ユーティリティ（書き込みコールバック型と `FILE*` ライタ、可変長配列、必要ならサイズ計算のオーバーフロー検査）、CI ワークフロー（5 環境でビルドとテスト）、ビルド構成の要件（グローバル設定を置かないこと、install 経路が動くこと）を CTest で自動検証するテスト、`.gitignore` の整備
- **Out**: 引数解析の機能一切、ヘルプ文面、Windows argv、README の本文、リリースワークフロー（docs-and-release が担う）

## Boundary Candidates
- ビルド構成（CMake ヘルパと install/export）
- テスト基盤（Naos の取得とテスト登録ヘルパ）
- CI ワークフロー
- 内部ユーティリティ（書き込みコールバック、可変長配列）

## Out of Boundary
- 公開 API の宣言型や解析の挙動（core-parser）
- 文面の内容（help-and-messages）
- プラットフォーム固有の argv / 環境変数の取得（platform-argv）
- 利用者向け文書と配布（docs-and-release）

## Upstream / Downstream
- **Upstream**: なし（最初の spec）。参照元は Monde の `cmake/`、`.github/workflows/ci.yml`、`CMakeLists.txt`、spec `project-foundation`
- **Downstream**: core-parser、help-and-messages、platform-argv、docs-and-release のすべてがこの spec のビルド・テスト基盤と内部ユーティリティに依存する

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: docs-and-release（install 経路の利用テストとリリース手順はそちらが担う。ここでは install が動くことの検証まで）

## Constraints
- C99、外部依存ゼロ、CMake 3.21 以上、Naos v0.4.0（MIT、テスト専用、pre-1.0 のため版の更新は 1 か所で管理）
- サブプロジェクトとして取り込まれる前提: `CMAKE_MSVC_RUNTIME_LIBRARY`、`CMAKE_C_STANDARD`、`CMAKE_*_OUTPUT_DIRECTORY`、`BUILD_SHARED_LIBS` の既定、ディレクトリ単位の `add_compile_options`、無条件の `include(CTest)` を置かない。`if(NOT TARGET clopts)` で二重定義を防ぐ
- GCC / Clang: `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、拡張機能オフ。MinGW では `__USE_MINGW_ANSI_STDIO=1`。MSVC: `/std:c11`（C99 として使う）、`/W4 /WX /utf-8 /Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を PRIVATE 定義
- Naos の `naos_add_executable` は `Naos::naos` を PRIVATE リンクするだけで C 規格を設定しないため、テスト実行ファイルにも CLOpts 側のコンパイラオプションを当てる
- コーディングスタイルは Monde と同じ（Allman、波括弧省略なし、複合条件の括弧、`//` 不使用）
