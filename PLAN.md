# あるべき姿の検討

CLOpts は未リリースで、あるべき姿のためなら設計と API を壊してよい（`AGENTS.md`）。これまでの整理では「公開 API と ABI は変えない」を前提にしていたため、手を付けずに残したところがある。ここでは、その前提を外したときにどうあるべきかを検討する。

各節の「案」が推奨で、「決めること」が未決の点である。

## 1. 型の名前に、どの領域のものかを入れる

### 今

公開 API の列挙型は、ほとんどが領域の名前を持つ。

| 型 | 領域 |
|---|---|
| `clopts_usage_error_t` | argv の使い方の誤り |
| `clopts_source_t` | オプションの値の出どころ |
| `clopts_write_status_t` | 文面の出力 |
| `clopts_argv_status_t` | argv の取得 |
| `clopts_env_error_t` | 環境変数の取得 |

`clopts_error_t` と `clopts_status_t` の 2 つだけが、領域を持たない汎用の名前になっている。

- `clopts_error_t`（`clopts/error.h`）の中身は宣言の誤りだけで、返すのも宣言の関数（`src/decl/`）と、宣言を作る `clopts_help_command_create` だけである。実装は既に `src/decl/error.c` にある。名前だけを見ると、ライブラリ全体の誤りの型に見える。`CLOPTS_ERROR_OUT_OF_MEMORY` も、解析の `CLOPTS_OUT_OF_MEMORY` と並ぶと区別しにくい。
- `clopts_status_t`（`clopts/parse.h`）は解析の結果の状態である。`CLOPTS_OK` は、どの関数の成功なのかが名前からわからない。

### 案

領域を名前に入れる。

| 今 | 新しい名前 |
|---|---|
| `clopts_error_t`、`CLOPTS_ERROR_*` | `clopts_decl_error_t`、`CLOPTS_DECL_ERROR_*` |
| `CLOPTS_ERROR_LIST` | `CLOPTS_DECL_ERROR_LIST` |
| `clopts_error_get_message` | `clopts_decl_error_get_message` |
| `clopts/error.h` | `clopts/decl_error.h` |
| `clopts_status_t`、`CLOPTS_OK` など | `clopts_parse_status_t`、`CLOPTS_PARSE_OK` など |

`clopts_parse_status_t` の値は次のとおりにする（2 節の変更の後）。

| 今 | 新しい名前 |
|---|---|
| `CLOPTS_OK` | `CLOPTS_PARSE_OK` |
| `CLOPTS_TERMINATED` | `CLOPTS_PARSE_TERMINATED` |
| `CLOPTS_NO_COMMAND` | `CLOPTS_PARSE_NO_COMMAND` |
| `CLOPTS_USAGE_ERROR` | `CLOPTS_PARSE_USAGE_ERROR` |
| `CLOPTS_OUT_OF_MEMORY` | `CLOPTS_PARSE_OUT_OF_MEMORY` |

`clopts_usage_error_t`、`clopts_source_t` は今のままで領域がわかるので変えない。

### 決めること

- `clopts/decl_error.h` を独立のヘッダにするか、`clopts/decl_types.h` に入れるか。help も宣言の誤りを返すので（`clopts_help_command_create`）、独立のヘッダのほうが取り込みやすい。

## 2. 環境変数：既定で UTF-8 にし、失敗をメモリ不足と分ける

### 今

- `clopts_parse()` の既定の引き方は `getenv()` である。Windows では、値が ANSI のコードページで返る。UTF-8 で得るには、利用者が `clopts_env_create()` で文脈を作り、`clopts_env_lookup` と一緒に `clopts_parse_with_env()` に渡し、後で `clopts_env_destroy()` を呼ぶ（`docs/windows.md`）。argv は `clopts_argv_create()` で UTF-8 になるのに、環境変数は既定のままだと UTF-8 にならず、扱いがそろっていない。
- 引き方の失敗（`CLOPTS_ENV_FAILED`）は、解析を `CLOPTS_OUT_OF_MEMORY` で終わらせる。Windows では、値が正しい UTF-16 でないときや、名前が正しい UTF-8 でないときも、この状態になる。本当のメモリ不足かどうかは、結果ではなく別の文脈（`clopts_env_get_error`、`clopts_env_get_error_name`）を見ないとわからない。誤りの理由が、結果と文脈の 2 か所に分かれている。
- その結果、公開 API には環境変数の文脈の型と関数が 5 つ（`clopts_env_t`、`clopts_env_create`、`clopts_env_destroy`、`clopts_env_lookup`、`clopts_env_get_error`、`clopts_env_get_error_name`）と、理由の型（`clopts_env_error_t`）がある。

### 案

失敗の理由を 3 つに分け、それぞれを、ほかの同じ種類の誤りと同じ場所で報告する。

| 失敗 | 今 | 案 |
|---|---|---|
| 名前が正しい UTF-8 でない（Windows） | 解析が `OUT_OF_MEMORY`、理由は文脈に | 宣言の誤りにする。`clopts_option_set_env` が `INVALID_NAME` を返す。名前は宣言で決まるので、解析の前に検査できる |
| 値が正しい UTF-16 でない（Windows） | 解析が `OUT_OF_MEMORY`、理由は文脈に | 使い方の誤りにする。新しい `CLOPTS_USAGE_INVALID_ENV_VALUE` で、`clopts_result_get_error_option` がそのオプションを返す。環境変数の値は、argv と同じく利用者からの入力である |
| メモリ不足 | 解析が `OUT_OF_MEMORY` | そのまま |

