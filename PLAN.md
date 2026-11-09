# src/parser の責務の整理

`src/parser` の宣言（ビルダー）と解析の分け方はおおむねできているが、解析側に責務が混ざっているところと、実装とずれたコメントが残っている。動作は変えずに、ファイルと関数の責務をそろえる。公開 API と ABI は変えない。

## 今の分担

| 層 | ファイル | 責務 |
|---|---|---|
| 宣言 | `option.c`、`positional.c` | 単体の属性と、それだけで決まる規則 |
| | `scope.c` | 同じ範囲（Scope）の中の規則 |
| | `command.c`、`program.c` | コマンドの組み立てと、コマンドどうしの規則 |
| 名前 | `parser_decl.c` | 名前の規則と検索（ビルダーと走査が共有） |
| 解析 | `parser_scan.c` | 入口（`clopts_parse*`）、argv の走査、解析全体の手順 |
| | `parser_resolve.c` | 環境変数・既定値からの解決と、必須の検査 |
| 結果 | `parser_result.c` | 結果の確保、スロットへの記録、公開の読み取り関数、解放 |

## ファイル名の規則（変えない）

ほかのモジュール（`core`、`help`、`platform`）と同じく、公開ヘッダの関数を実装するファイルはヘッダの名前（`program.c`、`option.c`、`core/error.c`）、内部だけのファイルはモジュール名を前に付けた名前（`parser_scan.c`、`help_layout.c`）にする。今の `src/parser` もこの規則に合っているので、既存のファイル名は変えない。新しく作るファイルもこの規則に従う。

## 変更

### A. 実装とずれたコメントを直す

| 場所 | 今 | 直し方 |
|---|---|---|
| `parser_result.c` の冒頭 | 「an option knows its index, so a value is found in constant time」 | オプションは自分の添字を知らない（`option.c` の冒頭）。公開の読み取り関数はスロットをオプションのアドレスで線形に探す（`find_slot` / `slot_of`）。実装どおりに書き直す |
| `parser_internal.h` の 2 つ目の冒頭コメント | ビルダーとして `program.c, command.c, option.c, positional.c` を挙げる | 1 つ目のコメントと同じく `scope.c` を入れる |
| `parser_internal.h` の 1 つ目の冒頭コメント | 「options have their own header」 | オプションの構造体は `option.c` だけが知っており、専用の内部ヘッダはない。「the layouts of a program, a command, an option and a positional are private to their files」のように書き直す |

### B. 結果への書き込みを `parser_result.c` に集める

`struct clopts_result` が `parser_internal.h` にあるため、`parser_scan.c`（`finish`、`store_pending_error`、`result->terminal`）と `parser_resolve.c`（`set_missing`）が `result->status` と `result->error.*` に直接書いている。`error` の 8 つのフィールドを全部埋めるコードが 2 か所にあり、フィールドを足すと片方を直し忘れやすい。スロットへの記録は既に `clopts_parser_slot_*` を通しているので、`status`、`error`、`terminal` も同じように関数を通す。

1. `error` の無名の構造体に名前を付け、`parser_internal.h` に置く。`parser_scan.c` の `scan_error_t` はこれに置き換える。

   ```c
   /* The fields of a usage error (result->error). */
   typedef struct clopts_parser_error
   {
       clopts_usage_error_t usage;
       const clopts_command_t *command;
       const clopts_option_t *option;
       const clopts_positional_t *positional;
       const char *token;   /* an argv element, or NULL */
       int argv_index;      /* -1 when there is none */
       size_t name_len;
       char short_name;
   } clopts_parser_error_t;
   ```

2. `parser_result.c` に書き込みの関数を足す。

   | 関数 | 役割 | 置き換える箇所 |
   |---|---|---|
   | `void clopts_parser_result_set_error(clopts_result_t *result, const clopts_parser_error_t *error)` | `error` を丸ごと写す | `store_pending_error`（`parser_scan.c`）、`set_missing`（`parser_resolve.c`） |
   | `void clopts_parser_result_set_terminal(clopts_result_t *result, const clopts_option_t *option)` | 終端オプションを記録する | `result->terminal = state.terminal`（`parser_scan.c`） |
   | `void clopts_parser_result_finish(clopts_result_t *result, clopts_status_t status)` | 最終の status を書く | `finish`（`parser_scan.c`） |

   `set_missing` は、`clopts_parser_error_t` を組み立てて `clopts_parser_result_set_error` に渡す形にする。

