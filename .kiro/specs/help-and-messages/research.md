# Research & Design Decisions

## Summary
- **Feature**: `help-and-messages`
- **Discovery Scope**: Extension（core-parser の宣言モデルと結果のアクセサの上に、新しい内部モジュール `help` と公開関数を足す）
- **Key Findings**:
  - 結果ハンドルは、メモリ不足のときに program を持たない共有の番兵になる（`src/parser/parser_result.c`）。また、エラーのアクセサからは program を辿れない（`error_command` は root なら NULL）。そのため、エラー文面の関数も program を引数に取る必要がある。brief の `clopts_write_error(result, flags, write_fn, ctx)` に program を加える。
  - 宣言モデル（`src/parser/parser_model.h`）は、ヘルプに要るすべての属性（種類、長い名前・短い名前、説明、値の表示名、既定値、環境変数、区切り文字、必須、否定可能、位置引数の必須・可変長、要約・説明、版、command の一覧）を const で読める。宣言エラーが記録された program では名前が NULL のことがある。
  - 走査器は、まとめ書きの中の誤りで `short_name` に生のバイトを入れる（`parser_scan.c` の `scan_short_option`）。ASCII 以外の 1 バイトを `-c` の形で書くと、UTF-8 として壊れた文面になる。

## Research Log

### core-parser の公開 API と内部モデル
- **Context**: help が依存してよい読み出し口の範囲を確かめる。
- **Sources Consulted**: `include/clopts/decl.h`、`include/clopts/parse.h`、`src/parser/parser_model.h`、`.kiro/specs/core-parser/design.md`（即時検査の規則表）
- **Findings**:
  - `clopts_status_t`: OK / HELP / VERSION / NO_COMMAND / USAGE_ERROR / DECL_ERROR / OUT_OF_MEMORY。
  - `clopts_usage_error_t` は 1〜7、`clopts_decl_error_t` は 1〜15。列挙は末尾に追加するだけ（ABI 規律）。
  - エラーのアクセサは種別、command（root は NULL）、option、positional、token、argv_index、name_len、short_name。USAGE_ERROR / DECL_ERROR 以外では NONE / NULL / 0 を返す。NULL の結果は番兵（OUT_OF_MEMORY）として読まれる。
  - 宣言エラーで報告されるハンドル: 追加関数が宣言エラーを起こしたときも新しいオブジェクトを繋いで報告する（名前が読める）。`clopts_program_new` の INVALID_NAME と、root の `set_handler` の MIXED_LAYOUT は、ハンドルなしで報告される。
  - `struct clopts_program.error.status` で、program に宣言エラー・メモリ不足が記録されているかどうかが分かる。
- **Implications**: help は `parser_model.h` と公開アクセサだけを使う。`parser_internal.h`（走査器、結果ストア）は include しない。

### 走査器が設定するエラーの項目
- **Context**: 要件 7.3〜7.6 で「利用者が書いた形」を再現するための材料を確かめる。
- **Sources Consulted**: `src/parser/parser_scan.c`
- **Findings**:
  - UNKNOWN_OPTION（長い形式）: token は argv の要素そのもので、name_len は `=` より前の `--name` の長さ。
  - UNKNOWN_OPTION（短い形式）: short_name は、まとめ書きの中の該当する 1 バイト。
  - MISSING_VALUE / UNEXPECTED_VALUE: 短い形式の場合は short_name も設定される（`scan_option_value` の戻りで設定）。長い形式の場合、token は argv の要素。予約されたオプションへの値（`--help=x`）は option が NULL。
- **Implications**: 文面の引数は、「short_name が ASCII ならその 1 文字、そうでなければ option の長い名前、それもなければ token の `=` より前」で決める。ASCII 以外の short_name は token 全体に置き換える（Monde と同じ扱い）。

### Monde の HelpFormatter
- **Context**: レイアウトをどこまで踏襲するかを決める。
- **Sources Consulted**: `ref/monde/src/cli_args/args_help.c`、`ref/monde/tests/cli_args/help_test.c`、`ref/monde/.kiro/specs/cli-args/design.md`（メッセージ表）
- **Findings**:
  - 字下げ 2、間隔 2、段落の前に空行、行末の空白なし。説明がない行は左列だけを書く。
  - 左列の幅を計算する関数と、左列を書く関数が別々にある（`option_left_width` と `append_option_left`）。表記を足すたびに両方を同時に直す必要があり、ずれる危険がある。
  - エラー文面: `unknown command '<t>'`、`unknown option '<arg>'`、`option '<arg>' requires a value`、`option '--<name>' does not take a value`、`missing argument <NAME>`、`unexpected argument '<t>'`、`missing required option '--<name>'`、`invalid argument declaration: <reason>`、`out of memory while parsing arguments`。
