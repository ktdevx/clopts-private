# TODO

## オプションを単独で作り、コマンドにアタッチする API へ変更する（破壊的変更）

状態: 検討中（未実装）。現行設計との互換は考えず、親子関係として綺麗な形を前提に検討する。

### 動機

現在、`clopts_option_t` は `clopts_command_add_flag` / `_count` / `_value` / `_list` でしか作れず、生成と同時にスコープへ登録される（`src/parser/parser_build.c` の `build_add_option`）。「オプションを作る → 設定する → コマンドに渡す（所有権を移す）」の方が操作概念として自然なので、そちらへ寄せる。

### 現状が親（program / scope）に依存している点

`struct clopts_option`（`src/parser/parser_model.h`）の `program` / `scope` は所有のためだけではなく、次に使われている。分離するにはこれらの扱いを決め直す必要がある。

1. **エラーの記録先**: 宣言エラーは program に最初の 1 件だけ記録され（sticky）、`clopts_parse` / `clopts_validate` の結果（`CLOPTS_DECL_ERROR`）で返る。setter のエラーも `option->program` に書く（`build_reject_option`）。
2. **凍結・エラー済みの判定**: すべての setter が `build_begin(option->program)` を通る。
3. **スコープを見る検査**: 重複（`CLOPTS_DECL_DUPLICATE_OPTION`）、予約名（`help` / `h` / `version`）、`set_negatable` での `no-x` との衝突（`build_has_negation_name`）。root とサブコマンドの間もまたいで調べる（`decl_find_in_range`）。
4. **`index`**: スコープの vec 内の位置で、結果のスロット番号と一致する。

### 方針 1: 所有権は成功したときだけ移る（失敗時は副作用なし）

一般的な親子関係の API は、ほぼ「アタッチは成功したら所有権が移り、失敗したら何も変わらず呼び出し側が持ち続ける」という原子的な意味になっている。

- DOM `appendChild`: 失敗すると例外になり、木もノードも変わらない。
- C++ の `std::unique_ptr` を受け取る関数: 例外が出たら所有権は呼び出し側に残る（強い保証）。
- libxml2 `xmlAddChild`: 失敗すると NULL を返し、子の解放は呼び出し側の責任のまま。

これに合わせる。

- `clopts_command_add_option(command, option)` は成功したら `CLOPTS_DECL_NONE` を返し、以降 `option` は command が所有する（呼び出し側の手元のポインタは program が生きている間の借用参照になる）。
- 失敗したら、エラーの種類を返し、command も option も変わらない。option は呼び出し側が持ち続けるので、直して再アタッチするか `clopts_option_free` で解放する。
- 「失敗しても必ず移る」（前案）は、壊れたオブジェクトを木に入れることになり、木の不変条件（「つながっているものはすべて正しい」）が崩れるのでやめる。

### 方針 2: エラーは即座に返す。オプション・program にエラー状態を持たせない

宣言エラーは利用者の入力ではなくプログラマの誤りなので、溜めておいて後で解析結果として返す理由は薄い。各呼び出しの戻り値で即座に返す。

宣言エラーは、何を見れば判定できるかで 2 つに分かれる。

| 分類 | 種別 | 判定する場所 |
|------|------|--------------|
| オプション単体で決まる（コマンドの使い方に関係しない） | `INVALID_NAME`（名前の形式）、`NO_NAME`、`NOT_NEGATABLE`、`DEFAULT_NOT_ALLOWED`、`ENV_NOT_ALLOWED`、`REQUIRED_WITH_DEFAULT`、`REQUIRED_NOT_ALLOWED`、`ATTRIBUTE_KIND` | `clopts_option_new_*` / `clopts_option_set_*` が即座に返す |
| 兄弟・親との関係で決まる（コマンドの使い方＝CLI の形に関係する） | `DUPLICATE_OPTION`、`RESERVED_NAME`、`NEGATION_CONFLICT`、（位置引数）`POSITIONAL_ORDER`、`VARIADIC_POSITION`、（コマンド）`DUPLICATE_COMMAND`、`MIXED_LAYOUT` | アタッチ（`clopts_command_add_*` / `clopts_program_add_command`）が即座に返す |

後者もアタッチの時点で判定でき、しかも方針 1（失敗したら何も変えない）を守るにはアタッチの戻り値で返すしかない。したがって後者も溜めずに即座に返す。

結果として:

- オプションにも program にもエラー状態は要らない（`program->error` と `build_begin` のエラー済み判定は廃止）。
- 宣言が通った program は常に正しいので、`clopts_parse` は宣言エラーを返さなくなる。`CLOPTS_DECL_ERROR`、`clopts_validate`、宣言エラー用の `clopts_result_error_*`、`clopts_write_error` の `invalid declaration` の文面は削除できる。
- 凍結後の変更は今と同じく契約違反（assert）のまま。

代償: 呼び出しごとに戻り値を見る必要がある。見なければ、そのオプションが黙って無いだけになる。`CLOPTS_NODISCARD`（`warn_unused_result` / `[[nodiscard]]`）を付けてコンパイラに警告させる。

