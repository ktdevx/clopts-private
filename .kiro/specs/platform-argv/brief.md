# Brief: platform-argv

## Problem
Windows の `main(argc, argv)` は ANSI コードページで引数を渡すため、日本語のファイル名などの非 ASCII 文字が壊れる。同様に `getenv` も ANSI コードページで値を返す。CLOpts の利用者は、Windows でも他の OS と同じように UTF-8 の引数と環境変数を扱いたい。Monde は `CommandLineToArgvW(GetCommandLineW())` で解決していたが、この関数は 2008 年以前の分割規則で動くため、現行 MSVC CRT が `main` に渡す argv と食い違う（引用符内の `""` の扱い、argv[0] の終端規則、先頭が空白のときの挙動）。利用者から見ると「同じ exe なのに引数の見え方が違う」というバグ報告の種になる。

## Current State
Monde の `src/cli_args/args_argv.{h,c}`（約 180 行）に、非 Windows ではポインタをそのまま返し、Windows では `CommandLineToArgvW` + `foundation/wide` の `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS)` で変換する実装と、`tests/cli_args/argv_test.c` に 5 件のテストがある。変換失敗の理由を文字列比較で判定している箇所がある。shell32 にリンクしている。CLOpts では project-foundation がビルド基盤を提供し、core-parser が環境変数の取得関数を差し替え可能な口として持つ。

## Desired Outcome
- `clopts_argv_get(argc, argv, &out)` 相当の関数が、非 Windows では引数をそのまま、Windows では `GetCommandLineW()` の文字列を**現行 MSVC CRT の規則**で分割し UTF-8 に変換した argv を返す。`main` の argv と要素数・区切りが一致する。
- `wmain` を使う利用者向けに `clopts_argv_from_wide(argc, wargv, &out)` 相当の変換関数がある。
- 取得した argv の解放関数があり、非 Windows では何も確保しない。
- 環境変数を UTF-8 で返す取得関数（Windows では `GetEnvironmentVariableW` + UTF-8 変換、他では `getenv` の複製）があり、core-parser のフォールバック解決に差し込める。
- 変換失敗（不正な UTF-16）は、該当する引数の添字を含む構造化エラーとして返る。
- 依存は kernel32 のみ（shell32 不要）。

## Approach
`GetCommandLineW()` の文字列を自前で分割する小関数（60 行程度。Rust std / Zig が採用する現行 CRT 規則: バックスラッシュと引用符の規則、引用符内の `""` は `"` 1 つを出して引用モードを維持、argv[0] は他の引数と同じ規則）を実装し、UTF-16 → UTF-8 変換（`WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS)`）と組み合わせる。分割関数は `wchar_t` の文字列を入力とする純粋関数にして、Windows 以外でも（`wchar_t` の幅に注意して、または UTF-16 の配列として）単体テストできるようにする。Monde の `args_argv.c` の所有モデル（`owned` フラグ、冪等な解放）は踏襲する。

## Scope
- **In**: Windows でのコマンドライン文字列の分割（現行 CRT 規則）、UTF-16 → UTF-8 変換、`wargv` からの変換、argv の所有と解放、環境変数の UTF-8 取得、非 Windows でのパススルー、構造化エラー（不正な UTF-16、取得失敗、OOM）、分割規則の表駆動テスト（CRT の既知の例を含む）
- **Out**: `activeCodePage=UTF-8` マニフェスト（実行ファイル側の設定であり境界外。文書で言及するにとどめる）、コンソール出力のコードページ設定、stdio のバイナリモード設定、引数の解析そのもの

## Boundary Candidates
- コマンドライン文字列の分割（純粋関数）
- UTF-16 → UTF-8 変換
- argv の取得・所有・解放の API
- 環境変数の UTF-8 取得

## Out of Boundary
- 解析の挙動（core-parser）
- 文面（help-and-messages）
- 利用者向けの手順書（docs-and-release。Windows 固有の注意はそちらに書く）

## Upstream / Downstream
- **Upstream**: project-foundation（ビルド基盤、可変長配列）。参照元は Monde の `src/cli_args/args_argv.{h,c}`、`src/foundation/wide.{h,c}`、`tests/cli_args/argv_test.c`、spec `cli-args` 要件 10、research.md の Windows に関する検討
- **Downstream**: core-parser（環境変数の取得関数を差し込む）、docs-and-release（Windows での利用手順）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: core-parser（環境変数取得関数のシグネチャを共有する。core-parser 側は差し替え可能な口を先に用意し、本 spec はそれに合わせる）

## Constraints
- C99、外部依存ゼロ、Windows では kernel32 の API のみ（`GetCommandLineW`、`WideCharToMultiByte`、`GetEnvironmentVariableW`）。shell32 にリンクしない
- MinGW-w64 でも動くこと（`wmain` は `-municode` が必要なため強制しない。`main` 経由で `GetCommandLineW` を読む方式を主にする）
- 現行 MSVC CRT の分割規則に一致すること。既知の例（`"foo""bar"`、末尾のバックスラッシュ、先頭の空白、空の引数 `""`）をテストに含める
- 変換失敗の理由は文字列比較ではなく列挙で判定する
- `_CRT_SECURE_NO_WARNINGS` を PRIVATE 定義（`getenv` の C4996 対策）
- コーディングスタイルは Monde と同じ