- **Implications**: 文面と注記は Monde を踏襲する。幅の計算と書き込みは 1 つの手続きにまとめる（後述の Decision）。

### 書き込みコールバック
- **Context**: 出力の抽象と失敗の扱いを確かめる。
- **Sources Consulted**: `include/clopts/writer.h`、`src/core/core_internal.h`、`src/core/core_writer.c`
- **Findings**: `clopts_write_fn(ctx, data, len)` は 0 なら成功。`clopts_core_write` / `clopts_core_write_str` はコールバックを 1 回呼んで、その戻り値をそのまま返し、再試行しない。`clopts_file_write` はヘッダ内の `static inline`。
- **Implications**: help は core の書き込み関数を使い、最初の失敗を覚えて、以降は呼ばない（要件 11.4）。

### ビルドとテストの登録
- **Sources Consulted**: `cmake/CloptsModules.cmake`、`src/CMakeLists.txt`、`tests/CMakeLists.txt`、`tests/parser/*`
- **Findings**: `clopts_add_module(<m> SOURCES ...)` で `clopts`（と共有ビルドのテスト用の `clopts_testing`）にソースを足す。`clopts_add_module_tests` は `src/` を include の基点にし、共有ビルドでは `clopts_testing` にリンクする。そのため、テストから内部関数を呼べる。公開シンボル一覧はまだない（abi-compat で作る）。
- **Implications**: 文面表の網羅テストは、内部関数（種別 → 文面）を直接呼んで行える。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| A: 幅の計算と書き込みを別関数にする（Monde 型） | `*_width()` と `append_*()` を対で持つ | 素直 | 表記を足すたびに 2 か所を直す必要があり、ずれると桁が狂う | Monde で実績はある |
| B: 計測モード付きの出力器（採用） | 同じ「行の左列を出す」手続きを、計測モードと書き込みモードで 2 回走らせる | 表記と幅が構造上一致する。確保をしない | 出力器の抽象が 1 段増える | 2 パスでも行数は小さい |
| C: 一時バッファに組み立ててから書く | 1 行ずつ確保して、測ってから書く | 実装が単純 | メモリ確保と OOM の経路が増える。要件にない失敗が増える | 却下 |

## Design Decisions

### Decision: エラー文面の関数も program を受け取る
- **Context**: 要件 9.2〜9.3 はプログラム名の接頭辞を求め、要件 8.4 はメモリ不足でも 1 行を求める。メモリ不足の結果は番兵で、program を持たない。
- **Alternatives Considered**:
  1. core-parser に `clopts_result_program()` を足す。上流の公開 API が変わり、番兵では NULL になるので、問題が解決しない。
  2. エラー文面の関数の引数に program を足す。
- **Selected Approach**: 2。`clopts_write_error(program, result, flags, write_fn, ctx)`。
- **Rationale**: 上流を変えずに済み、番兵の場合も含めてすべての状態で接頭辞を作れる。
- **Trade-offs**: 引数が 1 つ増える。結果と別の program を渡す誤用がありうる。`error_command` があれば、その所有者で assert して検出する。
- **Follow-up**: docs-and-release の API リファレンスで「結果を作った program を渡す」と明記する。

### Decision: 計測と書き込みを 1 つの出力器で行う
- **Context**: 要件 10.1〜10.2 の桁揃えでは、左列の幅をコードポイント数で事前に知る必要がある。
- **Selected Approach**: 出力器 `clopts_help_out_t` を、書き込みモード（コールバックへ書く）と計測モード（書かずにコードポイント数を足す）の 2 つで使う。左列を出す手続きは 1 つだけにし、1 パス目で一覧の最大幅を測り、2 パス目で書く。
- **Rationale**: 表記（`--[no-]`、`...`、区切り文字など）を足しても、幅の計算が自動的に追従する。メモリを確保しないので、メモリ不足の失敗経路がない。
- **Trade-offs**: 左列の手続きを 2 回呼ぶ。ヘルプの行数は数十なので問題ない。

### Decision: 戻り値は 3 値の列挙
- **Context**: 要件 11.5 は、成功・書き込みの失敗・文面を生成できなかった、の 3 つを区別させる。
- **Selected Approach**: `clopts_write_status_t { CLOPTS_WRITE_OK = 0, CLOPTS_WRITE_FAILED = 1, CLOPTS_WRITE_UNAVAILABLE = 2 }`。
- **Rationale**: `int` の 0 / 非 0 では 3 値を表せない。列挙は末尾に追加するだけにすれば ABI を保てる。

### Decision: 接頭辞はマクロのビットフラグ、引数は `unsigned`
- **Selected Approach**: `#define CLOPTS_PREFIX_PROGRAM 0x1u`、`#define CLOPTS_PREFIX_ERROR 0x2u`。未定義のビットは無視する（要件 9.6）。
- **Rationale**: 列挙の値の OR は、C では列挙型として扱えない（C++ からの利用でキャストが要る）。マクロ + `unsigned` が慣習的で、将来ビットを足しても ABI を壊さない。

