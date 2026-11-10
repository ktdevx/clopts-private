# TODO: 過剰な規則と検査を減らし、名前の規則の範囲を決め直す

この文書は、PR #29 で足した規則と検査のうち、守れる違反の割に重いものを減らす作業と、名前の規則を当てる範囲を決め直す作業の一覧である。機能とモジュールの構成は変えない。公開の名前は、2 章の E と I で変える分だけ変える（まだリリースしていないので、互換は保たない。`AGENTS.md`）。

作業がすべて終わったら、この文書を消す。

## 1. 方針

- 規則は、実際に起きた問題か、起きやすい問題を防ぐものだけを残す。
- 検査は、コンパイラにできることはコンパイラに任せ、CMake のスクリプトには、コンパイラにできないことだけを残す。
- 規則は、その根拠が効く範囲にだけ当てる。例外をなくすために、根拠の外まで広げない。

3 つ目は、前の版のこの文書への反省である。前の版は「ヘッダ `x.h` が宣言する名前は、関数も型も列挙子もマクロも `clopts_x` / `CLOPTS_X` で始まる。例外を置かない」とし、`export.h` を `api.h` に、`clopts_parse_status_t` を `clopts_result_status_t` に改名するなどの案（旧 H1 から H4）を出していた。これは、例外がないこと自体を目的にしていた。`CLOPTS_API` のようなライブラリ全体の印は、どのヘッダで定義されてもおかしくない。名前に合わせてヘッダを改名するのは、本末転倒である。旧 H1 から H4 は取り下げる（2.5）。

## 2. 見直す点

### 2.1 名前の規則を当てる範囲

「ヘッダ `x.h` の名前は `clopts_x_*` で始まる」という規則の根拠は、名前から置き場所が引けることである。`clopts_scope_find_long` を見れば、`scope.h` が宣言し、`scope.c` が定義すると分かる。この根拠が効くのは、概念の操作、つまり関数と、関数が操作する不透明な型である。

次のものは、概念の操作ではない。名前から置き場所を引く必要がなく、名前は役割や意味を表していれば足りる。

| 種類 | 例 | 名前の付け方 |
|------|----|--------------|
| ライブラリ全体の印 | `CLOPTS_API` | 役割で付ける。定義する場所はどこでもよい |
| 引数のフラグ | `CLOPTS_PREFIX_PROGRAM`、`CLOPTS_PREFIX_ERROR` | 引数の意味で付ける |
| 語彙としての列挙 | `clopts_parse_status_t`（解析の状態）、`clopts_source_t`（値の出所） | 表す意味で付ける |
| 利用者が埋めて渡すデータの型 | `clopts_allocator_t` | 中身で付ける |
| コールバックの型 | `clopts_write_fn`、`clopts_env_fn` | 呼ぶ側の概念の名前に `_fn` を付ける（今の規則のまま。意味で付けた結果、ヘッダの名前と一致している） |

これらも `clopts_` / `CLOPTS_` で始め、ライブラリの名前空間には入れる。その中では、意味で名付ける。

決め直した規則は次の 3 つである。

1. 内部ヘッダと公開ヘッダの**関数**は、ヘッダの名前を接頭辞にする（`x.h` なら `clopts_x_*`）。`static inline` の関数も含む。
2. 不透明な型 `clopts_x_t` は、その型の操作を宣言するヘッダ `x.h` が宣言する（今の `design.md` 3 章の 3 のまま）。
3. それ以外（マクロ、列挙、値として渡す構造体、コールバックの型）は、`clopts_` / `CLOPTS_` で始め、意味で名付ける。置く場所は、使う概念のヘッダにする。名前でヘッダを縛らない。

検査（B）が見るのも 1 だけにする。2 は今の書き方で崩れておらず、3 は検査しない。

### 2.2 今の公開の名前を当てはめた結果

公開ヘッダが宣言する名前を全部調べた（コメントの中の名前は除く）。

