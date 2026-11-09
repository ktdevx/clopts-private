# TODO

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

- [ ] スコープをまたぐ DUPLICATE_OPTION の検査（`decl_find_in_range`、root に足すときの全コマンド走査）
- [ ] スコープをまたぐ NEGATION_CONFLICT の検査
- [ ] RESERVED_NAME（組み込みが最下位なので、ユーザー定義が上書きする）
- [ ] `clopts_program_root()`（root は呼び出し側が作って渡す）
- [ ] `parser_command.c` と `parser_decl.c` からの `program->root`、`program->commands`、`program->version` への参照

### 作業項目

- [ ] `parser_model.h`：`program` メンバーを削除し `sealed` を追加する
- [ ] `parser_command.c`：`clopts_command_new` / `clopts_command_free` を追加し、検証を command 内で閉じるものだけにする。封印のチェックを追加する
- [ ] `parser_program.c`：`program_new(name, root, ...)` を実装し、`add_command` に封印とコマンドをまたぐ検証を実装する
- [ ] `parser_decl.c`：参照を内側優先に揃える（command → root → 組み込み。否定形はスコープの中で解決する）
- [ ] `parser_scan.c`：組み込みの `--help` / `--version` の判定を、ユーザー定義の参照の後に回す
- [ ] help：サブコマンドの help で root のオプションを並べるとき、隠されたものは除く
- [ ] `clopts_decl_error_t`：`CLOPTS_DECL_SEALED` を追加し、不要になったコードを削除する
- [ ] 公開ヘッダー（`command.h`、`program.h`、`decl_types.h`）とドキュメント（`docs/api.md`、`docs/parsing.md`）を更新する
- [ ] `abi/clopts.symbols` を更新する（ABI を破壊するので CHANGELOG に記載する）
- [ ] examples（`taskbook`、`textcount`）を新しい API に移行する
- [ ] テスト：シャドーイングの優先順位、否定形の解決順、封印後の変更、MIXED_LAYOUT、失敗したら何も変えないこと

### 代償と緩和策

- 意図しない名前の衝突が黙って起きる → 任意で呼べる診断関数 `clopts_program_lint()` でシャドーイングを報告する（任意）
- `-h` を上書きされるとヘルプが出せなくなる → 仕様としてドキュメントに明記する
- 組み立てる順番が下から上に固定される（program に渡した後でオプションを足すことはできない）→ 仕様として明記する
- 衝突エラーがなくなるので、シャドーイングが起きたことはエラーとして出ない（lint で補う）
