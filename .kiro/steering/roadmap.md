# Roadmap

## Overview

CLOpts は、C99 で書かれた外部依存ゼロのコマンドライン引数パーサライブラリである。出発点は [Monde](https://github.com/ktdevx/monde-private) の内部モジュール `src/cli_args/`（約 2,000 行、spec `.kiro/specs/cli-args`）だが、そのまま抜き出すのではなく、コマンドライン引数を扱うソフトウェア一般のための「あるべき設計」に作り直す。Monde 側のパース処理を CLOpts に置き換えることは本プロジェクトの境界外であり、Monde との API 互換は求めない。

Monde 版の強みである「`static const` の宣言テーブルを書くだけで、解析とヘルプ文面の両方が宣言から決まる」方式は保つ。一方で、汎用ライブラリとして見たときの歪み（整形済み文字列で返るエラー、文字列キーの線形探索、宣言と別に持つディスパッチ表、サブコマンド必須、foundation 型への依存）を取り除き、サブコマンドなし構成・グローバルオプション・構造化エラー・環境変数フォールバック・`--no-x`・カウントフラグを初版に含める。

## Approach Decision

- **Chosen**: 案A「静的宣言テーブル + 構造化結果」。呼び出し側が `static const` で program / command / option / positional を宣言し、解析器は結果ストアを埋める。値はオプション記述子へのポインタで参照し（文字列キー探索を廃止）、コマンド宣言に `handler` / `userdata` を持たせる。エラーは種別・該当トークン・argv 添字を持つ構造体で返し、文面の生成は別 API に分ける。ヘルプ・エラー文面の出力先は書き込みコールバック `write(ctx, s, n)` に抽象化する。
- **Why**: 初版に含める機能はすべて宣言テーブルの属性追加で表現でき、Monde 版の最大の強み（宣言と挙動とヘルプが一致する）を保ったまま弱点だけを直せる。Monde の要件・設計・約 196 件のテストの知見を最大限に流用できる。
- **Rejected alternatives**:
  - 案B「ビルダー API（実行時登録）」: 実行時に決まる値を扱いやすいが、宣言が手続きに散らばり、登録のたびに確保が走る。宣言とヘルプの対応を目で追いにくい。
  - 案C「変数バインド方式」: 利用コードは最短だが、C では型安全が弱く宣言を `static const` にできない。リスト値・環境変数フォールバック・明示指定と既定の区別が複雑化する。

## Scope

- **In**:
  - 宣言テーブルに基づく解析: サブコマンドあり／なしの両構成、サブコマンド前のグローバルオプション、長いオプション（`--name`、`--name=value`）、短いオプション（`-o v`、`-ov`、`-abc` のまとめ書き）、`--` による終端、位置引数とオプションの混在
  - 値の種類: フラグ、カウントフラグ（`-vvv`）、単一値、区切り文字付きリスト値、`--no-<name>` による否定、既定値、環境変数フォールバック（優先順位: 明示指定 > 環境変数 > 既定値）、値の出所（明示／環境変数／既定）の判別
  - 必須検査、宣言の妥当性検査（宣言エラーと使い方エラーの区別）、構造化エラー
  - ヘルプ・使い方・バージョン・エラー文面の生成（書き込みコールバック）と `FILE*` 向けの便宜関数
  - Windows での UTF-8 argv の取得（現行 MSVC CRT の分割規則を自前実装）と環境変数の UTF-8 取得
  - CMake 静的ライブラリとしての提供（`add_subdirectory` / `FetchContent` / `install` + `find_package(CLOpts)`）、Naos による単体テスト、CI マトリクス
  - README、API リファレンス、サンプルプログラム、リリース手順
- **Out**:
  - Monde 側の `cli_args` を CLOpts に置き換えること、Monde との API 互換
  - 値の型変換・妥当性検査（整数、選択肢、範囲）。値は文字列として返す
  - 長いオプションの前方一致による省略、未知のオプションに対する候補提示（did you mean）
  - サブコマンドの入れ子、オプションのグループ化・排他・依存関係、隠しオプション、別名
  - 端末幅に合わせたヘルプの折り返し、複数行の説明、epilog／examples セクション
  - 設定ファイルからの読み込み、レスポンスファイル（`@file`）
  - メッセージの多言語化（文面は英語。将来の差し替え口は設計で検討してよいが初版では実装しない）
  - 単一ファイル化（amalgamation）、事前ビルド済みバイナリの配布、パッケージマネージャ（vcpkg / Conan）への登録

## Constraints

- **言語**: C99（VLA と `_Complex` は使わない。MSVC で使えない機能は使わない）。GCC / Clang は `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、MSVC は `/W4 /WX /utf-8 /Zc:preprocessor`。
- **依存**: 実行時の外部依存はゼロ。C 標準ライブラリと、Windows では kernel32 の Win32 API（`GetCommandLineW`、`WideCharToMultiByte`、`GetEnvironmentVariableW`）のみ。shell32（`CommandLineToArgvW`）には依存しない。
- **ビルド**: CMake 3.21 以上。サブプロジェクトとして取り込まれる前提で、`CMAKE_MSVC_RUNTIME_LIBRARY`・`CMAKE_C_STANDARD`・ディレクトリ単位の `add_compile_options`・無条件の `include(CTest)` などグローバル設定は置かない。警告・C 規格はターゲット単位、テストと install と `-Werror` は `PROJECT_IS_TOP_LEVEL` で囲む。エイリアス `CLOpts::clopts` を両経路で提供する。
- **テスト**: [Naos](https://github.com/vabelorg/naos) v0.4.0（MIT、テスト専用）を `BUILD_TESTING` が ON のときだけ FetchContent で取得し、タグとコミット SHA の両方で固定する。pre-1.0 のため、版の更新は 1 か所（`cmake/CloptsNaos.cmake` 相当）で管理する。
- **CI**: GitHub Actions。MSVC、MinGW-w64（msys2 ucrt64）、Apple Clang、GCC、Clang。
- **ライセンス**: MIT（`The CLOpts Authors`）。Monde 由来コードの著作権表示は LICENSE に併記しない。Naos は配布物に含まれないため THIRD_PARTY_NOTICES には載せない。
- **命名**: 公開識別子の接頭辞は `clopts_` / `CLOPTS_`、ヘッダは `clopts/clopts.h`。C++ の別プロジェクト Sirraide/clopts（`clopts.hh`）と同名のため README で区別を一言添える。
- **コーディングスタイル**: Monde と同じ（Allman スタイル、本体 1 文でも波括弧、複合条件の各比較を括弧で囲む、`//` コメント不使用）。
- **参照元**: Monde の `src/cli_args/`、`src/foundation/wide.*`、`.kiro/specs/cli-args/{requirements,design,research,tasks}.md`、`tests/cli_args/`、`cmake/Monde*.cmake`、`.github/workflows/ci.yml`。spec 作成時に参照する。参照用 submodule として `ref/monde` に置いてある（`update = none` のため既定では取得されない。`git submodule update --init --checkout ref/monde` で取得する）。

## Boundary Strategy

- **Why this split**: ビルド基盤（foundation）、解析の中核（宣言型 + 走査器 + 結果）、文面生成（ヘルプ・エラー）、プラットフォーム層（Windows argv / 環境変数）、文書と配布の 5 つに分ける。文面生成は公開ヘッダの宣言型と構造化エラー型だけに依存し走査器の実装には依存しないので、core-parser の公開ヘッダが固まり次第、走査器の実装と並行して進められる。プラットフォーム層は foundation にだけ依存し、core-parser とは独立に進められる。
- **Shared seams to watch**:
  - **公開ヘッダ `clopts/clopts.h` の宣言型と構造化エラー型**: core-parser が所有し、help-and-messages が消費する。core-parser の設計で型を確定させ、変更時は help-and-messages に波及することを前提にレビューする。
  - **書き込みコールバックの型**: project-foundation が定義し、help-and-messages が使う。
  - **環境変数の取得関数**: platform-argv が UTF-8 で返す関数を提供し、core-parser のフォールバック解決が呼ぶ。core-parser 側は取得関数を差し替え可能にして（関数ポインタまたは内部リンク）、platform-argv 完了前にテストできるようにする。
  - **エラー種別の列挙**: core-parser が追加するたびに help-and-messages の文面表が追従する必要がある。網羅性をテストで検査する。

## Specs (dependency order)
- [ ] project-foundation -- CMake 骨格（ターゲット単位の警告と C99、`PROJECT_IS_TOP_LEVEL` ガード、install/export と package config、`CLOpts::clopts` エイリアス）、Naos の固定取り込み、CI マトリクス、内部ユーティリティ（可変長配列、書き込みコールバック型と `FILE*` ライタ）。Dependencies: none
- [ ] core-parser -- 公開ヘッダの宣言型（program / command / option / positional、FLAG / COUNT / VALUE / LIST、否定可能、環境変数名、グローバルオプション、handler / userdata、サブコマンドなし構成）、宣言検査、構造化エラー型、結果ストアと記述子ポインタによる参照 API、走査器（サブコマンド選択、長短オプション、`--`、`--no-x`、カウント、リスト、既定値と環境変数の優先順位、必須検査、help / version 検出とエラー優先度）。Dependencies: project-foundation
- [ ] help-and-messages -- 書き込みコールバックへのヘルプ・使い方・バージョン・エラー文面の生成、桁揃え、`FILE*` 便宜関数。Dependencies: core-parser
- [ ] platform-argv -- Windows での UTF-8 argv の取得（`GetCommandLineW` を現行 MSVC CRT の規則で自前分割し UTF-16 から UTF-8 へ変換）、`wmain` 利用者向けの `wargv` 変換、環境変数の UTF-8 取得。Dependencies: project-foundation
- [ ] docs-and-release -- README、API リファレンス、サンプルプログラム、FetchContent と find_package の両経路での利用テスト、リリース手順。Dependencies: core-parser, help-and-messages, platform-argv

## Future Phases (deferred)

初版の境界外として記録する。要望が出た時点で新しい spec として追加する。
- 値の型変換と妥当性検査（整数、選択肢、範囲）
- 端末幅に合わせたヘルプの折り返し、複数行の説明、epilog／examples
- 未知のオプションに対する候補提示、長いオプションの前方一致（opt-in）
- サブコマンドの入れ子、オプションのグループ化・排他・依存関係、隠しオプション、別名
- 設定ファイル、レスポンスファイル（`@file`）
- メッセージの差し替え（多言語化）
- 単一ファイル化（amalgamation）、パッケージマネージャへの登録
- Monde の `cli_args` を CLOpts に置き換える（Monde 側の spec として扱う）