| 種類 | 規則に合わないもの | 扱い |
|------|--------------------|------|
| 関数（規則 1） | `clopts/base/allocator.h` の `clopts_set_allocator` | E で `clopts_mem_set_allocator` にする |
| 関数（規則 1） | `clopts/text/write.h` の `static inline` の `clopts_file_write` | I で `clopts_write_to_file` にする |
| 不透明な型（規則 2） | なし | — |
| マクロ、列挙、データの型、コールバックの型（規則 3） | 規則 3 は置き場所を縛らないので、合わないものはない | 変えない |

前の版の調べでは、`clopts_file_write` を見落としていた。`static inline` で、ライブラリの記号（`abi/clopts.symbols`）に現れないためである。

### 2.3 規則と検査

| 点 | 今 | 推奨 | 理由 |
|----|----|------|------|
| A. 宣言のない定義 | `check_names.cmake` が、定義のある行を正規表現で探し、同じ名前のヘッダの宣言と照らす | ライブラリに `-Wmissing-prototypes` を付け、スクリプトからこの規則を消す | gcc と clang で今のライブラリに警告が出ず、残った定義を実際に捕まえる（確かめた）。書き方の前提に頼らない |
| B. 名前の検査の残り | 接頭辞（内部ヘッダの関数だけ）、`*_internal.h` の禁止、宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無 | 接頭辞と `*_internal.h` の禁止だけを残す。接頭辞の検査は、公開ヘッダの関数にも広げる（2.1 の規則 1）。型、列挙子、マクロは見ない | 名前は概念に付ける、という規則の核はこの 2 つである。残りは 40 ファイルほどの規模ならレビューで足り、スクリプトと fixture の量に見合わない |
| C. `process/env_impl.h` | `clopts_env_reserve_value` と `_reserve_wide` の 2 つだけを持つ | `process/env.h` に戻し、`env_impl.h` を消す | 実装ヘッダを置く 2 つの場面（埋め込み、OS ごとの実装に構造体を見せる）のどちらでもない。`env_win32.c` の `static` にすると、どの OS でも動いているバッファのテストが Windows だけになる |
| D. ほかのモジュールに見せる内部ヘッダの列挙 | `check_structure.cmake` の `_shared_headers` | 消す。`_impl.h` を同じモジュールに限る規則は残す | 列挙は 3 項目で、モジュールの依存の向きの検査がすでにある。`_impl.h` の規則は、実装を見せる面を外に漏らさないためのもので、同じ箇所に 1 行で書ける |
| F. `text/write.c` | 公開の入口だけを持ち、`help.c` と `message.c` を呼ぶ | 変えない | 公開ヘッダ `write.h` の関数を同じ名前の `.c` に置く規則に合う。入口の引数の検査を 1 か所にまとめる役もある |
| G. `docs/dev/design.md` の量 | 規則、理由、採らなかった案、参考にした設計 | A から D と 2.1 に合わせて削る。検査の説明は、何を検査するかだけにする | 文書がコードより先に古くなる。規則を減らした分、文書も減らす |

### 2.4 メモリ（E）

使うアロケーター（`allocator.c`）と確保の窓口（`mem.c`）を、1 つの概念 mem（ライブラリのメモリ）にまとめる。外から差し替える面と、ライブラリの中から使う面を、`base/mem.c` が持つ。

| 今 | 目標 | 根拠 |
|----|------|------|
| 公開ヘッダ `clopts/base/allocator.h` | `clopts/base/mem.h` | 公開ヘッダの関数は、同じ名前の `.c` が定義する（`design.md` 3 章の 1）。定義が `mem.c` に移るので、ヘッダも `mem.h` にする |
| `clopts_set_allocator` | `clopts_mem_set_allocator` | 2.1 の規則 1。mem という概念の、外から使える唯一の操作である |
| `clopts_allocator_t` | 変えない | 2.1 の規則 3。利用者が埋めて渡すデータの型で、中身（アロケーター）を表す名前がいちばん分かりやすい |
| `src/base/allocator.c`、`src/base/allocator.h`、`clopts_allocator_get` | なくす（`mem.c` の `static` 変数を直接読む） | 1 つの概念の中の受け渡しに、関数は要らない |
| 内部の `clopts_mem_*` | 変えない | — |

