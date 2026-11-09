# TODO

## 実装状況

この文書の内容は実装済み（下の各チェックリストも済み）。計画から変えた点は次のとおり。

- **封印のエラーコード**: `CLOPTS_DECL_SEALED` は足さず、`CLOPTS_DECL_ALREADY_ATTACHED` を使う。option と positional の「渡した後は読み取り専用」の規則と同じ扱いにそろえるため。
- **封印（方針 2）と確定ステップ（続き 3）の統合**: `clopts_program_new(root, commands, count, &program)` を一度で作る形にし、`clopts_program_add_command` はなくした。プログラムは作った後は変わらないので、別の builder 型や `clopts_program_build` は要らない。frozen と `CLOPTS_DECL_FROZEN` はなくなり、`clopts_parse` は `const clopts_program_t *` を受け取る。
- **版の文字列**: `--version` が parser の外に出たので、`clopts_program_set_version` はなくした。版は `clopts_write_version(program, version, ...)` に渡す。
- **`clopts_program_root`**: 書き込み用の取得はなくしたが、読み取りの関数（`const` を返す）として残した。help などの読み手が root を得るため。
- **否定形との衝突**: 同じコマンドの中の `NEGATION_CONFLICT` は残した（コマンドの中で閉じた検査なので）。
- **終端オプションの扱い**: 終端オプションが現れたら、そこで走査を止め、環境変数と既定値は解決し、必須の検査は飛ばして `CLOPTS_TERMINATED` を返す。使い方エラーの後ろにある終端オプションを優先する回復処理は、`--help` 専用ではなく終端オプション全般に一般化した。
- **見送ったもの**: `clopts_dispatch()`（任意の補助）と `clopts_program_lint()`（任意の診断）は作っていない。

## 宣言モデルの再設計：command を program から独立させる

前提：現行 API の互換性は破棄してよい。検証はゆるめてよい。

### 背景

- `struct clopts_command` は `program` への back-pointer を持つ。使っているのは宣言を組み立てる関数（`parser_command.c`、`parser_decl.c`）だけ。
  - frozen チェック
  - root かどうかの判定（`command == &program->root`）
  - MIXED_LAYOUT（サブコマンドを持つ root に handler か positional がある状態）
  - `version` を予約名にする判定（`program->version`）
  - root とサブコマンドの間の DUPLICATE_OPTION と NEGATION_CONFLICT（`decl_find_in_range`）
- どれも command 自身のルールではなく program の不変条件。command 側のコードが program の内部を直接読んで判定している。
- parse（`clopts_parse(program, ...)`）と help（`clopts_write_help(program, command, ...)`）は、すでに program を引数で受け取っている。back-pointer が要るのは builder だけ。

### 方針

1. **command は独立したオブジェクトにする**
   - `clopts_command_new(name, &cmd)` / `clopts_command_free(cmd)`（free は未 attach のときだけ）
   - `struct clopts_command` から `program` メンバーを削除する
   - command で検査するのは、command の中で閉じるルールだけ
     - 同じ command 内での名前の重複（エラーのまま残す）
     - POSITIONAL_ORDER、VARIADIC_POSITION
     - option と positional の ALREADY_ATTACHED

2. **program に渡した時点で封印する（下から上に組み立てる）**
   - `clopts_program_new(name, root, &program)`、`clopts_program_add_command(program, cmd)`
   - 渡した時点で所有権が program に移り、command は封印される（`bool sealed`）。封印後の変更は `CLOPTS_DECL_SEALED` で失敗させる
   - 失敗したら何も変えず、所有権は呼び出し側に残る（option と positional の規則に揃える）
   - 封印すると command 側の frozen チェックが不要になる。parse 後に宣言が変わって結果の index がずれる問題も防げる

