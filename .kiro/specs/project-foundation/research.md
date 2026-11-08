# Research & Design Decisions

## Summary
- **Feature**: `project-foundation`
- **Discovery Scope**: New Feature（greenfield。既存コードはなし。参照元 Monde の方針は steering に転記済み）
- **Key Findings**:
  - Naos v0.4.0 はコンパイル型の静的ライブラリ `Naos::naos` を提供し、`naos_add_executable(<target> SOURCES ... LABELS ...)` が実行ファイルの作成・リンク・CTest 登録（`<target>.<suite>.<test>`）までを担う。コンパイルオプションや C 規格は一切設定しないため、CLOpts 側で当てる必要がある。
  - Naos はサブプロジェクトとして取り込まれたとき `include(CTest)` / `enable_testing()` を呼ばず、グローバルな CMake 状態（`CMAKE_C_STANDARD`、`add_compile_options` 等）も変更しない。`enable_testing()` は CLOpts 側の責務。
  - Naos 0.4.0 では「アサーションを 1 つも実行しなかったテストは失敗」が既定。テストはすべて少なくとも 1 つのアサーションを持つ必要がある。
  - CMake 3.21 の `PROJECT_IS_TOP_LEVEL`、`write_basic_package_version_file`、`install(EXPORT ... NAMESPACE)`、`FETCHCONTENT_SOURCE_DIR_<UCNAME>` で、要件 2・4・5 は標準機能だけで満たせる。自作が必要なのはヘルパ関数とビルド構成の検証スクリプトのみ。

## Research Log

### Naos v0.4.0 の CMake 統合と API
- **Context**: 要件 4（テスト登録ヘルパ）と 2.10（Naos を `clopts` の依存に含めない）を設計するため、Naos の正確な契約を知る必要があった。
- **Sources Consulted**: `vabelorg/naos` @ `09fde7172c4b63d416c32e98499cb0c0306d3dea`（v0.4.0、2026-11-03）の `CMakeLists.txt`、`src/CMakeLists.txt`、`cmake/Naos.cmake`、`cmake/NaosDiscover.cmake`、`include/naos/*.h`、`docs/reference/*.md`、`docs/getting-started/installation.md`、`CHANGELOG.md`、`LICENSE`（サブエージェントが raw.githubusercontent.com から取得して読解）
- **Findings**:
  - `cmake_minimum_required(VERSION 3.21)`。FetchContent の推奨名は `Naos`（`FetchContent_Declare(Naos GIT_REPOSITORY ... GIT_TAG v0.4.0)`）。したがってローカル上書き変数は `FETCHCONTENT_SOURCE_DIR_NAOS`。
  - ターゲット: 生ターゲット `naos`、エイリアス `Naos::naos`。サブプロジェクト時は常に STATIC（利用側の `BUILD_SHARED_LIBS` に影響されない）。`target_compile_features(naos PUBLIC c_std_99)` が利用側（テスト実行ファイル）に伝播する。
  - オプション: `NAOS_SELFTEST`、`NAOS_INSTALL`（いずれも既定 `PROJECT_IS_TOP_LEVEL`、サブプロジェクトでは OFF）。`include(CTest)`、`CMAKE_RUNTIME_OUTPUT_DIRECTORY`、`BUILD_SHARED_LIBS` の設定はすべて `if(PROJECT_IS_TOP_LEVEL)` 内。無条件に実行されるのは `include(GNUInstallDirs)`（キャッシュ変数の定義のみ）と関数定義だけ。`set(CMAKE_C_STANDARD)`、`add_compile_options`、`add_definitions`、`link_libraries`、`enable_testing()` はどこにもない（grep で確認）。
  - ヘルパ: `naos_add_executable(<target> SOURCES <src>... [LIBRARIES <lib>...] [PREFIX <prefix>] [LABELS <label>...] [PROPERTIES <k> <v>...] [DISCOVERY_TIMEOUT <sec>] [ALLOW_NO_ASSERTIONS])`。本体は `add_executable` + `target_link_libraries(<target> PRIVATE Naos::naos ${LIBRARIES})` + テスト発見。コンパイルオプション・定義・C 規格プロパティは設定しない。既存ターゲットには `naos_discover_tests(<target> ...)`。
  - テスト発見: POST_BUILD で `<exe> --naos-list` を実行し `<target>_tests[-<CONFIG>].cmake` を生成、`TEST_INCLUDE_FILES` ディレクトリプロパティに追加。CTest 名は `<prefix><suite>.<test>`、既定 prefix は `<target>.`。各テストは `--naos-filter <suite>.<test>` で起動、`SKIP_RETURN_CODE 77`。`LABELS` を渡したときだけラベルが付く。ビルド前は `<prefix>NOT_BUILT` という失敗テストが 1 件見える。
  - 利用側は `enable_testing()` または `include(CTest)` を自分で呼ぶ必要がある。
  - C API: `NAOS_TEST(name)`、`NAOS_SUITE(name, NAOS_ADD_TEST(t), ...)`、`NAOS_DECLARE_SUITE(name)`、`NAOS_MAIN(NAOS_ADD_SUITE(s), ...)`、`NAOS_SETUP` / `NAOS_TEARDOWN`（テスト単位）。アサーションは `NAOS_CHECK_*`（継続）/ `NAOS_REQUIRE_*`（longjmp で中断）: `_TRUE/_FALSE`、`_INT_*`、`_UINT_*`、`_DOUBLE_NEAR`、`_STR_EQ/NE`、`_MEM_EQ`、`_PTR_EQ`、`_NULL/_NOT_NULL`、`NAOS_PASS()`、`NAOS_FAIL`、`NAOS_SKIP`、`NAOS_NOTE`、`NAOS_TRACE_PUSH/POP`。マクロの後ろに `;` を置くと `-Wpedantic` で警告（空文）。
  - 0.4.0 の破壊的変更: アサーションを実行しなかったテストは失敗（`ALLOW_NO_ASSERTIONS` で緩和可）。テストのないスイートは使用法エラー（exit 2）。
  - 出力キャプチャはない（テストが stdout に書くとそのまま出る）。終了コード: 0 pass / 1 fail / 2 usage / 77 skip。
  - 依存: libc のみ（`math.h` の `isnan` / `isinf` / `fabs` を含む）、POSIX では `clock_gettime`、Windows では kernel32。Naos 自身は `-lm` をリンクしない（静的 Naos を Linux でリンクする際に `m` が必要かは UNVERIFIED）。
  - CI 実績: Linux GCC/Clang、macOS Clang、Windows MSVC / MinGW-w64（MSYS2 UCRT64）。自己テストは `-Wall -Wextra -Wpedantic -Werror` / `/W4 /WX` で通している（トップレベル時のみ、利用側には漏れない）。
  - ライセンス: MIT（The Naos Authors）。pre-1.0 のため `find_package(Naos 0.4)` は 0.4.x のみ受理。
