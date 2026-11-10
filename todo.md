# TODO

## モジュール構成を整える

### あるべき姿

モジュールの数と分け方（core、decl、parser、text、platform の 5 つ）と依存の向きは今のままでよい。直すのは次の 2 点。

- 名前が構成と合っていない。公開ヘッダ `clopts/platform.h`、モジュール名 `platform`、ファイル名の接頭辞が揃っていない。
- 境界が文書（`docs/dev/architecture.md`）に書いてあるだけで、検査されていない。インクルードのルートが `src/` なので、どの内部ヘッダでも include できてしまう。

目指す形:

| モジュール | 責務 | 依存してよい内部ヘッダ | 公開ヘッダ |
|---|---|---|---|
| core | 確保の窓口、サイズの計算、可変長配列 | なし | `alloc.h` |
| decl | 宣言モデルとビルダー、名前の規則 | core | `decl.h`（`decl_types.h`、`option.h`、`command.h`、`program.h`、`positional.h`、`decl_error.h`） |
| parser | 解析エンジンと結果 | core、decl、process（`parse.c` の入口だけ） | `parse.h`、`env.h` |
| process | プロセスの入力（argv、環境変数）を OS から UTF-8 で得る。OS に触るのはここだけ | core | `argv.h` |
| text | 利用者向けの文面 | core（decl と parser の内部は読まない） | `text.h`、`writer.h` |

```
src/core/     alloc.c size.c vec.c                          core_internal.h
src/decl/     program.c command.c option.c positional.c
              scope.c names.c error.c                       decl_internal.h
src/parser/   parse.c scan.c resolve.c result.c             parser_internal.h
src/text/     layout.c message.c out.c                      text_internal.h
src/process/  argv.c split.c env.c win32.c posix.c          process_internal.h
```

変えないもの:

- モジュールの数と分け方。フラットにしない。text を parser に入れない。process を解体しない。
  - process を分けておく理由: OS 依存（`<windows.h>`、`getenv`、win32/posix のソースの切り替え）をここに閉じ込めるため。また、argv と環境変数が Windows の UTF-16→UTF-8 変換（`platform_win32_measure`）を共有しているため。argv の API も parser から独立した機能である。
- `tests/<module>/` が `<module>_tests` 実行ファイルと ctest のラベルに対応する形。
- `clopts_add_module` / `clopts_add_module_tests` の仕組み。

互換は保たない（AGENTS.md）。古い名前は残さない。

ABI の検査で使う「platform tag」（OS の注記 `windows`）は別の概念なので、どの手順でも変えない。

### 1. 公開ヘッダを `clopts/argv.h` にする

中身は `clopts_argv_*` だけで、ほかの公開ヘッダと同じく中身の名詞で名前を付ける。

- [ ] `include/clopts/platform.h` → `include/clopts/argv.h`（インクルードガードを `CLOPTS_ARGV_H` にし、冒頭のコメントの「Platform API」も直す）
- [ ] `include/clopts/clopts.h` の include
- [ ] include している箇所: `src/platform/platform_internal.h`、`tests/platform/platform_probe.c`、`tests/integration/fixtures/consumer/main.c`
- [ ] ABI のシンボル一覧のセクション見出し `# clopts/platform.h`: `abi/clopts.symbols`、`tests/abi/fixtures/exports/expected.symbols`、`tests/abi/fixtures/exports/mismatch.symbols`、`tests/docs/fixtures/api_coverage/symbols.txt`
- [ ] 文書: `docs/api.md`（ヘッダ一覧の表、「## プラットフォームの関数」の節名とアンカー）、`docs/windows.md`（本文と `api.md#プラットフォームの関数` へのリンク）、`docs/dev/abi-checks.md`
- [ ] `CHANGELOG.md`

### 2. platform → process の改名と、ファイル名を揃えることを 1 回で行う

どちらもファイルの移動なので、まとめて 1 回で行う。

`.c` のファイル名の接頭辞は外し、ディレクトリを名前空間にする（今は decl と parser が接頭辞なしで、core、text、platform が接頭辞あり）。内部ヘッダはほかのモジュールからも include されるので、`<module>_internal.h` のまま残す。

- [ ] `src/platform/` → `src/process/`
  - `platform_argv.c` → `argv.c`、`platform_split.c` → `split.c`、`platform_env.c` → `env.c`、`platform_win32.c` → `win32.c`、`platform_posix.c` → `posix.c`
  - `platform_internal.h` → `process_internal.h`
