# Research & Design Decisions: platform-argv

## Summary
- **Feature**: `platform-argv`
- **Discovery Scope**: Extension（既存のライブラリ `clopts` に内部モジュール `platform` を足す。外部依存は Windows の kernel32 のみ）。discovery は light で行い、リスクの大きい CRT の分割規則だけを詳しく確かめた。
- **Key Findings**:
  - 現行の MSVC CRT（ucrt の `parse_command_line`）は argv[0] を他の引数と別の規則で切り出す。引用符は出現するたびに引用モードを切り替えるだけで、バックスラッシュはエスケープにならない。brief の「argv[0] は他の引数と同じ規則」は誤りで、requirements（2.3）は CRT に合わせて書き直してある。
  - 期待値を手で書いた表だけでは「CRT と一致する」ことを証明できない。Windows では、子プロセスを生のコマンドライン文字列で起動して、CRT が作った `__argv` と本モジュールの結果を突き合わせられる。
  - core-parser の環境変数コールバック（`clopts_getenv_fn`）は ctx を持てるので、ctx を不透明ハンドル `clopts_env_t` にすれば、失敗の理由を core-parser を変えずに利用者へ渡せる。

## Research Log

### 現行 MSVC CRT のコマンドライン分割規則
- **Context**: 要件 2（`main` の argv と要素数・区切りを一致させる）。`CommandLineToArgvW` は 2008 年以前の規則で動き、CRT と食い違う。
- **Sources Consulted**: ucrt のソース `minkernel/crts/ucrt/src/startup/argv_parsing.cpp`（Windows SDK 同梱）、Rust std `library/std/src/sys/args/windows.rs` の `parse_lp_cmd_line`、Zig std `ArgIteratorWindows`、David Deley「How Command Line Parameters Are Parsed」（2009 年の変更点の一覧）。
- **Findings**:
  - argv[0]: 引用符で引用モードを切り替え（引用符は出力しない）、引用モードの外の空白またはタブで終わる。バックスラッシュは常に文字どおり。先頭が空白なら argv[0] は空文字列。空のコマンドラインでも argv[0] は空文字列で、argc は 1。
  - 引数: 空白とタブの並びを読み飛ばし、終端に達したら終わる。バックスラッシュの並びのあとに引用符が来たとき、偶数個（2n）なら n 個のバックスラッシュを出して、引用符で引用モードを切り替える。ただし引用モード中で次も引用符（`""`）なら引用符 1 個を出し、引用モードを維持する（2008 年の変更点）。奇数個（2n+1）なら n 個のバックスラッシュと引用符 1 個を出す。引用符が続かないバックスラッシュは文字どおり。
  - 区切りは空白（U+0020）とタブ（U+0009）だけ。改行などは区切りにしない。
  - ワイルドカードの展開は `setargv.obj` をリンクしたときだけ行われ、既定では展開しない。
  - Rust と Zig は、どちらも上の規則を自前で実装している（Rust は 2022 年に `CommandLineToArgvW` から移行した）。
- **Implications**: 分割は UTF-16 の配列を入力とする純粋関数にでき、2 パス（数える → 埋める）で 1 回の確保に収まる。argv[0] 用と引数用で 2 つのループに分ける。

### main の argv との一致を検証する方法
- **Context**: 要件 2.2、9.3。
- **Findings**:
  - MSVC と MinGW-w64（ucrt64）は、どちらも ucrt の `__getmainargs` / `_configure_narrow_argv` で `main` の argv を作るので、規則は同じである。CRT が作った argv は `__argc` / `__argv`（`<stdlib.h>`）で、`main` の外からも読める。
  - ナローの argv は ANSI コードページに変換したコマンドラインから作られる。ASCII だけの入力なら、ワイド版と同じ結果になる。
  - 子プロセスを `CreateProcessW(app, cmdline, ...)` で起動すると、`cmdline` がそのまま子の `GetCommandLineW()` になる。生の文字列（先頭の空白、`"foo""bar"`、閉じられない引用符など）を子に与えて、子の中で `__argv` と本モジュールの結果を比べられる。
  - 旧来の msvcrt.dll を使う MinGW（CI の対象外）は 2008 年以前の規則で `main` の argv を作るため、本モジュールの結果と食い違うことがある。
