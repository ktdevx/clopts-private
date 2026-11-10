# TODO: 過剰な規則と検査を減らし、名前の規則の範囲を決め直す

この文書は、PR #29 で足した規則と検査のうち、守れる違反の割に重いものを減らす作業と、名前の規則を当てる範囲を決め直す作業の一覧である。機能とモジュールの構成は変えない。公開の名前は変えない。公開ヘッダは 1 つ改名する（E。まだリリースしていないので、互換は保たない。`AGENTS.md`）。

作業がすべて終わったら、この文書を消す。

## 1. 方針

- 規則は、実際に起きた問題か、起きやすい問題を防ぐものだけを残す。
- 検査は、コンパイラにできることはコンパイラに任せ、CMake のスクリプトには、コンパイラにできないことだけを残す。
- 規則は、その根拠が効く範囲にだけ当てる。例外をなくすために、根拠の外まで広げない。

3 つ目は、前の 2 つの版のこの文書への反省である。

- 最初の版は「ヘッダ `x.h` が宣言する名前は、関数も型も列挙子もマクロも `clopts_x` / `CLOPTS_X` で始まる。例外を置かない」とした。`export.h` を `api.h` に、`clopts_parse_status_t` を `clopts_result_status_t` に改名するなどの案を出していた。これは、例外がないこと自体を目的にしていた。`CLOPTS_API` のようなライブラリ全体の印は、どのヘッダで定義されてもおかしくない。
- 次の版は、規則を「関数」に絞った。`clopts_set_allocator` を `clopts_mem_set_allocator` に、`clopts_file_write` を `clopts_write_to_file` に改名する案を出していた。これもまだ広すぎた。型に属さない関数は、`CLOPTS_API` と同じく、置き場所を名前から引く必要がない。

取り下げた案は 2.5 にまとめる。

## 2. 見直す点

### 2.1 公開の名前の規則

「ヘッダ `x.h` の名前は `clopts_x_*` で始まる」という規則の根拠は、名前から置き場所が引けることである。この根拠が効くのは、型と、その型の操作の組だけである。`clopts_result_get_status` を見れば、`result.h` の型 `clopts_result_t` の操作だと分かる。

利用者はふつう `clopts/clopts.h` だけを include する。利用者にとって、公開の名前は語彙であり、置き場所の手がかりではない。型に属さない名前は、何をするかが読めれば足りる。

| 種類 | 例 | 名前の付け方 |
|------|----|--------------|
| 不透明な型とその操作 | `clopts_result_t`、`clopts_result_get_status`、`clopts_option_create` | 型は `x.h` が宣言し、操作は `clopts_x_<動詞>` とする |
| 型に属さない関数 | `clopts_parse`、`clopts_write_help`、`clopts_set_allocator`、`clopts_file_write` | 何をするかで付ける。置き場所は縛らない |
| ライブラリ全体の印 | `CLOPTS_API` | 役割で付ける |
| 引数のフラグ | `CLOPTS_PREFIX_PROGRAM`、`CLOPTS_PREFIX_ERROR` | 引数の意味で付ける |
| 語彙としての列挙 | `clopts_parse_status_t`、`clopts_source_t` | 表す意味で付ける |
| 利用者が埋めて渡すデータの型 | `clopts_allocator_t` | 中身で付ける |
| コールバックの型 | `clopts_write_fn`、`clopts_env_fn` | 呼ぶ側の概念の名前に `_fn` を付ける（今の規則のまま） |

どれも `clopts_` / `CLOPTS_` で始め、ライブラリの名前空間には入れる。

- `clopts_parse` と `clopts_write_help` は、今たまたまヘッダの名前（`parse.h`、`write.h`）と一致している。意味で付けた結果であり、規則で縛っているからではない。
- 公開ヘッダの置き場所は、名前ではなく、定義の置き場所で決める（`design.md` 3 章の 1。公開ヘッダ `x.h` の関数は `src/<モジュール>/x.c` が定義する）。この規則は変えない。

今の公開の名前は、すべてこの規則に合う。改名は要らない。

### 2.2 内部の名前の規則

内部の関数は、今のとおり、宣言するヘッダの名前を接頭辞にする（`src/<モジュール>/x.h` と `x_impl.h` の関数は `clopts_x_*`）。型に属さない関数（`clopts_utf8_valid`、`clopts_mem_alloc`、`clopts_scan`）にも当てる。

