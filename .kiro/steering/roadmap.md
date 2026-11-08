# Roadmap

## Overview

CLOpts は、C99 で書かれた外部依存ゼロのコマンドライン引数パーサライブラリである。出発点は [Monde](https://github.com/ktdevx/monde-private) の内部モジュール `src/cli_args/`（約 2,000 行、spec `.kiro/specs/cli-args`）だが、そのまま抜き出すのではなく、コマンドライン引数を扱うソフトウェア一般のための「あるべき設計」に作り直す。Monde 側のパース処理を CLOpts に置き換えることは本プロジェクトの境界外であり、Monde との API 互換は求めない。

Monde 版の強みである「宣言を 1 か所に書けば、解析とヘルプ文面の両方がその宣言から決まる」方式は保つ。一方で、汎用ライブラリとして見たときの歪み（整形済み文字列で返るエラー、文字列キーの線形探索、宣言と別に持つディスパッチ表、サブコマンド必須、foundation 型への依存）は取り除き、サブコマンドなし構成・グローバルオプション・構造化エラー・環境変数フォールバック・`--no-x`・カウントフラグを初版に含める。2026-11-08 の改訂で、共有ライブラリとしての提供と 1.0.0 以降の ABI 後方互換を範囲に入れた。それに合わせて、宣言は `static const` のテーブルから「ビルダー API + 不透明ハンドル」に切り替えた。

## Approach Decision

- **Chosen（2026-11-08 改訂）**: 案B「ビルダー API + 不透明ハンドル」。利用側は `clopts_program_new` から始め、`clopts_add_command` / `clopts_add_flag` / `clopts_add_value` などで宣言を組み立てる。属性は `clopts_opt_env` / `clopts_opt_default` などの設定関数で付ける。宣言・結果・エラーはすべて不透明ハンドルで、公開ヘッダに構造体の中身を置かない。値は宣言時に返ったオプションハンドルで参照する（文字列キー探索はしない）。コマンドには `handler` / `userdata` を設定関数で持たせる。エラーは種別・該当トークン・argv 添字をアクセサ関数で読み、文面の生成は別 API に分ける。ヘルプ・エラー文面の出力先は書き込みコールバック `write(ctx, s, n)` で抽象化する。ライブラリは静的ライブラリとしても共有ライブラリとしてもビルドでき、1.0.0 以降は共有ライブラリの ABI 後方互換を保証する。
- **Why**: 共有ライブラリを範囲に入れて ABI 互換を保つには、利用側のバイナリに構造体の配置や配列の刻み幅が焼き込まれてはいけない。ビルダー方式なら属性を足すことは関数を足すことと同じで、ABI を壊さない。配列の要素数・番兵・初期化子の順序も要らなくなり、使いやすい。失敗の扱いは「最初のエラーを program に記録し、以降の呼び出しは何もしない（NULL を渡しても安全）」に統一する。これで呼び出しごとの検査が要らず、誤りは解析時にまとめて返る。宣言とヘルプが同じ所から決まるという案A の利点は、宣言をライブラリ内部の 1 つのモデルに集めることで保てる。
- **History**: 初版（2026-11-07）では案A「静的宣言テーブル + 構造化結果」を選び、project-foundation と core-parser をその方式で実装した。共有ライブラリと ABI 互換を範囲に入れたことで、公開構造体を `static const` の配列で渡す方式は拡張のたびに ABI を壊すと分かった。そこで、実装済みの 2 spec を大きく変えることを前提に、案B に切り替えた。
- **Rejected alternatives**:
  - 案A「静的宣言テーブル（現行）」: 構造体にフィールドを足すと配列の刻み幅が変わり、既存のバイナリが壊れる。
  - 案A'「版番号付きの宣言構造体」（libgit2 型）: `static const` は保てるが、各宣言に版番号を埋める初期化マクロが要り、宣言はポインタ配列になる。ライブラリは過去の版の配置をすべて持ち続けることになる。使いやすさと保守費用が見合わない。
  - 案A''「タグ付き属性列」: `static const` にでき、ABI も壊れないが、値が共用体になって型の安全性と読みやすさが落ちる。
  - 案C「変数バインド方式」: C では型の安全性が弱い。リスト値・環境変数フォールバック・明示指定と既定の区別が複雑になる。

## Scope

- **In**:
  - ビルダー API による宣言: サブコマンドあり／なしの両構成、サブコマンド前のグローバルオプション、長いオプション（`--name`、`--name=value`）、短いオプション（`-o v`、`-ov`、`-abc` のまとめ書き）、`--` による終端、位置引数とオプションの混在
  - 値の種類: フラグ、カウントフラグ（`-vvv`）、単一値、区切り文字付きリスト値、`--no-<name>` による否定、既定値、環境変数フォールバック（優先順位: 明示指定 > 環境変数 > 既定値）、値の出所（明示／環境変数／既定）の判別
  - 必須検査、宣言の妥当性検査（宣言エラーと使い方エラーの区別）、構造化エラー（不透明ハンドルとアクセサ）
  - ヘルプ・使い方・バージョン・エラー文面の生成（書き込みコールバック）と、`FILE*` 向けの便宜手段（ヘッダ内の `static inline`）
  - Windows での UTF-8 argv の取得（現行 MSVC CRT の分割規則を自前で実装）と、環境変数の UTF-8 取得
  - CMake での提供（静的・共有の両方、`add_subdirectory` / `FetchContent` / `install` + `find_package(CLOpts)`）、シンボルの export 制御、VERSION / SOVERSION、Naos による単体テスト、CI マトリクス（各 OS で静的・共有の両方）
  - ABI 互換の保証（1.0.0 から）と検査（公開シンボル一覧の照合、Linux での abidiff）
  - README、API リファレンス、サンプルプログラム、リリース手順
- **Out**:
  - Monde 側の `cli_args` を CLOpts に置き換えること、Monde との API 互換
  - 0.x 系での ABI・API 互換（0.x の間は minor ごとに壊してよい）
  - 値の型変換・妥当性検査（整数、選択肢、範囲）。値は文字列で返す
  - 長いオプションの前方一致による省略、未知のオプションに対する候補提示（did you mean）
  - サブコマンドの入れ子、オプションのグループ化・排他・依存関係、隠しオプション、別名
  - 宣言を読み出す公開 API（イントロスペクション）。ヘルプ生成はライブラリ内部の宣言モデルを読む
  - 端末幅に合わせたヘルプの折り返し、複数行の説明、epilog／examples セクション
  - 設定ファイルからの読み込み、レスポンスファイル（`@file`）
  - メッセージの多言語化（文面は英語）
  - 単一ファイル化（amalgamation）、事前ビルド済みバイナリの配布、パッケージマネージャ（vcpkg / Conan）への登録、ELF のシンボルバージョニング（version script）

## Constraints

- **言語**: C99（VLA と `_Complex` は使わない。MSVC で使えない機能は使わない）。GCC / Clang は `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、MSVC は `/W4 /WX /utf-8 /Zc:preprocessor`（`/Wall` は使わない。ヘッダ内の未使用 `inline` 関数で C4514 が出るため）。
- **依存**: 実行時の外部依存はゼロ。使うのは C 標準ライブラリと、Windows では kernel32 の Win32 API（`GetCommandLineW`、`WideCharToMultiByte`、`GetEnvironmentVariableW`）だけ。shell32（`CommandLineToArgvW`）には依存しない。
- **ビルド**: CMake 3.21 以上。サブプロジェクトとして取り込まれる前提で、`CMAKE_MSVC_RUNTIME_LIBRARY`・`CMAKE_C_STANDARD`・`BUILD_SHARED_LIBS`・ディレクトリ単位の `add_compile_options`・無条件の `include(CTest)` などのグローバル設定は置かない。警告と C 規格はターゲット単位で設定し、テスト・install・`-Werror` は `PROJECT_IS_TOP_LEVEL` で囲む。エイリアス `CLOpts::clopts` を両経路で提供する。
- **静的／共有の選択**: `option(CLOPTS_BUILD_SHARED ... ${BUILD_SHARED_LIBS})` を設け、`add_library(clopts ${type})` の型を明示的に選ぶ（型を省略すると `BUILD_SHARED_LIBS` しか見ないため）。親プロジェクトからは CMP0077 により通常変数で上書きできる。静的版は `POSITION_INDEPENDENT_CODE ON` とし、`CLOPTS_STATIC` を INTERFACE の定義として伝える。MSVC では静的版の `.lib` と DLL の import library の名前がぶつかるので、静的版は別名（例: `clopts_static`）にする。
- **export**: 公開ヘッダ `clopts/export.h` で `CLOPTS_API` を定める。判定の順は `CLOPTS_STATIC` → `_WIN32 || __CYGWIN__`（`dllexport` / `dllimport`）→ `__GNUC__ >= 4`（`visibility("default")`）。MinGW の GCC は visibility 属性に `-Wattributes` を出すので、Windows の分岐を先にする。`C_VISIBILITY_PRESET hidden` と `VISIBILITY_INLINES_HIDDEN` を設定し、公開関数にはすべて `CLOPTS_API` を付ける（MinGW では dllexport を 1 つでも付けると、付いたものだけが export される）。
- **版**: `project(VERSION)` が唯一の版番号。SOVERSION は 0.x の間は `0.<minor>`、1.0.0 から `1`。macOS では minor ごとに `MACHO_COMPATIBILITY_VERSION` を上げるかを abi-compat の設計で決める。Windows の DLL 名には版が入らないので、そのことを文書に書く。
- **ABI 規律**: 公開ヘッダに構造体の定義を置かない（不透明型と関数ポインタ型、列挙だけ）。列挙は末尾に追加するだけにし、既存の値は変えない。ライブラリが確保したメモリはライブラリが解放する。`FILE*` やライブラリ外で確保したメモリを DLL の境界越しに受け渡さない。コールバックが返す文字列は借用で、ライブラリが複製する。
- **テスト**: [Naos](https://github.com/vabelorg/naos) v0.4.0（MIT、テスト専用）を、`BUILD_TESTING` が ON のときだけ FetchContent で取得する。タグとコミット SHA の両方で固定する。pre-1.0 なので、版の更新は `cmake/CloptsNaos.cmake` の 1 か所で管理する。Windows の共有ビルドでは、テスト実行ファイルが DLL を見つけられるよう `$<TARGET_RUNTIME_DLLS>` で DLL をコピーする。
- **CI**: GitHub Actions。MSVC、MinGW-w64（msys2 ucrt64）、Apple Clang、GCC、Clang。それぞれ静的版と共有版の両方をビルドする。
- **ライセンス**: MIT（`The CLOpts Authors`）。Monde 由来のコードの著作権表示は LICENSE に併記しない。Naos は配布物に含まれないので THIRD_PARTY_NOTICES には載せない。
- **命名**: 公開識別子の接頭辞は `clopts_` / `CLOPTS_`、ヘッダは `clopts/clopts.h`。C++ の別プロジェクト Sirraide/clopts（`clopts.hh`）と同名なので、README で区別を一言添える。
- **コーディングスタイル**: Monde と同じ（Allman スタイル、本体が 1 文でも波括弧、複合条件の各比較を括弧で囲む、`//` コメントは使わない）。
- **参照元**: Monde の `src/cli_args/`、`src/foundation/wide.*`、`.kiro/specs/cli-args/{requirements,design,research,tasks}.md`、`tests/cli_args/`、`cmake/Monde*.cmake`、`.github/workflows/ci.yml`。spec 作成時に参照する。参照用 submodule として `ref/monde` に置いてある（`update = none` なので既定では取得されない。`git submodule update --init --checkout ref/monde` で取得する）。

## Boundary Strategy

- **Why this split**: 6 つに分ける。ビルド基盤（foundation、静的／共有と export を含む）、解析の中核（ビルダー + 宣言モデル + 走査器 + 結果）、文面生成（ヘルプ・エラー）、プラットフォーム層（Windows argv / 環境変数）、ABI 互換の検査、文書と配布。ABI 検査は、公開ヘッダとビルド成果物だけを見る外側の検査なので、走査器の実装から独立して進められる。宣言が不透明になったので、文面生成は公開ヘッダではなく core-parser が用意する**内部の宣言モデルの読み出し口**（`src/` 内の内部ヘッダ）に依存する。
- **Shared seams to watch**:
  - **内部の宣言モデルの読み出し口**: core-parser が所有し、help-and-messages が読む。読み取り専用の内部ヘッダとして core-parser の設計で確定させる。変更は help-and-messages に波及する前提でレビューする。
  - **公開 API のハンドル型とエラーのアクセサ**: core-parser が所有し、help-and-messages（エラー文面）と platform-argv（環境変数コールバック）が使う。
  - **`CLOPTS_API` と公開シンボル一覧**: project-foundation が仕組みを、abi-compat が一覧と検査を所有する。新しい公開関数を足す spec は、その一覧も更新する。
  - **書き込みコールバックの型と `FILE*` 便宜関数**: project-foundation が定義し、help-and-messages が使う。`FILE*` は境界を越えさせない。
  - **環境変数の取得コールバック**: core-parser がシグネチャ（借用した文字列を返し、ライブラリが複製する）を定め、platform-argv が UTF-8 の実装を提供する。
  - **エラー種別の列挙**: core-parser が値を足すたびに help-and-messages の文面表を追従させる必要がある。網羅性をテストで検査する。列挙は末尾への追加だけにする（ABI 規律）。

## Existing Spec Updates

`/kiro-spec-batch` の前にこの順で行う。どちらも requirements から作り直す（`/kiro-spec-requirements <feature>` → design → tasks → impl）。

- [x] project-foundation -- 静的／共有の選択（`CLOPTS_BUILD_SHARED`）、`clopts/export.h` と `CLOPTS_API`、visibility を hidden に、VERSION / SOVERSION、静的版の PIC と `CLOPTS_STATIC`、MSVC での静的版の別名、install の export セットを静的・共有で分け config で選ばせる（`COMPONENTS static|shared`）、Windows のテストで DLL を配置、CI を両方の形式に広げる、書き込みコールバックの扱いを変える（`clopts_writer_t` の値渡しと `clopts_writer_file(FILE*)` をやめ、`(write_fn, ctx)` を引数で渡す形とヘッダ内の `static inline` の `FILE*` 用コールバックにする）、統合テストを両形式で行う。Non-Goals の「共有ライブラリとしてのビルド」を外す。Dependencies: none
- [x] core-parser -- 宣言 API をビルダー + 不透明ハンドルに作り直す（program / command / option / positional のハンドル、種類別の追加関数と属性の設定関数、最初のエラーを記録して以降は何もしない方式、NULL を渡しても安全、文字列は複製）。結果は不透明な `clopts_result *` をライブラリが確保し、OOM のときも状態を読めるようにする。構造化エラーはアクセサで読む。環境変数のコールバックは借用した文字列を返す形にし、ライブラリが複製する。help-and-messages 向けに内部の宣言モデルの読み出し口を用意する。全公開関数に `CLOPTS_API` を付ける。既存の解析規則（走査、優先順位、エラー優先度）とテストの観点は引き継ぎ、API の形だけを置き換える。Dependencies: project-foundation（更新）

## Direct Implementation Candidates

- [x] steering-sync -- `tech.md` / `structure.md` / `product.md` の「静的ライブラリ」「`static const` の宣言テーブル」「help は公開ヘッダのみに依存」などの記述を、改訂後の方針に合わせる。`/kiro-steering` で行い、spec の作成ではない。

## Specs (dependency order)
- [x] project-foundation -- CMake 骨格（ターゲット単位の警告と C99、`PROJECT_IS_TOP_LEVEL` ガード、install/export と package config、`CLOpts::clopts` エイリアス）、Naos の固定取り込み、CI マトリクス、内部ユーティリティ（可変長配列、書き込みコールバック型と `FILE*` ライタ）。Dependencies: none（共有ライブラリ化の改訂は Existing Spec Updates を参照）
- [x] core-parser -- ビルダー API と不透明ハンドルによる宣言、走査器、結果ハンドル（初版の静的宣言テーブルから作り直し済み。Existing Spec Updates を参照）。Dependencies: project-foundation
- [x] help-and-messages -- 書き込みコールバックへのヘルプ・使い方・バージョン・エラー文面の生成、桁揃え、`FILE*` 便宜手段。program / command / result のハンドルを受け取り、内部の宣言モデルを読む。Dependencies: core-parser（更新後）
- [ ] platform-argv -- Windows での UTF-8 argv の取得（`GetCommandLineW` を現行 MSVC CRT の規則で自前分割し、UTF-16 から UTF-8 へ変換）、`wmain` 利用者向けの `wargv` 変換、環境変数の UTF-8 取得（core-parser の環境変数コールバックとして差し込める）。取得した argv はライブラリが所有し、解放する。Dependencies: project-foundation（更新後）, core-parser（更新後）
- [ ] abi-compat -- ABI 互換の方針（SOVERSION の規則、互換を保つ変更と壊す変更）、公開シンボル一覧と全 OS の共有ビルドでの照合、公開ヘッダに構造体の定義がないことの検査、Linux での abidiff（直近のリリースタグを CI で作り直して比べる。1.0.0 以降に有効）。Dependencies: project-foundation（更新後）, core-parser（更新後）
- [ ] docs-and-release -- README、API リファレンス、ABI 方針と静的／共有の使い分けの文書、サンプルプログラム、FetchContent と find_package の両経路・両形式での利用テスト、リリース手順（ABI 検査の基準の扱いを含む）。Dependencies: core-parser, help-and-messages, platform-argv, abi-compat

## Future Phases (deferred)

初版の境界外として記録する。要望が出た時点で新しい spec として追加する。
- 値の型変換と妥当性検査（整数、選択肢、範囲）
- 端末幅に合わせたヘルプの折り返し、複数行の説明、epilog／examples
- 未知のオプションに対する候補提示、長いオプションの前方一致（opt-in）
- サブコマンドの入れ子、オプションのグループ化・排他・依存関係、隠しオプション、別名
- 設定ファイル、レスポンスファイル（`@file`）
- メッセージの差し替え（多言語化）
- 単一ファイル化（amalgamation）、パッケージマネージャへの登録
- 宣言のイントロスペクション API（シェル補完の生成や独自ヘルプのため）
- ELF のシンボルバージョニング（version script）
- Monde の `cli_args` を CLOpts に置き換える（Monde 側の spec として扱う）