- **Implications**: Naos のテストで `__argc` / `__argv` と比べる（9.3）。あわせて、突き合わせ専用の小さな実行ファイル（probe）を子プロセスとして起動し、既知の例を実際の CRT と比べる。msvcrt 系の食い違いは docs-and-release で言及する。

### UTF-16 ↔ UTF-8 の変換
- **Context**: 要件 3、7。
- **Sources Consulted**: Microsoft Learn「WideCharToMultiByte」「MultiByteToWideChar」「GetEnvironmentVariableW」「GetCommandLineW」。
- **Findings**:
  - `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)` は、対になっていないサロゲートがあると 0 を返し、`GetLastError()` が `ERROR_NO_UNICODE_TRANSLATION` になる（Vista 以降）。長さ 0 の入力は `ERROR_INVALID_PARAMETER` で失敗する。
  - `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` は不正な UTF-8 で 0 を返し、`ERROR_NO_UNICODE_TRANSLATION` になる。
  - `GetEnvironmentVariableW` は、バッファが足りないときは必要な長さ（終端を含む）を返し、成功したときは長さ（終端を含まない）を返す。変数がなければ 0 を返して `ERROR_ENVVAR_NOT_FOUND` になる。値が空のときも 0 を返すため、呼ぶ前に `SetLastError(0)` しておいて区別する。2 回の呼び出しの間に値が変わることがあるので、収まるまで繰り返す。
  - いずれも kernel32 にあり、MSVC と MinGW-w64 の既定のリンクで解決する。
- **Implications**: 変換は Win32 API を採用する（自前実装しない）。長さ 0 の要素と空の値は API を呼ばずに扱う。失敗の理由は `GetLastError()` の値で判定し、文字列比較はしない。

### 既存コードとの接点
- **Context**: Extension discovery。
- **Findings**:
  - モジュールは `clopts_add_module(<module> SOURCES ...)`、テストは `clopts_add_module_tests(<module> SOURCES ...)` で足す（`cmake/CloptsModules.cmake`）。共有ビルドのテストは `clopts_testing` にリンクするので、内部関数も呼べる。
  - parser のメモリ確保失敗の検査は、内部関数に probe（`fail_at` / `calls` / `live`）を渡す方式で行っている（`src/parser/parser_alloc.c`）。グローバルな状態は持たない。
  - 結果ハンドルは、メモリ不足のとき共有の番兵（`static const`）を返し、NULL は返さない。NULL のハンドルは番兵と同じに読む（`include/clopts/parse.h`）。
  - core-parser の既定の環境変数取得は `clopts_parser_getenv_default`（`getenv`）。`clopts_parse_with_env` に NULL 以外を渡せば差し替えられる。
  - サイズ計算の桁あふれ検査は `clopts_core_size_mul` / `clopts_core_size_add`（`src/core/core_internal.h`）。
  - 公開シンボル一覧（abi-compat）はまだない（abi-compat は brief だけの段階）。
- **Implications**: probe 方式、番兵、NULL を番兵として読む規則を踏襲する。core-parser には手を入れない。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| A: `CommandLineToArgvW` を使い続ける | Monde と同じ | 実装が最小 | CRT と食い違う（要件 2 を満たせない）、shell32 に依存 | 退ける |
| B: 自前の分割 + Win32 の変換 API | 分割だけ自前で、変換は kernel32 | 要件 2 を満たす。変換の正しさは OS に任せる | 変換のテストは Windows でしか動かない | **採用** |
| C: 分割も変換も自前 | UTF-16 → UTF-8 も自前実装 | 全 OS で変換をテストできる | 自前のコードが増え、OS の API と細部が食い違う危険がある | 単純化の観点で退ける |

## Design Decisions

### Decision: 分割関数の入力型を Windows では `wchar_t`、それ以外では `uint16_t` にする
- **Context**: 分割規則を全 OS でテストしたい（9.1）。一方で Windows では `GetCommandLineW()` の `wchar_t *` をそのまま渡したい。
- **Alternatives Considered**:
  1. 常に `uint16_t` を受け取り、Windows では `wchar_t *` をキャストして渡す。厳密な別名規則（strict aliasing）に反する。
  2. `wchar_t` を受け取る。Windows 以外では 32 bit なので、テストの意味が変わる。
  3. 内部型 `clopts_platform_u16_t` を Windows では `wchar_t`、それ以外では `uint16_t` と定義する。
