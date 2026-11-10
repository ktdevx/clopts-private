# TODO: 過剰な規則と検査を減らし、公開の名前をヘッダに揃える

この文書は、PR #29 で足した規則と検査のうち、守れる違反の割に重いものを減らす作業と、公開の名前をそれを宣言するヘッダの名前に揃える作業の一覧である。機能とモジュールの構成は変えない。公開の名前は、2 章の E と H のとおりに変える（まだリリースしていないので、互換は保たない。`AGENTS.md`）。

作業がすべて終わったら、この文書を消す。

## 1. 方針

- 規則は、実際に起きた問題か、起きやすい問題を防ぐものだけを残す。
- 検査は、コンパイラにできることはコンパイラに任せ、CMake のスクリプトには、コンパイラにできないことだけを残す。
- 名前の規則は、内部と公開で 1 つにする。ヘッダ `x.h` が宣言する名前は、関数も型も列挙子もマクロも、`clopts_x` / `CLOPTS_X` で始まる。例外を置かない。

## 2. 見直す点

### 2.1 規則と検査

| 点 | 今 | 推奨 | 理由 |
|----|----|------|------|
| A. 宣言のない定義 | `check_names.cmake` が、定義のある行を正規表現で探し、同じ名前のヘッダの宣言と照らす | ライブラリに `-Wmissing-prototypes` を付け、スクリプトからこの規則を消す | gcc と clang で今のライブラリに警告が出ず、残った定義を実際に捕まえる（確かめた）。書き方の前提に頼らない |
| B. 名前の検査の残り | 接頭辞、`*_internal.h` の禁止、宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無 | 接頭辞と `*_internal.h` の禁止だけを残す。接頭辞の検査は公開ヘッダにも広げる（H） | 名前は概念に付ける、という規則の核はこの 2 つである。残りは 40 ファイルほどの規模ならレビューで足り、スクリプトと fixture の量に見合わない |
| C. `process/env_impl.h` | `clopts_env_reserve_value` と `_reserve_wide` の 2 つだけを持つ | `process/env.h` に戻し、`env_impl.h` を消す | 実装ヘッダを置く 2 つの場面（埋め込み、OS ごとの実装に構造体を見せる）のどちらでもない。`env_win32.c` の `static` にすると、どの OS でも動いているバッファのテストが Windows だけになる |
| D. ほかのモジュールに見せる内部ヘッダの列挙 | `check_structure.cmake` の `_shared_headers` | 消す。`_impl.h` を同じモジュールに限る規則は残す | 列挙は 3 項目で、モジュールの依存の向きの検査がすでにある。`_impl.h` の規則は、実装を見せる面を外に漏らさないためのもので、同じ箇所に 1 行で書ける |
| F. `text/write.c` | 公開の入口だけを持ち、`help.c` と `message.c` を呼ぶ | 変えない | 公開ヘッダ `write.h` の関数を同じ名前の `.c` に置く規則に合う。入口の引数の検査を 1 か所にまとめる役もある |
| G. `docs/dev/design.md` の量 | 規則、理由、採らなかった案、参考にした設計 | A から D と H に合わせて削る。検査の説明は、何を検査するかだけにする | 文書がコードより先に古くなる。規則を減らした分、文書も減らす |

### 2.2 メモリ（E）

使うアロケーター（`allocator.c`）と確保の窓口（`mem.c`）を、1 つの概念 mem（ライブラリのメモリ）にまとめる。外から差し替える面と、ライブラリの中から使う面を、`base/mem.c` が持つ。

| 今 | 目標 |
|----|------|
| 公開ヘッダ `clopts/base/allocator.h` | `clopts/base/mem.h` |
| `clopts_allocator_t` | `clopts_mem_allocator_t` |
| `clopts_set_allocator` | `clopts_mem_set_allocator` |
| `src/base/allocator.c`、`src/base/allocator.h`、`clopts_allocator_get` | なくす（`mem.c` の `static` 変数を直接読む） |
| 内部の `clopts_mem_*` | 変えない |

- 概念の名前は `mem` にする。`alloc` だと `clopts_alloc_alloc`、`clopts_alloc_calloc` のように概念と動詞が重なる。`allocator` だと、約 150 か所で使う内部の関数がすべて長くなる。Lua も同じ切り方である（`lmem.c`、`luaM_malloc`）。
- 公開の名前を `clopts_mem_*` にするのは、H のとおり、ヘッダの名前に揃えるためである。今、ヘッダの名前と合わない公開の関数は `clopts_set_allocator` だけで、「型に属さない操作は動詞で始める」という別の規則は、実際には「ヘッダを動詞の名前にしているので、接頭辞と一致する」というだけだった（`clopts_parse`、`clopts_write_*`）。利用者がこの 2 つを書くのは、差し替えるときの 1 回だけである。

### 2.3 公開の名前をヘッダに揃える（H）

