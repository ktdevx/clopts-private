# 公開 API の命名の見直し

名前から何をするかがわかるように、公開 API（`include/clopts/*.h`）の関数名をそろえる。まだ公開していないので、互換を気にせずに変える。

## 命名規則

関数名は `clopts_<型>_<動詞>[_<対象>]` の形にし、動詞で何をするかを示す。

| 種類 | 形 | 例 |
|---|---|---|
| 作成 | `clopts_<型>_create` | `clopts_option_create` |
| 解放 | `clopts_<型>_destroy` | `clopts_option_destroy` |
| 読み取り | `clopts_<型>_get_<属性>` | `clopts_option_get_help` |
| 読み取り（bool の属性） | `clopts_<型>_is_<属性>` | `clopts_option_is_required` |
| 書き込み | `clopts_<型>_set_<属性>` | `clopts_option_set_help` |
| 追加 | `clopts_<型>_add_<要素>` | `clopts_program_add_option` |
| 要素の数・要素 | `clopts_<型>_get_<要素>_count` / `clopts_<型>_get_<要素>_at` | `clopts_program_get_option_count` |
| 検索 | `clopts_<型>_find_<対象>` | `clopts_program_find_command` |
| その他の動作 | 動詞 | `clopts_parse`、`clopts_write_help`、`clopts_env_lookup` |

- `get` は読み取りだけに使い、作成には使わない。作成した物は必ず `destroy` で解放する。
- `is_` は宣言の bool の属性（`set_<属性>` と組になるもの）に使う。解析結果の FLAG の値は、ほかの値と並べて `get_flag` とする。

## 変更

### A. 作成と解放の名前をそろえる

今は `create`/`destroy`、`new`/`free`、`get`/`free` の 3 種類が混ざっている。`create`/`destroy` に統一する。`clopts_argv_get` はハンドルを確保して解放が必要なのに、名前は読み取りに見えるので特に直す。

| 今 | 新しい名前 |
|---|---|
| `clopts_argv_get` | `clopts_argv_create` |
| `clopts_argv_from_wide` | `clopts_argv_create_from_wide` |
| `clopts_argv_free` | `clopts_argv_destroy` |
| `clopts_env_new` | `clopts_env_create` |
| `clopts_env_free` | `clopts_env_destroy` |
| `clopts_result_free` | `clopts_result_destroy` |

`clopts_parse` と `clopts_parse_with_env` は動作として読めるので変えない。

### B. 読み取りに `get_` を付ける

名詞だけの名前は、`clopts_option_default`（「既定に戻す」とも読める）や `clopts_result_count`（結果の件数に見える）のように読み違えやすい。読み取りには `get_` を付ける。

#### `clopts_program_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_program_name` | `clopts_program_get_name` |
| `clopts_program_description` | `clopts_program_get_description` |
| `clopts_program_option_count` | `clopts_program_get_option_count` |
| `clopts_program_option_at` | `clopts_program_get_option_at` |
| `clopts_program_positional_count` | `clopts_program_get_positional_count` |
| `clopts_program_positional_at` | `clopts_program_get_positional_at` |
| `clopts_program_command_count` | `clopts_program_get_command_count` |
| `clopts_program_command_at` | `clopts_program_get_command_at` |

#### `clopts_command_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_command_name` | `clopts_command_get_name` |
| `clopts_command_summary` | `clopts_command_get_summary` |
| `clopts_command_description` | `clopts_command_get_description` |
| `clopts_command_option_count` | `clopts_command_get_option_count` |
| `clopts_command_option_at` | `clopts_command_get_option_at` |
| `clopts_command_positional_count` | `clopts_command_get_positional_count` |
| `clopts_command_positional_at` | `clopts_command_get_positional_at` |

#### `clopts_option_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_option_kind_of` | `clopts_option_get_kind` |
| `clopts_option_long_name` | `clopts_option_get_long_name` |
| `clopts_option_short_name` | `clopts_option_get_short_name` |
| `clopts_option_help` | `clopts_option_get_help` |
| `clopts_option_value_name` | `clopts_option_get_value_name` |
| `clopts_option_env` | `clopts_option_get_env` |
| `clopts_option_default` | `clopts_option_get_default` |
| `clopts_option_delimiter` | `clopts_option_get_delimiter` |
| `clopts_option_count_max` | `clopts_option_get_count_max` |

#### `clopts_positional_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_positional_name` | `clopts_positional_get_name` |
| `clopts_positional_help` | `clopts_positional_get_help` |