### Decision: 宣言エラーの文面は「理由 + 対象」の一律の形
- **Context**: 要件 8.2〜8.3 は、名前があれば含め、なければ省くことを求める。15 種別それぞれに文型を作ると、名前の有無で 30 通りになる。
- **Selected Approach**: `invalid declaration: <reason>` の後に、対象があれば ` (<subject>)` を付ける。対象は option > positional > command の順で 1 つ選び、option / positional が command 配下ならば ` in command '<name>'` を足す。
- **Rationale**: 名前がない場合の分岐が 1 か所で済む。要件 8.2 は「サブコマンド名を文面に含める」なので、接頭辞を選ばない場合でも本文にコマンド名が残る。
- **Trade-offs**: プログラム名の接頭辞を選ぶと、コマンド名が接頭辞と本文の両方に出る（例: `tool build: error: invalid declaration: duplicate option name (option '--out' in command 'build')`）。宣言エラーは開発者向けなので、冗長でも許容する。

### Decision: 文面表の網羅性の検出
- **Context**: 要件 12.1〜12.2。公開の列挙に「最大値」の番兵はなく、ABI 規律の都合で足すこともできない。
- **Selected Approach**:
  1. 種別 → 文面の内部関数を、`default` のない `switch` で書く。GCC / Clang の `-Wall`（`-Wswitch`）と `-Werror` により、列挙に値が足されて分岐がないとビルドが失敗する（CI の Linux / macOS / MinGW）。
  2. 網羅テストは、既知の全種別（1〜最大値）の文面が空でなく互いに異なることと、「最大値 + 1」に文面がない（内部関数が NULL を返す）ことを検査する。最大値はテストの中の定数で持つ。
- **Rationale**: 1 は「列挙に値を足したのに表を足さなかった」場合を、2 は「表を足したのにテストを足さなかった」場合を捉える。どちらの漏れも CI で失敗する。
- **Trade-offs**: MSVC の `/W4` は C4062 を出さないので、MSVC だけのビルドでは 1 が効かない。CI のマトリクスには GCC / Clang があるので問題ない。

### Decision: 幅は厳密な UTF-8 デコードで数え、不正なバイトは 1 桁
- **Selected Approach**: 先頭バイトと継続バイトの数・範囲（過長形式、サロゲート、U+10FFFF 超を不正とする）を検査し、正しい列を 1 桁、不正なバイトを 1 つずつ 1 桁と数える。
- **Rationale**: 要件 10.2〜10.3。東アジア幅は境界外（要件の範囲外）。

## Synthesis Outcomes
- **Generalization**: 全体ヘルプ・サブコマンドのヘルプ・サブコマンドなしのヘルプは、「Usage 行 → 説明 → 0 個以上の一覧 → 末尾の案内（任意）」の同じ骨格である。一覧は「左列 + 説明 + 注記」の行の並びで、Commands / Arguments / Options / Global Options はどれも同じ桁揃えの規則に従う。一覧の行を「行の種類（command、positional、option、予約行）」として表し、一覧を書く手続きを 1 つにする。
- **Build vs. Adopt**: 外部依存ゼロの制約があり、文面生成のライブラリは使えない。UTF-8 のデコードは 30 行程度なので自作する。
- **Simplification**: 一時バッファ、ヘルプの書式の差し替え口、端末幅の考慮は持たない。予約行（`-h, --help`、`--version`、`help`）は固定の文字列とし、宣言モデルには足さない。

## Risks & Mitigations
- core-parser の宣言モデルの構造が変わる → help は `parser_model.h` のフィールドだけを読み、変更時は本 spec のテストが壊れて気づく（roadmap の Shared seams）。
- 宣言エラーが記録された program でヘルプを書くと、NULL の名前を読む → ヘルプ系の関数は `program->error.status != CLOPTS_OK` なら何も書かずに UNAVAILABLE を返す（要件 11.6）。エラー文面だけが、NULL を許す読み方をする。
- 公開関数の追加が abi-compat の公開シンボル一覧に漏れる → abi-compat は本 spec の後に作られるので、一覧の初版に本 spec の 5 関数を含める（Revalidation Triggers に記載）。

## References
- `ref/monde/src/cli_args/args_help.c` — レイアウトの出典
- `ref/monde/.kiro/specs/cli-args/design.md` の「メッセージ」表 — エラー文面の出典
- `.kiro/specs/core-parser/design.md` の「即時検査の規則表」 — 宣言エラーで報告されるハンドル
- RFC 3629（UTF-8） — 不正な列の定義
