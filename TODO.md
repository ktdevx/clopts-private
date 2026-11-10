# TODO: 過剰な規則と検査を減らす

この文書は、PR #29 で足した規則と検査のうち、守れる違反の割に重いものを減らす作業の一覧である。機能、公開 API、モジュールの構成は変えない。

作業がすべて終わったら、この文書を消す。

## 1. 方針

規則は、実際に起きた問題か、起きやすい問題を防ぐものだけを残す。検査は、コンパイラにできることはコンパイラに任せ、CMake のスクリプトには、コンパイラにできないことだけを残す。

## 2. 見直す点

| 点 | 今 | 推奨 | 理由 |
|----|----|------|------|
| A. 宣言のない定義 | `check_names.cmake` が、定義のある行を正規表現で探し、同じ名前のヘッダの宣言と照らす | ライブラリに `-Wmissing-prototypes` を付け、スクリプトからこの規則を消す | gcc と clang で今のライブラリに警告が出ず、残った定義を実際に捕まえる。書き方の前提に頼らない |
| B. 名前の検査の残り | 接頭辞、`*_internal.h` の禁止、宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無 | 接頭辞と `*_internal.h` の禁止だけを残す | 名前は概念に付ける、という規則の核はこの 2 つである。残りは 40 ファイルほどの規模ならレビューで足り、スクリプトと fixture の量に見合わない |
| C. `process/env_impl.h` | `clopts_env_reserve_value` と `_reserve_wide` の 2 つだけを持つ | `process/env.h` に戻し、`env_impl.h` を消す | 中身は env.c と env_win32.c とテストが使う関数 2 つで、実装ヘッダを置く 2 つの場面（埋め込み、OS ごとの実装に構造体を見せる）のどちらでもない。`env_win32.c` の `static` にすると、どの OS でも動いているバッファのテストが Windows だけになる |
| D. ほかのモジュールに見せる内部ヘッダの列挙 | `check_structure.cmake` の `_shared_headers`（base はすべて、decl は `lookup.h`、process は `env.h`） | 消す。`_impl.h` を同じモジュールに限る規則は残す | 列挙は 3 項目で、モジュールの依存の向きの検査がすでにある。`_impl.h` の規則は、実装を見せる面を外に漏らさないためのもので、同じ箇所に 1 行で書ける |
| E. `allocator.c` と `mem.c` | 公開ヘッダ `allocator.h` に合わせて分けている | 変えない | まとめると、公開ヘッダと同じ名前の `.c` で定義する規則か、`clopts_mem_*` の接頭辞の規則のどちらかが崩れる。分けたことの代償はファイル 1 つである |
| F. `text/write.c` | 公開の入口だけを持ち、`help.c` と `message.c` を呼ぶ | 変えない | 公開ヘッダ `write.h` の関数を同じ名前の `.c` に置く規則に合う。入口の引数の検査を 1 か所にまとめる役もある |
| G. `docs/dev/design.md` の量 | 規則、理由、採らなかった案、参考にした設計 | A から D に合わせて削る。検査の説明は、何を検査するかだけにする | 文書がコードより先に古くなる。規則を減らした分、文書も減らす |

## 3. 判断が要る点

2 章の推奨のうち、A から D は検査と規則を減らす。どれも推奨するが、採らない場合は該当する作業（4 章で印を付けてある）を外す。

| 点 | 推奨 | 採らない場合 |
|----|------|--------------|
| A. `-Wmissing-prototypes` に任せる | 任せる | `check_names.cmake` の「定義には宣言が要る」を残す |
| B. 名前の検査を 2 つに絞る | 絞る | 今の規則をすべて残す |
| C. `env_impl.h` を `env.h` に戻す | 戻す | `env_impl.h` を残す |
| D. `_shared_headers` を消す | 消す | 列挙を残す |

## 4. 作業

### 共通の完了条件

- 静的と共有の両方、Debug と Release で、gcc と clang でビルドでき、`ctest` がすべて通る。
- MinGW のクロスビルドを wine で実行し、Windows 向けの単体テストと構造の検査が通る。
- 公開 API と `abi/clopts.symbols` は変わらない。

### フェーズ 1: コンパイラの警告（A）

- [ ] `cmake/CloptsCompilerOptions.cmake` で、gcc と clang のライブラリの対象（`clopts` と `clopts_testing`）に `-Wmissing-prototypes` を付ける。テストとサンプルには付けない（Naos のテスト関数は宣言を持たない）。MSVC には同じ警告がないので付けない。
- [ ] `structure.names` から「定義には宣言が要る」を消し、その fixture（`names/bad/undeclared`、`defined_elsewhere` と `public_defined_elsewhere` の 2 件目の違反）を直す。

### フェーズ 2: 名前の検査を絞る（B）

- [ ] `check_names.cmake` を、接頭辞の規則（`src/<m>/x.h` と `x_impl.h` の関数は `clopts_x` か `clopts_x_*`）と、`*_internal.h` の禁止だけにする。
- [ ] 宣言と定義のファイルの対応、公開関数の定義の置き場所、最初の include、`_impl.h` の `.c` の有無の規則と、その fixture を消す。
- [ ] `tests/structure/CMakeLists.txt` の登録を直す。

### フェーズ 3: `env_impl.h` を戻す（C）

- [ ] `clopts_env_reserve_value` と `_reserve_wide` を `src/process/env.h` に戻し、`src/process/env_impl.h` を消す。
- [ ] `env.c`、`env_win32.c`、`tests/process/test_env.c` の include を直す。

### フェーズ 4: 内部ヘッダの列挙を消す（D）

- [ ] `check_structure.cmake` から `_shared_headers` と「共有されていない」の違反を消す。`_impl.h` をほかのモジュールから include できない規則は残す。
- [ ] fixture `bad/reads_unshared_header` を消す。

### フェーズ 5: 文書（G）

- [ ] `docs/dev/design.md` の「内部ヘッダの種類」を直す。表から「ライブラリ内部」の列挙の説明を消し、実装ヘッダの例を Scope と argv の 2 つにする。
- [ ] `docs/dev/design.md` の検査の説明を、何を検査するかだけにする（依存の向き、OS の境界、`_impl.h` の範囲、接頭辞、`*_internal.h`、宣言のない定義はコンパイラ）。
- [ ] `docs/dev/architecture.md` の依存の検査の説明と、構造体の表の `env` の行を直す。
- [ ] この文書を消す。
