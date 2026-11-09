# TODO

## オプションを単独で作り、コマンドにアタッチする API へ変更する（破壊的変更）

状態: 検討中（未実装）

### 動機

現在、`clopts_option_t` は `clopts_command_add_flag` / `_count` / `_value` / `_list` でしか作れず、生成と同時にスコープへ登録される（`src/parser/parser_build.c` の `build_add_option`）。「オプションを作る → 設定する → コマンドに渡す（所有権を移す）」の方が操作概念として自然なので、そちらへ寄せる。

### 現状が親（program / scope）に依存している点

`struct clopts_option`（`src/parser/parser_model.h`）の `program` / `scope` は所有のためだけではなく、次に使われている。分離するにはこれらの置き場所を決める必要がある。

1. **エラーの記録先**: 宣言エラーは program に最初の 1 件だけ記録される。setter のエラーも `option->program` に書く（`build_reject_option`）。
2. **凍結・エラー済みの判定**: すべての setter が `build_begin(option->program)` を通る。
3. **スコープを見る検査**: 重複（`CLOPTS_DECL_DUPLICATE_OPTION`）、予約名（`help` / `h` / `version`）、`set_negatable` での `no-x` との衝突（`build_has_negation_name`）。root とサブコマンドの間もまたいで調べる（`decl_find_in_range`）。
4. **`index`**: スコープの vec 内の位置で、結果のスロット番号と一致する。

### 提案 API

```c
/* 種類ごとのコンストラクタ（kind の enum は内部型のままにする） */
CLOPTS_API clopts_option_t *clopts_option_new_flag (const char *long_name, char short_name);
CLOPTS_API clopts_option_t *clopts_option_new_count(const char *long_name, char short_name);
CLOPTS_API clopts_option_t *clopts_option_new_value(const char *long_name, char short_name);
CLOPTS_API clopts_option_t *clopts_option_new_list (const char *long_name, char short_name);

/* 未アタッチのオプションだけを解放する。アタッチ済みを渡すのは契約違反（assert） */
CLOPTS_API void clopts_option_free(clopts_option_t *option);

/* option の所有権は、成功・失敗にかかわらず常に command に移る */
CLOPTS_API void clopts_command_add_option(clopts_command_t *command, clopts_option_t *option);
```

使用例:

```c
clopts_option_t *verbose = clopts_option_new_count("verbose", 'v');
clopts_option_set_help(verbose, "Print more details");
clopts_option_set_count_max(verbose, 3);
clopts_command_add_option(root, verbose);   /* 以降 verbose は借用参照（結果の読み出しに使う） */
```

### 決めること（推奨案）

1. **アタッチ失敗時の所有権 → 常に移る**
   - 現行の「エラーがあっても adder はつないで返す」方針に合わせ、重複などでもつないでからエラーを記録する。
   - 呼び出し側は失敗時の解放を書かなくてよい。
   - `clopts_command_add_option(cmd, NULL)` は OOM として記録する。これで `clopts_command_add_option(root, clopts_option_new_flag(...))` の 1 行書きでもリークや見落としが起きない。
2. **アタッチ前のエラーの置き場所 → オプション自身に最初の 1 件を持たせ、アタッチ時に program へ移す**
   - 種類だけで決まる検査はアタッチ前に判定できる: `DEFAULT_NOT_ALLOWED`、`ENV_NOT_ALLOWED`、`REQUIRED_WITH_DEFAULT`、`REQUIRED_NOT_ALLOWED`、`NOT_NEGATABLE`、`ATTRIBUTE_KIND`、`NO_NAME`、名前の形式。
   - エラーは解析結果（`clopts_result_*`）から読むので、記録がアタッチ時まで遅れても利用者の体験は変わらない。
   - `clopts_result_error_option` はアタッチ後に program が所有するオプションを指すので、今と同じく使える。
3. **スコープを見る検査 → すべてアタッチ時に行う**
   - 重複、予約名、否定形の衝突の両方向（否定可能な `x` と `no-x`）。
   - 今は setter と adder に分かれている検査が 1 か所にまとまる。
4. **アタッチ後の setter → 禁止する（設定 → アタッチ → 読み取り専用）**
   - 禁止すれば、スコープを見る検査がアタッチ時だけになり、経路が増えない。
   - 許すと、`set_negatable` の衝突検査をアタッチ前と後の両方で持つことになる。
   - 代償として、現行の `opt = add(...); set_help(opt, ...)` の順の書き方はできなくなる。
5. **二重アタッチ → 契約違反（assert。assert なしなら記録して無視）**
   - 同じオプションを複数のコマンドで共有することはできない（`index` と結果のスロットがスコープごとのため）。
   - 使い回しが必要になったら `clopts_option_clone` を後で検討する。
6. **位置引数・コマンドへの展開**
   - 位置引数: 揃える（`clopts_positional_new` + `clopts_command_add_positional(cmd, pos)`）。`POSITIONAL_ORDER` と `VARIADIC_POSITION` はアタッチ時に検査する。
   - コマンド: program とスコープの関係（`MIXED_LAYOUT`、root の扱い）が強いので、今回は対象外でよい。

### デメリット

- 呼び出しが 2 回に増え、アタッチし忘れるとリークする。現行は program を解放すれば全部消える「親が全部持つ」設計で、リークは構造上起きない。
- アロケーターはグローバルなので、program より先にオプションを確保しても問題はない。

### 影響範囲

- [ ] `src/parser/parser_build.c`: `build_add_option` を new とアタッチに分け、setter の前処理と検査の位置を変える
- [ ] `src/parser/parser_model.h`: オプションにアタッチ前のエラー記録用フィールドを追加する
- [ ] `include/clopts/decl.h`: API の差し替え
- [ ] `abi/clopts.symbols`
- [ ] `docs/api.md`: 宣言、メモリの所有と寿命、宣言エラーの表
- [ ] `README.md`、`examples/taskbook/taskbook.c`、`examples/textcount/textcount.c`
- [ ] `tests/parser/`（`test_build.c`、`test_decl_check.c`、`test_oom.c` など）、`tests/help/` のフィクスチャ
- [ ] `CHANGELOG.md`: 破壊的変更として記載する
