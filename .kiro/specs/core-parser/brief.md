# Brief: core-parser

## Problem
コマンドラインツールを C で書く開発者は、MSVC に `getopt` がなく、外部ライブラリに頼れない環境でも、サブコマンド・オプション・位置引数を宣言するだけで解析が済む部品を必要としている。Monde の `cli_args` はこの用途をほぼ満たすが、汎用ライブラリとしては次の歪みがある: (1) エラーが `"<prog> <cmd>: ..."` の整形済み文字列で返るため、利用側が接頭辞を剥がして再整形している、(2) 値の参照が長い名前の文字列キーによる線形探索で、未宣言の名前を黙って NULL で返す、(3) 宣言にハンドラがなく利用側が並行のディスパッチ表を持つ、(4) サブコマンドが必須でグローバルオプションがない、(5) `--no-x`、カウントフラグ、環境変数フォールバックがない、(6) foundation の診断型に依存する。

## Current State
Monde の `src/cli_args/`（`args.h`、`args_decl.c`、`args_parse.c`、`args_result.c`、`args_internal.h`）に、静的宣言テーブル + 単一パス走査器の実装と、`tests/cli_args/` に約 180 件（decl / dispatch / long / short / values / positional / errors）の Naos テストがある。spec `.kiro/specs/cli-args` の要件 1〜7 と design.md のエラー表・走査規則は、本 spec の要件と設計の出発点として流用できる。CLOpts 側には project-foundation が用意するビルド・テスト基盤と内部ユーティリティがある。

## Desired Outcome
- 利用側は `static const` の宣言テーブル（program → commands → options / positionals、またはサブコマンドなしの program → options / positionals）を書き、`clopts_parse(&program, argc, argv, &result)` を呼ぶだけで、解析結果か構造化エラーを受け取れる。
- 値はオプション記述子へのポインタ（宣言テーブルの要素のアドレス）で参照し、未宣言の記述子を渡した場合はデバッグ時に検出できる。各値について、出所（明示指定／環境変数／既定値／未指定）を判別できる。
- コマンド宣言に `handler` と `userdata` を持たせ、解析後にそのまま呼び出せる。
- サブコマンド前のグローバルオプション、サブコマンドなし構成、長短オプション、まとめ書き、`--name=value`、`--`、位置引数の混在、リスト値、カウントフラグ（`-vvv`）、`--no-<name>` 否定、既定値、環境変数フォールバック（明示 > 環境変数 > 既定）、必須検査が動く。
- `--help` / `-h` / `help <cmd>` / `--version` の要求は、他のエラーに優先して状態として返る。
- 宣言の誤り（重複、予約名との衝突、否定できない種類への `--no-`、フラグへの既定値など）は使い方エラーと区別された宣言エラーとして返る。
- エラーは種別の列挙・該当コマンド・該当オプション記述子・該当トークン・argv 添字を持つ構造体で返り、ライブラリは stdout / stderr に書かず `exit` もしない。

## Approach
案A「静的宣言テーブル + 構造化結果」。Monde の走査器（`scan_tokens` / `scan_long` / `scan_short` / `post_check`）の構造とエラー優先度の考え方（最初のエラーを保留し、`--help` を再走査して優先する）を引き継ぎつつ、結果ストアを記述子ポインタで索引する形に変える。環境変数の取得は関数ポインタで差し替え可能にし、platform-argv の完了前に `getenv` 相当で単体テストできるようにする。公開ヘッダ `clopts/clopts.h` の型は本 spec の設計で確定させる（help-and-messages がこの型に依存する）。

## Scope
- **In**: 公開ヘッダの宣言型（program / command / option / positional、種類 FLAG / COUNT / VALUE / LIST、`negatable`、`env`、`required`、`default_value`、`value_name`、`help`、グローバルオプション、`handler` / `userdata`、位置引数の個数制約）、宣言検査、構造化エラー型と種別の列挙、解析状態（OK / HELP / VERSION / NO_COMMAND / USAGE_ERROR / DECL_ERROR / OUT_OF_MEMORY 相当）、結果ストア（値、出所、位置引数）と参照 API、走査器、環境変数フォールバックの解決順、必須検査、メモリ所有（宣言と argv は利用側が保持、リスト項目のみ複製）、`tests/` の単体テスト（架空のプログラム宣言のみを使う）
- **Out**: 文面の生成（help-and-messages）、Windows での argv と環境変数の取得（platform-argv）、値の型変換・妥当性検査、前方一致、候補提示、入れ子のサブコマンド、オプションのグループ化・排他、設定ファイル、レスポンスファイル