3. **オプション名の衝突はシャドーイング（内側優先）で解決する**
   - 優先順位：今いるサブコマンド ＞ root ＞ 組み込みの `--help` / `-h` / `--version`
   - `--no-xxx` による否定形も同じスコープの中で解決し、外のスコープへ出るのはその後にする（否定形かどうかよりスコープを優先する）
   - 結果はハンドルで引く（`clopts_result_flag(result, option)` など）ので、名前が同じでも曖昧にならない

4. **program 側に残す検証は2つだけ**
   - コマンド名の重複
   - MIXED_LAYOUT（`program_new` と `add_command` で判定する）

### 削除するもの

- [x] スコープをまたぐ DUPLICATE_OPTION の検査（`decl_find_in_range`、root に足すときの全コマンド走査）
- [x] スコープをまたぐ NEGATION_CONFLICT の検査
- [x] RESERVED_NAME（組み込みが最下位なので、ユーザー定義が上書きする）
- [x] `clopts_program_root()`（root は呼び出し側が作って渡す）
- [x] `parser_command.c` と `parser_decl.c` からの `program->root`、`program->commands`、`program->version` への参照

### 作業項目

- [x] `parser_model.h`：`program` メンバーを削除し `sealed` を追加する
- [x] `parser_command.c`：`clopts_command_new` / `clopts_command_free` を追加し、検証を command 内で閉じるものだけにする。封印のチェックを追加する
- [x] `parser_program.c`：`program_new(name, root, ...)` を実装し、`add_command` に封印とコマンドをまたぐ検証を実装する
- [x] `parser_decl.c`：参照を内側優先に揃える（command → root → 組み込み。否定形はスコープの中で解決する）
- [x] `parser_scan.c`：組み込みの `--help` / `--version` の判定を、ユーザー定義の参照の後に回す
- [x] help：サブコマンドの help で root のオプションを並べるとき、隠されたものは除く
- [x] `clopts_decl_error_t`：`CLOPTS_DECL_SEALED` を追加し、不要になったコードを削除する
- [x] 公開ヘッダー（`command.h`、`program.h`、`decl_types.h`）とドキュメント（`docs/api.md`、`docs/parsing.md`）を更新する
- [x] `abi/clopts.symbols` を更新する（ABI を破壊するので CHANGELOG に記載する）
- [x] examples（`taskbook`、`textcount`）を新しい API に移行する
- [x] テスト：シャドーイングの優先順位、否定形の解決順、封印後の変更、MIXED_LAYOUT、失敗したら何も変えないこと

### 代償と緩和策

- 意図しない名前の衝突が黙って起きる → 任意で呼べる診断関数 `clopts_program_lint()` でシャドーイングを報告する（任意）
- `-h` を上書きされるとヘルプが出せなくなる → 仕様としてドキュメントに明記する
- 組み立てる順番が下から上に固定される（program に渡した後でオプションを足すことはできない）→ 仕様として明記する
- 衝突エラーがなくなるので、シャドーイングが起きたことはエラーとして出ない（lint で補う）

## 宣言モデルの再設計（続き）：他に分離できる箇所

前提は上と同じ（API 破壊可、検証の緩和可）。効果の大きい順に並べる。

### 1. 宣言から実行（handler）を外す ★★★

- 今の状態
  - `clopts_handler_fn` は `decl_types.h` にあり、引数に `clopts_result_t` を取る。宣言の型が parse 結果の型に依存している
  - `clopts_command_set_handler()` と `clopts_result_run()` によって、command が「宣言」と「呼び出し先」の両方を持っている
  - MIXED_LAYOUT の判定条件の半分はこの handler（サブコマンドを持つ root に handler がある）
- 変更案
  - command から `handler` と `userdata` を削除する
  - 振り分けはユーザーが `clopts_result_command()` の戻り値で分岐して行う
  - 補助が要るなら宣言とは別の部品にする（例: `{command, fn, userdata}` の配列を受け取る `clopts_dispatch()`）
- 効果：宣言が parse 結果を知らなくなる。MIXED_LAYOUT が「root に positional がある」だけになる。`clopts_result_run` が消える

