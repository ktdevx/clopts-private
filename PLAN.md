# src/parser の責務の整理

`src/parser` の宣言（ビルダー）と解析の分け方はおおむねできているが、解析側に責務が混ざっているところと、実装とずれたコメントが残っている。動作は変えずに、ファイルと関数の責務をそろえる。公開 API と ABI は変えない。

## 方針

- 1 つの関心ごとは 1 つのファイルに閉じ込める。あるデータの中身を知り、それに書き込むのは、そのデータのファイルだけにする。ほかのファイルは関数を通して読み書きする。
- この規則はなるべくコンパイラに守らせる。構造体の定義はそのファイルの中に置き、ヘッダには型の名前だけを出す。
- `src/parser` の中のファイルは、モジュール名を前に付けずに、受け持つ関心ごとの名前を付ける（`scan.c`、`result.c`）。ほかのモジュール（`core`、`help`、`platform`）の名前は今回は変えない。

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

`struct clopts_result` は `parser_internal.h` にあり、`parser_scan.c` と `parser_resolve.c` が直接読み書きしている。

## 整理した後の分担

| 層 | ファイル | 責務 |
|---|---|---|
| 宣言 | `option.c`、`positional.c`、`scope.c`、`command.c`、`program.c` | 変えない |
| 名前 | `names.c` | 名前の規則と検索 |
| 解析 | `parse.c` | 入口（`clopts_parse*`）と解析全体の手順 |
| | `scan.c` | argv の走査 |
| | `resolve.c` | 環境変数・既定値からの解決 |
| | `check.c` | 必須の検査 |
| 結果 | `result.c` | `struct clopts_result` の定義と、それを読み書きするすべての関数 |

## 変更

### A. 内部のファイルから `parser_` を外す

| 今 | 新しい名前 |
|---|---|
| `src/parser/parser_decl.c` | `src/parser/names.c` |
| `src/parser/parser_scan.c` | `src/parser/scan.c` |
| `src/parser/parser_resolve.c` | `src/parser/resolve.c` |
| `src/parser/parser_result.c` | `src/parser/result.c` |

- `parser_decl.c` は `decl.c` にしない。中身は名前の規則と検索だけで、宣言の API 全体をまとめる公開ヘッダ `clopts/decl.h` と同じ名前にすると、そのヘッダを実装しているように見えるため。
- `parser_internal.h` は変えない。parser の中の共有の宣言であり、ほかのモジュールの `core_internal.h`、`help_internal.h`、`platform_internal.h` と同じ形のため。インクルードの書き方（`"parser/parser_internal.h"`）も変わらない。
- 関数名の接頭辞 `clopts_parser_` は変えない。ファイル名ではなく、ライブラリの中の名前空間のため。
- ほかのモジュールのファイル名（`core_vec.c`、`help_layout.c` など）は変えない。
- 名前を変えるだけのコミットにし、`git mv` で移す（`git log --follow` と `git blame` で履歴を追えるように、中身は同じコミットで変えない）。ファイル名を書いている所（`src/parser/CMakeLists.txt`、`command.c` と `parser_internal.h` のコメント、`docs/dev/architecture.md`）は次のコミットで直す。

`docs/dev/abi-checks.md` の `src/parser/parser_positional.c` は、過去に行った確認の記録なので変えない。

以下、B から先は新しいファイル名で書く。

### B. 実装とずれたコメントを直す

| 場所 | 今 | 直し方 |
|---|---|---|
| `result.c` の冒頭 | 「an option knows its index, so a value is found in constant time」 | オプションは自分の添字を知らない（`option.c` の冒頭）。公開の読み取り関数はスロットをオプションのアドレスで線形に探す（`find_slot` / `slot_of`）。実装どおりに書き直す |
| `parser_internal.h` の 2 つ目の冒頭コメント | ビルダーとして `program.c, command.c, option.c, positional.c` を挙げる | 1 つ目のコメントと同じく `scope.c` を入れる |
| `parser_internal.h` の 1 つ目の冒頭コメント | 「options have their own header」 | オプションの構造体は `option.c` だけが知っており、専用の内部ヘッダはない。「the layouts of a program, a command, an option and a positional are private to their files」のように書き直す |