#### `clopts_result_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_result_status` | `clopts_result_get_status` |
| `clopts_result_command` | `clopts_result_get_command` |
| `clopts_result_terminal` | `clopts_result_get_terminal_option` |
| `clopts_result_usage_error` | `clopts_result_get_usage_error` |
| `clopts_result_error_command` | `clopts_result_get_error_command` |
| `clopts_result_error_option` | `clopts_result_get_error_option` |
| `clopts_result_error_positional` | `clopts_result_get_error_positional` |
| `clopts_result_error_token` | `clopts_result_get_error_token` |
| `clopts_result_error_argv_index` | `clopts_result_get_error_argv_index` |
| `clopts_result_error_name_len` | `clopts_result_get_error_name_len` |
| `clopts_result_error_short_name` | `clopts_result_get_error_short_name` |
| `clopts_result_flag` | `clopts_result_get_flag` |
| `clopts_result_count` | `clopts_result_get_count` |
| `clopts_result_value` | `clopts_result_get_value` |
| `clopts_result_list_count` | `clopts_result_get_list_count` |
| `clopts_result_list_at` | `clopts_result_get_list_at` |
| `clopts_result_source` | `clopts_result_get_source` |
| `clopts_result_positional_count` | `clopts_result_get_positional_count` |
| `clopts_result_positional_at` | `clopts_result_get_positional_at` |

`clopts_result_terminal` は、`get_` を付けても bool に見えるので、返す物（解析を止めたオプション）を名前に入れる。

`clopts_result_get_flag`、`get_count`、`get_value`、`get_list_*` は、オプションの種別（`CLOPTS_OPTION_FLAG`、`COUNT`、`VALUE`、`LIST`）の値を読む関数として並ぶ。`get_count` は COUNT 型の値、`get_list_count` は LIST 型の要素の数である。

#### `clopts_argv_*`、`clopts_env_*`、`clopts_error_*`

| 今 | 新しい名前 |
|---|---|
| `clopts_argv_status` | `clopts_argv_get_status` |
| `clopts_argv_count` | `clopts_argv_get_count` |
| `clopts_argv_values` | `clopts_argv_get_values` |
| `clopts_argv_error_index` | `clopts_argv_get_error_index` |
| `clopts_env_error` | `clopts_env_get_error` |
| `clopts_env_error_name` | `clopts_env_get_error_name` |
| `clopts_error_message` | `clopts_error_get_message` |

### C. bool の属性の読み取りに `is_` を付ける

`clopts_option_required(opt)` は「必須にする」とも読めるので、問い合わせだとわかる形にする。setter（`set_required` など）は変えない。

| 今 | 新しい名前 |
|---|---|
| `clopts_option_required` | `clopts_option_is_required` |
| `clopts_option_negatable` | `clopts_option_is_negatable` |
| `clopts_option_terminal` | `clopts_option_is_terminal` |
| `clopts_positional_required` | `clopts_positional_is_required` |
| `clopts_positional_variadic` | `clopts_positional_is_variadic` |

### D. 読み取りでない処理に動詞を付ける

| 今 | 新しい名前 | 理由 |
|---|---|---|
| `clopts_help_target` | `clopts_help_find_target` | 名前を引き、見つからなければ false を返す。`clopts_program_find_command` にそろえる |

### 変えないもの

| 名前 | 理由 |
|---|---|
| `clopts_*_create`、`clopts_*_destroy`（宣言の型） | すでに規則どおり |
| `clopts_*_set_*`、`clopts_*_add_*` | すでに規則どおり |
| `clopts_program_find_command` | すでに規則どおり |
| `clopts_parse`、`clopts_parse_with_env` | 動詞 |
| `clopts_write_*` | 動詞 |
| `clopts_help_command_create` | 作成 |
| `clopts_env_lookup` | 動詞。`clopts_getenv_fn` として渡すコールバック |
| `clopts_set_allocator` | ライブラリ全体の書き込み |
| `clopts_file_write` | 動詞。書き込みのコールバック |

### E. 命名規則を文書に書く

`docs/api.md` に「命名規則」の節を足し、上の命名規則の表を載せる。

## 影響範囲

- 公開ヘッダ（`include/clopts/`）
- 実装（`src/`）。公開関数を内部で呼んでいる箇所を含む
- テスト（`tests/`）
- 文書（`README.md`、`docs/`）。文書のコード例はテスト（ラベル `docs`）で実際のソースと照合している
- サンプル（`examples/`）
- 公開シンボル一覧（`abi/clopts.symbols`）。テスト（ラベル `abi`）で照合している
- `CHANGELOG.md` の `[Unreleased]`。未公開なので「互換性のない変更」には入れず、「追加」の記述を新しい名前に書き換える

## 手順

1. 上の表から「今の名前 → 新しい名前」の対応表を作り、単語の境界（`\b`）を付けて一括で置き換える。`clopts_option_count_max` と `clopts_result_count` のように前方が重なる名前があるので、長い名前から順に置き換える
2. ヘッダ、`src/`、`tests/`、`examples/`、`abi/clopts.symbols` を置き換える
3. `README.md`、`docs/`、`CHANGELOG.md` を置き換え、`docs/api.md` に命名規則の節を足す（E）
4. 古い名前が残っていないことを `grep` で確かめる
5. 名前が長くなって行が長くなりすぎた箇所の折り返しを、周りの書き方に合わせて手で直す
6. 静的版と共有版の両方でビルドし、全テスト（`docs`、`abi` のラベルを含む）を通す
