# TODO

## platform モジュールの改名

### 背景

`platform` という名前が責務と合っていない。

- 公開ヘッダ `clopts/platform.h` の中身は `clopts_argv_*` だけ。ほかの公開ヘッダ（`env.h`、`writer.h`、`option.h` など）は中身の名詞で名前が付いているが、このヘッダだけが「どこで動くか」で名前が付いている。
- 内部モジュール `src/platform/` が実際に担うのは「プロセスの入力（argv と環境変数）を OS から UTF-8 で得る」ことだけ。汎用の移植層（メモリ確保や出力も含めた OS 抽象）ではない。中のコード（分割、ハンドル、バッファ）の大半は OS に依存しない。
- 接頭辞が揃っていない。公開側は `clopts_argv_*`、内部は `clopts_platform_argv_*`。`clopts_platform_env_t` の中身は環境ではなく、参照に使うバッファ（文脈）。

モジュールとして分けていること自体は残す。分ける理由は、OS 依存（`<windows.h>`、`getenv`、win32/posix のソースの切り替え）をここに閉じ込めること、argv と環境変数が Windows の UTF-16→UTF-8 変換（`platform_win32_measure`）を共有していること、argv の API が parser から独立した機能であること。

互換は保たない（AGENTS.md）。古い名前は残さない。

### 1. 公開ヘッダを `clopts/argv.h` にする（優先）

- [ ] `include/clopts/platform.h` → `include/clopts/argv.h`（インクルードガード `CLOPTS_ARGV_H`、冒頭のコメントの「Platform API」も直す）
- [ ] `include/clopts/clopts.h` の include
- [ ] include している箇所: `src/platform/platform_internal.h`、`tests/platform/platform_probe.c`、`tests/integration/fixtures/consumer/main.c`
- [ ] ABI のシンボル一覧のセクション見出し `# clopts/platform.h`: `abi/clopts.symbols`、`tests/abi/fixtures/exports/expected.symbols`、`tests/abi/fixtures/exports/mismatch.symbols`、`tests/docs/fixtures/api_coverage/symbols.txt`
- [ ] 文書: `docs/api.md`（ヘッダ一覧の表、「## プラットフォームの関数」の節名とアンカー）、`docs/windows.md`（本文と `api.md#プラットフォームの関数` へのリンク）、`docs/dev/abi-checks.md`
- [ ] `CHANGELOG.md` に記載

ABI の検査で使う「platform tag」（OS の注記 `windows`）は別の概念なので、そのままにする。

### 2. 内部モジュールを `process` にする

- [ ] `src/platform/` → `src/process/`、ファイル名を `platform_*.c` → `process_*.c`、`platform_internal.h` → `process_internal.h`
- [ ] `src/CMakeLists.txt` の `add_subdirectory`、`clopts_add_module(platform ...)` → `clopts_add_module(process ...)`
- [ ] 内部シンボルの接頭辞を変える
  - `clopts_platform_argv_*` → `clopts_process_argv_*`
  - `clopts_platform_split` → `clopts_process_split`
  - `clopts_platform_env_*` → `clopts_process_env_*`
  - `clopts_platform_u16_t` → `clopts_process_u16_t`
  - `clopts_platform_env_t` → `clopts_process_env_ctx_t`（中身が環境ではなく参照の文脈なので）
  - static な関数・型（`platform_win32_*`、`platform_argv_*`、`PLATFORM_WIN32_*`）も揃える
- [ ] 利用側: `src/parser/resolve.c`、`tests/parser/test_resolve.c`
- [ ] テスト: `tests/platform/` → `tests/process/`、`tests/CMakeLists.txt` の `add_subdirectory`、`clopts_add_module_tests(platform ...)`（実行ファイル名とラベルが `process` になる）、`platform_probe` → `process_probe`（`PLATFORM_PROBE_PATH` も）、`platform.no_shell32` → `process.no_shell32`
- [ ] 文書: `docs/dev/architecture.md`（モジュール表、依存の図、本文）、`docs/dev/release.md`（`ctest -L platform` → `-L process`）

### 確認

- [ ] `grep -rni platform` で、OS の注記（platform tag）と「on every platform」のような一般的な英語以外に残りがないこと
- [ ] Linux と Windows でビルドとテストが通ること（ABI の検査、文書の検査を含む）