- **Implications**:
  - `clopts_add_module_tests` は `naos_add_executable(<module>_tests SOURCES ... LIBRARIES CLOpts::clopts LABELS <module>)` を呼び、続けて `clopts_apply_compiler_options(<module>_tests)` を適用する（要件 4.4、3.1、3.3）。CTest 名の形式 `<module>_tests.<suite>.<test>` は Naos の既定で満たされる（要件 4.8）。
  - ルート `CMakeLists.txt` は `CLOPTS_BUILD_TESTING` が ON のときだけ `include(CTest)` を呼ぶ（要件 2.5、2.6）。
  - `-Werror=vla` と `-Wpedantic` 下でテストを書く。Naos のマクロ末尾に `;` を付けない。すべてのテストに少なくとも 1 つのアサーションを置く。
  - `tests/toolchain/` の「グローバル状態なし」検証では、Naos が無条件に定義する `GNUInstallDirs` のキャッシュ変数と `NAOS_*` オプションを「許容されるキャッシュ変数」として扱う。
  - リスク: Linux で静的 Naos のリンクに `m` が要るなら、CLOpts 側でテスト実行ファイルに `m` を足す必要がある。実装時に GCC/Clang で確認する。

### CMake の標準機能で要件を満たせるかの確認
- **Context**: 要件 2・4・5 を自作ヘルパで満たすか、標準モジュールに委ねるかの判断。
- **Sources Consulted**: CMake 3.21〜3.28 の標準モジュール（`CMakePackageConfigHelpers`、`FetchContent`、`GNUInstallDirs`、`CTest`）に関する既知の仕様。ローカルの CMake 3.28.3 で確認可能。
- **Findings**:
  - `PROJECT_IS_TOP_LEVEL` は 3.21 で導入。`option(CLOPTS_BUILD_TESTING "" ${PROJECT_IS_TOP_LEVEL})` で「トップレベルのとき既定 ON」が 1 行で書ける。
  - `include(CTest)` は `BUILD_TESTING` オプション（既定 ON）を定義し `enable_testing()` を呼ぶ。CLOpts では `CLOPTS_BUILD_TESTING` が ON のときだけ `include(CTest)` し、さらに `BUILD_TESTING` を尊重する（`-DBUILD_TESTING=OFF` でも Naos を取得しない。tech.md の Common Commands と一致）。
  - `FetchContent_Declare(Naos GIT_REPOSITORY ... GIT_TAG <SHA>)` でコミットを固定できる。タグとの一致は、取得後に `git rev-parse v0.4.0` を呼ぶか、`GIT_TAG` に SHA を使いつつコメントと `CLOPTS_NAOS_TAG` 変数でタグを併記して `cmake/CloptsNaos.cmake` 内で両方を 1 か所に置くかのどちらか。ネットワーク不要で確実なのは後者（SHA で取得し、タグは記録・確認用）。要件 4.6 の「両方で検証」は、取得した Naos の `include/naos/version.h` から読める版が `CLOPTS_NAOS_TAG` と一致することを構成時に `message(FATAL_ERROR)` で検査することで満たす。
  - `FETCHCONTENT_SOURCE_DIR_NAOS` を指定すると FetchContent はネットワークにアクセスしない（要件 4.7）。
  - `install(EXPORT CLOptsTargets NAMESPACE CLOpts:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts)` と `configure_package_config_file` + `write_basic_package_version_file(... COMPATIBILITY SameMinorVersion)`（`ARCH_INDEPENDENT` なし）で要件 5.1〜5.5 を満たす。`target_compile_features(clopts PUBLIC c_std_99)` は `INTERFACE_COMPILE_FEATURES` としてエクスポートされ、`find_package` 経由でも利用側に伝わる（要件 5.3）。
  - `set_target_properties(clopts PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)` はターゲット単位で、`CMAKE_C_STANDARD` を触らない（要件 2.1、2.3）。
  - MSVC の `/std:c11` は VS 2019 16.8 以降。GitHub Actions の `windows-latest` は十分新しい。CMake は MSVC に対して `c_std_99` から `/std:` フラグを生成しないので、`/std:c11` を明示的に PRIVATE で付ける（公開ヘッダは C89 互換の範囲で書く必要はないが、利用側のフラグには干渉しない）。
  - `CMAKE_MSVC_RUNTIME_LIBRARY` は未設定（既定 `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`）のことが多く、設定時はジェネレータ式を含む文字列。install 時の値を文字列として config に埋め、利用側の値と文字列比較して非空かつ不一致なら `message(WARNING)` とする（要件 5.6）。ジェネレータ式の評価はしない（構成時には不可能）ので、警告は「ベストエフォート」として config ファイルにコメントで明記する。