- [ ] `src/core/`: `core_alloc.c` → `alloc.c`、`core_size.c` → `size.c`、`core_vec.c` → `vec.c`
- [ ] `src/text/`: `text_layout.c` → `layout.c`、`text_message.c` → `message.c`、`text_out.c` → `out.c`
- [ ] 各 `CMakeLists.txt` のソース一覧、`src/CMakeLists.txt` の `add_subdirectory`、`clopts_add_module(platform ...)` → `clopts_add_module(process ...)`
- [ ] 内部シンボルの接頭辞を変える
  - `clopts_platform_argv_*` → `clopts_process_argv_*`
  - `clopts_platform_split` → `clopts_process_split`
  - `clopts_platform_env_*` → `clopts_process_env_*`
  - `clopts_platform_u16_t` → `clopts_process_u16_t`
  - `clopts_platform_env_t` → `clopts_process_env_ctx_t`（中身は環境そのものではなく、参照に使う文脈なので）
  - static な関数と型（`platform_win32_*`、`platform_argv_*`、`PLATFORM_WIN32_*`）も揃える
- [ ] 利用側: `src/parser/resolve.c`、`tests/parser/test_resolve.c`
- [ ] テスト: `tests/platform/` → `tests/process/`、`tests/CMakeLists.txt` の `add_subdirectory`、`clopts_add_module_tests(platform ...)`（実行ファイル名とラベルが `process` になる）、`platform_probe` → `process_probe`（`PLATFORM_PROBE_PATH` も）、`platform.no_shell32` → `process.no_shell32`
- [ ] ファイルの先頭コメントで、ほかのファイルを旧名で指している箇所（例: `platform_internal.h` の「platform_split.c, platform_argv.c, ...」、`platform_env.c` の「platform_win32.c or platform_posix.c」）
- [ ] 文書: `docs/dev/architecture.md`（モジュール表、依存の図、本文）、`docs/dev/release.md`（`ctest -L platform` → `-L process`）

### 3. 環境変数の取得元をつなぐ場所を `parse.c` の入口に移す

今は `clopts_parser_resolve` の中で、`getenv_fn == NULL` のときにプロセスの環境変数の参照（文脈の初期化、参照、解放）へ差し替えている（`src/parser/resolve.c` の `clopts_parser_resolve`）。これを入口（`parse.c` の `parse`）に移す。そうすると、走査・解決・検査（scan、resolve、result）は OS を知らないエンジンになり、process に依存するのは入口だけになる。

- [ ] `parse.c`: `getenv_fn == NULL`（`clopts_parse`、および `clopts_parse_with_env` に NULL を渡した場合）のとき、`clopts_process_env_ctx_t` をスタックに置いて初期化する。プロセスの環境変数の参照をつないで走らせ、終わったら解放する
- [ ] `resolve.c`: process の内部ヘッダの include と差し替えをやめる。`clopts_parser_resolve` は `getenv_fn != NULL` を前提にする（`assert`）
- [ ] `parser_internal.h` の `clopts_parser_resolve` の説明を直す
- [ ] `tests/parser/test_resolve.c`: 既定の参照を確かめるテスト（`null_getenv_fn_uses_the_default_lookup`）は入口を通るので、そのまま通ることを確認する。直前の見出しのコメントにある存在しない名前 `clopts_platform_getenv_default` も直す
- [ ] `docs/dev/architecture.md`: 依存の説明（「parser は、プロセスの環境変数を platform の内部ヘッダで読む」「文脈は解決（`resolve.c`）のスタックに置き」）と、解析の流れの 4

### 4. 境界を検査する

2 と 3 の後に足す（最初から通る状態で入れるため）。`tests/platform/check_no_shell32.cmake` と同じやり方の CMake スクリプトにし、コメントを除いてから照合する。

- [ ] 内部ヘッダの依存の行列: `src/<m>/` が include してよい `<module>/<module>_internal.h` を上の表のとおりに決め、それ以外を見つけたら `<file>:<line>` を並べてエラーにする
  - parser から process への依存は `src/parser/parse.c` だけに許す
  - text は `core_internal.h` 以外の内部ヘッダを読まない
- [ ] OS の境界: `<windows.h>` の include と `getenv(` の呼び出しが `src/process/` の外に現れないこと
- [ ] `tests/` 直下に置く（例: `tests/structure/`、ラベル `structure`）
- [ ] 失敗することを確かめる fixture（`tests/abi/fixtures` と同じ形）
- [ ] `docs/dev/architecture.md` に、依存の向きが検査されていることと検査の名前を書く

### 確認

- [ ] `grep -rni platform` で、残っているのが OS の注記（platform tag）と、「on every platform」のような一般的な英語だけであること
- [ ] Linux と Windows でビルドとテストが通ること（ABI の検査、文書の検査を含む）
