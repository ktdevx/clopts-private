# TODO: 差し替え可能なアロケーターの導入と確保プローブの撤去

## 背景

いまは、確保の失敗を試すための確保プローブ（`clopts_parser_probe_t` / `clopts_platform_probe_t`）が `src/` 側にある。

- 窓口: `src/parser/parser_alloc.c`、`src/platform/platform_alloc.c`
- 構造体のフィールド: `program->probe`、`result->probe`、`env->probe`
- テスト専用の内部の口: `clopts_parser_set_probe()`、`clopts_platform_env_new(probe)`、`clopts_platform_argv_*(probe, ...)`

本番では常に NULL だが、ほぼすべての内部関数が `probe` を引数として受け回しており、製品コードにテスト用のものが混ざっている。また、プローブを通らない確保がある。

- `clopts_program_new` の `malloc` / `free`（`src/parser/parser_build.c:309`, `:361`）
- `core_vec` の `realloc` / `free`（`src/core/core_vec.c:45`, `:88`）

## 目的

- ハンドルごとに差し替えられるアロケーターを公開 API として持つ。
- 失敗の注入と生存ブロックの計数は、`tests/` 側のアロケーターで行う。`src/` からテスト専用のものをなくす。
- ライブラリの確保をすべて、例外なく、アロケーター経由にする。

## 方針（案）

### 公開 API

tech.md の「公開ヘッダに構造体の定義を置かない」「構造体を値で受け渡さない」に従う。そのため、関数ポインタ型と、生成関数の別版で表す。

```c
/* include/clopts/alloc.h（新規） */
typedef void *(*clopts_alloc_fn)(void *ctx, size_t size);  /* size >= 1。失敗は NULL */
typedef void (*clopts_free_fn)(void *ctx, void *ptr);      /* ptr は NULL でない */

/* decl.h */
CLOPTS_API clopts_program_t *clopts_program_new_with_allocator(const char *name,
    clopts_alloc_fn alloc_fn, clopts_free_fn free_fn, void *ctx);

/* platform.h */
CLOPTS_API clopts_env_t *clopts_env_new_with_allocator(clopts_alloc_fn alloc_fn, clopts_free_fn free_fn, void *ctx);
CLOPTS_API clopts_argv_t *clopts_argv_get_with_allocator(int argc, char *const *argv,
    clopts_alloc_fn alloc_fn, clopts_free_fn free_fn, void *ctx);
#ifdef _WIN32
CLOPTS_API clopts_argv_t *clopts_argv_from_wide_with_allocator(int argc, wchar_t *const *wargv,
    clopts_alloc_fn alloc_fn, clopts_free_fn free_fn, void *ctx);
#endif
```

- `alloc_fn` と `free_fn` が両方 NULL なら `malloc` / `free` を使う。既存の `clopts_program_new` などは、この形で呼ぶ薄い版にする。片方だけ NULL は契約違反として `assert` で捕まえる。
- `realloc` は求めない。vec を伸ばすときは、確保 → `memcpy` → 解放で行う（vec は旧サイズ `cap * elem_size` を知っている）。利用者が実装する関数を 2 つに抑え、失敗する位置も単純になる。
- `size` が 0 で呼ばれることはない（ライブラリ側で 1 に丸める）。`NULL` は必ず失敗を意味する。
- 結果は、プログラムのアロケーターを値として複製して持つ。`clopts_result_free` は `clopts_program_free` の後でも呼べる（`docs/api.md:159`）ので、プログラムを参照してはいけない。
- `ctx` の寿命: それを渡して作ったハンドルと、そこから得た結果がすべて解放されるまで有効であること。
- ライブラリは呼び出しを同期しない。同じ `ctx` を複数のスレッドから使うなら、利用者が保護する。
- グローバルに既定のアロケーターを設定する関数は持たない（可変のグローバル状態、スレッド安全性、初期化の順序の問題を避ける）。
- CRT の境界の規則（「ライブラリが確保したものはライブラリの関数で解放する」）は変わらない。ライブラリは、渡された `free_fn` で解放する。

### 内部