- **Implications**: 自作が必要なのは (a) `cmake/Clopts*.cmake` の 4 つの薄いヘルパ、(b) `cmake/CLOptsConfig.cmake.in`、(c) `tests/toolchain/` と `tests/integration/` の `cmake -P` スクリプトとフィクスチャだけ。それ以外は CMake 標準機能に委ねる。

### ビルド構成の自動検証の方式
- **Context**: 要件 6（グローバル設定なし、警告のエラー化、VLA 禁止、add_subdirectory / install 経路）を CTest から実行可能な形にする。
- **Sources Consulted**: Monde の `.github/workflows/ci.yml` の方針（steering に転記済み）、CMake の `add_test` / `cmake -P` / `--build` / `--install` の既知の動作。
- **Findings**:
  - 「ビルドが失敗すること」を検証するには、`add_test(... COMMAND ${CMAKE_COMMAND} -D ... -P <script>)` で子プロセスとしてフィクスチャ項目を構成・ビルドし、スクリプト側で終了コードを反転するのが最も移植性が高い（`WILL_FAIL` は構成失敗とビルド失敗を区別できない）。
  - 「グローバル設定がない」ことは、フィクスチャ項目のトップレベルで CLOpts を `add_subdirectory` した直後に、`CMAKE_C_STANDARD` 等が未定義であること、ディレクトリプロパティ `COMPILE_OPTIONS` / `COMPILE_DEFINITIONS` / `INCLUDE_DIRECTORIES` が空であることを `message(FATAL_ERROR)` で検査すれば構成時に判定できる。
  - 利用側への伝播がないことは、利用側ソースに「`-Wall -Wextra` 下で警告になるが合法なコード」（未使用引数）と `#ifdef _CRT_SECURE_NO_WARNINGS #error` を置いてビルドが成功することで検証できる。
  - install 経路は、ルートのビルドディレクトリに対して `cmake --install <build> --prefix <tmp>`（マルチ構成では `--config $<CONFIG>`）を実行し、`CMAKE_PREFIX_PATH=<tmp>` で利用側フィクスチャを構成・ビルド・実行する。テストから自身のビルドディレクトリを install するため、`CLOPTS_INSTALL` が ON の構成でのみ登録する。
- **Implications**: `tests/toolchain/` と `tests/integration/` は Naos を使わず、`cmake -P` スクリプト 1 本（`tests/cmake/run_fixture.cmake`）を共有し、引数でフィクスチャと期待結果（成功 / 失敗）を切り替える。マルチ構成ジェネレータ（MSVC）では `$<CONFIG>` を `add_test` の引数に渡す。

### 書き込みコールバックの契約
- **Context**: 要件 8。help-and-messages の brief は「書き込みコールバックの失敗（短い書き込み）を検出して呼び出し側へ返す」ことと、公開 API `clopts_write_help(..., writer)` が writer を受け取ることを前提にしている。
- **Sources Consulted**: `.kiro/specs/help-and-messages/brief.md`、`.kiro/steering/roadmap.md`（共有契約の節）、`.kiro/steering/structure.md`（命名規約）
- **Findings**:
  - writer 型は公開 API の引数になるため、`src/core/` の内部ヘッダではなく公開ヘッダ `include/clopts/writer.h` に置く必要がある。実装（`FILE*` ライタ）は `src/core/` に置く。
  - コールバックの戻り値は「書けたバイト数」より「成否」の方が呼び出し側（文面生成）で扱いやすく、部分書き込みの再試行をライブラリ側で行わない方針（出力先の都合は利用側が吸収）と整合する。
  - `FILE*` を使うため `writer.h` は `<stdio.h>` を include する。tech.md の「`stdio` への依存は `FILE*` 便宜関数の翻訳単位に閉じ込める」は実装（呼び出し）についての制約であり、型の参照は許容する。
- **Implications**: 公開契約 `clopts_writer_t { clopts_write_fn write; void *ctx; }`、`typedef int (*clopts_write_fn)(void *ctx, const char *data, size_t len)`（0 = 成功、非 0 = 失敗）、`clopts_writer_t clopts_writer_file(FILE *stream)`。内部ヘルパ `clopts_core_write` / `clopts_core_write_str` は `core_internal.h` に置き、後続モジュールはそれを使う。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 単一ライブラリ + `target_sources` によるモジュール追加（採用） | `clopts` ターゲットは 1 つ。各 `src/<module>/CMakeLists.txt` が `clopts_add_module(<module> SOURCES ...)` でソースを足す | 利用側に見えるターゲットが 1 つ。install / export が単純。モジュール追加が 1 行 | モジュール間の依存方向を CMake では強制できない（レビューと include 規約で守る） | steering `structure.md` の方針と一致 |
| モジュールごとの OBJECT ライブラリ | `clopts_core`、`clopts_parser` … を OBJECT ライブラリにし `clopts` に束ねる | モジュール間依存を `target_link_libraries` で表現できる | export 時に OBJECT ライブラリの扱いが煩雑。利用側に内部ターゲットが見える。オプション適用先が増える | 利点が今の規模に見合わない |
| ヘッダ単位の INTERFACE + 単一ソース | amalgamation 前提の構成 | 取り込みが容易 | roadmap で amalgamation は将来 phase。今やると後続 spec の構成が縛られる | 不採用 |
| テストを Naos の `naos_add_executable` に直接委ねる（ヘルパなし） | 各 `tests/<module>/CMakeLists.txt` が Naos を直接呼ぶ | 層が 1 つ減る | コンパイルオプションの適用・ラベル・リンクを毎回書くことになり、漏れが出る（要件 4.4） | 薄いラッパ `clopts_add_module_tests` を採用 |