3. 環境変数から得た値の記録を 1 つの関数にする。今は `resolve_slot` が `clopts_parser_slot_own()` と `clopts_parser_slot_set_value()` を続けて呼び、「value == owned」を呼び出し側が守っている。

   | 今 | 新しい関数 |
   |---|---|
   | `clopts_parser_slot_own(slot, copy)` の後に `clopts_parser_slot_set_value(slot, copy, CLOPTS_SOURCE_ENV)` | `void clopts_parser_slot_set_owned_value(clopts_parser_slot_t *slot, char *owned, clopts_source_t source)` |

   `clopts_parser_slot_own` はこれで使う所がなくなるので削除する。

4. `parser_internal.h` の `struct clopts_result` の前に、「読むのは parser の中とテストのどこからでもよいが、`src/` から書くのは `parser_result.c` の関数だけ」と書く。テスト（`tests/parser/test_result.c` など）は読み取り関数の検査の準備として `status` や `error` に直接書いているが、白箱のテストなのでそのままにする（新しい関数に書き換えてもよい）。

この規則を決まりで守るか、構造体を隠して守らせるかは E で決める。

### C. 解析の入口と手順を `parse.c` に分ける

`parser_scan.c`（706 行）は、argv の走査のほかに、`clopts_parse` / `clopts_parse_with_env` の入口と、解析全体の手順（`docs/dev/architecture.md` の 1〜7）を回す処理（`scan_enter`、`scan_run`、`parse`）を持っている。手順の 4〜7 は走査ではなく、`parser_resolve.c` と同じ高さの処理を呼び分けているだけである。

公開ヘッダ `clopts/parse.h` の関数を実装するファイルとして `parse.c` を作り、入口と手順を移す。`parser_scan.c` には走査（手順 2 と 3）だけを残す。

`parser_internal.h` に足す走査の入口:

```c
/* Outcome of clopts_parser_scan(). */
typedef enum clopts_parser_scan_status
{
    CLOPTS_PARSER_SCAN_DONE,           /* every token was read, or a terminal option ended the scan */
    CLOPTS_PARSER_SCAN_USAGE_ERROR,    /* recorded in result->error */
    CLOPTS_PARSER_SCAN_OUT_OF_MEMORY
} clopts_parser_scan_status_t;

/*
 * Steps 2 and 3: scans argv into result (selecting a command when program has
 * subcommands), then recovers a terminal option after a usage error. Sets
 * *terminal to the terminal option that ended the scan, or NULL. Does not
 * write result->status.
 */
clopts_parser_scan_status_t clopts_parser_scan(clopts_result_t *result, const clopts_program_t *program,
                                               int argc, char *const *argv, const clopts_option_t **terminal);
```

`parse.c` の中身（今の `scan_enter`、`scan_run`、`parse` と 2 つの公開関数）:

1. NULL のプログラムと結果の確保の失敗は番兵を返す。
2. 3. `clopts_parser_scan` を呼ぶ。`USAGE_ERROR` / `OUT_OF_MEMORY` なら `clopts_parser_result_finish` で終わる。
4. `clopts_parser_resolve`。
5. 終端オプションがあれば `clopts_parser_result_set_terminal` と `CLOPTS_TERMINATED`。
6. `clopts_parser_check_required`。
7. `CLOPTS_OK` か `CLOPTS_NO_COMMAND`。

終端オプションを結果に書くのは今と同じく手順 5 にする（手順 4 でメモリ不足になったときは `terminal` は NULL のまま）。手順の番号付きのコメントは `parse.c` の冒頭に移し、`parser_scan.c` の冒頭は走査の説明だけにする。

### D. 必須の検査を `parser_check.c` に分ける

