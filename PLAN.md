# 公開 API の命名の見直し

名前から何をするかがわかるように、公開 API（`include/clopts/*.h`）の関数名をそろえる。まだ公開していないので、互換を気にせずに変える。

## 命名規則

| 種類 | 形 | 例 |
|---|---|---|
| 作成 | `clopts_<型>_create` | `clopts_option_create` |
| 解放 | `clopts_<型>_destroy` | `clopts_option_destroy` |
| 読み取り | `clopts_<型>_<属性>`（`get` を付けない） | `clopts_option_help` |
| 読み取り（bool） | `clopts_<型>_is_<属性>` | `clopts_option_is_required` |
| 書き込み | `clopts_<型>_set_<属性>` | `clopts_option_set_help` |
| 追加 | `clopts_<型>_add_<要素>` | `clopts_program_add_option` |
| 要素の数・要素 | `clopts_<型>_<要素>_count` / `clopts_<型>_<要素>_at` | `clopts_program_option_count` |
| 検索 | `clopts_<型>_find_<対象>` | `clopts_program_find_command` |
| 動作 | 動詞 | `clopts_parse`、`clopts_write_help`、`clopts_env_lookup` |

## 変更

### A. 作成と解放の名前をそろえる

今は `create`/`destroy`、`new`/`free`、`get`/`free` の 3 種類が混ざっている。`create`/`destroy` に統一し、解放が必要なことを名前で示す。

| 今 | 新しい名前 | 理由 |
|---|---|---|
| `clopts_argv_get` | `clopts_argv_create` | 属性の読み取りに見えるが、実際はハンドルを作り、解放が必要 |
| `clopts_argv_from_wide` | `clopts_argv_create_from_wide` | `clopts_argv_create` と同じ組だとわかるように |
| `clopts_argv_free` | `clopts_argv_destroy` | 統一 |
| `clopts_env_new` | `clopts_env_create` | 統一 |
| `clopts_env_free` | `clopts_env_destroy` | 統一 |
| `clopts_result_free` | `clopts_result_destroy` | 統一 |

`clopts_parse` は動作として読めるので変えない。

### B. bool を返す読み取りに `is_` を付ける

`clopts_option_required(opt)` は「必須にする」とも読めるので、問い合わせだとわかる形にする。setter（`set_required` など）は変えない。

| 今 | 新しい名前 |
|---|---|
| `clopts_option_required` | `clopts_option_is_required` |
| `clopts_option_negatable` | `clopts_option_is_negatable` |
| `clopts_option_terminal` | `clopts_option_is_terminal` |
| `clopts_positional_required` | `clopts_positional_is_required` |
| `clopts_positional_variadic` | `clopts_positional_is_variadic` |

### C. 形がほかと違うもの・意味があいまいなもの

| 今 | 新しい名前 | 理由 |
|---|---|---|
| `clopts_option_kind_of` | `clopts_option_kind` | `_of` が付くのはこれだけ |
| `clopts_result_terminal` | `clopts_result_terminal_option` | bool にも見えるが、実際は解析を止めたオプションを返す |
| `clopts_help_target` | `clopts_help_find_target` | 名前を引き、見つからなければ false を返す処理。`clopts_program_find_command` にそろえる |

### D. 未決: `clopts_result_count`

`clopts_result_count(result, option)` は COUNT 型オプションの値を返す。ところが、ほかの `_count`（`clopts_result_list_count`、`clopts_result_positional_count`）は要素の数を表すので、結果の件数を返す関数に見える。

- 案 1: `clopts_result_count_value` に変える
- 案 2: そのままにする

決まるまで変えない。

### E. 命名規則を文書に書く

`docs/api.md` に「命名規則」の節を足し、上の表を載せる。

## 影響範囲

- 公開ヘッダ（`include/clopts/`）
- 実装（`src/`）
- テスト（`tests/`）
- 文書（`README.md`、`docs/`）。文書のコード例はテスト（ラベル `docs`）で照合している
- サンプル（`examples/`）
- 公開シンボル一覧（`abi/clopts.symbols`）。テスト（ラベル `abi`）で照合している
- `CHANGELOG.md` の `[Unreleased]`。未公開なので「互換性のない変更」には入れず、「追加」の記述を新しい名前に書き換える

## 手順

1. A・B・C の名前をヘッダ、`src/`、`tests/`、`examples/`、`abi/clopts.symbols` で置き換える
2. `README.md`、`docs/`、`CHANGELOG.md` の名前を置き換え、`docs/api.md` に命名規則の節を足す（E）
3. 古い名前が残っていないことを `grep` で確かめる
4. 静的版と共有版の両方でビルドし、全テスト（`docs`、`abi` のラベルを含む）を通す
5. D を決めて、変えるなら同じ手順で置き換える