## Design Decisions

### Decision: テスト有効化の条件を `CLOPTS_BUILD_TESTING AND BUILD_TESTING` にする
- **Context**: 要件 2.5〜2.7、2.10。tech.md は `-DBUILD_TESTING=OFF` と `-DCLOPTS_BUILD_TESTING=OFF` の両方を「ライブラリだけビルドする方法」として挙げている。
- **Alternatives Considered**:
  1. `CLOPTS_BUILD_TESTING` だけを見る — `BUILD_TESTING=OFF` が効かず、CTest の慣習に反する
  2. `BUILD_TESTING` だけを見る — サブプロジェクト時に利用側の `BUILD_TESTING=ON` に引きずられて Naos を取得してしまう
- **Selected Approach**: `option(CLOPTS_BUILD_TESTING "Build CLOpts tests" ${PROJECT_IS_TOP_LEVEL})`。ON のときだけ `include(CTest)` を呼び、`if(CLOPTS_BUILD_TESTING AND BUILD_TESTING)` で Naos 取得と `tests/` の追加を行う。
- **Rationale**: サブプロジェクトでは既定で何もせず、トップレベルでは CTest の慣習どおり `BUILD_TESTING=OFF` でも止められる。
- **Trade-offs**: 条件が 2 変数になる。README（docs-and-release）で説明が必要。
- **Follow-up**: `tests/toolchain/` の「サブプロジェクトとして取り込んだとき `BUILD_TESTING` が定義されない」検査に含める。

### Decision: Naos をコミット SHA で取得し、タグは構成時検査で照合する
- **Context**: 要件 4.6（タグと SHA の両方で固定・検証）。
- **Alternatives Considered**:
  1. `GIT_TAG v0.4.0` で取得し、取得後に `git rev-parse HEAD` を SHA と比較 — `git` 実行ファイルが必要、タグの付け替えに弱い
  2. `GIT_TAG <SHA>` で取得し、タグは文書化のみ — 「両方で検証」を満たさない
- **Selected Approach**: `cmake/CloptsNaos.cmake` に `CLOPTS_NAOS_TAG`（`v0.4.0`）と `CLOPTS_NAOS_COMMIT`（SHA）を並べて置き、`GIT_TAG ${CLOPTS_NAOS_COMMIT}` で取得する。取得後、`${naos_SOURCE_DIR}/include/naos/version.h` から版を読み取り、`CLOPTS_NAOS_TAG` の数字部分と一致しなければ `message(FATAL_ERROR)` とする。`FETCHCONTENT_SOURCE_DIR_NAOS` 指定時も同じ検査を行う。
- **Rationale**: 取得は SHA で決定的、タグとの対応は Naos 自身が持つ版情報で検証でき、`git` に依存しない。版の更新は 2 変数を同時に書き換えるだけ。
- **Trade-offs**: Naos が `version.h` の形式を変えると検査を直す必要がある（pre-1.0 なので起こり得る）。
- **Follow-up**: Naos の `version.h` のマクロ名を実装時に確認する（`NAOS_VERSION_MAJOR` 等と推定、UNVERIFIED）。

### Decision: package config のバージョン互換は `SameMinorVersion`
- **Context**: 要件 5.4〜5.5。CLOpts は 0.x から始まる。
- **Alternatives Considered**:
  1. `SameMajorVersion` — 0.1 と 0.2 を互換とみなしてしまう
  2. `ExactVersion` — パッチ更新で利用側の `find_package(CLOpts 0.1.0)` が失敗する
- **Selected Approach**: `write_basic_package_version_file(... COMPATIBILITY SameMinorVersion)`、`ARCH_INDEPENDENT` なし。
- **Rationale**: pre-1.0 では minor が互換性の単位（Naos も同じ運用）。1.0 以降に `SameMajorVersion` へ切り替えるかは docs-and-release の `release.md` に委ねる。
- **Trade-offs**: 1.0 で方針変更が必要になる可能性。
- **Follow-up**: `tests/integration/` で `find_package(CLOpts <現在の版> CONFIG REQUIRED)` が成功することを確認する。

### Decision: 書き込みコールバックは「成否を返す」契約にし、公開ヘッダに置く
- **Context**: 要件 8。詳細は Research Log「書き込みコールバックの契約」。
- **Alternatives Considered**:
  1. 書けたバイト数を返す（`fwrite` 互換） — 呼び出し側が毎回 `len` と比較する必要があり、短い書き込みの再試行責務が曖昧
  2. `void` で失敗を無視 — brief の「失敗を呼び出し側へ返す」に反する