## Boundary Candidates
- 公開ヘッダの宣言型と構造化エラー型（契約。最初に確定させる）
- 宣言検査（宣言エラーの検出）
- 結果ストアと参照 API（記述子ポインタによる索引、出所の判別）
- 走査器（トークンの分類、オプションの照合、位置引数、終端）
- 値の解決（既定値、環境変数、リストの分割、カウント、否定）
- エラー優先度（help / version の優先、最初のエラーのみ報告）

## Out of Boundary
- ヘルプ・エラーの文面と出力先（help-and-messages）
- プラットフォーム固有の argv / 環境変数の取得（platform-argv。本 spec は取得関数を差し替え可能な口として持つだけ）
- 利用者向け文書とサンプル（docs-and-release）

## Upstream / Downstream
- **Upstream**: project-foundation（ビルド・テスト基盤、可変長配列、オーバーフロー検査）。参照元は Monde の `src/cli_args/`、`tests/cli_args/`、spec `cli-args`（requirements / design / research）
- **Downstream**: help-and-messages（宣言型・エラー型・結果を文面にする）、platform-argv（環境変数取得関数を差し込む）、docs-and-release（API リファレンスとサンプル）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: help-and-messages（公開ヘッダの型を共有する。型の変更は両 spec に波及する）、platform-argv（環境変数の取得関数のシグネチャを共有する）

## Constraints
- C99、外部依存ゼロ、`int argc` / `char *const *argv` を受け取り、argv を書き換えない
- 宣言テーブルは `static const` で書けること（実行時に決まるバージョン文字列は program の `version` をポインタとして持つことで対応）
- 値の文字列は argv または宣言の既定値を指し複製しない（リスト項目と環境変数の値は複製する）。結果は OOM 後も解放可能、解放は冪等
- 予約名（`help`、`-h`、`--help`、`--version`）の扱いを設計で決める（Monde は固定予約。少なくとも宣言検査で衝突を報告する）
- 規約: `--` 以降はすべて位置引数、`-` 単体は位置引数、サブコマンド名より前はグローバルオプションのみ、サブコマンド以降はオプションと位置引数を並べ替え可、長いオプションの前方一致は非対応、`--no-x` は FLAG のみ、`--no-...` で始まる実名との衝突は宣言エラー、カウントは上限でクランプ
- 環境変数フォールバックの値は UTF-8（Windows では platform-argv が `GetEnvironmentVariableW` 経由で返す）。`getenv` の戻り値は即座に複製する
- コーディングスタイルは Monde と同じ

## 改訂（2026-11-08: ビルダー API と不透明ハンドル）
共有ライブラリの ABI 互換のため、実装済みの本 spec を大きく変える。roadmap.md の「Approach Decision」と「Existing Spec Updates」を正とする。要点:
- `static const` の宣言構造体（`clopts_program_t` / `clopts_command_t` / `clopts_option_t` / `clopts_positional_t`）を廃止し、不透明ハンドルとビルダー関数にする。例: `clopts_program_new`、`clopts_add_command`、`clopts_add_flag` / `clopts_add_count` / `clopts_add_value` / `clopts_add_list`、`clopts_add_positional`、属性の設定関数 `clopts_opt_env` / `clopts_opt_default` / `clopts_opt_required` / `clopts_opt_negatable` など。最終的な名前は設計で決める。
- 失敗の扱い: 最初のエラー（宣言エラーや OOM）を program に記録し、それ以降のビルダー呼び出しは何もしない。NULL のハンドルを渡しても安全にする。宣言エラーは、解析のとき、または明示的な検査関数でまとめて返す。
- 宣言に渡された文字列は複製する（利用側は寿命を気にしなくてよい）。argv から得た値の寿命は設計で決める。
- 結果は、ライブラリが確保した不透明な `clopts_result *` にする。OOM のときも状態を読めるようにする（例: 静的な番兵オブジェクト）。状態・選ばれたコマンド・エラーの各項目（種別、トークン、argv 添字、該当するオプション／コマンドのハンドル）はアクセサで読む。
- 環境変数のコールバックは、借用した `const char *` を返す形にする（コールバックが返るまで有効）。ライブラリはそれを複製する。利用側で確保したメモリをライブラリが `free` することはしない。
- help-and-messages のために、内部の宣言モデルを読み取り専用で読める内部ヘッダを用意する（公開しない）。
- 全公開関数に `CLOPTS_API` を付ける。列挙は末尾への追加だけにする。
- 解析規則（走査、`--`、まとめ書き、`--no-x`、カウント、リスト、優先順位、必須検査、help / version の検出、エラーの優先度）と、既存テストの観点はそのまま引き継ぐ。API の形だけを置き換える。