その上で、次のようにする。

- 既定の引き方を、platform の UTF-8 の引き方にする（Windows は `GetEnvironmentVariableW` で読んで UTF-8 に変換する。それ以外の OS は `getenv` のパススルー）。変換に使うバッファは、解析が 1 回ごとに内部で作り、解析の終わりに解放する。値は受け取った直後に複製するので、結果はバッファに依存しない。解析ごとに作るので、同じプログラムを複数のスレッドから同時に解析できることも変わらない。
- `clopts_parse_with_env()` は、テストや設定ファイルなど、環境変数の代わりの出どころを渡すために残す。`getenv_fn` が NULL なら上の既定の引き方を使う。
- コールバックの戻り値（`clopts_env_status_t`）に `CLOPTS_ENV_INVALID`（値を UTF-8 で表せない）を足し、`CLOPTS_ENV_FAILED` はメモリ不足だけを意味するようにする。既定の引き方も同じ戻り値を使う。
- `clopts_env_t`、`clopts_env_create`、`clopts_env_destroy`、`clopts_env_lookup`、`clopts_env_get_error`、`clopts_env_get_error_name`、`clopts_env_error_t` を公開 API から外す。`clopts/platform.h` は argv だけになる。
- `clopts_write_error()` に、`CLOPTS_USAGE_INVALID_ENV_VALUE` の 1 行（例：`environment variable TOOL_FORMAT for --format is not valid UTF-16`）を足す。

### 利用者のコードの変わり方

Windows で環境変数を UTF-8 で得るための手順（文脈を作って渡し、後で解放する）がなくなり、`clopts_parse()` を呼ぶだけになる。`examples/textcount` と `examples/taskbook` から、その手順を消す。

### 決めること

- `CLOPTS_USAGE_INVALID_ENV_VALUE` を使い方の誤りに入れてよいか。誤りの元は argv ではないが、利用者が直せる入力の誤りという点で同じ扱いにできる。`clopts_result_get_error_token` と `clopts_result_get_error_argv_index` は、NULL と -1 を返す（必須の誤りと同じ）。
- 宣言の検査で、環境変数の名前に `=` を含むものも `INVALID_NAME` にするか。どの OS でも、`=` を含む名前の変数は引けない。

## 3. 互換のための約束を外す

### 今

公開ヘッダに、ABI の互換を守るための約束が書かれている。

| 場所 | 約束 |
|---|---|
| `clopts/alloc.h` | `clopts_allocator_t` の配置は凍結し、メンバーを足しも消しも並べ替えもしない |
| `clopts/platform.h` | `clopts_argv_status_t` と `clopts_env_error_t` の値は末尾に足すだけ |
| `clopts/option.h` | `clopts_option_kind_t` の値は ABI の一部 |

`docs/abi.md` は「ABI の後方互換は 1.0.0 から保証し、0.x の間は minor ごとに壊してよい」としており、これらの約束は今の段階では方針より強い。

### 案

- 上の 3 つの約束をヘッダから消す。1.0.0 を出すときに、そのときの形を見て書き直す。
- `docs/abi.md` は変えない。1.0.0 からの方針として、今のままで正しい。
- ABI の検査（`abi/clopts.symbols` との照合、最新のタグとの `abidiff`）は残す。照合は「内部の関数を export していない」ことの検査として今も役に立つ。`abidiff` は版が 1.0.0 より前のあいだと、比べるタグがないあいだは何もしない（`tests/abi/run_abidiff.cmake`）。公開 API を変えたら、`abi/clopts.symbols` を同じ変更で直す。
- `CHANGELOG.md` の `[Unreleased]` は、変更履歴ではなく「最初のリリースに入るもの」として書き直す（前回の API の改名と同じ扱い）。

## 4. ほかに見つけたもの（優先度は低い）

| 項目 | 今 | あるべき姿の候補 |
|---|---|---|
| NULL のプログラムを解析する | メモリ不足の番兵を返す（`clopts_parse`） | 宣言の関数と同じく、NULL のハンドルは契約違反として `assert` にする。番兵は確保に失敗したときだけにする |
| help の宣言の補助 | `clopts_help_command_create` は help モジュールにあり、宣言の誤りを返す | 公開 API だけで書ける補助なので、help にあってよい。1 節の改名で戻り値が `clopts_decl_error_t` になるだけで、置き場所は変えない |

## 進め方

1、3 は名前と文書の変更で、挙動は変わらない。2 は API の形と挙動が変わる。次の順に進めると、それぞれの変更が小さく保てる。

1. 3（約束を外す）：ヘッダのコメントだけ。
2. 1（改名）：前回の API の改名と同じく、対応表で一括して置き換える。`abi/clopts.symbols`、`docs/`、`examples/`、`CHANGELOG.md` も同じ変更で直す。
3. 2（環境変数）：
   1. `clopts_option_set_env` で名前を検査する（宣言の誤り）。
   2. `CLOPTS_ENV_INVALID` と `CLOPTS_USAGE_INVALID_ENV_VALUE` を足し、`clopts_write_error` の文面を足す。
   3. 既定の引き方を platform の UTF-8 の引き方にする。
   4. `clopts_env_t` とその関数を公開 API から外し、`docs/windows.md`、`docs/api.md`、`examples/` を直す。
4. 4 は、必要と判断したものだけ別に行う。