- 概念の名前は `mem` にする。`alloc` だと `clopts_alloc_alloc`、`clopts_alloc_calloc` のように概念と動詞が重なる。`allocator` だと、約 150 か所で使う内部の関数がすべて長くなる。Lua も同じ切り方である（`lmem.c`、`luaM_malloc`）。
- 利用者のコードは `clopts_allocator_t a = {...}; clopts_mem_set_allocator(&a);` になる。型は「何を渡すか」、関数は「どの概念の何の操作か」を表し、それぞれの規則に合う。
- `clopts_set_allocator` を「ライブラリ全体の設定」と見て、今の名前のまま残す読み方もある。その場合は、規則 1 の唯一の例外として、検査に例外の一覧を持たせる（3 章）。

### 2.5 `clopts_file_write`（I）

`clopts/text/write.h` の `static inline int clopts_file_write(void *ctx, const char *data, size_t len)` は、`FILE *` に書く出来合いの `clopts_write_fn` である。関数なので、2.1 の規則 1 に当たる。

| 案 | 呼ぶ側の見え方 | 評価 |
|----|----------------|------|
| `clopts_write_to_file`（推奨） | `clopts_write_help(program, clopts_write_to_file, stdout)` | 規則 1 に合い、「ファイルへ書く」と読める |
| `clopts_write_file` | `clopts_write_help(program, clopts_write_file, stdout)` | 「ファイルを書く」とも読め、`clopts_write_help`（ヘルプを書く）と並べると紛らわしい |
| 今のまま、例外にする | — | `static inline` で、ライブラリの記号ではない。ただ、利用者のコードには `clopts_` の関数として現れ、置き場所を名前から引けない点はほかの関数と同じである |

使う箇所は、`README.md`、`docs/api.md`、`docs/windows.md`、`docs/dev/abi-checks.md`、`CHANGELOG.md`、`examples/` の 2 つ、`tests/text/test_file_write.c`、`tests/text/test_out.c`、`tests/integration/fixtures/consumer/` の 2 つ、`tests/docs/readme_minimal.c` で、機械的に置き換えられる。`tests/text/test_file_write.c` も `test_write_to_file.c` に改名する。

### 2.6 取り下げた案（旧 H1 から H4）

| 旧案 | 取り下げる理由 |
|------|----------------|
| H1. `clopts/base/export.h` を `api.h` に改名する | `CLOPTS_API` はライブラリ全体の印で、規則 3 に当たる。ヘッダの名前は、そのヘッダが何をするか（記号を書き出す設定）で付ける |
| H2. `CLOPTS_PREFIX_*` を `CLOPTS_WRITE_PREFIX_*` にする | `clopts_write_error` の引数のフラグで、規則 3 に当たる。今の名前で意味が通る |
| H3. `clopts_parse_status_t` / `CLOPTS_PARSE_*` を `clopts_result_status_t` / `CLOPTS_RESULT_*` にする | 解析の状態を表す語彙で、規則 3 に当たる。改名の理由は「`result.h` にあるから」だけで、意味の説明にならない。利用者のコードで最もよく書かれる名前（`switch` の `case`）を、その理由で変えない |
| H4. `clopts_source_t` / `CLOPTS_SOURCE_*` を `clopts/parse/source.h` に移す | 規則 3 は置き場所を縛らないので、規則に合わせるために移す必要がない。値の出所を独立した概念として切り出す話は、名前の規則とは別に、要るときに考える |

## 3. 判断が要る点