`parser_resolve.c` の `clopts_parser_check_required`（と `set_missing`、`slot_given`、`first_missing_option`）は、値を埋める処理ではなく、埋めた後の検査（手順 6）である。`parser_check.c` に移し、`parser_internal.h` でも「Resolver」の節から「Required checks (parser_check.c)」の節に分ける。関数の名前とテスト（`test_resolve.c`）は変えない。

B と C の後なら 60 行ほどの移動だけで済む。小さすぎると判断するなら、ファイルは分けずに `parser_resolve.c` の冒頭の説明だけ直す。

### E. `struct clopts_result` を隠すか（未決定）

B で「`src/` から書くのは `parser_result.c` の関数だけ」と決めるが、`struct clopts_result` が `parser_internal.h` にある限り、ほかのファイルから書けてしまう。守り方は次の 2 案のどちらかにする。

どちらの案でも、`clopts_parser_slot_t` の定義は `parser_internal.h` に残す。スロットは `parser_resolve.c` / `parser_check.c` とテストが中身（`source`、`value`、`items` など）を読んでおり、書き込みは既に `clopts_parser_slot_*` を通しているため。隠す対象は `struct clopts_result` だけである。

#### 案 1：定義を `parser_result.c` に移す（コンパイラが守る）

`struct clopts_result` の定義を `parser_result.c` に移す。`parser_internal.h` には不完全型（`clopts/parse.h` の `typedef`）だけが残り、ほかのファイルで `result->` と書くとコンパイルが通らなくなる。

B の後で `parser_result.c` の外に残る直接の読み取りと、その置き換え:

| 使う場所 | 読んでいるもの | 置き換え |
|---|---|---|
| `parser_resolve.c`、`parser_check.c` | `global_slots` / `global_count`、`command_slots` / `command_count` | 新しい内部関数 `clopts_parser_slot_t *clopts_parser_result_slots(clopts_result_t *result, bool global, size_t *count)` |
| `parser_check.c` | `positionals.len` | 新しい内部関数 `size_t clopts_parser_result_positional_count(const clopts_result_t *result)`。公開の `clopts_result_get_positional_count()` は status が OK / TERMINATED / NO_COMMAND でないと 0 を返し、必須の検査の時点では status がまだ決まっていないので使えない |
| `parser_scan.c`、`parser_resolve.c`、`parser_check.c`、`parse.c` | `command` | 公開の `clopts_result_get_command()`。status を見ずに返すのでそのまま使える |

テストは `result->` を `test_result.c` で 59 か所、`test_resolve.c` で 41 か所、`test_dispatch.c` で 9 か所、`test_long.c` で 2 か所使っている。公開の読み取り関数は status によって値を隠すので、そのままでは置き換えられない所がある（status を決める前の `error` など）。テストのために次の内部関数を足す。

| 関数 | 置き換える使い方 |
|---|---|
| `clopts_parser_slot_t *clopts_parser_result_find_slot(clopts_result_t *result, const clopts_option_t *option)` | テストが自分で持っている `slot_of`（`global_slots` / `command_slots` の線形探索）。`parser_result.c` の `find_slot` と違い、status を見ない |
| `const clopts_parser_error_t *clopts_parser_result_get_error(const clopts_result_t *result)` | `result->error.*` の読み取り |
| `const clopts_option_t *clopts_parser_result_get_terminal(const clopts_result_t *result)` | `result->terminal` の読み取り（status を見ない） |
| `clopts_parser_result_slots`、`clopts_parser_result_positional_count`（上と同じ） | `global_slots[i]`、`global_count`、`command_count`、`positionals.len` |
| B の `clopts_parser_result_set_error`、`set_terminal`、`finish` | テストの `result->status = ...`、`result->error.* = ...`、`result->terminal = ...` |

`result->status` の読み取りは公開の `clopts_result_get_status()`、`result->command` は `clopts_result_get_command()` に置き換える。`result->positionals.data` を読んでいる 1 か所（`test_result.c`）は、`clopts_parser_result_positional_count` と、status を OK にしてからの公開の `clopts_result_get_positional_at()` で書き直す。

- 良い点：規則をコンパイラが守る。結果の内部の配置を変えてもテストを直さなくてよくなる
- 悪い点：内部関数が 5〜6 個増え、テストの約 110 か所を書き換える（大半は機械的な置き換え）

