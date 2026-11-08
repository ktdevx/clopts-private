# Brief: help-and-messages

## Problem
コマンドラインツールの利用者は、マニュアルを開かずに `--help` で使い方を確認し、誤った指定をしたときには何を直せばよいか分かる英語のメッセージを読みたい。ツールの開発者は、ヘルプの文面を手で書くとオプションの追加・変更のたびに実際の挙動とずれるため、宣言から自動生成したい。Monde の `args_help.c` はこれを実現しているが、出力先が foundation の `mnd_buf_t` に固定され、エラーは走査器が文字列に整形してしまうため、利用側が接頭辞を剥がして自分の書式（`monde: error: ...`）に合わせ直していた。

## Current State
Monde の `src/cli_args/args_help.c`（約 360 行）に、全体ヘルプ（Usage 行、説明、Commands、Options、末尾の案内）とサブコマンドヘルプ（Usage 行に位置引数、Arguments、Options に `(required)` / `(default: X)` の注記）、バージョン行、使い方の案内（`Run '<prog> <cmd> --help' for usage.`）の生成がある。桁揃えはインデント 2・間隔 2 で、幅はバイト数で数える。`tests/cli_args/help_test.c` はバイト単位で文面を検査している。エラー文面の一覧は Monde spec `cli-args` の design.md のエラー表にある。CLOpts では core-parser が宣言（不透明ハンドル）と構造化エラー（結果ハンドルのアクセサ）を提供し、project-foundation が書き込みコールバック型 `clopts_write_fn` と、公開ヘッダ内の `static inline` の `FILE*` 用コールバックを提供する（2026-11-08 の改訂で、共有ライブラリと ABI 互換が範囲に入った）。

## Desired Outcome
- `clopts_write_help(program, command_or_NULL, write_fn, ctx)`、`clopts_write_usage(...)`、`clopts_write_version(...)`、`clopts_write_error(result, flags, write_fn, ctx)`（名称は設計で決める）が、宣言と構造化エラーから決定的な英語の文面を書き込みコールバックに書く。引数はすべて不透明ハンドル・関数ポインタ・整数で、構造体を値で受け渡さない。
- サブコマンドなし構成・グローバルオプション・カウントフラグ・否定可能フラグ（`--[no-]color` のような表記）・環境変数名（`[env: NAME]` のような注記）・既定値・必須が、ヘルプ上で表現される。
- エラー文面は種別ごとに 1 行で、該当トークンとコマンド名を含む。接頭辞（プログラム名、`error:`）の有無を利用側が選べる。選択はビットフラグ（将来の選択肢を ABI を壊さずに足せる）で渡す。
- `FILE*` へは、project-foundation がヘッダ内に用意する `static inline` のコールバックを渡すだけで、stdout / stderr に出せる。ライブラリ本体は `FILE*` を受け取らない（DLL と利用側で CRT が違っても壊れない）。
- 文面は LF 固定・端末幅に依存せず、同じ宣言からは常に同じバイト列になる（テストでバイト単位に検査できる）。
- 桁揃えは UTF-8 のコードポイント数で数える（Monde のバイト数より改善。東アジア幅の考慮は境界外）。

## Approach
Monde の `args_help.c` のレイアウト（セクション構成、注記、桁揃え）を踏襲しつつ、出力を書き込みコールバックに置き換える。エラー文面は core-parser のエラー種別の列挙を網羅する表として実装し、列挙に追加があったときにテストで検出できるようにする（`switch` の網羅性 + 全種別を生成するテスト）。文面の生成は走査器の実装には依存せず、core-parser が用意する内部の宣言モデルの読み出し口（読み取り専用の内部ヘッダ）と、結果ハンドルのアクセサだけに依存する。

## Scope
- **In**: 全体ヘルプ、サブコマンドヘルプ、サブコマンドなし構成のヘルプ、使い方 1 行（Usage）、バージョン行、使い方の案内、構造化エラーからの文面生成、桁揃え（コードポイント幅）、`tests/` の単体テスト（バイト単位の比較、全エラー種別の網羅）
- **Out**: 端末幅に合わせた折り返し、複数行の説明、epilog / examples、色付け、候補提示（did you mean）、多言語化、文面を実際に stdout / stderr に出す判断と終了コードの決定（利用側）

## Boundary Candidates
- ヘルプ本文の生成（セクション構成と注記）
- Usage 行の生成（位置引数の必須・任意・可変の表記）
- エラー文面の生成（種別ごとの表）
- 桁揃えと幅の計算

## Out of Boundary
- 宣言型・エラー型の定義と解析の挙動（core-parser）
- 出力先の決定と終了コード（利用側。本 spec は書き込み先を抽象化するだけ）
- サンプルや README での説明（docs-and-release）

## Upstream / Downstream
- **Upstream**: core-parser（更新後: program / command / result のハンドル、エラーのアクセサ、内部の宣言モデルの読み出し口）、project-foundation（更新後: 書き込みコールバック型、`FILE*` 用の `static inline` コールバック、`CLOPTS_API`）、abi-compat（新しい公開関数を公開シンボル一覧に足す）。参照元は Monde の `src/cli_args/args_help.{h,c}`、`tests/cli_args/help_test.c`、spec `cli-args` 要件 7〜9 と design.md のエラー表
- **Downstream**: docs-and-release（ヘルプ出力例を文書とサンプルに載せる）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: core-parser（内部の宣言モデルを共有する。エラー種別の追加には本 spec の表を追従させる必要がある）、abi-compat（公開シンボル一覧）

## Constraints
- C99、外部依存ゼロ。ライブラリ本体は `stdio` に依存しない（`FILE*` はヘッダ内の `static inline` だけで扱う）
- 全公開関数に `CLOPTS_API` を付け、公開ヘッダに構造体の定義を置かない（ABI 規律）
- 文面は英語、LF 固定、決定的。Monde の `tests/cli_args/help_test.c` と同様にバイト単位で検査できること
- 書き込みコールバックの失敗（短い書き込み）を検出して呼び出し側へ返す
- 走査器の実装に依存しない（宣言モデルの読み出し口と、結果のアクセサだけを使う）
- コーディングスタイルは Monde と同じ
