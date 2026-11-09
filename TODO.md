# TODO: グローバルアロケーターの導入と確保プローブの撤去

## 背景

いまは、確保の失敗を試すための確保プローブ（`clopts_parser_probe_t` / `clopts_platform_probe_t`）が `src/` 側にある。

- 窓口: `src/parser/parser_alloc.c`、`src/platform/platform_alloc.c`
- 構造体のフィールド: `program->probe`、`result->probe`、`env->probe`
- テスト専用の内部の口: `clopts_parser_set_probe()`、`clopts_platform_env_new(probe)`、`clopts_platform_argv_*(probe, ...)`

本番では常に NULL だが、ほぼすべての内部関数が `probe` を引数として受け回しており、製品コードにテスト用のものが混ざっている。また、プローブを通らない確保がある。

- `clopts_program_new` の `malloc` / `free`（`src/parser/parser_build.c:309`, `:361`）
- `core_vec` の `realloc` / `free`（`src/core/core_vec.c:45`, `:88`）

## 目的

- ライブラリ全体で 1 つの、差し替えられるアロケーター（グローバル）を公開 API として持つ。
- 失敗の注入と生存ブロックの計数は、`tests/` 側のアロケーターで行う。`src/` からテスト専用のものをなくす。
- ライブラリの確保をすべて、例外なく、アロケーター経由にする。内部関数から `probe` の引数がなくなる。

## 方針

### 公開 API

```c
/* include/clopts/alloc.h（新規） */
typedef struct clopts_allocator
{
    void *(*malloc_fn)(size_t size);
    void *(*calloc_fn)(size_t count, size_t size);
    void *(*realloc_fn)(void *ptr, size_t size);
    void (*free_fn)(void *ptr);
} clopts_allocator_t;  /* 凍結: メンバーの追加・削除・並べ替え・型の変更をしない */

/* allocator の 4 つの関数を写して使う。NULL なら malloc / calloc / realloc / free に戻す */
CLOPTS_API void clopts_set_allocator(const clopts_allocator_t *allocator);
```

- 各関数の引数と意味は、標準の `malloc` / `calloc` / `realloc` / `free` と同じにする。`ctx` は持たない。
- メンバー名に `_fn` を付ける。利用側やライブラリの中で `malloc` などがマクロになっていても（MSVC の `_CRTDBG_MAP_ALLOC` など）、メンバーの宣言と呼び出しが展開されないようにするため。
- 既定に戻すときは `clopts_set_allocator(NULL)` を呼ぶ。いまの設定を読む関数（`clopts_get_allocator`）は持たない。
- 構造体はポインタで渡し、ライブラリは値を写して持つ。呼び出しの後に利用側の構造体を変えても、解放しても影響しない。
- 4 つすべてが非 NULL であること。1 つでも NULL なら、契約違反として `assert` で捕まえる。混ぜて使うと、ある関数で確保したものを別の系統の関数で解放することになるため、項目ごとに既定へ戻すことはしない。
- ライブラリが関数をどう使うか:
  - `malloc_fn`: 1 ブロックの確保と文字列の複製（`strdup` / `strndup` はライブラリの中で `malloc_fn` と `memcpy` で作る）。
  - `calloc_fn`: いま `calloc` を使っているところ（スロットの配列など）。`count * size` のオーバーフローは、呼ぶ前にライブラリが検査する。
  - `realloc_fn`: vec を伸ばすときだけ。失敗したときは、元のブロックが残っている前提で扱う（標準の `realloc` と同じ）。
  - `free_fn`: NULL を渡さない。
  - サイズ 0 では呼ばない（ライブラリが 1 に丸める）。そのため NULL は必ず失敗を意味する。
