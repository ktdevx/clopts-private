# 調査と設計判断: core-parser

## Summary
- **Feature**: `core-parser`
- **Discovery Scope**: New Feature（新しい内部モジュール `parser`。project-foundation の基盤と内部ユーティリティの上に積む。参照元として Monde の `cli_args` を読み替える）
- **Key Findings**:
  - project-foundation は `include/clopts/writer.h`（書き込みコールバック）と `src/core/core_internal.h`（`clopts_core_vec_*`、`clopts_core_size_*`）を提供済みで、`clopts_add_module` / `clopts_add_module_tests` でモジュールとテストを追加できる。傘ヘッダ `clopts/clopts.h` は現状 `writer.h` を include するだけで、本 spec が解析系のヘッダを足す。
  - Monde の `cli_args` の走査器は「サブコマンド前の表」「サブコマンド後の表」「最初の誤りを保留して完全一致の `--help` / `-h` を探す回復走査」「走査後の検査（位置引数の不足 → 必須オプション → 既定値）」の 4 段構成で、本 spec の要件 4〜9 はこの構成にグローバルオプション・環境変数・否定・カウントを足した形で素直に写像できる。
  - 「公開ヘッダに内部データ構造を出さない」（tech.md）と「呼び出し側がゼロ初期化した結果を解放できる」（12.3）を両立させるには、結果構造体を公開の薄い殻（状態・選択コマンド・エラー）と不透明な実装ポインタに分ける必要がある。
  - 記述子ポインタによる参照は、宣言配列の要素アドレスとの等値比較による線形探索で実現できる（配列外のポインタ同士の大小比較は C では未定義のため、`<` / `>=` による範囲判定は使わない）。

## Research Log

### 既存の基盤（project-foundation の成果物）
- **Context**: 依存してよい内部ユーティリティと、モジュール追加の手順を確認する。
- **Sources Consulted**: `include/clopts/clopts.h`、`include/clopts/writer.h`、`src/core/core_internal.h`、`src/CMakeLists.txt`、`src/core/CMakeLists.txt`、`tests/core/`、`cmake/CloptsModules.cmake`、`cmake/CloptsInstall.cmake`、`.kiro/specs/project-foundation/design.md`
- **Findings**:
  - `clopts_core_vec_t` はゼロ初期化が有効な空状態、`push` は失敗時に状態を変えず、`free` は冪等。`clopts_core_size_mul` / `_add` はオーバーフローで `-1`。
  - モジュールは `src/<module>/CMakeLists.txt` で `clopts_add_module(<module> SOURCES ...)`、テストは `tests/<module>/CMakeLists.txt` で `clopts_add_module_tests(<module> SOURCES ...)`。テスト実行ファイルは `src/` を include 基点として内部ヘッダを読める。
  - テストは Naos（`NAOS_DECLARE_SUITE` / `NAOS_MAIN` / `NAOS_TEST` / `NAOS_CHECK_*`）で、`<module>_tests_main.c` にスイートを並べる。
  - install は `include/clopts/` ディレクトリごと行うため、公開ヘッダを足しても CMake の変更は不要。
  - `_CRT_SECURE_NO_WARNINGS` は `clopts` ターゲットに PRIVATE 定義済み（`getenv` の C4996 対策）。
- **Implications**: 新規は `src/parser/`、`tests/parser/`、`include/clopts/decl.h`、`include/clopts/parse.h`。変更は `src/CMakeLists.txt`、`tests/CMakeLists.txt`、`include/clopts/clopts.h` の 3 ファイルだけ。

### Monde `cli_args` の構造と、本 spec への写像
- **Context**: brief は Monde の走査器の構造とエラー優先度の考え方を引き継ぐとしている。写像できる部分と作り直す部分を切り分ける。
- **Sources Consulted**: `ref/monde/src/cli_args/{args.h,args_internal.h}`、`ref/monde/.kiro/specs/cli-args/{requirements,design,research}.md`
- **Findings**:
  - Monde のモジュール内の依存は `args.h`（型）← `args_decl.c`（検査と検索）← `args_result.c`（結果）← `args_parse.c`（走査）。ヘルプは型と検索関数にだけ依存する。
  - 結果は選択コマンドの `options` と同じ添字の `values[]` 配列で持ち、長い名前で線形探索して返す。未宣言の名前は `false` / `NULL` / `0`。
  - 走査の表（サブコマンド前・後）、短いオプションの 1 文字ずつの処理、回復走査、走査後の検査順（位置引数の不足 → 必須 → 既定値）がすべて design.md に表として書かれている。
  - 宣言検査の規則表（名前の形式、予約名、フラグに必須・既定値なし、位置引数の範囲）がある。
  - Monde は `mnd_diag_list_t` に複数件の宣言エラーを積むが、本 spec は構造体 1 件。