公開ヘッダが宣言する名前（関数、型、列挙子、マクロ）を全部調べた。ヘッダの名前と合わないのは、E の 2 つのほかに次のものだけである。

| ヘッダ | 今 | 推奨 | 理由 |
|--------|----|------|------|
| `clopts/base/export.h` | マクロ `CLOPTS_API` | ヘッダを `clopts/base/api.h` に改名する。マクロは変えない | マクロは、すべての公開関数に付く印である。ヘッダの名前を合わせる方が、変更が小さい |
| `clopts/text/write.h` | `CLOPTS_PREFIX_PROGRAM`、`CLOPTS_PREFIX_ERROR` | `CLOPTS_WRITE_PREFIX_PROGRAM`、`CLOPTS_WRITE_PREFIX_ERROR` | `clopts_write_error` の引数で、使う箇所は少ない。`CLOPTS_WRITE_OK` などの列挙子とぶつからない |
| `clopts/parse/result.h` | `clopts_parse_status_t`、`CLOPTS_PARSE_OK` など 5 つ | `clopts_result_status_t`、`CLOPTS_RESULT_OK`、`_TERMINATED`、`_NO_COMMAND`、`_USAGE_ERROR`、`_OUT_OF_MEMORY` | 読む関数が `clopts_result_get_status` なので、型の名前と揃う。列挙子は、ほかの状態の列挙（`CLOPTS_ARGV_*`、`CLOPTS_ENV_*`、`CLOPTS_WRITE_*`）と同じく、概念の名前に値を続ける形になる。`parse.h` に置くと、`parse.h` と `result.h` が互いを include することになり、C99 では書けない |
| `clopts/parse/result.h` | `clopts_source_t`、`CLOPTS_SOURCE_*` | 型だけのヘッダ `clopts/parse/source.h` に移す。名前は変えない | 値の出所は、結果だけでなく、解析の途中の記録（slot）と解決（resolve）も使う 1 つの概念である。型だけのヘッダは `.c` を持たなくてよい（`design.md` の 3 章）。改名せずに規則に合う |

- `CLOPTS_PARSE_*` は、利用者のコードで最もよく書かれる名前である（`switch` の `case`）。改名の影響は大きいが、機械的に置き換えられ、まだリリースしていない。
- 揃えたあとは、ヘッダ `x.h` が宣言する公開の名前がすべて `clopts_x` / `CLOPTS_X` で始まる。B の接頭辞の検査を、公開ヘッダの関数、型、列挙子、マクロにも広げる（インクルードガードは除く）。

## 3. 判断が要る点

どれも推奨するが、採らない場合は該当する作業（4 章で印を付けてある）を外す。

| 点 | 推奨 | 採らない場合 |
|----|------|--------------|
| A. `-Wmissing-prototypes` に任せる | 任せる | `check_names.cmake` の「定義には宣言が要る」を残す |
| B. 名前の検査を絞る | 絞る | 今の規則をすべて残す |
| C. `env_impl.h` を `env.h` に戻す | 戻す | `env_impl.h` を残す |
| D. `_shared_headers` を消す | 消す | 列挙を残す |
| E. メモリを 1 つの概念 mem にまとめる | まとめ、公開の名前も `clopts_mem_*` にする | まとめるが、公開の名前は `clopts_allocator_t` と `clopts_set_allocator` のままにする（H の例外として残す） |
| H1. `export.h` を `api.h` に改名する | 改名する | `CLOPTS_API` を H の例外として残す |
| H2. `CLOPTS_PREFIX_*` を `CLOPTS_WRITE_PREFIX_*` にする | 改名する | H の例外として残す |
| H3. 結果の状態を `clopts_result_status_t` / `CLOPTS_RESULT_*` にする | 改名する | H の例外として残す |
| H4. 値の出所を `clopts/parse/source.h` に移す | 移す | `result.h` に残し、H の例外とする |

H のどれかを採らない場合も、公開ヘッダの接頭辞の検査は、例外の一覧を持たせて入れる。

## 4. 作業

### 共通の完了条件

- 静的と共有の両方、Debug と Release で、gcc と clang でビルドでき、`ctest` がすべて通る。
- MinGW のクロスビルドを wine で実行し、Windows 向けの単体テストと構造の検査が通る。
- 公開の名前を変えたら、同じコミットで `abi/clopts.symbols`、`docs/api.md`、`docs/parsing.md`、`docs/windows.md`、`README.md`、`CHANGELOG.md`、`examples/`、`tests/docs/readme_minimal.c` を直す（`docs.sources`、`docs.api_coverage`、`docs.outputs`）。

### フェーズ 1: コンパイラの警告（A）