公開の名前と根拠が違う。

- 内部の名前は、開発者がファイルをたどるためのものである。`clopts_utf8_valid` を見て `utf8.c` を開けることには、実際の価値がある。内部には `clopts.h` のような全部入りのヘッダがなく、どのヘッダを include するかを毎回選ぶ。
- 名前を付ける側と読む側が同じ開発者なので、縛っても負担が小さい。今の内部の関数は、すべて合っている。

公開の規則を内部の都合で決めず、内部の規則を公開の都合で緩めない。

### 2.3 規則と検査

| 点 | 今 | 推奨 | 理由 |
|----|----|------|------|
| A. 宣言のない定義 | `check_names.cmake` が、定義のある行を正規表現で探し、同じ名前のヘッダの宣言と照らす | ライブラリに `-Wmissing-prototypes` を付け、スクリプトからこの規則を消す | gcc と clang で今のライブラリに警告が出ず、残った定義を実際に捕まえる（確かめた）。書き方の前提に頼らない |
| B. 名前の検査の残り | 内部の関数の接頭辞、`*_internal.h` の禁止、宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無 | 内部の関数の接頭辞と `*_internal.h` の禁止だけを残す。公開ヘッダの名前は検査しない | 2.2 の規則の核はこの 2 つである。2.1 の公開の規則は、型とその操作の組の名前の付け方で、検査するほど崩れやすくない。残りは 40 ファイルほどの規模ならレビューで足り、スクリプトと fixture の量に見合わない |
| C. `process/env_impl.h` | `clopts_env_reserve_value` と `_reserve_wide` の 2 つだけを持つ | `process/env.h` に戻し、`env_impl.h` を消す | 実装ヘッダを置く 2 つの場面（埋め込み、OS ごとの実装に構造体を見せる）のどちらでもない。`env_win32.c` の `static` にすると、どの OS でも動いているバッファのテストが Windows だけになる |
| D. ほかのモジュールに見せる内部ヘッダの列挙 | `check_structure.cmake` の `_shared_headers` | 消す。`_impl.h` を同じモジュールに限る規則は残す | 列挙は 3 項目で、モジュールの依存の向きの検査がすでにある。`_impl.h` の規則は、実装を見せる面を外に漏らさないためのもので、同じ箇所に 1 行で書ける |
| F. `text/write.c` | 公開の入口だけを持ち、`help.c` と `message.c` を呼ぶ | 変えない | 公開ヘッダ `write.h` の関数を同じ名前の `.c` に置く規則に合う。入口の引数の検査を 1 か所にまとめる役もある |
| G. `docs/dev/design.md` の量 | 規則、理由、採らなかった案、参考にした設計 | A から D と 2.1、2.2 に合わせて削る。検査の説明は、何を検査するかだけにする | 文書がコードより先に古くなる。規則を減らした分、文書も減らす |

### 2.4 メモリ（E）

使うアロケーター（`allocator.c`）と確保の窓口（`mem.c`）を、1 つの概念 mem（ライブラリのメモリ）にまとめる。外から差し替える面と、ライブラリの中から使う面を、`base/mem.c` が持つ。名前の規則のためではなく、1 つの概念を 1 つの `.c` に置くためである。

| 今 | 目標 | 根拠 |
|----|------|------|
| `src/base/allocator.c`、`src/base/allocator.h`、`clopts_allocator_get` | なくす。中身を `src/base/mem.c` に移し、使うアロケーターは `mem.c` の `static` 変数として直接読む | 1 つの概念の中の受け渡しに、関数とヘッダは要らない |
| 公開ヘッダ `clopts/base/allocator.h` | `clopts/base/mem.h` | 公開ヘッダ `x.h` の関数は、同じ名前の `.c` が定義する（`design.md` 3 章の 1）。`clopts_set_allocator` の定義が `mem.c` に移るので、ヘッダも `mem.h` にする |
| `clopts_set_allocator` | 変えない | 2.1。ライブラリ全体の設定で、型に属さない |
| `clopts_allocator_t` | 変えない | 2.1。利用者が埋めて渡すデータの型で、中身を表す名前がいちばん分かりやすい |
| 内部の `clopts_mem_*` | 変えない | 2.2 |