- **Selected Approach**: 3。テストは ASCII 文字列や符号単位の配列から `clopts_platform_u16_t` の配列を作る補助関数を使い、どの OS でも同じ表を使う。
- **Rationale**: キャストなしで両方を満たす。どちらの型も 16 bit の符号なし整数として扱える。
- **Trade-offs**: 内部型が 1 つ増える。公開ヘッダには出さない。

### Decision: argv の取得結果は 1 回の確保（ハンドル + 配列 + 文字列）にする
- **Context**: 要件 5（失敗時に何も残さない、解放関数 1 つで全部を解放）。
- **Selected Approach**: Windows では、UTF-8 に変換したときの長さを先に数えて、ハンドル・ポインタ配列・文字列を 1 つのブロックに置く。分割の作業用の UTF-16 バッファは、変換後すぐに解放する。パススルーと失敗のハンドルは、ハンドル単体を確保する。
- **Rationale**: 解放が `free` 1 回で済み、途中で失敗したときの片付けが単純になる。
- **Trade-offs**: 長さを数えるパスと変換するパスで、変換 API を 2 回ずつ呼ぶ。引数の数とコマンドラインの長さ（最大 32767 文字）を考えると、費用は無視できる。

### Decision: メモリ不足は共有の番兵、それ以外の失敗は確保したハンドルで返す
- **Context**: 要件 5.4、6.2（不正な UTF-16 は添字を持つ）。
- **Selected Approach**: `clopts_argv_get` / `clopts_argv_from_wide` は NULL を返さない。メモリ不足では `static const` の番兵を返す。不正な UTF-16 とコマンドライン文字列の取得失敗では、要素数 0 の小さなハンドルを確保して返し、それも確保できなければ番兵を返す。
- **Rationale**: core-parser の結果ハンドルと同じ使い方になる（NULL 検査が要らず、番兵を解放しても安全）。
- **Trade-offs**: 失敗のハンドルを確保できないときは、本来の理由（不正な UTF-16）ではなくメモリ不足が報告される。

### Decision: 環境変数の失敗の理由は ctx（`clopts_env_t`）に記録する
- **Context**: 要件 7.8〜7.10。core-parser の契約では、`CLOPTS_ENV_FAILED` を返すと解析は `CLOPTS_OUT_OF_MEMORY` で終わり、理由を運ぶ手段がない。ユーザーの判断で core-parser は変えない。
- **Selected Approach**: ctx を不透明ハンドル `clopts_env_t` とし、各呼び出しの最初に失敗の記録を消し、失敗したら種別と変数名を記録する。解析が `CLOPTS_OUT_OF_MEMORY` で終わったら、利用者は `clopts_env_error()` で理由を読める。
- **Rationale**: 既存のシグネチャのままで、理由を失わない。core-parser は失敗した時点で解析を打ち切るので、直近の呼び出しが失敗の原因になる。
- **Trade-offs**: ctx はスレッドごとに 1 つ必要になる（同時に使えない）。

### Decision: `clopts_env_lookup` は NULL の ctx に「失敗」を返す
- **Context**: 要件 7.12（ctx の作成がメモリ不足なら NULL）。
- **Selected Approach**: ctx が NULL のときは何も読まずに `CLOPTS_ENV_FAILED` を返す。利用者は `clopts_env_new()` の戻り値を検査しなくても、解析が `CLOPTS_OUT_OF_MEMORY` で終わる。
- **Rationale**: `clopts_parse` が NULL の program を番兵として扱うのと同じく、NULL 検査の手間を利用者に負わせない。ctx の作成が失敗する原因はメモリ不足だけなので、報告される状態も正しい。