- **Implications**:
  - 内部の依存方向（型 ← 宣言検査 ← 結果ストア ← 走査器）をそのまま採用する。
  - 結果ストアは「グローバルのスロット配列 + 選択コマンドのスロット配列」の 2 本にし、記述子ポインタ → スロットの対応は「どちらの配列の何番目か」を要素アドレスの等値比較で求める。
  - 走査の表は「サブコマンド前」にグローバルオプションの照合を加え、「サブコマンド後」に `--no-<name>`、COUNT、グローバルの照合を加える。走査後の検査に「環境変数の解決」を既定値の前に差し込む。

### コマンドライン慣習（POSIX、getopt、現代の実装）
- **Context**: 要件 5〜6 の走査規則が既存の慣習と整合しているかを確認し、`--no-<name>` とカウントの扱いの先例を見る。
- **Sources Consulted**: POSIX.1-2017 XBD 12.2 Utility Syntax Guidelines、glibc `getopt_long(3)`、clap（Rust）の `ArgAction::Count` / 否定フラグの慣習、Monde research.md の同節
- **Findings**:
  - まとめ書き、値の連結、`--` 終端、`-` 単体のオペランド扱い、次のトークンを `-` 始まりでも値として消費する、はいずれも POSIX / getopt と一致する。
  - `--no-<name>` は GNU のツール群（`--no-color` など）と clap の慣習どおり「`<name>` が否定可能なフラグのときだけ `--no-` を剥がして照合する」で足りる。実名 `no-x` を持つオプションとの衝突は宣言時に排除する。
  - カウントフラグ（`-vvv`）は clap の `Count` 相当。上限は実装によってまちまちで、オーバーフローさえ起こさなければよい。
  - 長いオプションの前方一致は clap が非対応（opt-in）。本 spec も非対応。
- **Implications**: 要件どおりで先例と矛盾しない。カウントの既定上限は `unsigned` の最大値とし、宣言で小さい上限を与えられるようにする。

### 公開ヘッダの分割と結果構造体の形
- **Context**: help-and-messages は宣言型・エラー型・結果型に依存し、走査器の実装には依存しない。tech.md は内部データ構造を公開ヘッダに出さないことを求める。
- **Sources Consulted**: `.kiro/steering/{tech,structure}.md`、`.kiro/specs/help-and-messages/brief.md`、`.kiro/specs/platform-argv/brief.md`
- **Findings**:
  - help は「宣言型 + 構造化エラー型 + 結果の状態と選択コマンド」を読む。結果の値そのものは読まない。
  - platform-argv は「環境変数を UTF-8 で返す取得関数」を提供し、本 spec の差し替え口に合わせる。関数の形は本 spec が先に決める。
  - 呼び出し側が結果をスタックに置けること（`clopts_result_t result = {0};`）と、内部のスロット配列や可変長配列を隠すことは、公開構造体に不透明ポインタ 1 つを持たせれば両立する。
- **Implications**: `include/clopts/decl.h`（宣言型・種類・ハンドラ型）と `include/clopts/parse.h`（状態・エラー・結果・環境変数取得関数の型・公開関数）に分ける。`clopts_result_t` は `status` / `command` / `error` / `impl`（不透明）の 4 フィールド。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 宣言テーブル + 単一パス走査器 + 構造化結果（採用） | `static const` の宣言を読む 1 つの走査器が結果ストアを埋め、状態と構造体エラーを返す | 宣言・挙動・ヘルプが 1 か所。Monde の走査表と約 180 件のテストの知見を流用できる。確保点が少ない | 走査器の分岐が増える（グローバル、否定、カウント、環境変数） | roadmap の案A。help は型だけに依存できる |
| 2 パス（先に `--help` / `--version` を全走査、次に本走査） | ヘルプ優先を単純化する | 実装が分かりやすい | `-o -h` のように値として渡した `-h` を誤ってヘルプにする | Monde research.md と同じ理由で却下 |
| 結果を公開構造体で全部見せる（Monde 方式） | `values[]` や可変長配列を公開ヘッダに置く | アクセサが不要 | 内部構造が ABI になり、tech.md の方針に反する。記述子ポインタ参照の意味が薄れる | 却下 |
| 結果を完全に不透明（ヒープ確保で返す） | `clopts_result_t *` を確保して返す | 実装の自由度が最大 | 確保失敗時に結果を返せない。ゼロ初期化して解放する要件（12.3）に合わない | 却下 |