どれも推奨するが、採らない場合は該当する作業（4 章で印を付けてある）を外す。

| 点 | 推奨 | 採らない場合 |
|----|------|--------------|
| 2.1. 名前の規則を関数に限る | 限る | 前の版のとおり、型、列挙子、マクロにも当てる（旧 H1 から H4 を戻す） |
| A. `-Wmissing-prototypes` に任せる | 任せる | `check_names.cmake` の「定義には宣言が要る」を残す |
| B. 名前の検査を絞る | 絞る | 今の規則をすべて残す |
| C. `env_impl.h` を `env.h` に戻す | 戻す | `env_impl.h` を残す |
| D. `_shared_headers` を消す | 消す | 列挙を残す |
| E1. メモリを 1 つの概念 mem にまとめる（公開ヘッダを `mem.h` にする） | まとめる | `allocator.c` と `allocator.h` を残す |
| E2. `clopts_set_allocator` を `clopts_mem_set_allocator` にする | 改名する | 今の名前のまま、規則 1 の例外として検査に例外の一覧を持たせる |
| I. `clopts_file_write` を `clopts_write_to_file` にする | 改名する | 今の名前のまま、規則 1 の例外とする（E2 と同じく、例外の一覧に入れる） |

E2 と I のどちらかを採らない場合も、公開ヘッダの関数の接頭辞の検査は、例外の一覧を持たせて入れる。

## 4. 作業

### 共通の完了条件

- 静的と共有の両方、Debug と Release で、gcc と clang でビルドでき、`ctest` がすべて通る。
- MinGW のクロスビルドを wine で実行し、Windows 向けの単体テストと構造の検査が通る。
- 公開の名前を変えたら、同じコミットで `abi/clopts.symbols`、`docs/api.md`、`docs/parsing.md`、`docs/windows.md`、`docs/dev/abi-checks.md`、`README.md`、`CHANGELOG.md`、`examples/`、`tests/docs/readme_minimal.c`、`tests/integration/fixtures/consumer/` を直す（`docs.sources`、`docs.api_coverage`、`docs.outputs`、`abi.*`）。

### フェーズ 1: コンパイラの警告（A）

- [ ] `cmake/CloptsCompilerOptions.cmake` で、gcc と clang のライブラリの対象（`clopts` と `clopts_testing`）に `-Wmissing-prototypes` を付ける。テストとサンプルには付けない（Naos のテスト関数は宣言を持たない）。MSVC には同じ警告がないので付けない。
- [ ] `structure.names` から「定義には宣言が要る」を消し、その fixture を直す。

### フェーズ 2: 名前の検査を絞る（B）

- [ ] `check_names.cmake` を、接頭辞の規則と `*_internal.h` の禁止だけにする。接頭辞の規則は、内部ヘッダ（`src/<m>/x.h`、`x_impl.h`）の関数に加え、公開ヘッダ（`include/clopts/<m>/x.h`）の関数（`static inline` を含む）にも当てる。型、列挙子、マクロは見ない。
- [ ] 宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無の規則と、その fixture を消す（`names/bad/internal_first_include`、`names/bad/public_defined_elsewhere` など）。
- [ ] 公開ヘッダの関数の接頭辞の fixture を足す（合わない関数を宣言する公開ヘッダが違反になる。合わないマクロや列挙は違反にならない）。
- [ ] `tests/structure/CMakeLists.txt` の登録を直す。
- [ ] フェーズ 5 と 6 が終わるまで、公開ヘッダの検査は `clopts_set_allocator` と `clopts_file_write` を例外として持つ。採った改名が終わったら、その例外を消す。

### フェーズ 3: `env_impl.h` を戻す（C）

- [ ] `clopts_env_reserve_value` と `_reserve_wide` を `src/process/env.h` に戻し、`src/process/env_impl.h` を消す。
- [ ] `env.c`、`env_win32.c`、`tests/process/test_env.c` の include を直す。