- 概念の名前は `mem` にする。`alloc` だと `clopts_alloc_alloc`、`clopts_alloc_calloc` のように概念と動詞が重なる。`allocator` だと、約 150 か所で使う内部の関数がすべて長くなる。Lua も同じ切り方である（`lmem.c`、`luaM_malloc`）。
- 内部ヘッダ `src/base/mem.h` は、最初に公開ヘッダ `clopts/base/mem.h` を include する（`design.md` 3 章の 2）。
- 利用者のコードは変わらない。`clopts.h` を include していれば、ヘッダの改名も見えない。

### 2.5 取り下げた案

| 案 | 取り下げる理由 |
|----|----------------|
| `clopts/base/export.h` を `api.h` に改名する | `CLOPTS_API` はライブラリ全体の印で、型に属さない。ヘッダの名前は、そのヘッダが何をするか（記号を書き出す設定）で付ける |
| `CLOPTS_PREFIX_*` を `CLOPTS_WRITE_PREFIX_*` にする | `clopts_write_error` の引数のフラグで、型に属さない。今の名前で意味が通る |
| `clopts_parse_status_t` / `CLOPTS_PARSE_*` を `clopts_result_status_t` / `CLOPTS_RESULT_*` にする | 解析の状態を表す語彙である。改名の理由は「`result.h` にあるから」だけで、意味の説明にならない。利用者のコードで最もよく書かれる名前（`switch` の `case`）を、その理由で変えない |
| `clopts_source_t` / `CLOPTS_SOURCE_*` を `clopts/parse/source.h` に移す | 公開の規則は、型に属さない名前の置き場所を縛らない。値の出所を独立した概念として切り出す話は、名前の規則とは別に、要るときに考える |
| `clopts_allocator_t` を `clopts_mem_allocator_t` にする | 利用者が埋めて渡すデータの型で、型の操作の組ではない |
| `clopts_set_allocator` を `clopts_mem_set_allocator` にする | ライブラリ全体の設定で、型に属さない。「アロケーターを設定する」と読めれば足りる |
| `clopts_file_write` を `clopts_write_to_file` にする | `FILE *` に書く出来合いのコールバックで、型に属さない。名前は役割を表している |
| 公開ヘッダの名前の接頭辞を検査する | 2.1 の公開の規則は、型に属さない名前を縛らないので、検査する対象がほとんど残らない |

## 3. 判断が要る点

どれも推奨するが、採らない場合は該当する作業（4 章で印を付けてある）を外す。

| 点 | 推奨 | 採らない場合 |
|----|------|--------------|
| 2.1. 公開の名前の規則を、型とその操作の組に限る | 限る | 関数すべて、または名前すべてに当てる（2.5 の改名を戻す） |
| 2.2. 内部の名前は、型に属さない関数もヘッダの接頭辞にする | 今のとおり当てる | 公開と同じく、型とその操作の組に限る |
| A. `-Wmissing-prototypes` に任せる | 任せる | `check_names.cmake` の「定義には宣言が要る」を残す |
| B. 名前の検査を絞る | 絞る | 今の規則をすべて残す |
| C. `env_impl.h` を `env.h` に戻す | 戻す | `env_impl.h` を残す |
| D. `_shared_headers` を消す | 消す | 列挙を残す |
| E. メモリを 1 つの概念 mem にまとめる（公開ヘッダを `mem.h` にする） | まとめる | `allocator.c` と `allocator.h` を残す |

## 4. 作業

### 共通の完了条件

- 静的と共有の両方、Debug と Release で、gcc と clang でビルドでき、`ctest` がすべて通る。
- MinGW のクロスビルドを wine で実行し、Windows 向けの単体テストと構造の検査が通る。
- 公開ヘッダを変えたら、同じコミットで、それを書いている文書、サンプル、テストを直す（`docs.sources`、`docs.api_coverage`、`docs.outputs`、`abi.*`）。

### フェーズ 1: コンパイラの警告（A）

- [ ] `cmake/CloptsCompilerOptions.cmake` で、gcc と clang のライブラリの対象（`clopts` と `clopts_testing`）に `-Wmissing-prototypes` を付ける。テストとサンプルには付けない（Naos のテスト関数は宣言を持たない）。MSVC には同じ警告がないので付けない。
- [ ] `structure.names` から「定義には宣言が要る」を消し、その fixture を直す。