## Design Decisions

### Decision: 公開ヘッダを `decl.h` と `parse.h` に分け、傘ヘッダから include する
- **Context**: 1.1〜1.9、10.1〜10.7、structure.md の「公開ヘッダに置くのは契約だけ」。
- **Alternatives Considered**:
  1. 1 つの `clopts/parser.h` にすべて置く
  2. 宣言型（`decl.h`）と解析契約（`parse.h`）に分ける
- **Selected Approach**: 2。`decl.h` は宣言型・種類・ハンドラ型・`CLOPTS_` 定数だけを持ち、`parse.h` は `decl.h` を include して状態・エラー・結果・環境変数取得関数の型・公開関数を持つ。
- **Rationale**: help-and-messages は主に `decl.h` と `parse.h` のエラー型を読む。利用者は傘ヘッダ 1 つで済む。
- **Trade-offs**: ファイルが 1 つ増える。ヘッダガードは `CLOPTS_DECL_H` / `CLOPTS_PARSE_H`。

### Decision: 結果は「公開の殻 + 不透明な実装ポインタ」
- **Context**: 3.4〜3.6、11.1〜11.8、12.1〜12.6、tech.md「内部データ構造を公開ヘッダに出さない」。
- **Alternatives Considered**: 上表の 3 案。
- **Selected Approach**: `clopts_result_t { status; command; error; impl }`。`impl` は `src/parser/parser_internal.h` の `clopts_parser_store_t` を指し、`clopts_parse` が確保し `clopts_result_free` が解放する。ゼロ初期化（`impl == NULL`）は有効な空状態。
- **Rationale**: 呼び出し側がスタックに置け、OOM でも `status` と `error` を返せ、内部は自由に変えられる。
- **Trade-offs**: 解析ごとにストアの確保が 1 回以上走る。値の参照は関数経由になる（型で守られるので利点でもある）。
- **Follow-up**: `clopts_result_free` を 2 回呼んでも安全なことと、ゼロ初期化した結果の解放をテストで固定する。

### Decision: 記述子ポインタ → スロットの対応は要素アドレスの等値比較で求める
- **Context**: 11.1〜11.3。
- **Alternatives Considered**:
  1. `opt >= options && opt < options + count` の範囲判定
  2. `for (i) if (&options[i] == opt)` の線形探索
  3. 記述子に ID を持たせる
- **Selected Approach**: 2。選択コマンドのオプション → グローバルオプションの順に探す。見つからなければ `assert` で契約違反を報告し、`NDEBUG` では未指定相当を返す（実装の都合であり、契約としては未定義）。
- **Rationale**: 1 は異なる配列のポインタ比較が C では未定義。3 は宣言の手間が増える。オプション数は数十個で線形探索で十分。
- **Trade-offs**: 参照ごとに O(n)。利用側は解析後に数回しか呼ばないので問題にならない。

### Decision: 環境変数の取得は関数ポインタ + コンテキストで差し替え、返した文字列はライブラリが `free` する
- **Context**: 7.6、7.7、7.9、platform-argv との継ぎ目。
- **Alternatives Considered**:
  1. program 宣言に関数ポインタを持たせる
  2. グローバル変数で差し替える
  3. `clopts_parse_with_env(program, argc, argv, getenv_fn, ctx, result)` で渡す。`clopts_parse` は既定の取得関数で呼ぶ
- **Selected Approach**: 3。型は `clopts_env_status_t (*clopts_getenv_fn)(void *ctx, const char *name, char **out)`。`CLOPTS_ENV_FOUND` のとき `*out` は `malloc` 系で確保された複製で、ライブラリが `free` する。`CLOPTS_ENV_UNSET` / `CLOPTS_ENV_FAILED` のとき `*out` は触らない。
- **Rationale**: 宣言は `static const` のまま、グローバル状態も持たない。既定の取得関数は `getenv` の戻り値を即座に複製する（brief の制約）。platform-argv の UTF-8 取得関数はこの型に合わせて実装し、Windows の利用者は `clopts_parse_with_env` に渡す。
- **Trade-offs**: 呼び出しが 1 つ増える。`free` の所有権が関数ポインタをまたぐが、C 標準の `free` に固定することで曖昧さをなくす。
- **Follow-up**: platform-argv の brief に「`clopts_getenv_fn` に合わせる」と明記されているので、型名とシグネチャを design.md で確定させる。