- **設定してよい時機**: ライブラリが確保したもの（プログラム、結果、argv のハンドル、環境変数の文脈）が 1 つも生きていないときだけ。生きている間に変えると、別のアロケーターで解放されてしまうので、契約違反とする。通常は `main` の最初で 1 回呼ぶ。
- **スレッド**: `clopts_set_allocator` は同期しない。ほかのスレッドがライブラリを使っていない間に呼ぶ。設定した関数そのものは、利用側でスレッド安全にする。
- 番兵（メモリ不足の結果、argv の番兵）はこれまでどおり静的オブジェクトとし、アロケーターとは関係しない。
- CRT の境界の規則（「ライブラリが確保したものはライブラリの関数で解放する」）は変わらない。ライブラリは、設定された `free_fn` で解放する。

### 公開ヘッダの構造体の扱い

tech.md の「公開ヘッダに構造体の定義を置かない」という規則は廃止する。代わりに、次の規則にする。

- 公開ヘッダに定義した構造体は凍結する。定義した後は、メンバーの追加・削除・並べ替え・型の変更をしない。拡張が必要になったら、別の型と別の関数（例: `clopts_allocator2_t` と `clopts_set_allocator2`）を足す。
- 既存の公開構造体の配置を変えることは、互換を壊す変更とする。新しい構造体を足すことは、互換を保つ変更とする。
- 1.0.0 以降は、abidiff が配置の変化を検出する（既存の abi-compat の検査）。
- 「構造体を値で受け渡さない」は残す（`clopts_set_allocator` もポインタで受け取る）。

### 内部

- `src/core/core_alloc.c` に、現在のアロケーター（`static clopts_allocator_t`、初期値は標準関数）と、`clopts_set_allocator` を置く。
- `core_internal.h` に窓口を宣言する: `clopts_core_malloc`、`clopts_core_calloc`、`clopts_core_realloc`、`clopts_core_strdup`、`clopts_core_strndup`、`clopts_core_free`、`clopts_core_malloc_array`。窓口でサイズ 0 の丸めとオーバーフローの検査を行う。
- 窓口は引数を増やさない（`clopts_core_malloc(size)` の形）。ハンドルにアロケーターを持たせる必要もない。

## 決定事項

- 生きているハンドルがある間の `clopts_set_allocator` の呼び出しは、デバッグビルドでも検出しない。生きているハンドルを数えるには、グローバルな計数器をスレッドを考えて更新しなければならないため。文書の契約にとどめる。
- `clopts_get_allocator` は持たない。既定に戻すには `clopts_set_allocator(NULL)` を使う。
- ヘッダは新しく `include/clopts/alloc.h` を作り、`clopts.h` から取り込む。
- メンバー名は `malloc_fn` / `calloc_fn` / `realloc_fn` / `free_fn`（標準関数のマクロ置き換えと衝突しないように `_fn` を付ける）。
- 「公開ヘッダに構造体の定義を置かない」は廃止し、「公開構造体は凍結する」に置き換える。

## TODO

### 0. 仕様とsteering（Kiro の文書は更新するが、Kiro の手順・コマンドは使わない）

- [x] `.kiro/steering/tech.md`:
  - 64 行目の「`malloc` / `calloc` / `realloc` / `free` を直接使い」を、「確保は core の窓口を通す（窓口が、設定されたアロケーターを呼ぶ）」に改める。
  - 「公開ヘッダに構造体の定義を置かない」を削除し、「公開ヘッダに定義した構造体は凍結する（拡張は別の型を足す）」に置き換える。「構造体を値で受け渡さない」は残す。
- [ ] `.kiro/specs/core-parser/`（requirements / design / tasks）: 確保プローブの記述をアロケーターに置き換える（design.md の 29, 41, 58, 127, 137, 547, 568–591 行あたり、requirements の 1.8, 13.5, 13.7, 15.7 に関わる箇所）。
- [ ] `.kiro/specs/platform-argv/`: プローブと窓口の記述を置き換える。
- [ ] `.kiro/specs/abi-compat/`（brief / requirements / design / tasks）:
  - requirements 4.2 と 4.3（構造体・共用体の定義があれば失敗、前方宣言は許す）を削除する。ヘッダの検査は `CLOPTS_API` の付け漏れと、C / C++ でのコンパイルだけにする。
  - 壊す変更の一覧（requirements 7）の「公開ヘッダへの構造体・共用体の定義の追加」を、「既存の公開構造体・共用体の配置の変更（メンバーの追加・削除・並べ替え・型の変更）」に置き換える。
  - brief と requirements の冒頭の概要にある、構造体の定義の検査の記述も直す。