### フェーズ 2: 名前の検査を絞る（B）

- [ ] `check_names.cmake` を、内部の関数の接頭辞の規則（`src/<m>/x.h` と `x_impl.h` の関数は `clopts_x_*`）と `*_internal.h` の禁止だけにする。公開ヘッダは見ない。
- [ ] 宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無の規則と、その fixture を消す（`names/bad/internal_first_include`、`names/bad/public_defined_elsewhere` など）。
- [ ] `tests/structure/CMakeLists.txt` の登録を直す。

### フェーズ 3: `env_impl.h` を戻す（C）

- [ ] `clopts_env_reserve_value` と `_reserve_wide` を `src/process/env.h` に戻し、`src/process/env_impl.h` を消す。
- [ ] `env.c`、`env_win32.c`、`tests/process/test_env.c` の include を直す。

### フェーズ 4: 内部ヘッダの列挙を消す（D）

- [ ] `check_structure.cmake` から `_shared_headers` と「共有されていない」の違反を消す。`_impl.h` をほかのモジュールから include できない規則は残す。
- [ ] fixture `bad/reads_unshared_header` を消す。

### フェーズ 5: メモリ（E）

- [ ] `src/base/allocator.c` の中身（標準の関数、使うアロケーター、`clopts_set_allocator`）を `src/base/mem.c` に移し、`src/base/allocator.c` と `src/base/allocator.h` を消す。`clopts_allocator_get` は消し、使うアロケーターは `mem.c` の中の `static` 変数として直接読む。
- [ ] `include/clopts/base/allocator.h` を `include/clopts/base/mem.h` に改名する（インクルードガードも `CLOPTS_BASE_MEM_H` に）。`clopts/clopts.h` の include を直す。
- [ ] 内部ヘッダ `src/base/mem.h` は、最初に公開ヘッダ `clopts/base/mem.h` を include する。
- [ ] `abi/clopts.symbols` の見出し `# clopts/base/allocator.h` を `# clopts/base/mem.h` にする。
- [ ] `tests/base/test_alloc.c` を `tests/base/test_mem.c` に改名し、スイートの名前も `mem` にする。
- [ ] `allocator.h` を名前で書いている文書（`README.md`、`docs/api.md`、`docs/dev/architecture.md`、`CHANGELOG.md`）を直す。
- [ ] `structure` の fixture のうち、`allocator.h` / `allocator.c` を例に使っているもの（`names/good` など）を、フェーズ 2 で残った規則に合う例に直す。

### フェーズ 6: 文書（G）

- [ ] `docs/dev/design.md` 2 章の「公開の関数の名前」を、「名前の付け方」として 2.1 と 2.2 で書き直す。
  - 公開：型とその操作の組は `x.h` と `clopts_x_<動詞>`。それ以外（型に属さない関数、マクロ、列挙、渡すデータの型）は意味で名付け、置き場所は縛らない。根拠（利用者にとって公開の名前は語彙で、置き場所の手がかりではない）を書く。
  - 内部：関数はヘッダの名前を接頭辞にする。根拠（開発者がファイルをたどる）を書く。
  - 「型に属さない操作は動詞で始める」は、「何をするかで付ける」に置き換える。
  - コールバックの型（`clopts_x_fn`）と、同じ値を 2 つの型から読めるようにしない規則は残す。
- [ ] `docs/dev/design.md` 3 章の 5 の `export.h` の説明は残す。
- [ ] `docs/dev/design.md` の「内部ヘッダの種類」を直す。表から「ライブラリ内部」の列挙の説明を消し、実装ヘッダの例を Scope と argv の 2 つにする。
- [ ] `docs/dev/design.md` の検査の説明を、何を検査するかだけにする（依存の向き、OS の境界、`_impl.h` の範囲、内部の関数の接頭辞、`*_internal.h`、宣言のない定義はコンパイラ）。
- [ ] `docs/dev/architecture.md` の base の行（`allocator` を消し、`mem` に差し替えの面を書く）、依存の検査の説明、構造体の表を直す。
- [ ] `docs/api.md` の「関数名の付け方」の表を、2.1 に合わせて直す（型の操作は `clopts_<型>_<動詞>`、型に属さない関数は何をするかで付ける）。
- [ ] この文書を消す。