### C. 結果への書き込みを `result.c` に集める

`scan.c`（`finish`、`store_pending_error`、`result->terminal`）と `resolve.c`（`set_missing`）が `result->status` と `result->error.*` に直接書いている。`error` の 8 つのフィールドを全部埋めるコードが 2 か所にあり、フィールドを足すと片方を直し忘れやすい。スロットへの記録は既に `clopts_parser_slot_*` を通しているので、`status`、`error`、`terminal` も同じように関数を通す。

1. `error` の無名の構造体に名前を付け、`parser_internal.h` に置く。`scan.c` の `scan_error_t` はこれに置き換える。

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

2. `result.c` に書き込みの関数を足す。

   | 関数 | 役割 | 置き換える箇所 |
   |---|---|---|
   | `void clopts_parser_result_set_error(clopts_result_t *result, const clopts_parser_error_t *error)` | `error` を丸ごと写す | `store_pending_error`（`scan.c`）、`set_missing`（`resolve.c`） |
   | `void clopts_parser_result_set_terminal(clopts_result_t *result, const clopts_option_t *option)` | 終端オプションを記録する | `result->terminal = state.terminal`（`scan.c`） |
   | `void clopts_parser_result_finish(clopts_result_t *result, clopts_status_t status)` | 最終の status を書く | `finish`（`scan.c`） |

   `set_missing` は、`clopts_parser_error_t` を組み立てて `clopts_parser_result_set_error` に渡す形にする。

3. 環境変数から得た値の記録を 1 つの関数にする。今は `resolve_slot` が `clopts_parser_slot_own()` と `clopts_parser_slot_set_value()` を続けて呼び、「value == owned」を呼び出し側が守っている。

   | 今 | 新しい関数 |
   |---|---|
   | `clopts_parser_slot_own(slot, copy)` の後に `clopts_parser_slot_set_value(slot, copy, CLOPTS_SOURCE_ENV)` | `void clopts_parser_slot_set_owned_value(clopts_parser_slot_t *slot, char *owned, clopts_source_t source)` |

   `clopts_parser_slot_own` はこれで使う所がなくなるので削除し、テスト（`test_result.c`）も新しい関数に書き換える。

この段階では `struct clopts_result` はまだ `parser_internal.h` にある。ほかのファイルから書けなくするのは F で行う。

### D. 解析の入口と手順を `parse.c` に分ける

`scan.c`（706 行）は、argv の走査のほかに、`clopts_parse` / `clopts_parse_with_env` の入口と、解析全体の手順（`docs/dev/architecture.md` の 1〜7）を回す処理（`scan_enter`、`scan_run`、`parse`）を持っている。手順の 4〜7 は走査ではなく、`resolve.c` と同じ高さの処理を呼び分けているだけである。

`parse.c` を作り、入口と手順を移す。`scan.c` には走査（手順 2 と 3）だけを残す。

`parser_internal.h` に足す走査の入口:

```c
/* Outcome of clopts_parser_scan(). */
typedef enum clopts_parser_scan_status
{
    CLOPTS_PARSER_SCAN_DONE,           /* every token was read, or a terminal option ended the scan */
    CLOPTS_PARSER_SCAN_USAGE_ERROR,    /* recorded in the error of result */
    CLOPTS_PARSER_SCAN_OUT_OF_MEMORY
} clopts_parser_scan_status_t;

/*
 * Steps 2 and 3: scans argv into result (selecting a command when program has
 * subcommands), then recovers a terminal option after a usage error. Sets
 * *terminal to the terminal option that ended the scan, or NULL. Does not
 * write the status of result.
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

終端オプションを結果に書くのは今と同じく手順 5 にする（手順 4 でメモリ不足になったときは `terminal` は NULL のまま）。手順の番号付きのコメントは `parse.c` の冒頭に移し、`scan.c` の冒頭は走査の説明だけにする。

### E. 必須の検査を `check.c` に分ける

`resolve.c` の `clopts_parser_check_required`（と `set_missing`、`slot_given`、`first_missing_option`）は、値を埋める処理ではなく、埋めた後の検査（手順 6）である。`check.c` に移し、`parser_internal.h` でも「Resolver」の節から「Required checks (check.c)」の節に分ける。関数の名前とテスト（`test_resolve.c`）は変えない。

### F. `struct clopts_result` を `result.c` に隠す

C で書き込みを関数に集めても、`struct clopts_result` が `parser_internal.h` にある限り、ほかのファイルから書けてしまう。定義を `result.c` に移し、`parser_internal.h` には不完全型（`clopts/parse.h` の `typedef`）だけを残す。ほかのファイルで `result->` と書くとコンパイルが通らなくなる。

`clopts_parser_slot_t` の定義は `parser_internal.h` に残す。スロットは `resolve.c` / `check.c` とテストが中身（`source`、`value`、`items` など）を読んでおり、書き込みは既に `clopts_parser_slot_*` を通しているため。

#### `src/` の置き換え

C の後で `result.c` の外に残る直接の読み取りと、その置き換え:

| 使う場所 | 読んでいるもの | 置き換え |
|---|---|---|
| `resolve.c`、`check.c` | `global_slots` / `global_count`、`command_slots` / `command_count` | 新しい内部関数 `clopts_parser_slot_t *clopts_parser_result_get_slots(clopts_result_t *result, bool global, size_t *count)` |
| `check.c` | `positionals.len` | 新しい内部関数 `size_t clopts_parser_result_get_positional_count(const clopts_result_t *result)`。公開の `clopts_result_get_positional_count()` は status が OK / TERMINATED / NO_COMMAND でないと 0 を返し、必須の検査の時点では status がまだ決まっていないので使えない |
| `scan.c`、`resolve.c`、`check.c`、`parse.c` | `command` | 公開の `clopts_result_get_command()`。status を見ずに返すのでそのまま使える |

#### テストの置き換え

テストは `result->` を `test_result.c` で 59 か所、`test_resolve.c` で 41 か所、`test_dispatch.c` で 9 か所、`test_long.c` で 2 か所使っている。公開の読み取り関数は status によって値を隠すので、そのままでは置き換えられない所がある（status を決める前の `error` など）。次の内部関数を足して置き換える。

| 関数 | 置き換える使い方 |
|---|---|
| `clopts_parser_slot_t *clopts_parser_result_find_slot(clopts_result_t *result, const clopts_option_t *option)` | テストが自分で持っている `slot_of`（`global_slots` / `command_slots` の線形探索）。公開の読み取り関数が使う `find_slot` と違い、status を見ない。見つからなければ NULL |
| `const clopts_parser_error_t *clopts_parser_result_get_error(const clopts_result_t *result)` | `result->error.*` の読み取り |
| `const clopts_option_t *clopts_parser_result_get_terminal(const clopts_result_t *result)` | `result->terminal` の読み取り（status を見ない） |
| `clopts_parser_result_get_slots`、`clopts_parser_result_get_positional_count`（上と同じ） | `global_slots[i]`、`global_count`、`command_count`、`positionals.len` |
| C の `clopts_parser_result_set_error`、`set_terminal`、`finish` | `result->status = ...`、`result->error.* = ...`、`result->terminal = ...` |

`result->status` の読み取りは公開の `clopts_result_get_status()`、`result->command` は `clopts_result_get_command()` に置き換える。`result->positionals.data` を読んでいる 1 か所（`test_result.c`）は、`clopts_parser_result_get_positional_count` と、status を OK にしてからの公開の `clopts_result_get_positional_at()` で書き直す。

`result.c` の中では、公開の読み取り関数が使う `find_slot` を、`clopts_parser_result_find_slot` に status の確認を足した形に書き直し、探索のコードを 1 つにする。

## やらないこと

| 案 | やらない理由 |
|---|---|
| `names.c` を名前の規則と検索に分ける | 両方とも「名前をどう比べるか」という同じ関心ごとで、比べ方（完全一致、大文字と小文字を区別、`--no-` の扱い）を 1 か所で決めている。分けると比べ方が 2 か所に散る。221 行と小さく、テストも `decl` の組で一緒に検査している |
| `result.c` を記録と公開の読み取り関数に分ける | F で `struct clopts_result` を `result.c` に隠すので、中身を読み書きする関数はすべて `result.c` に置くしかない。分けるには構造体をまた共有のヘッダに出すことになり、方針に反する |
| ほかのモジュールのファイル名から接頭辞を外す | 今回は `src/parser` の整理に絞る。必要なら別の計画にする |

## 影響範囲

- `src/parser/`：A で 4 つのファイルの名前を変える。`parser_internal.h`、`scan.c`、`resolve.c`、`result.c`、`command.c`（コメント）、`CMakeLists.txt` を直し、`parse.c` と `check.c` を足す
- `tests/parser/`：`clopts_parser_slot_own` を `clopts_parser_slot_set_owned_value` に書き換える（`test_result.c`）。F で `test_result.c`、`test_resolve.c`、`test_dispatch.c`、`test_long.c` の `result->` を内部関数に置き換える
- `docs/dev/architecture.md`：
  - 「構造体の置き場所」の `clopts_result_t` の行を「`src/parser/result.c`（中身を読み書きするのは `result.c` の関数だけ）」にする
  - 「検査の置き場所」の `parser_decl.c` を `names.c` にする
  - 「解析の流れ」の `clopts_parse`（`parser_scan.c`）を `parse.c` に、`parser_resolve.c` を `resolve.c` に、手順 6 に `check.c` を、`parser_result.c` を `result.c` にする
- 公開ヘッダ、`abi/clopts.symbols`、`CHANGELOG.md`：変えない（内部の整理だけ）

## 手順

変更ごとにコミットを分け、それぞれでビルドと全テストを通す。

1. A：`git mv` で 4 つのファイルの名前だけを変える。`CMakeLists.txt` が古い名前を指すとビルドできないので、`CMakeLists.txt` の 1 行だけはこのコミットで直す
2. A の続き：コメントと `docs/dev/architecture.md` のファイル名を直す。`grep -rnE 'parser_(decl|scan|resolve|result)\.c' --exclude-dir=.git . | grep -v abi-checks.md` で古い名前が残っていないことを確かめる
3. B：コメントを直す
4. C：`clopts_parser_error_t` と書き込みの関数を足し、`scan.c` と `resolve.c` の直接の書き込みを置き換える。`clopts_parser_slot_set_owned_value` を足して `clopts_parser_slot_own` を消し、テストを直す
5. D：`parse.c` を作り、入口と手順を移す。`scan.c` に `clopts_parser_scan` を足す
6. E：必須の検査を `check.c` に移す
7. F：内部の読み取り関数を足し、`src/` の `result->` を置き換える
8. F の続き：テストの `result->` を内部関数に置き換える
9. F の仕上げ：`struct clopts_result` の定義を `result.c` に移す。`grep -rn 'result->' src tests | grep -v 'src/parser/result.c'` で `result.c` の外に残っていないことを確かめる（定義を移した時点でコンパイルが通れば残っていない）
10. `docs/dev/architecture.md` の残り（構造体の置き場所、解析の流れ）を直す
11. 静的版と共有版の両方でビルドし、全テスト（`docs`、`abi` のラベルを含む）を通す。`abi` の検査で公開シンボルが変わっていないことを確かめる