### Decision: 宣言エラーも使い方エラーも 1 つの `clopts_error_t` で返す
- **Context**: 2.17、10.1〜10.7。
- **Alternatives Considered**:
  1. `clopts_usage_error_t` と `clopts_decl_error_t` の 2 構造体
  2. 1 つの構造体に両方の種別フィールドと共通のポインタフィールドを持つ
- **Selected Approach**: 2。`status` で有効な種別フィールドが決まる。`command` / `option` / `positional` は両方で使い、`token` / `argv_index` / `short_name` / `name_len` は使い方エラーでだけ意味を持つ。
- **Rationale**: 結果構造体に埋め込みやすく、help-and-messages は `status` で分岐して 1 つの型を読めばよい。
- **Trade-offs**: 使わないフィールドが残る。コメントで「いつ有効か」を明記する。

### Decision: 走査は Monde の 4 段構成（前段 → 本走査 → 回復走査 → 走査後の検査）を踏襲し、走査後の検査に環境変数の解決を加える
- **Context**: 4.x、5.x、7.x、8.x、9.x。
- **Selected Approach**: 前段でグローバルオプション・予約語・サブコマンド名を処理し、本走査で選択コマンドとグローバルの両方を照合し、最初の誤りを保留して回復走査、走査後に「環境変数 → 既定値 → 必須」の順に解決する。
- **Rationale**: 要件 9.12 が回復走査をそのまま要求している。環境変数は「明示指定がないとき」にだけ参照するので走査後にまとめて解決するのが自然で、参照の範囲も選択されたスコープに限定できる（7.8）。
- **Trade-offs**: 走査中に状態が増える（保留エラー、`--` 到達、選択コマンド）。走査器を 1 ファイルに閉じ、状態を 1 つの構造体にまとめて見通しを保つ。

### Synthesis: 一般化と単純化
- **一般化**: 長い名前・短い名前・まとめ書き・否定形はすべて「記述子を 1 つ特定し、必要なら値を 1 つ取る」に帰着する。値の記録は種類ごとの 1 関数（FLAG: 真偽、COUNT: +1 とクランプ、VALUE: 置換、LIST: 分割して追加）にまとめ、明示指定・環境変数・既定値のどの出所からでも同じ記録関数を出所付きで呼ぶ。
- **一般化**: 「グローバルのスロット」と「選択コマンドのスロット」は同じスロット型の配列 2 本で表し、照合も記録も参照も「どちらの配列か + 添字」で統一する。サブコマンドなし構成はコマンド配列が空の特殊ケースとして同じコードで動く。
- **単純化**: 結果の値はスロット配列だけで持ち、リスト項目は `char *` の可変長配列（`clopts_core_vec_t`）で持つ。別の結果型やイテレータは作らない。
- **単純化**: 宣言検査は解析のたびに行い、キャッシュしない（数十要素の O(n²) で十分）。
- **Build vs. Adopt**: steering で外部依存ゼロ・自前実装と決まっている。getopt は MSVC になく、サブコマンド・否定・環境変数・構造化エラーの組み合わせを満たす既存の C ライブラリもない。

## Risks & Mitigations
- メモリ確保失敗の経路をテストしにくい — 環境変数取得関数に `CLOPTS_ENV_FAILED` を返させることで OUT_OF_MEMORY の経路と解放を検証する。`malloc` そのものの失敗は注入しない（tech.md が直接呼び出しを求めるため）。
- 回復走査で、誤りの後に値として書かれた `-h` がヘルプになる — 要件 9.12 が明示的に許容する挙動。テストで固定する。
- 記述子ポインタの線形探索で、未宣言の記述子が `NDEBUG` で黙って「未指定」になる — 契約違反として文書化し、デバッグビルドでは `assert` で止める。
- 宣言検査の漏れが使い方エラーに化ける — 宣言エラー種別ごとにテストを 1 件以上置き、help-and-messages の網羅テストと合わせて列挙の追加を検出する。
- `argv_index` が `int` のため `argc` の範囲に収まることが前提 — `argc` も `int` なので矛盾しない。

## References
- [POSIX.1-2017 XBD 12.2 Utility Syntax Guidelines](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html) — まとめ書き、値の連結、`--`、`-`
- [getopt(3) — Linux manual page](https://man7.org/linux/man-pages/man3/getopt.3.html) — 値トークンの消費規則
- [clap `ArgAction`](https://docs.rs/clap/latest/clap/enum.ArgAction.html) — Count、SetTrue / SetFalse（否定）の先例
- `ref/monde/.kiro/specs/cli-args/design.md` — 走査表、回復走査、宣言検査の規則表
- `.kiro/specs/project-foundation/design.md` — `core_internal.h` の契約、モジュール追加の手順