- [ ] `.kiro/specs/project-foundation/` に core のファイル構成の記述があれば、`core_alloc.c` を書き足す。
- [ ] 各 spec の `spec.json` の `updated_at` を更新する。

### 1. 公開ヘッダ

- [x] `include/clopts/alloc.h` を新しく作り、`clopts_allocator_t` と `clopts_set_allocator` を宣言する。`clopts.h` から取り込む。
- [x] `tests/abi/check_header_rules.cmake`: 構造体・共用体の定義の検査を削除する（ファイル冒頭の説明、155–156 行の判定、263 行の報告文）。この検査のための例 `tests/abi/fixtures/header_rules/bad/struct_def/`・`bad/union_def/` と、それを使う `tests/abi/CMakeLists.txt` のテスト（158–162 行あたり）を削除する。構造体の定義を含むヘッダが通ることを、`good/` に例を足して確かめる。
- [x] ヘッダの C99 / C++ のコンパイル検査が通ることを確かめる。

### 2. core

- [x] `src/core/core_alloc.c` を追加し、`src/core/CMakeLists.txt` に登録する。
- [x] `core_vec.c` の `realloc` / `free` を、`clopts_core_realloc` / `clopts_core_free` に置き換える。vec の関数の引数は変えない。
- [x] `tests/core/` に窓口のテスト（サイズ 0 の丸め、オーバーフローの検査、`strndup` の終端、設定した関数が呼ばれること、NULL で既定に戻ること）を追加する。

### 3. parser

- [x] `parser_alloc.c` を削除し、`src/parser/CMakeLists.txt` から外す。
- [x] `parser_internal.h` から `clopts_parser_probe_t`、`clopts_parser_set_probe`、窓口の宣言を削除する。
- [x] `program->probe` / `result->probe` のフィールドと、番兵の初期化子の `.probe` を削除する。
- [x] `parser_build.c` / `parser_scan.c` / `parser_resolve.c` / `parser_result.c`: `probe` の引数をなくし、`clopts_parser_*` の窓口の呼び出しを `clopts_core_*` に置き換える。
- [x] `clopts_program_new` の 1 ブロックも、`clopts_core_malloc` / `clopts_core_free` で確保・解放する。

### 4. platform

- [x] `platform_alloc.c` を削除する（`malloc_array` は core の窓口に移す）。
- [x] `platform_internal.h` から `clopts_platform_probe_t` と窓口の宣言を削除し、`env->probe` を削除する。
- [x] `clopts_platform_env_new(probe)` は、公開の `clopts_env_new()` に統合する。`clopts_platform_argv_*` からも `probe` の引数を取り除く。
- [x] `platform_env.c` / `platform_argv.c` / `platform_win32.c` の呼び出しを、`clopts_core_*` に置き換える。

### 5. テスト