- **Selected Approach**: `int (*)(void *ctx, const char *data, size_t len)`、0 = 全バイト書き込み成功、非 0 = 失敗。`FILE*` ライタは `fwrite` の戻りが `len` 未満なら失敗。内部ヘルパ `clopts_core_write` は失敗をそのまま返し、再試行しない。
- **Rationale**: 文面生成側は「失敗したら以降を書かずに返す」だけでよく、出力先の都合（ノンブロッキング等）は利用側のコールバックが吸収する。
- **Trade-offs**: 部分書き込みの正確なバイト数は呼び出し側に見えない。CLI 用途では不要。
- **Follow-up**: help-and-messages の設計で「失敗時に以降の出力を中断する」規約を採用すること（revalidation trigger として記録）。

### Decision: 可変長配列はゼロ初期化を有効な空状態とし、解放を冪等にする
- **Context**: 要件 9.3〜9.5、9.8。
- **Alternatives Considered**:
  1. `init` 必須で未初期化の `free` は UB — brief の「解放は冪等」に反する
  2. ヒープ確保したハンドルを返す — 結果オブジェクトに埋め込めず、OOM 時の解放経路が増える
- **Selected Approach**: `clopts_core_vec_t` は値型。`{0}` または `clopts_core_vec_init(&v, elem_size)` 後が有効な空状態。`push` は `clopts_core_size_mul` でサイズを検査し、`realloc` 失敗時は旧バッファと `len` / `cap` を保つ。`free` は `data` を解放して `data = NULL, len = cap = 0` に戻し、`elem_size` は保つ（再利用可）。
- **Rationale**: 後続 spec の結果ストアが OOM 途中でも 1 つの `free` 関数で解放できる。
- **Trade-offs**: `elem_size == 0` の vec に `push` すると契約違反（`assert`）。
- **Follow-up**: `tests/core/` で、確保失敗を再現するため `realloc` を差し替え可能にするか（内部リンクのフック）、または上限値（`SIZE_MAX` 付近）の `elem_size` でオーバーフロー経路を踏ませるかを実装時に選ぶ。設計では後者（フックなし）を既定とし、OOM 経路は巨大な `elem_size` による `realloc` 失敗で踏む。

### Synthesis: 一般化・採用・単純化の結果
- **一般化**: 「ターゲットにコンパイルオプションを当てる」はライブラリとテスト実行ファイルの両方に必要 → `clopts_apply_compiler_options(<target>)` 1 つに集約（要件 3.6）。「子プロセスで CMake 項目を構成・ビルドし結果を判定する」は toolchain と integration の両方に必要 → `tests/cmake/run_fixture.cmake` 1 本に集約。
- **採用**: テストフレームワークは Naos（steering で決定済み）。CTest 登録・テスト名・ラベルは Naos の `naos_add_executable` に委ね、自作しない。package config は `CMakePackageConfigHelpers` に委ねる。
- **単純化**: サイズ計算のオーバーフロー検査は専用ファイルにせず `core_vec.c` と同じ翻訳単位に置くことも検討したが、後続モジュール（リスト値の複製、環境変数の複製）が vec を使わずに `malloc` する場面があるため、`core_size.c` として独立させる。`clopts_core_vec_reserve` などの API は現要件にないので提供しない（`push` と `at` と `free` だけ）。ロギングやフックの抽象化は置かない。

## Risks & Mitigations
- Linux で静的 Naos のリンクに `m` が必要になる可能性（UNVERIFIED） — `clopts_add_module_tests` 内で `if(UNIX AND NOT APPLE) target_link_libraries(<t> PRIVATE m)` を置ける構造にしておき、CI の GCC/Clang ジョブで判定する。
- Naos の `version.h` のマクロ名の変更で版照合が壊れる — 照合は `cmake/CloptsNaos.cmake` の 1 か所に閉じ、失敗時のメッセージで修正箇所を案内する。
- MSVC マルチ構成ジェネレータで `tests/integration/` の install 経路が `--config` を要する — `add_test` の引数に `$<CONFIG>` を渡し、スクリプト側で `--config` / `-DCMAKE_BUILD_TYPE` を切り替える。
- MinGW（MSYS2 UCRT64）で CMake の既定ジェネレータが Ninja でない場合がある — CI では `-G Ninja` を明示し、`ninja` を msys2 のパッケージで入れる。
- `-Wpedantic` 下で Naos のマクロ末尾の `;` が警告になる — コーディング規約（design の Implementation Notes）に明記し、`tests/core/` で実際に `-Werror` を通す。
- CRT 種別の警告はジェネレータ式を文字列比較するベストエフォート — config ファイルのコメントと docs-and-release の取り込み手順で制約を明記する。