### 2. `--help` / `--version` / `help` コマンドを parser から追い出す ★★★

- 今の状態
  - `parser_scan.c` が help の概念を直接扱っている（`is_help_token`、`prelude_help_command`、`reserved_version`、`help copy --help` のような並びの解釈）
  - そのため parse 結果に `CLOPTS_HELP` / `CLOPTS_VERSION` という状態があり、宣言側に RESERVED_NAME がある
- 変更案
  - parser には組み込みのオプションを一切持たせない
  - option に「見つけた時点で必須チェックを打ち切る」属性を追加する（例: `clopts_option_set_terminal()`）
  - help と version はその属性を付けたただの FLAG にし、help モジュールのヘルパーで提供する（例: `clopts_help_add_options(command, &help_opt)`）
  - `help <cmd>` も help モジュールが「help という名前の普通のサブコマンド」として提供する
- 効果：parser が help を知らない汎用の argv 解析器になる。上の方針 3（シャドーイング）の「組み込みは最下位」という規則が不要になる

### 3. builder と、変更できない宣言の型を分ける ★★☆

- 今の状態
  - 初回の parse が `program->frozen = true` と書き込むので、`clopts_parse(clopts_program_t *)` が const でない
  - そのため「同じ program を複数のスレッドから同時に parse してはいけない」という制約がある
- 変更案
  - 組み立てが終わったら明示的に確定させる。例: `clopts_program_build(builder, &spec)` で、変更できない `const clopts_spec_t *` を作る
  - parse と help は const な spec だけを受け取る
- 効果：frozen と FROZEN エラーが消える。同じ spec を複数のスレッドから同時に parse できる
- 注意：上の方針 2（封印）は、この確定ステップの粒度の細かい版にあたる。どちらで実現するかはまとめて決める

### 4. help モジュールが parser の内部構造を直接読まない ★★☆

- 今の状態
  - `help_internal.h` が `parser/parser_model.h` を include している
  - `help_layout.c` が `program->root`、`program->commands`、`program->version`、`scope->options` を直接読んでいる
- 変更案
  - 宣言を読み取るだけの公開 API を用意する（例: `clopts_command_option_count/at`、`clopts_option_long_name`、`clopts_program_command_at`）
  - help はこの API だけを使って書く
- 効果：help が parser の構造体の配置に依存しなくなる。help を別ライブラリにすることも、ユーザーが独自の help を書くこともできる

### 5. result が program を持たないようにする ★☆☆

- 今の状態
  - `result->program` を通じて、値の参照（`find_slot` で root の options を線形探索）と `run` を行っている
  - そのため「値を読めるのは program を解放する前だけ」という寿命の約束がある
- 変更案
  - attach の時点で option に通し番号を振る。番号は option 自身が持ち、command へのポインタは持たない
  - result は番号から値を引くだけにする
- 効果：値の参照に program が不要になり、O(n) から O(1) になる
- 注意：別の program の option を渡されたことを見分けるには、program ごとの識別子が要る
- 順序：1 と 3 を先にやると、`result->program` を使う理由は値の参照だけになる。手を付けるのはその後で十分

### 小さいもの

- [x] `clopts_decl_error_message()` を `help.h` から `decl_types.h` 側へ移す（宣言エラーのメッセージなので）
- [x] `platform.h` が `parse.h` を include しているのをやめる。`clopts_getenv_fn` と `clopts_env_status_t` を小さなヘッダーに切り出し、platform を parser に依存させない

### まとめると

- 1 と 2 は上の再設計（封印と内側優先）と強く噛み合う。1 で MIXED_LAYOUT が縮み、2 で予約名の扱いがまるごと消える
- すべて入れると、program 側の検証は「コマンド名の重複」と「root の positional」の2つだけになる

### 見送り

- `clopts_set_allocator` のグローバル状態を program ごとの allocator にする案。分離の効果に対して API の負担が大きいため、やらない