- [x] `tests/support/test_alloc.{c,h}` を追加する。失敗を注入するアロケーターで、`fail_at`、`calls`、`live` を静的な変数に持つ（`ctx` がないため）。各ブロックに目印を付け、自分が確保していないポインタの解放と二重解放を検出する。`realloc` が失敗したときに元のブロックが残ることも試せるようにする。
- [x] 支援用の関数: `test_alloc_begin(fail_at)`（計数を初期化して `clopts_set_allocator`）と `test_alloc_end()`（失敗をやめ、生存ブロック 0・不正な解放 0・大きさ 0 の要求 0 を確かめる）。実施では、途中で止まったテストのブロックも同じアロケーターで解放されるよう、`test_alloc_end()` は既定に戻さないことにした（案の `install` / `uninstall` から変更）。
- [x] parser / platform / core のテストで共有する。実施では静的ライブラリにせず、各テスト実行ファイルのソースに `../support/test_alloc.c` を加え、`clopts_add_module_tests` でインクルードの場所を足した。
- [x] `tests/parser/test_oom.c`（21 件）と `tests/platform/test_oom.c`（13 件）: 窓口を直接試すテストは `tests/core/` に移す。スイープは、公開 API と `test_alloc_install` を使う形に書き直す。
- [x] `test_build.c`、`test_dispatch.c`、`test_resolve.c`、`test_result.c`、`test_fixtures.{c,h}`（parser）と、`test_argv.c`、`test_convert.c`、`test_env.c`（platform）から、`set_probe` / `probe_t` の利用を取り除く。
- [x] 失敗する位置の数が変わることへの対応: `vec_push` は、伸ばさないときには失敗しなくなる。`strdup(NULL)` も確保しない。スイープは、固定の回数ではなく「`fail_at = n` で最後まで成功し、かつ `calls < n` になったら終わる」という条件で回す。
- [x] 新しく対象になった、`clopts_program_new` の確保と vec の `realloc` の失敗を試す。
- [x] 4 つのうち 1 つが NULL のときの `assert` は、death test が書けなければ試さず、文書の契約にとどめる（実施: 試さず、文書の契約にとどめた）。
- [x] OOM のスイープは公開 API だけで書けるので、共有ビルドでも `CLOpts::clopts` に対して流せる。可能なら、そのように構成する（実施: 見送り。スイープは内部関数を使うテストと同じ実行ファイルにあるので、共有ビルドではこれまでどおり `clopts_testing` に対して流す）。

### 6. ABI・文書・変更履歴

- [x] `abi/clopts.symbols` に `clopts_set_allocator` を追加する。
- [x] `docs/abi.md`: 74 行目（構造体の定義の追加は互換を壊す）を、「既存の公開構造体の配置の変更は互換を壊す。拡張するときは別の型を足す」に置き換える。ヘッダの検査の説明があれば、構造体の検査を削除する。
- [x] `docs/api.md`: アロケーターの節を追加する（4 つの関数の契約、設定してよい時機、スレッド、サイズ 0 と NULL の扱い、番兵との関係）。519 行目の「解放」の規則に、アロケーターとの関係を書き足す。
- [x] 文書の検査（ラベル `docs`、`tests/docs/check_docs.cmake`）が、シンボルの一覧と文書を照合するので、新しいシンボルが文書に載っていることを確かめる。
- [x] `CHANGELOG.md` の `## [Unreleased]` / `### 追加` に、アロケーターの API を書く。0.1.0 は未リリースなので、互換性のない変更の節は要らない。
- [x] 必要なら、README か `examples/` にアロケーターの短い例を足す（実施: 例は足さず、README の特徴の一覧に 1 項目を足した）。

### 7. 後片付けと確認

- [x] `grep -rnE "probe|\b(malloc|calloc|realloc|free)\(" src` を実行し、`core_alloc.c` の既定値（標準関数のアドレス）以外に該当がないことを確かめる。
- [x] `clopts_testing` がまだ必要かを見直す（実施: 内部関数を呼ぶテストのために残す）。
- [x] 静的・共有ビルドの両方、全ラベル（core / parser / platform / help / abi / docs / integration）で CTest を通す。
- [ ] Windows（MSVC / MinGW）で、platform の OOM スイープと `platform_probe` が通ることを確かめる。

## 完了条件

- `src/` に、テスト専用の型、フィールド、関数、引数がない。
- ライブラリのすべての確保と解放が、設定されたアロケーターを通る。
- これまでの OOM の網羅試験（すべての失敗位置で、エラーを返し、生存ブロックが 0 になる）を、公開 API とテスト側のアロケーターで同じ以上に満たす。
- 「公開構造体は凍結する」という規則が、steering、abi-compat の spec、`docs/abi.md`、ヘッダの検査で一貫している。
- 公開シンボルの一覧、文書、変更履歴が更新され、すべてのテストが通る。