- `src/core/core_alloc.c` / `core_internal.h` に `clopts_core_alloc_t { alloc_fn, free_fn, ctx }` を置く（内部の構造体なので定義してよい）。窓口は `clopts_core_malloc`、`clopts_core_calloc`（`size_mul` で検査してから `memset`）、`clopts_core_strdup`、`clopts_core_strndup`、`clopts_core_free` とする。
- 既定のアロケーター（`malloc` / `free` を包むもの）を `const` の静的オブジェクトとして 1 つ持つ。
- `clopts_core_vec_push` / `clopts_core_vec_free` は `const clopts_core_alloc_t *` を受け取る。
- `probe` のフィールドと引数を、`clopts_core_alloc_t`（構造体の中では値、関数の引数ではポインタ）に置き換える。

## 未決事項（仕様化の前に決める）

- [ ] 関数名: `_with_allocator` と `_ex` のどちらにするか（案: `_with_allocator`。何が違うのかが名前からわかる）。
- [ ] `free_fn` にサイズを渡すか（案: 渡さない。サイズを覚えておく負担がライブラリ側に生じるため。アリーナで使う場合は無視すればよい）。
- [ ] `realloc` 相当を任意で受け付けるか（案: 受け付けない。必要になったら関数を追加する）。
- [ ] argv のパススルー（Windows 以外）でもハンドルの確保にアロケーターを使うか（案: 使う。ハンドルが 1 ブロックあるため）。
- [ ] 番兵（メモリ不足の結果、argv の番兵）はこれまでどおり静的オブジェクトとし、アロケーターとは関係しないことを明記する。

## TODO

### 0. 仕様（Kiro の手順）

- [ ] `/kiro-spec-init "差し替え可能なアロケーター"` で新しい spec（例: `custom-allocator`）を作る。
- [ ] requirements: 上の公開 API の振る舞い、NULL の扱い、`ctx` の寿命、結果がアロケーターを複製すること、すべての確保がアロケーターを通ること、サイズ 0 で呼ばないこと。
- [ ] design: `core_alloc` の構成、vec の伸ばし方の変更、プローブの撤去、テストの支援用アロケーター。
- [ ] tasks を生成し、各フェーズで人の承認を得る。
- [ ] steering の `tech.md:64`（「`malloc` / `calloc` / `realloc` / `free` を直接使い」）を、「確保はアロケーターの窓口を通す」に改める。
- [ ] 既存の spec の記述に、置き換えを指す注記を入れる。
  - `core-parser/design.md`: 29, 41, 58, 127, 137, 547, 568–591 行あたりの確保プローブ
  - `platform-argv/design.md`: プローブと窓口
  - `abi-compat`: シンボルの追加

### 1. 公開ヘッダ

- [ ] `include/clopts/alloc.h` を新しく作り、`clopts_alloc_fn` / `clopts_free_fn` を定義する。`clopts.h` から取り込む。
- [ ] `decl.h` に `clopts_program_new_with_allocator` を追加する。
- [ ] `platform.h` に `clopts_env_new_with_allocator`、`clopts_argv_get_with_allocator`、`clopts_argv_from_wide_with_allocator`（`_WIN32`）を追加する。
- [ ] ヘッダの規則の検査（`tests/abi/check_header_rules.cmake`）が通ることを確かめる。

### 2. core

- [ ] `src/core/core_alloc.c` を追加し、`src/core/CMakeLists.txt` に登録する。
- [ ] `core_internal.h` に `clopts_core_alloc_t`、既定のアロケーター、窓口の宣言を置く。
- [ ] `core_vec.c`: `realloc` をやめて確保 → 複製 → 解放にする。`push` / `free` がアロケーターを受け取るようにする。
- [ ] `tests/core/test_vec.c` を新しい引数に合わせ、失敗する位置（伸ばすときだけ）を検査する。

### 3. parser

- [ ] `parser_alloc.c` を削除する。
- [ ] `parser_internal.h` から `clopts_parser_probe_t`、`clopts_parser_set_probe`、窓口の宣言を削除する。
- [ ] `program->probe` / `result->probe` を `clopts_core_alloc_t alloc` に置き換える。
- [ ] `parser_build.c`: `clopts_program_new_with_allocator` を実装し、`clopts_program_new` をその薄い版にする。program の 1 ブロックもアロケーターで確保・解放する。
- [ ] `parser_result.c`: 結果の作成時にプログラムのアロケーターを複製する。番兵の初期化子から `.probe` を削除する。
- [ ] `parser_build.c` / `parser_scan.c` / `parser_resolve.c` / `parser_result.c` の `probe` の引数を、`const clopts_core_alloc_t *alloc` に置き換える。

### 4. platform

