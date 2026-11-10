# TODO: 内部ヘッダの種類と実装ヘッダ

この文書は、内部ヘッダを「誰に見せるか」で分け、継承される型の中身を実装ヘッダ `x_impl.h` に分ける作業の一覧である。機能と公開 API は変えない。

規則とその理由は、作業の中で `docs/dev/design.md` に書く。作業がすべて終わったら、この文書を消す。

## 1. 目標

### 1.1 内部の種類

| 種類 | 誰が使うか | 置き場所 |
|------|------------|----------|
| 公開 | 利用者 | `include/clopts/<m>/x.h` |
| ライブラリ内部 | ほかのモジュール | `src/<m>/x.h`（2.2 の表に列挙したものだけ） |
| モジュール内部 | 同じモジュールのほかの概念 | `src/<m>/x.h` |
| 実装 | x を埋め込む概念と、x の OS ごとの実装 | `src/<m>/x_impl.h` |
| ファイルの中だけ | 1 つのファイル | `.c` の中（`static`、構造体の定義） |

- 実装ヘッダ `x_impl.h` は、概念 x のものである。関数は `clopts_x_*` と名付け、`x.c` か `x_<os>.c` で定義する。`x.h` と `x_impl.h` の間で関数を動かしても、改名は要らない。
- `x_impl.h` を置いてよいのは、ほかの型が x を値として埋め込むときと、x の実装が OS ごとのファイルに分かれるときだけである。それ以外は `.c` に閉じる。
- テストは、どの種類の内部ヘッダも include してよい。

### 1.2 構造体の中身の見せ方

| 種類 | 基準 | 置き場所 | 例 |
|------|------|----------|----|
| 値の型 | フィールドがそのまま使い方で、直接書き換えても壊れるものがない | `x.h` | `clopts_vec_t`、`clopts_slot_t`、`clopts_token_t`、`clopts_lookup_t`、`clopts_out_t`、`clopts_env_t`、内部の `clopts_usage_error_t` |
| ハンドル | 不変条件があり、ほかのファイルは中身を知らなくてよい | `x.c` | program、command、option、positional、matches、result |
| 継承される型 | 不変条件があるが、埋め込む・実装する側は中身を知る必要がある | `x_impl.h` | `clopts_scope_t`、`struct clopts_argv` |

- 公開の構造体は、上の基準に加えて「その形で ABI を凍結してよいか」で決める。公開で中身を出すのは `clopts_allocator_t` だけである（利用者が埋めて渡す型）。`clopts_usage_error_t` は、内部では値の型、公開では不透明な型である。

### 1.3 公開ヘッダと同じ名前の内部ヘッダ

- 内部ヘッダ `src/<m>/x.h` が公開ヘッダ `clopts/<m>/x.h` と同じ名前なのは、1 つの概念の公開の面と内部の面だからである。内部ヘッダは、最初にその公開ヘッダを include する。
- 区別はパスが担う（`"decl/scope.h"` は `src/`、`"clopts/decl/scope.h"` は `include/` にしかない）。インクルードガードも別にする（`CLOPTS_DECL_SCOPE_H` と `CLOPTS_SRC_DECL_SCOPE_H`）。

## 2. 当てはめ

### 2.1 実装ヘッダ

| 新しいヘッダ | 移すもの | include するもの |
|--------------|----------|------------------|
| `src/decl/scope_impl.h` | `struct clopts_scope`、`clopts_scope_init`、`_release`、`_add_option`、`_add_positional` | `scope.c`、`program.c`、`command.c`、テスト |
| `src/process/argv_impl.h` | `struct clopts_argv`、`clopts_argv_sentinel`、`_failure`、`_passthrough`、Windows の `_convert`、`_from_cmdline` | `argv.c`、`argv_posix.c`、`argv_win32.c`、テスト |
| `src/process/env_impl.h` | `clopts_env_reserve_value`、`_reserve_wide` | `env.c`、`env_win32.c`、テスト |

- `src/decl/scope.h` には、`lookup.c` が使う `clopts_scope_find_long` と `_find_short` が残る。
- 内部の `src/process/argv.h` は中身がなくなるので消す。
- `src/process/env.h` には、parse が使う `clopts_env_t`、`clopts_env_init`、`_release`、`_lookup` が残る。
- ほかの内部ヘッダは変えない。