- [ ] `cmake/CloptsCompilerOptions.cmake` で、gcc と clang のライブラリの対象（`clopts` と `clopts_testing`）に `-Wmissing-prototypes` を付ける。テストとサンプルには付けない（Naos のテスト関数は宣言を持たない）。MSVC には同じ警告がないので付けない。
- [ ] `structure.names` から「定義には宣言が要る」を消し、その fixture を直す。

### フェーズ 2: 名前の検査を絞る（B）

- [ ] `check_names.cmake` を、接頭辞の規則と `*_internal.h` の禁止だけにする。接頭辞の規則は、内部ヘッダ（`src/<m>/x.h`、`x_impl.h` の関数）に加え、公開ヘッダ（`include/clopts/<m>/x.h` の関数、型、列挙子、マクロ）にも当てる。
- [ ] 宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無の規則と、その fixture を消す。公開ヘッダの接頭辞の fixture を足す。
- [ ] `tests/structure/CMakeLists.txt` の登録を直す。
- [ ] フェーズ 5 と 6 が終わるまで、公開ヘッダの検査は、まだ揃っていない名前を例外として持つ。フェーズ 6 の最後に例外を消す。

### フェーズ 3: `env_impl.h` を戻す（C）

- [ ] `clopts_env_reserve_value` と `_reserve_wide` を `src/process/env.h` に戻し、`src/process/env_impl.h` を消す。
- [ ] `env.c`、`env_win32.c`、`tests/process/test_env.c` の include を直す。

### フェーズ 4: 内部ヘッダの列挙を消す（D）

- [ ] `check_structure.cmake` から `_shared_headers` と「共有されていない」の違反を消す。`_impl.h` をほかのモジュールから include できない規則は残す。
- [ ] fixture `bad/reads_unshared_header` を消す。

### フェーズ 5: メモリ（E）

- [ ] `include/clopts/base/allocator.h` を `include/clopts/base/mem.h` に改名する（インクルードガードも）。
- [ ] `clopts_allocator_t` を `clopts_mem_allocator_t` に、`clopts_set_allocator` を `clopts_mem_set_allocator` に改名する。
- [ ] `src/base/allocator.c` の中身（標準の関数、使うアロケーター、`clopts_mem_set_allocator`）を `src/base/mem.c` に移し、`src/base/allocator.c` と `src/base/allocator.h` を消す。`clopts_allocator_get` は消し、使うアロケーターは `mem.c` の中の `static` 変数として直接読む。
- [ ] 内部ヘッダ `src/base/mem.h` は、最初に公開ヘッダ `clopts/base/mem.h` を include する。
- [ ] `tests/base/test_alloc.c` を `tests/base/test_mem.c` に改名し、スイートの名前も `mem` にする。`tests/support/test_alloc.c`（失敗を注入するアロケーター）の呼び出しも直す。

### フェーズ 6: 公開の名前（H）

- [ ] （H1）`include/clopts/base/export.h` を `include/clopts/base/api.h` に改名し、すべての公開ヘッダの include を直す。
- [ ] （H2）`CLOPTS_PREFIX_PROGRAM` / `_ERROR` を `CLOPTS_WRITE_PREFIX_PROGRAM` / `_ERROR` に改名する。
- [ ] （H3）`clopts_parse_status_t` を `clopts_result_status_t` に、`CLOPTS_PARSE_OK`、`_TERMINATED`、`_NO_COMMAND`、`_USAGE_ERROR`、`_OUT_OF_MEMORY` を `CLOPTS_RESULT_` の同じ名前に改名する。
- [ ] （H4）`clopts_source_t` と `CLOPTS_SOURCE_*` を、型だけのヘッダ `include/clopts/parse/source.h` に移す。`result.h` と、内部の `parse/slot.h` は、それを include する。
- [ ] `check_names.cmake` の公開ヘッダの例外の一覧を消す。

### フェーズ 7: 文書（G）

- [ ] `docs/dev/design.md` の「公開の関数の名前」を、「ヘッダ `x.h` が宣言する公開の名前は、関数も型も列挙子もマクロも `clopts_x` / `CLOPTS_X` で始まる」の 1 つにする。「型に属さない操作は動詞で始める」を消す。コールバックの型（`clopts_x_fn`）と、同じ値を 2 つの型から読めるようにしない規則は残す。
- [ ] `docs/dev/design.md` の「内部ヘッダの種類」を直す。表から「ライブラリ内部」の列挙の説明を消し、実装ヘッダの例を Scope と argv の 2 つにする。
- [ ] `docs/dev/design.md` の検査の説明を、何を検査するかだけにする（依存の向き、OS の境界、`_impl.h` の範囲、内部と公開の接頭辞、`*_internal.h`、宣言のない定義はコンパイラ）。
- [ ] `docs/dev/architecture.md` の base の行、依存の検査の説明、構造体の表を直す。
- [ ] `docs/api.md` の「関数名の付け方」の表を、ヘッダの名前と接頭辞の対応で書き直す。
- [ ] この文書を消す。