#### 案 2：定義を専用のヘッダに分ける（決まりで守る）

`struct clopts_result` の定義を `src/parser/parser_result_layout.h` に移し、インクルードしてよいのは `parser_result.c` とテストだけにする。`parser_internal.h` からは定義を消す。

決まりを守っていることは、`src/` の中で `parser_result.c` 以外が `parser_result_layout.h` をインクルードしていないことを `grep` で確かめるテスト（`docs` や `abi` と同じくラベルを付ける）で検査する。

- 良い点：`src/` の内部関数は案 1 の表の 2 つ（`clopts_parser_result_slots`、`clopts_parser_result_positional_count`）だけで済み、テストは `#include` を 1 行足すだけでほぼ変わらない
- 悪い点：コンパイラは止めないので、検査のテストに頼る。テストは引き続き結果の内部の配置に依存する

#### どちらにするか

書き込む場所を 1 つにすることが目的なら案 2 で足りる。テストを結果の内部の配置から切り離したいなら案 1 にする。決めたら、この節を選んだ案だけに書き換える。

## やらないこと

| 案 | やらない理由 |
|---|---|
| `parser_` を外したファイル名にそろえる | 「ファイル名の規則」のとおり、今の名前は他のモジュールと同じ規則に合っている |
| `parser_decl.c` を名前の規則と検索に分ける | 両方とも「名前をどう比べるか」という同じ知識に基づいており、221 行と小さい。テストも `decl` の組で一緒に検査している |
| `parser_result.c` を記録と公開の読み取り関数に分ける | B で書き込みの関数が増えても 600 行に届かない。大きくなったら `parse.c` と同じ規則で検討する |

## 影響範囲

- `src/parser/`：`parser_internal.h`、`parser_scan.c`、`parser_resolve.c`、`parser_result.c`、新しい `parse.c`（と D をするなら `parser_check.c`、E を案 2 にするなら `parser_result_layout.h`）、`CMakeLists.txt`
- `tests/parser/`：`clopts_parser_slot_own` を使っている `test_result.c` を `clopts_parser_slot_set_owned_value` に書き換える。E が案 1 なら `test_result.c`、`test_resolve.c`、`test_dispatch.c`、`test_long.c` の `result->` を内部関数に置き換える。案 2 なら `parser_result_layout.h` のインクルードを足し、インクルードの検査のテストを足す
- `docs/dev/architecture.md`：「構造体の置き場所」の `clopts_result_t` の行（書き込みは `parser_result.c` の関数だけ）、「解析の流れ」の `clopts_parse`（`parser_scan.c`）を `parse.c` に、`parser_decl.c` の説明はそのまま
- 公開ヘッダ、`abi/clopts.symbols`、`CHANGELOG.md`：変えない（内部の整理だけ）
- `docs/dev/abi-checks.md` の `src/parser/parser_positional.c`：過去に行った確認の記録なので変えない

## 手順

変更ごとにコミットを分け、それぞれでビルドと全テストを通す。

1. A：コメントだけを直す
2. B：`clopts_parser_error_t` と書き込みの関数を足し、`parser_scan.c` と `parser_resolve.c` の直接の書き込みを置き換える。`clopts_parser_slot_set_owned_value` を足して `clopts_parser_slot_own` を消し、テストを直す。最後に `grep -nE 'result->(status|error|terminal)[^=]*=[^=]' src/` で `parser_result.c` の外に書き込みが残っていないことを確かめる
3. C：`parse.c` を作り、入口と手順を移す。`parser_scan.c` に `clopts_parser_scan` を足す
4. D：必須の検査を `parser_check.c` に移す
5. E：選んだ案で `struct clopts_result` を移す。案 1 ならテストの書き換えを別のコミットに分ける
6. `docs/dev/architecture.md` を直す。E の結果に合わせて「構造体の置き場所」の `clopts_result_t` の行も直す
7. 静的版と共有版の両方でビルドし、全テスト（`docs`、`abi` のラベルを含む）を通す。`abi` の検査で公開シンボルが変わっていないことを確かめる