### 2.2 ライブラリ内部のヘッダ

ほかのモジュールが include してよい内部ヘッダは、次のものだけにする。

| モジュール | ほかのモジュールに見せる内部ヘッダ |
|------------|------------------------------------|
| base | すべて |
| decl | `lookup.h` |
| process | `env.h` |
| parse、text | なし |

## 3. 判断が要る点

| 点 | 推奨 | 理由 | 採らない場合 |
|----|------|------|--------------|
| 実装ヘッダの名前 | `_impl` | LLVM の `*Impl.h` と同じ意味で、「x の実装を見せる面」と読める | `_private`（GLib）か `_base` にして、4 章の名前を置き換える |
| 2.2 を検査で守る | 守る | 列挙が小さく、モジュール内部のヘッダが外から使われるのを防げる。今の使われ方は 2.2 の表に収まっている | フェーズ 3 の「ライブラリ内部の列挙」を外す。`_impl` の検査は残す |

## 4. 作業

### 共通の完了条件

- 静的と共有の両方、Debug と Release で、gcc と clang でビルドでき、`ctest` がすべて通る。
- MinGW のクロスビルドを wine で実行し、Windows 向けの単体テストが通る。
- 公開 API と `abi/clopts.symbols` は変わらない。

### フェーズ 1: 実装ヘッダ

- [ ] `src/decl/scope_impl.h` を作り、2.1 のとおりに移す。`program.c`、`command.c`、`scope.c` の include を直す。
- [ ] `src/process/argv_impl.h` を作り、内部の `argv.h` を消す。`argv*.c` と `tests/process/` の include を直す。
- [ ] `src/process/env_impl.h` を作り、`reserve_*` を移す。`env.c`、`env_win32.c`、`tests/process/test_env.c` の include を直す。
- [ ] 実装ヘッダは、最初に概念 x のヘッダ（内部の `x.h` があればそれ、なければ公開の `clopts/<m>/x.h`）を include する。

### フェーズ 2: 名前の検査（`structure.names`）

- [ ] `x_impl.h` の関数を概念 x のものとして扱う。名前は `clopts_x` か `clopts_x_*`、定義は `x.c` か `x_<os>.c` にあることを確かめる。
- [ ] 定義から宣言を探すときは、`x.h`、`x_impl.h`、公開の `clopts/<m>/x.h` を見る。
- [ ] `x_impl.h` に対応する `x.c` か `x_<os>.c` があることを確かめる（`x_impl.c` は置かない）。
- [ ] 公開ヘッダと同じ名前の内部ヘッダが、最初にその公開ヘッダを include していることを確かめる。
- [ ] 各規則に、良い例と悪い例の fixture を足す（`tests/structure/fixtures/names/`）。

### フェーズ 3: 依存の検査（`structure.modules`）

- [ ] `<m>/x_impl.h` は、モジュール `<m>` のファイルからしか include できない。
- [ ] （2.2）ほかのモジュールの内部ヘッダは、2.2 の表に列挙したものしか include できない。規則は `check_structure.cmake` に `_shared_headers` として持つ。
- [ ] 各規則に、悪い例の fixture を足す（parse が `decl/scope_impl.h` を include する、parse が `decl/scope.h` を include する）。

### フェーズ 4: 文書

- [ ] `docs/dev/design.md` に「内部の種類」の節を足す。1.1、1.2、1.3 を、理由とともに書く。
- [ ] `docs/dev/design.md` の「参考にした設計」に足す。libgit2（公開と内部で同じ名前のヘッダ。ただし内部の関数は `git_x__` で区別し、CLOpts は区別しない）、CPython（ヘッダを 3 段に分ける）、GLib（`*-private.h`）、LLVM（`*Impl.h` は表現を見せる面）。
- [ ] `docs/dev/design.md` の「採らなかった案」を直す。`*_internal.h`（見え方の段階を名前にする）と `sqliteInt.h` のような全部入りは残す。`*_impl.h` は採用した側に移し、役割を名前にすることと、見え方の段階を名前にすることの違いを書く。
- [ ] `docs/dev/architecture.md` の「構造体の置き場所」の表と、依存の検査の説明を直す。
- [ ] この文書を消す。