### フェーズ 4: 内部ヘッダの列挙を消す（D）

- [ ] `check_structure.cmake` から `_shared_headers` と「共有されていない」の違反を消す。`_impl.h` をほかのモジュールから include できない規則は残す。
- [ ] fixture `bad/reads_unshared_header` を消す。

### フェーズ 5: メモリ（E1、E2）

- [ ] （E1）`include/clopts/base/allocator.h` を `include/clopts/base/mem.h` に改名する（インクルードガードも `CLOPTS_BASE_MEM_H` に）。`clopts/clopts.h` の include を直す。
- [ ] （E1）`src/base/allocator.c` の中身（標準の関数、使うアロケーター、差し替える関数）を `src/base/mem.c` に移し、`src/base/allocator.c` と `src/base/allocator.h` を消す。`clopts_allocator_get` は消し、使うアロケーターは `mem.c` の中の `static` 変数として直接読む。
- [ ] （E1）内部ヘッダ `src/base/mem.h` は、最初に公開ヘッダ `clopts/base/mem.h` を include する。
- [ ] （E1）`abi/clopts.symbols` の見出し `# clopts/base/allocator.h` を `# clopts/base/mem.h` にする。
- [ ] （E1）`tests/base/test_alloc.c` を `tests/base/test_mem.c` に改名し、スイートの名前も `mem` にする。
- [ ] （E2）`clopts_set_allocator` を `clopts_mem_set_allocator` に改名する。`tests/support/test_alloc.c`（失敗を注入するアロケーター）、`tests/parse/test_oom.c`、`tests/parse/test_fixtures.h` の呼び出しも直す。
- [ ] `clopts_allocator_t` は変えない。
- [ ] `structure` の fixture のうち、`allocator.h` / `allocator.c` を例に使っているもの（`names/good` など）を、フェーズ 2 で残った規則の説明に合う例に直す。

### フェーズ 6: `clopts_file_write`（I）

- [ ] `clopts_file_write` を `clopts_write_to_file` に改名する。2.5 に挙げた箇所をすべて直す。
- [ ] `tests/text/test_file_write.c` を `tests/text/test_write_to_file.c` に改名し、テストの名前も直す。
- [ ] `check_names.cmake` の公開ヘッダの例外の一覧を消す（E2 と I を採った場合）。

### フェーズ 7: 文書（G）

- [ ] `docs/dev/design.md` 2 章の「公開の関数の名前」を、2.1 の規則 1 から 3 で書き直す。規則 1 と 2 の根拠（名前から置き場所が引ける）と、規則 3 の名前に当てない理由（概念の操作ではなく、置き場所を引く必要がない。`CLOPTS_API` はどこで定義されてもよい）を書く。「型に属さない操作は動詞で始める」は消す（`clopts_parse` と `clopts_write_*` は、規則 1 でそのまま説明できる）。コールバックの型（`clopts_x_fn`）と、同じ値を 2 つの型から読めるようにしない規則は残す。
- [ ] `docs/dev/design.md` 3 章の 5 の `export.h` の説明は残す。
- [ ] `docs/dev/design.md` の「内部ヘッダの種類」を直す。表から「ライブラリ内部」の列挙の説明を消し、実装ヘッダの例を Scope と argv の 2 つにする。
- [ ] `docs/dev/design.md` の検査の説明を、何を検査するかだけにする（依存の向き、OS の境界、`_impl.h` の範囲、内部と公開の関数の接頭辞、`*_internal.h`、宣言のない定義はコンパイラ）。
- [ ] `docs/dev/architecture.md` の base の行（`allocator` を消し、`mem` に差し替えの面を書く）、依存の検査の説明、構造体の表を直す。
- [ ] `docs/api.md` の「関数名の付け方」の表を、ヘッダの名前と関数の接頭辞の対応で書き直す。型、列挙、マクロは意味で名付けることも書く。
- [ ] この文書を消す。