### Decision: 公開ヘッダは `include/clopts/platform.h` の 1 つにする
- **Context**: argv と環境変数の 2 つの API がある。structure.md の例は `clopts/argv.h`。
- **Selected Approach**: 1 モジュール 1 ヘッダ（`help` ↔ `help.h`）にそろえ、`platform.h` に両方を置き、傘ヘッダから include する。環境変数の取得関数は `clopts_getenv_fn` と `clopts_env_status_t` を使うので、`clopts/parse.h` を include する。
- **Trade-offs**: structure.md の例と名前が違う。例は例示なので更新は不要だが、steering 同期のときに `platform.h` へ直す。

### Decision: OS ごとの実装は CMake で翻訳単位を選ぶ
- **Context**: steering は `<windows.h>` を Windows 専用の翻訳単位に閉じ込めることを求める。
- **Selected Approach**: 全 OS 共通の `platform_split.c` / `platform_argv.c` / `platform_env.c` には OS の分岐を書かない。`platform_win32.c`（`WIN32`）と `platform_posix.c`（それ以外）を `src/platform/CMakeLists.txt` で選ぶ。公開ヘッダの `clopts_argv_from_wide` だけは `#ifdef _WIN32` で宣言する。
- **Rationale**: ソースの中の `#ifdef` が公開ヘッダの 1 か所に限られ、各ファイルの責務が 1 つになる。

## Synthesis

- **Generalization**: `clopts_argv_get`（Windows）と `clopts_argv_from_wide` は、どちらも「NUL 終端の UTF-16 文字列の列 → 取得結果」の変換に帰着する。内部の変換器を 1 つにして、get は「分割 → 変換器」、from_wide は「変換器」だけにする。コマンドライン文字列から取得結果を作る内部関数も切り出し、`GetCommandLineW()` を経ずに非 ASCII や不正な UTF-16 を含む文字列でテストする。
- **Build vs. Adopt**: 分割は自前（`CommandLineToArgvW` は規則が古い。CRT の分割関数は公開されていない）。変換は Win32 API を採用する。メモリ確保失敗の注入は parser の probe 方式を踏襲する（共有のテスト基盤にはしない。parser の probe は parser の内部ヘッダにあり、platform は parser に依存できないため、同じ形の小さな probe を platform 内に持つ）。
- **Simplification**: 公開構造体、`owned` フラグ、利用側への `free` の委譲をなくし、不透明ハンドルと 1 回の確保にまとめた。分割関数は「数える」と「埋める」を同じ関数（出力先が NULL なら数えるだけ）にして、規則の二重実装を避ける。環境変数の既定を変えないので、core-parser への変更はない。

## Risks & Mitigations
- CRT の規則の読み違い — 既知の例の表に加え、Windows では子プロセスで実際の CRT と突き合わせる（probe）。
- msvcrt.dll を使う古い MinGW で `main` の argv と食い違う — 対象は現行 CRT（ucrt）と明記し、docs-and-release で注意を書く。CI の MinGW は ucrt64。
- MinGW のワイルドカード展開（`_dowildcard`）が有効な環境では、`*` / `?` を含む引数で `main` の argv と食い違う — 本モジュールは展開しない（2.13）。突き合わせのテストにはワイルドカードを含めない。docs-and-release で言及する。
- `GetEnvironmentVariableW` の 2 回の呼び出しの間に値が変わる — 収まるまで繰り返す。
- abi-compat の公開シンボル一覧がまだない — 本 spec が追加する公開関数を design.md に列挙し、一覧ができた時点（または実装時に一覧があればその時点）で載せる。

## References
- ucrt `argv_parsing.cpp`（Windows SDK の `Source/<ver>/ucrt/startup/`）— 現行 CRT の分割規則
- [Rust std: sys/args/windows.rs](https://github.com/rust-lang/rust/blob/master/library/std/src/sys/args/windows.rs) — 同じ規則の自前実装と、その根拠のコメント
- [Microsoft Learn: Parsing C command-line arguments](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments) — 規則の公式な説明（`""` の扱いを含む）
- [Microsoft Learn: WideCharToMultiByte](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte) — `WC_ERR_INVALID_CHARS`
- [Microsoft Learn: GetEnvironmentVariableW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablew) — 戻り値の規則
- David Deley, "How Command Line Parameters Are Parsed" — 2008 年の規則変更の記録