### 提案 API

```c
/* 生成。不正な名前と OOM を区別するため、戻り値はエラー、オブジェクトは out で返す */
CLOPTS_API clopts_decl_error_t clopts_option_new_flag (const char *long_name, char short_name, clopts_option_t **out);
CLOPTS_API clopts_decl_error_t clopts_option_new_count(const char *long_name, char short_name, clopts_option_t **out);
CLOPTS_API clopts_decl_error_t clopts_option_new_value(const char *long_name, char short_name, clopts_option_t **out);
CLOPTS_API clopts_decl_error_t clopts_option_new_list (const char *long_name, char short_name, clopts_option_t **out);

/* 未アタッチのオプションだけを解放する。アタッチ済みを渡すのは契約違反（assert）。NULL は何もしない */
CLOPTS_API void clopts_option_free(clopts_option_t *option);

/* setter はすべてエラーを返す（失敗時は属性を変えない） */
CLOPTS_API clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value);
/* ...他の setter も同様 */

/* 成功時のみ所有権が command に移る。失敗時は何も変わらない */
CLOPTS_API clopts_decl_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option);
```

`clopts_decl_error_t` にはメモリ不足を表す種別（例: `CLOPTS_DECL_OUT_OF_MEMORY`）を足す。あるいは、戻り値の型を宣言用の別の enum にする（下の「未決」参照）。

使用例:

```c
clopts_option_t *verbose;

if (clopts_option_new_count("verbose", 'v', &verbose) != CLOPTS_DECL_NONE) { /* ... */ }
clopts_option_set_help(verbose, "Print more details");
clopts_option_set_count_max(verbose, 3);
if (clopts_command_add_option(root, verbose) != CLOPTS_DECL_NONE)
{
    clopts_option_free(verbose);   /* 失敗時は呼び出し側が持ったまま */
    /* ... */
}
/* 以降 verbose は借用参照（結果の読み出しに使う） */
```

### その他の決めること（推奨案）

1. **アタッチ後の setter → 禁止する（設定 → アタッチ → 読み取り専用）**
   - 許すと、`set_negatable` などの setter がアタッチ前（単体の検査だけ）とアタッチ後（兄弟との検査も）で 2 通りの意味を持つ。
   - 禁止すれば、関係の検査はアタッチの 1 か所だけになる。
2. **二重アタッチ → `CLOPTS_DECL_ALREADY_ATTACHED`（仮）を返し、何も変えない**
   - DOM のような付け替え（元の親から外して移す）はしない。`index` と結果のスロットがスコープごとで、外すと兄弟の `index` がずれるため。
   - 同じ定義を複数のコマンドで使いたい場合は、作る関数を呼び分ける。必要なら `clopts_option_clone` を後で検討する。
3. **位置引数・コマンドへの展開**
   - 位置引数: 揃える（`clopts_positional_new` + `clopts_command_add_positional(cmd, pos)`）。
   - コマンド: 方針 2 の即時エラーは `clopts_program_add_command` などにも適用する。`clopts_command_new` + アタッチの形にまで揃えるかは別途判断（program とスコープの関係、`MIXED_LAYOUT`、root の扱いが強いので、今回は対象外でもよい）。

### 未決

- [ ] 宣言 API の戻り値の型: `clopts_decl_error_t` に OOM を足すか、`clopts_decl_status_t` のような別の enum を作るか。
- [ ] `CLOPTS_NODISCARD` をどの API に付けるか（全 setter か、new とアタッチだけか）。
- [ ] 宣言エラーの文面（今の `clopts_write_error` の `invalid declaration: ...`）を残すか。残すなら `clopts_decl_error_name(kind)` のような、種別から文面を引く関数にする。

### 影響範囲

- [ ] `src/parser/parser_build.c`: new とアタッチの分離、setter の戻り値化、関係の検査をアタッチへ集約、`build_begin` / `build_record_*` の整理
- [ ] `src/parser/parser_model.h`: `program->error` の削除、未アタッチ状態（`scope == NULL`）の扱い
- [ ] `src/parser/parser_scan.c`・`parser_result.c`: 宣言エラーを結果で返す経路の削除
- [ ] `src/help/help_message.c`・`help_layout.c`: `CLOPTS_DECL_ERROR` の文面と `program->error.status` 参照の削除
- [ ] `include/clopts/decl.h`・`parse.h`・`help.h`: API の差し替え、`CLOPTS_DECL_ERROR` / `clopts_validate` の削除
- [ ] `abi/clopts.symbols`
- [ ] `docs/api.md`: 宣言、メモリの所有と寿命、宣言エラーの表
- [ ] `README.md`、`examples/taskbook/taskbook.c`、`examples/textcount/textcount.c`
- [ ] `tests/parser/`（`test_build.c`、`test_decl_check.c`、`test_errors.c`、`test_oom.c` など）、`tests/help/`
- [ ] `CHANGELOG.md`: 破壊的変更として記載する