- [ ] `platform_alloc.c` を削除する。`malloc_array`（`size_mul` で検査）は core の窓口に移す。
- [ ] `platform_internal.h` から `clopts_platform_probe_t` と窓口の宣言を削除する。`env->probe` を `alloc` に置き換える。
- [ ] `platform_env.c`: `clopts_env_new_with_allocator` を実装し、`clopts_env_new` をその薄い版にする。
- [ ] `platform_argv.c` / `platform_win32.c`: `_with_allocator` 版を実装する。argv のハンドルが、どのアロケーターで確保されたかを持つようにする（`clopts_argv_free` で使うため）。
- [ ] 「サイズ 0 は 1 バイトとして確保する」規則を core の窓口に移す。

### 5. テスト

- [ ] `tests/support/test_alloc.{c,h}` を追加する。失敗を注入するアロケーターで、`fail_at`、`calls`、`live` を持つ。各ブロックに目印を付け、自分が確保していないポインタの解放と二重解放を検出する。parser / platform / core のテストから共有できる静的ライブラリにする。
- [ ] `tests/parser/test_oom.c`: 窓口を直接試すテスト（21 件のうち該当するもの）を `tests/core/` の窓口のテストに移す。スイープは、公開 API の `_with_allocator` 版を使う形に書き直す。
- [ ] `tests/platform/test_oom.c`（13 件）も同じように書き直す。
- [ ] `test_build.c`、`test_dispatch.c`、`test_resolve.c`、`test_result.c`、`test_fixtures.{c,h}`（parser）と、`test_argv.c`、`test_convert.c`、`test_env.c`（platform）から `set_probe` / `probe_t` の利用を取り除く。
- [ ] 失敗する位置の数が変わることへの対応: `vec_push` が、伸ばさないときには失敗しなくなる。スイープは、固定の回数ではなく「`fail_at = n` で最後まで成功し、かつ `calls < n` になったら終わる」という条件で回す。
- [ ] 新しく対象になった `clopts_program_new` の失敗（NULL を返す）を試す。
- [ ] 結果をプログラムの後に解放しても、プログラムのアロケーターで正しく解放されることを試す。
- [ ] 両方 NULL なら既定になること、`alloc_fn` に 0 が渡らないことを試す。
- [ ] OOM のスイープは公開 API だけで書けるので、共有ビルドでも `CLOpts::clopts` に対して流せる。可能なら、そのように構成する。

### 6. ABI・文書・変更履歴

- [ ] `abi/clopts.symbols` に 4 シンボルを追加する（`clopts_argv_from_wide_with_allocator` には `windows` の印を付ける）。
- [ ] `docs/api.md`: アロケーターの節を追加する（関数ポインタの契約、`ctx` の寿命、スレッド、結果が複製すること）。519 行目の「解放」の規則に、アロケーターとの関係を書き足す。
- [ ] `docs/windows.md`: argv / env の `_with_allocator` 版を書き足す。
- [ ] 文書の検査（ラベル `docs`）が、シンボルの一覧と文書を照合する（`tests/docs/check_docs.cmake`）。新しいシンボルが文書に載っていることを確かめる。
- [ ] `CHANGELOG.md` の `## [Unreleased]` / `### 追加` にアロケーターの API を書く。0.1.0 は未リリースなので、互換性のない変更の節は要らない。
- [ ] 必要なら、`examples/` か README にアロケーターの短い例を足す。その場合は docs のテストで照合する。

### 7. 後片付けと確認

- [ ] `grep -rnE "probe|\b(malloc|calloc|realloc|free)\(" src` を実行し、`core_alloc.c` の既定のアロケーター以外に該当がないことを確かめる。
- [ ] `clopts_testing` がまだ必要かを見直す。OOM 以外の内部テストのために残るはず。
- [ ] 静的・共有ビルドの両方、全ラベル（core / parser / platform / help / abi / docs / integration）で CTest を通す。
- [ ] Windows（MSVC / MinGW）の CI で、platform の OOM スイープと `platform_probe` が通ることを確かめる。
- [ ] `/kiro-validate-impl custom-allocator` で最終確認する。

## 完了条件

- `src/` にテスト専用の型、フィールド、関数がない。
- ライブラリのすべての確保と解放が、ハンドルのアロケーターを通る。
- これまでの OOM の網羅試験（すべての失敗位置で、エラーを返し、生存ブロックが 0 になる）を、公開 API とテスト側のアロケーターで同じ以上に満たす。
- 公開シンボルの一覧、文書、変更履歴が更新され、すべてのテストが通る。