## References
- [vabelorg/naos v0.4.0](https://github.com/vabelorg/naos/tree/09fde7172c4b63d416c32e98499cb0c0306d3dea) — テストフレームワーク。`cmake/Naos.cmake`（`naos_add_executable`）、`docs/getting-started/installation.md`（FetchContent の手順）、`CHANGELOG.md`（0.4.0 の破壊的変更）
- [CMake `PROJECT_IS_TOP_LEVEL`](https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html) — 3.21 で導入
- [CMake `CMakePackageConfigHelpers`](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html) — `write_basic_package_version_file` の `COMPATIBILITY` と `ARCH_INDEPENDENT`
- [CMake `FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html) — `FETCHCONTENT_SOURCE_DIR_<UCNAME>` によるローカル上書き
- [CMake `CTest` module](https://cmake.org/cmake/help/latest/module/CTest.html) — `BUILD_TESTING` オプションと `enable_testing()`
- `.kiro/steering/tech.md`、`structure.md` — ターゲット単位設定、命名規約、テスト配置
- `.kiro/specs/help-and-messages/brief.md` — 書き込みコールバックの利用側要件

---

# Gap Analysis（2026-11-08: 共有ライブラリ化の改訂要件 対 実装済みコード）

> 対象は改訂後の requirements.md（要件 1〜15）と、初版の実装（`CMakeLists.txt`、`cmake/Clopts*.cmake`、`cmake/CLOptsConfig.cmake.in`、`src/core/`、`include/clopts/writer.h`、`tests/{core,toolchain,integration}/`、`tests/cmake/run_fixture.cmake`、`.github/workflows/ci.yml`）。要件はまだ承認されていない。この分析の結果で要件を直すこともありうる。

## Summary
- 初版の土台は「ターゲット単位の設定」「`PROJECT_IS_TOP_LEVEL` ガード」「fixture を `cmake -P` で動かす検証」という形がしっかりしていて、改訂の大半は既存の部品の**拡張**で済む。新しく作るのは `include/clopts/export.h`、package config の形式選択、いくつかの fixture だけ。
- 最大の構造的な問題は**単体テストが内部関数を直接呼んでいること**（`tests/core/*` と、`tests/parser/` の 4 ファイルが `clopts_core_*` / `clopts_parser_*` を参照している）。共有版で内部シンボルを隠すと、今のテスト登録の仕組み（`CLOpts::clopts` にリンクする）ではリンクできない（要件 4.11 との衝突）。
- 既存の検証の 1 つが**改訂要件と矛盾する**: `tests/toolchain/fixtures/no_global_state` は `clopts` の `INTERFACE_COMPILE_DEFINITIONS` が空であることを求めるが、要件 12.5 は静的版で利用側に定義を伝えることを求める。
- **spec の間の順序の問題**: core-parser の公開関数（`parse.h`）には export の印がない。本 spec だけを先に実装すると、共有版では解析 API が利用側から見えず、`tests/integration` の利用側と `tests/parser` が壊れる。core-parser の作り直しまでの間をどう埋めるかを設計で決める必要がある。
- 規模は M、リスクは Medium（CMake の標準機能の範囲内だが、Windows の DLL まわりと 5 環境 × 2 形式の組み合わせで検証の手間が大きい）。

## Requirement-to-Asset Map

| 要件 | 既存の資産 | 状態 | ギャップ |
| --- | --- | --- | --- |
| 1.1 ターゲット | `CMakeLists.txt` の `add_library(clopts STATIC)` | **Missing** | 型を固定している。要件 12 の選択に置き換える |
| 1.6 5 環境 × 2 形式で成功 | CI は 5 環境・静的のみ | **Missing** | 共有版を全環境で通す必要がある |
| 2.1 グローバル変数を設定しない | `no_global_state` fixture | Constraint | `CMAKE_C_VISIBILITY_PRESET` を検査対象に足す。`BUILD_SHARED_LIBS` は既に検査済み |
| 2.x（全般） | `no_global_state` の `INTERFACE_COMPILE_DEFINITIONS` 空の検査 | **Constraint（矛盾）** | 静的版では印の定義（例: `CLOPTS_STATIC`）だけを許すように検査を変える |
| 3 コンパイラオプション | `CloptsCompilerOptions.cmake` | 既存で充足 | 共有版でも同じヘルパを使える。visibility の設定はターゲットのプロパティで、ヘルパに足すか別にするかは設計で決める |
| 4.3〜4.4 テスト登録 | `clopts_add_module_tests`（`naos_add_executable(... LIBRARIES CLOpts::clopts)`） | **Constraint** | 共有版では内部関数が見えない（4.11） |
| 4.10 Windows で DLL を見つける | なし | **Missing** | テスト実行ファイルの横に DLL を置く。Naos のテスト発見が POST_BUILD で `<exe> --naos-list` を実行するので、**DLL の配置はテスト発見より前**に終わっている必要がある（Research Needed） |
| 4.11 共有版で内部関数をテスト | なし | **Missing** | 下の Option で検討 |
| 5.1 install の内容 | `install(TARGETS clopts EXPORT CLOptsTargets ARCHIVE ...)` | **Missing** | `LIBRARY` / `RUNTIME` / NAMELINK の行き先がない。export セットが 1 つだけ |
| 5.4 版の互換 | `write_basic_package_version_file(... SameMinorVersion)` | Constraint | 1.0 より前は今のままでよい。1.0 以降は `SameMajorVersion` に切り替える条件分岐が要る |
| 5.6 CRT の記録 | `CLOptsConfig.cmake.in` の警告 | 既存で充足（静的版に限定する） | 共有版の config では警告を出さないようにする |
| 5.7 install 経路の検証 | `integration.install_find_package` | Constraint | 共有版では、利用側の実行ファイルが install 先の DLL / so を見つける必要がある（Windows は PATH かコピー、Linux/macOS は RPATH） |
| 6.6 add_subdirectory 経路（両形式） | `integration.subdirectory` | Constraint | 共有版の Windows では、fixture の実行ファイルの横へ DLL をコピーする必要がある（fixture は `RUNTIME_OUTPUT_DIRECTORY` を別にしている） |
| 6.7 並存 install と形式選択 | なし | **Missing** | 1 つのテストの中で CLOpts を 2 回（静的・共有）構成・ビルド・install し、利用側を 4〜5 通りで構成する。`run_fixture.cmake` の拡張が必要 |
| 6.8 内部関数でリンク失敗 | `run_fixture.cmake` の `EXPECT=build_fail` | 部品はある | 内部関数を呼ぶ利用側 fixture を足し、共有版のときだけ登録する |
| 7 CI | `ci.yml`（5 ジョブ、ラベル別の再実行は linux-gcc のみ） | **Missing** | マトリクスに形式の軸を足す（10 ジョブ）。並存 install の検証を 1 環境で |
| 8 書き込みコールバック | `writer.h`（`clopts_writer_t` 構造体、`clopts_writer_file(FILE*)`）、`core_writer_file.c`、`core_writer.c`、`tests/core/test_writer.c` | **Missing（作り直し）** | 構造体と `FILE*` 関数を公開ヘッダから外す。`FILE*` 用コールバックを公開ヘッダの `static inline` にする。内部の `clopts_core_write` は `(fn, ctx)` を受け取る形に変えるか、内部用の構造体を `core_internal.h` に移す。`core_writer_file.c` は削除。`tests/integration/fixtures/consumer/main.c` も新しい形に直す |
| 9 可変長配列 | `core_vec.c`、`core_size.c` | 既存で充足 | 変更なし（内部関数なので印は付けない） |
| 10、11 | `.gitignore`、スタイル | 既存で充足 | 変更なし |
| 12.2〜12.3 `CLOPTS_BUILD_SHARED` | なし | **Missing** | `option()` の既定値に `BUILD_SHARED_LIBS` を使う。CMP0077 は `cmake_minimum_required(VERSION 3.21)` で NEW なので、親の通常変数で上書きできる |
| 12.4 静的版の PIC | なし | **Missing** | `POSITION_INDEPENDENT_CODE ON` |
| 12.5 静的版の定義を伝える | なし | **Missing** | INTERFACE の定義。`install(EXPORT)` で伝わる |
| 12.6 MSVC での静的版の名前 | なし | **Missing** | `OUTPUT_NAME`（例: `clopts_static`）。MinGW では `libclopts.a` / `libclopts.dll.a` なので衝突しない |
| 13 形式の選択 | `CLOptsConfig.cmake.in`（`CLOptsTargets.cmake` を 1 つ読む） | **Missing** | `CLOptsStaticTargets.cmake` / `CLOptsSharedTargets.cmake` を分け、config で選ぶ。`check_required_components` の扱い、`CLOpts_FOUND` を偽にする方法、選んだ形式を知らせる変数（13.6）を設計する |
| 14.1〜14.3 export マクロ | なし | **Missing** | `include/clopts/export.h`。CMake は共有版のビルド時に `clopts_EXPORTS` を自動で定義するので、それを「出す」の判定に使える |
| 14.2 印のない関数を出さない | なし | **Missing** | `C_VISIBILITY_PRESET hidden`（ELF / Mach-O）。MinGW では visibility は効かず、「dllexport が 1 つでもあれば、それだけを出す」という ld の規則に頼る |
| 14.4 警告なし | なし | Constraint | MinGW の GCC に visibility 属性を見せると `-Wattributes` になる。Windows の判定を先にする |
| 14.5 構造体の定義なし | `writer.h` に `clopts_writer_t` | **Missing** | 要件 8 の作り直しで解消する |
| 14.6 C++ から使える | `writer.h` に `extern "C"` あり | 既存で充足 | `export.h` にも同じ配慮をする |
| 15 版 | `project(clopts VERSION 0.1.0)` | **Missing** | `VERSION` / `SOVERSION` のプロパティ。SOVERSION を `PROJECT_VERSION_MAJOR` / `MINOR` から計算する |

## 既存コードで確認できた事実
- **Naos は常に静的**: research の初版の調査で「サブプロジェクト時の Naos は常に STATIC（利用側の `BUILD_SHARED_LIBS` に影響されない）」と確認済み。トップレベルで `-DBUILD_SHARED_LIBS=ON` にしても、Naos が DLL になってテストが壊れることはない。
- **内部関数を直接呼ぶテスト**: `tests/core/test_size.c`、`test_vec.c`、`test_writer.c`、`tests/parser/test_decl_check.c`、`test_resolve.c`、`test_result.c`、`test_values.c`。テストには `src/` が include パスとして与えられている（`clopts_add_module_tests` の `target_include_directories(... PRIVATE src)`）。
- **書き込みコールバックの利用者は core だけ**: `clopts_writer_t` と `clopts_core_write*` は `src/core/` の外（`src/parser/`）からは使われていない。作り直しの影響は core、`tests/core/test_writer.c`、integration の利用側 fixture、公開ヘッダに限られる。
- **ラベル別の再実行**: CI の linux-gcc だけが `core` / `toolchain` / `integration` のラベルで再実行している。形式の軸を足すと、この「1 環境だけ」の扱いをどの形式でやるかを決める必要がある。

## Implementation Approach Options

### Option A: 既存の部品を拡張する（ターゲットは 1 つのまま、テストは静的なコピーにリンクする）
- `add_library(clopts ${type})` に変え、`export.h`、visibility、版、install と config の分割、CI のマトリクスを既存のファイルに足す。
- 共有版のときは、単体テスト用に同じソースから**静的なテスト用ライブラリ**（例: `clopts_testing`、install しない）を別に作り、`clopts_add_module_tests` をそこへリンクする。
- ✅ 内部関数のテストがそのまま動く。公開シンボルを汚さない。
- ❌ 共有版のビルドで全ソースを 2 回コンパイルする。単体テストは共有ライブラリそのものを動かさない（共有版の挙動は integration と abi-compat で見る）。

### Option B: OBJECT ライブラリを共有する
- ソースを OBJECT ライブラリ（例: `clopts_objects`）にまとめ、`clopts`（静的または共有）とテスト実行ファイルの両方がそのオブジェクトを使う。
- ✅ コンパイルは 1 回。テストは製品と同じオブジェクトを動かす。
- ❌ 共有版のオブジェクトは「出す」側の印（Windows では `dllexport`）でコンパイルされるので、テスト実行ファイルが公開関数を export する（MSVC では `.lib` / `.exp` ができる）。害はないが見た目が悪い。`clopts_add_module` が足すソースの行き先を OBJECT ライブラリに変える必要がある。visibility と PIC の設定を OBJECT ライブラリ側に置く必要がある。

### Option C: テストを公開 API の範囲に寄せ、内部関数のテストは静的版のジョブだけで行う
- 共有版のジョブでは、内部関数を呼ぶテストを登録しない（または公開 API のテストだけを登録する）。
- ✅ 仕組みが最も単純。
- ❌ 要件 4.11（共有版でも内部関数をテストできる）を満たさない。要件を変える必要がある。

**推奨（設計への入力）**: Option A か B。Option A は Naos のテスト発見（POST_BUILD でテスト実行ファイルを起動する）と相性がよい。テスト実行ファイルが DLL に依存しないので、Windows での DLL の配置の問題が単体テストから消える。Option B はビルドの時間で有利。

## core-parser の作り直しまでの間の扱い（順序の問題）
- **案 1**: 本 spec の範囲で、既存の `parse.h` の公開関数に印を付けるだけの最小の変更を入れる。core-parser の作り直しで API ごと置き換わるので、捨てる作業になるが量は小さい。
- **案 2**: 本 spec の実装中は、共有版のジョブを CI で「失敗しても全体を落とさない」扱いにし、core-parser の作り直しの完了で必須にする。要件 7 の一時的な例外になる。
- **案 3**: 本 spec と core-parser の作り直しを同じブランチ（または連続する PR）で実装し、間の状態を main に入れない。
- 設計で 1 つ選び、tasks の境界に書く。案 1 が最も安全で、要件を変えずに済む。

## Research Needed（設計に持ち越す）
1. Windows で、Naos のテスト発見（POST_BUILD の `--naos-list`）より前に DLL をテスト実行ファイルの横に置く方法（POST_BUILD コマンドの順序、または `RUNTIME_OUTPUT_DIRECTORY` をそろえる）。Option A を採ればこの問題は単体テストからは消え、integration の fixture だけの問題になる。
2. install した共有版を使う利用側の実行（5.7）: Linux / macOS では install 先の RPATH（`INSTALL_RPATH` に `$ORIGIN/../lib` / `@loader_path/../lib` を入れるか、fixture 側で `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` を渡すか）。macOS の SIP で `DYLD_*` が消える場合がある。Windows では prefix の `bin/` を PATH に足すか、DLL をコピーする。
3. `check_required_components` と、自前の形式選択（13.2〜13.5）との組み合わせ方。`CLOpts_FOUND` を偽にして `find_package` を失敗させる書き方、`REQUIRED` のときのメッセージ。
4. 並存 install の検証（6.7）を 1 つの CTest のテストに収める方法と、その実行時間（CLOpts を 2 回構成・ビルドする）。CI の 1 環境に限定するかどうか。
5. `-fvisibility=hidden` の下で Naos と CLOpts の内部関数を静的にリンクする場合（Option A）の警告の有無。MinGW で、dllexport の印のあるオブジェクトを静的ライブラリに入れて実行ファイルにリンクしたときの挙動（`--export-all-symbols` が無効になるか）。
6. `static inline` の `FILE*` 用コールバックが、MSVC（`/W4 /WX`、C モード）と GCC / Clang（`-Wall -Wextra -Wpedantic -Werror`）で、使わなかったときに警告を出さないことの実機確認（要件 8.7）。事前の調査では、ヘッダで定義した未使用の `static inline` は GCC / Clang / MSVC の `/W4` で警告にならない。

## Implementation Complexity & Risk
- **Effort: M（3〜7 日）** — 個々の変更は CMake の標準機能で済むが、install / config の作り直し、fixture の追加（形式選択、内部関数のリンク失敗）、CI の 10 ジョブ化、書き込みコールバックの作り直しが重なる。
- **Risk: Medium** — 技術は既知（GenerateExportHeader 相当の手書き、visibility、SOVERSION、export セットの分割）。ただし Windows の DLL 配置、MinGW の export 規則、macOS の RPATH など、環境ごとの差が CI で初めて見つかる種類の問題が多い。

## Recommendations for Design Phase
- 単体テストと共有版の関係（Option A / B）を最初に決める。テスト登録ヘルパ（要件 4）、DLL 配置（4.10）、CI の構成のすべてに効く。
- core-parser の作り直しまでの間の扱い（案 1〜3）を決め、Boundary Commitments に書く。
- 書き込みコールバックは「公開: 関数ポインタ型と `static inline` の `FILE*` 用コールバック」「内部: core の書き込みヘルパ」に分ける。`clopts_writer_t` を残すなら内部用として `core_internal.h` に移す。
- `no_global_state` fixture の検査を、改訂後の要件（静的版の定義だけを許す、`CMAKE_C_VISIBILITY_PRESET` を検査する）に合わせて直すことを、タスクに明示する。
- 版の互換（5.4）と SOVERSION（15）は同じ規則なので、1 か所で計算して両方に使う。
