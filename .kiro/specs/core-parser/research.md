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

---

# ギャップ分析: ビルダー API と不透明ハンドルへの改訂（2026-11-08）

対象: 改訂後の `requirements.md`（要件 1〜15）と、初版で実装済みのコード（`include/clopts/{decl,parse}.h`、`src/parser/`、`tests/parser/`）。

## 1. 現状

| 資産 | 行数 | 内容 | 改訂での扱い |
|---|---|---|---|
| `include/clopts/decl.h` | 147 | 公開構造体 `clopts_option_t` / `clopts_positional_t` / `clopts_command_t` / `clopts_program_t`、`clopts_kind_t` | 構造体は公開ヘッダから消す。`clopts_kind_t` は列挙として残せる |
| `include/clopts/parse.h` | 264 | 状態・使い方エラー・宣言エラー・出所・環境変数状態の列挙、公開構造体 `clopts_error_t` / `clopts_result_t`（`impl` を持つ）、`clopts_getenv_fn`（`char **out` で `malloc` した複製を返す）、`CLOPTS_API` 付きの 13 関数 | 列挙は大半を流用できる（値の明示と、種別の追加が要る）。構造体と関数のシグネチャは作り直す |
| `src/parser/parser_decl.c` | 456 | 宣言検査（テーブル順、最初の 1 件）、名前の照合（`find_command` / `find_long` / `find_negation` / `find_short`） | 照合の論理と検査の論理は流用できる。入力が「配列 + 要素数」から内部モデルに変わる |
| `src/parser/parser_scan.c` | 721 | 前段 → 本走査 → 回復走査 → 走査後の検査、の 4 段の走査器 | 走査規則はそのまま流用できる。宣言の参照（15 か所）と結果・エラーの書き込み先だけを差し替える |
| `src/parser/parser_resolve.c` | 255 | 既定の環境変数取得、環境変数 → 既定値の解決、必須検査 | 解決順と必須検査は流用できる。コールバックの契約が変わる（借用、3 値） |
| `src/parser/parser_store.c` | 442 | スロット（option と同じ添字）、リストの分割・複製、位置引数、参照 API、`result_run`、冪等な `result_free` | スロットの構造は流用できる。結果がライブラリ確保のハンドルになり、OOM の番兵と、別 program のハンドルの検出が要る |
| `src/parser/parser_internal.h` | 167 | ref（`option` + `global` + `index` + `negated`）、slot、store の内部契約 | ref が配列の添字で option を特定する設計は、内部モデルでもそのまま使える |
| `tests/parser/` | 約 5,750 | 9 スイート、約 270 件。`test_fixtures.{c,h}` に `static const` の架空宣言（`tool` / `filt` / `strict`）と環境変数の模擬関数 | 観点はすべて引き継ぐ（要件 15.8）。宣言の書き方が全面的に変わるので、全スイートの書き換えが要る |
| `src/core/` | — | `clopts_core_vec_t`（可変長配列）、`clopts_core_size_mul` / `add` | そのまま使う。宣言モデル（子の配列）と結果に使える |
| 統合テスト | — | `consumer_cpp.cpp` が `&clopts_parse` を volatile ポインタで参照し、export を確かめている。`internal_symbol` は `clopts_core_size_mul` だけを見る | `clopts_parse` のシグネチャが変わるので、`decltype(&clopts_parse)` は再コンパイルで追従する。同名の公開関数を残すかどうかで修正量が変わる |

既存の規約:
- `clopts_<名詞>_<動作>` の命名、`CLOPTS_API`、Allman スタイル、`assert` による契約違反の検出。
- 内部モジュールのヘッダは `src/<module>/<module>_internal.h`。
- 内部関数の接頭辞は `clopts_parser_`。visibility が hidden なので、共有版では外に出ない（`internal_symbol` の統合テストと同じ仕組み）。

## 2. 要件と資産の対応

タグ: **Reuse**（流用）、**Adapt**（入出力を変えて流用）、**Missing**（新規）、**Constraint**（制約）、**Unknown**（設計で調べる）

| 要件 | 対応する資産 | ギャップ |
|---|---|---|
| 1 ビルダーによる宣言 | なし | **Missing**: ハンドル型、内部の宣言モデル（子の可変長配列、複製した文字列）、追加関数・設定関数、program の解放 |
| 1.11 ハンドルの安定 | `clopts_core_vec_t` | **Constraint**: vec は `realloc` で要素が動く。要素を値で持つとハンドルが無効になる。option などを個別に確保し、vec にはポインタを持たせる必要がある |
| 1.12 解析後の凍結 | なし | **Missing**: program の「使用済み」フラグ。解析関数は `const clopts_program_t *` を受けるので、フラグの書き込みと const の扱いが **Unknown** |
| 2 最初のエラーの記録、NULL 安全 | `parser_decl.c` の `fail()` の形 | **Missing**: program に保持するエラー（種別と該当ハンドル）、全ビルダー関数の先頭での「NULL またはエラー済みなら何もしない」 |
| 2.8 / 12.3 別 program のハンドル | `find_slot` の `assert` | **Adapt**: option に所有者の program（または scope）へのポインタを持たせれば O(1) で判定できる |
| 3 宣言検査 | `parser_decl.c` | **Adapt**: 検査の時期が問題になる。「ビルダー呼び出しの順で最初に成立したもの」（3.19）を満たす方法は 2 つある。(a) 各ビルダー呼び出しの中で、その呼び出しで成立する誤りだけを即時に検査する。(b) 解析・検査関数の中でまとめて検査し、各宣言に通し番号を持たせて最小のものを選ぶ。**Unknown**（設計で選ぶ） |
| 3.16 / 3.17 新しい種別 | なし | **Missing**: 宣言エラー列挙の末尾に「種類に合わない属性」「構成の混在」を足す。help-and-messages の文面表は未実装なので、波及はない |
| 4 解析と状態 | `clopts_parse_with_env` | **Adapt**: 戻り値を結果ハンドルにする。走査の本体は流用できる |
| 4.8 OOM の番兵 | なし | **Missing**: 静的な番兵の結果オブジェクト。`result_free` は番兵を見分けて何もしない。番兵は読み取り専用なので、共有版の複数スレッドから読んでも安全 |
| 4.10 同じ program の繰り返し解析 | 現行も可 | **Reuse** |
| 5〜7, 9, 10 走査規則・値・必須・優先度 | `parser_scan.c`、`parser_store.c`、`parser_resolve.c` | **Reuse**: 規則は変えない。宣言の参照を内部モデルに差し替えるだけ |
| 8.6〜8.8 借用コールバック | `clopts_parser_getenv_default` | **Adapt**: シグネチャを「`const char **out` に借用文字列を返す」形にする。既定の実装は `getenv` の戻り値を借用として返すだけになり、複製はライブラリ側の 1 か所に集まる |
| 8.6 コールバックの渡し方 | `clopts_parse_with_env` | **Unknown**: 解析関数を 2 つ（既定用と差し替え用）にするか、1 つの関数で NULL を既定とするか |
| 11 エラーのアクセサ | 公開構造体 `clopts_error_t` | **Adapt**: 項目は同じ（種別、command、option、positional、token、argv_index、short_name）。アクセサ関数に置き換える。11.7 の「エラーなし」の値は列挙への追加が要る（または 0 を予約する） |
| 3.2 検査関数のエラー情報 | `clopts_decl_validate(program, &error)` | **Unknown**: 結果ハンドルを返して同じアクセサで読ませるか、program のアクセサで読ませるか |
| 11.8 / 12.8 文字列の寿命 | 現行は argv と宣言テーブルを借用 | **Unknown**: argv を借用する（現行どおり。「argv を保持している間」の条件が付く）か、すべてを結果に複製する（寿命が「結果を解放するまで」だけになる）か。既定値は宣言モデルが持つ複製を借用する。この場合、program を先に解放すると壊れる。13.3 は「参照関数は program を解放する前にだけ呼べる」と定めているので、両立する |
| 12.5 / 12.6 handler | `clopts_result_run` | **Adapt**: handler の型が `int (*)(const clopts_result_t *, void *)` なら、そのまま使える。ハンドル型の const 修飾の方針が **Unknown** |
| 13.3 解放順の自由 | `result_free` は program を参照しない | **Reuse**: store が program を参照しない解放処理を保てばよい |
| 13.7 ビルダー中の OOM | なし | **Missing**: 部分的に作った子を、親に繋いでから属性を設定する順序にする（漏れを防ぐ） |
| 14.1 / 14.3 公開ヘッダ | `decl.h` / `parse.h` に構造体がある | **Missing**: 構造体を `src/parser/` に移す。公開ヘッダには前方宣言だけを置く |
| 14.4 列挙の数値 | 現行は暗黙の連番 | **Adapt**: すべての列挙子に値を明示する |
| 14.5 内部の読み出し口 | なし | **Missing**: help-and-messages が読む内部ヘッダ（例: `src/parser/parser_model.h`）。読み出しの単位（構造体を直接見せるか、内部関数か）は **Unknown** |
| 15.7 OOM の注入 | なし（初版は「`malloc` 失敗は注入しない」と決めていた） | **Constraint**: tech.md は `malloc` / `free` の直接使用を求めている。ビルダー・解析・結果確保の各段で OOM を再現するには、テスト用の失敗注入の仕組みが要る。**Unknown**（Research Needed） |
| 15 テスト | `tests/parser/` の約 270 件 | **Adapt**: 宣言を書く部分（`test_fixtures.c` と、各スイート内の `static const` の表）を、すべてビルダー呼び出しに書き換える。検証部分の多くは、参照関数の名前を変えるだけで残せる |

## 3. 実装の選択肢

### 案A: 既存のモジュールを拡張する（内部構造体を流用する）
`decl.h` の構造体を、ほぼそのまま `src/parser/` の内部モデルにする。ビルダーは、その構造体の配列（ポインタ配列）を組み立てる。走査器・検査・store は、`option->long_name` などのフィールド参照をそのまま使う。
- ✅ `parser_scan.c` / `parser_resolve.c` / `parser_store.c` の変更が最小になる（フィールド名を保てば、ほぼ型名の置換で済む）。
- ✅ 走査規則が変わらないことを、既存テストの書き換え後の全件合格で示せる。
- ❌ 「配列 + 要素数」を前提にした箇所（`options[i]`、`option_count`）は、「ポインタの vec」に合わせる必要がある。
- ❌ 宣言検査はテーブル順のままになる。3.19 の「呼び出し順」と食い違う場合がある（例: グローバルオプションを後から足した場合の重複）。

### 案B: 宣言層を新しく作る
新しいファイルを作る:
- `parser_build.c`（ビルダーと即時検査）
- `parser_model.h`（内部モデルと読み出し口）
- `parser_result.c`（結果ハンドル、番兵、アクセサ）

宣言検査は、ビルダー呼び出しの中で即時に行う。そのため、初版の `parser_decl.c` の検査部分は、照合関数だけを残して作り直す。
- ✅ 3.19（呼び出し順で最初のエラー）と 2.1（program への記録）を、構造的に満たせる。
- ✅ 内部モデルの読み出し口を、help-and-messages のために最初から整えられる。
- ❌ 検査の論理を、追加・設定関数の各所に分散させることになる。属性の設定順に依存する誤り（必須を設定してから既定値を設定するなど）を、両方の設定関数で検査する必要がある。
- ❌ 変更量が多い。

### 案C: 両者の折衷（推奨の候補）
- **内部モデル**: 案A に近い構造体を `parser_model.h` に置き、子はポインタの vec で持つ。走査・解決・store は案A のとおり流用する。
- **ビルダー**: 新しいファイル `parser_build.c` に置く。ビルダー呼び出しの中で行うのは、その場で決まる誤りの検査（名前の形式、重複、予約名、構成の混在など）と OOM の記録である。
- **組み合わせで決まる誤り**: 種類と属性の組み合わせ（既定値・環境変数・必須・否定・区切り文字・上限）と位置引数の順序が該当する。これらは、各ビルダー呼び出しに通し番号を付けておき、凍結時（最初の解析・検査）にまとめて検査する。番号が最小のものを選べば、呼び出し順を再現できる。
- ✅ 走査器を守りながら、3.19 を満たせる。
- ✅ 検査の論理の分散を、名前に関わるものだけに抑えられる。
- ❌ 通し番号という仕掛けが 1 つ増える。「その時点で成立したもの」の定義を設計で表にする必要がある。

## 4. 規模とリスク

- **規模: L（1〜2 週間）**。走査規則は流用できるが、変更は公開 API の全面的な置き換え、内部モデルの新設、約 5,750 行のテストの宣言部分の書き換え、OOM 注入の新設に及ぶ。
- **リスク: Medium**。規則も技術も既知で、初版のテストが回帰の網になる。不確かさが残るのは次の 3 点で、いずれも設計で決められる範囲である:
  - 検査の時期（3.19）
  - OOM 注入の方法（15.7）
  - 文字列の寿命（11.8 / 12.8）

## 5. 設計フェーズへの申し送り

### 決めること
1. 宣言検査の時期: 即時か、凍結時か、案C の混合か。3.19 の「成立した時点」の定義表。
2. ハンドルの const 修飾の方針。ビルダーは非 const、解析は `const clopts_program_t *` で受けたい。しかし凍結フラグ（1.12）の書き込みが要る。選択肢は、const を外す、凍結を内部の可変フィールドとして扱う、凍結を「解析の時点で宣言検査を行う」と読み替える、のいずれか。
3. 解析関数の形: 既定の環境変数用と差し替え用の 2 関数にするか、`getenv_fn == NULL` を既定とする 1 関数にするか。番兵との関係も決める。
4. 検査関数の戻し方: 結果ハンドルを流用するか、program のアクセサにするか。
5. 値の寿命: argv を借用するか、結果に複製するか。既定値の借用と、program の解放順（13.3）の関係も決める。
6. 「エラーなし」を表す列挙値と、全列挙子への数値の明示（14.4）。
7. 内部の読み出し口の形: 構造体を直接見せるか、内部関数にするか。help-and-messages が必要とする項目（宣言順、名前、説明文、値名、既定値、環境変数名、必須、否定可能、種類、バージョン文字列）を列挙する。
8. 公開関数の名前の一覧。structure.md の `clopts_<名詞>_<動作>` に沿わせる。abi-compat の公開シンボル一覧の初版にもなる。

### Research Needed
- **OOM 注入**（15.7）: 次のどれにするかを、tech.md の「`malloc` を直接使う」との整合を含めて決める。
  - テスト専用ビルド（`clopts_testing` 静的コピー）でだけ有効な、内部の確保関数の差し替え口
  - リンク時の差し替え
  - 確保回数の上限によるカウンタ方式

  `clopts_testing` は共有ビルドでもテスト用の静的コピーとして存在する（project-foundation 7.2）ので、ここにだけ差し替え口を入れれば、公開 API と ABI に影響しない。
- **番兵の結果オブジェクト**: `const` な静的オブジェクトを、非 const のハンドルとして返す場合の扱い。C99 で未定義動作にならない形（書き込まないことの保証）を確認する。
- **統合テストの追従**: `consumer_cpp.cpp` の `decltype(&clopts_parse)` は、名前を保てばそのまま通る。名前を変えるなら fixture の修正が要る（requirements の In scope で、本 spec が直すと決めてある）。

---

# 設計判断（2026-11-08 改訂）

## Summary
- **Feature**: core-parser（ビルダー API と不透明ハンドルへの改訂）
- **Discovery Scope**: Extension（ライトな調査）。走査規則は初版の実装を流用し、新しい部分は宣言層・結果ハンドル・確保プローブだけである。外部ライブラリは使わない。
- **参照したスキル・ガイド**: `kiro-spec-design` の design-principles、design-discovery-light、design-synthesis、design-review-gate。ドメイン固有のスキルは該当なし。
- **Key Findings**:
  - 宣言エラーの規則はどれも、「その呼び出しで新しく成立する誤り」として、その呼び出しの中で判定できる。設定関数で付ける属性にも、その場で種類の検査を掛けられる。そのため、3.19（呼び出し順で最初の誤り）に通し番号や凍結時の一括検査は要らない。
  - 静的ビルドのテストは `clopts` に、共有ビルドのテストは `clopts_testing` にリンクする。したがって、テスト専用の口は、hidden な内部関数として常に置けば足りる。ビルド構成（project-foundation の境界）を変えずに済む。
  - CI は Release 構成（`NDEBUG`）なので、契約違反の「リリースでは何もしない」側をテストできる。

## Design Decisions

### Decision: 宣言エラーはすべてビルダー呼び出しの中で即時に検査する
- **Context**: 2.1、3.3〜3.19。
- **Alternatives Considered**:
  1. 凍結時（最初の解析・検査）にまとめて検査し、各事実に通し番号を付けて、成立順の最小を選ぶ。
  2. 即時検査と凍結時の検査を混ぜる（ギャップ分析の案C）。
  3. すべて即時に検査する。
- **Selected Approach**: 3。規則表は呼び出しごとに定める（design.md の「即時検査の規則表」）。
- **Rationale**:
  - 位置引数の必須・可変長は、設定関数の中で前後の位置引数と比べれば判定できる。種類と属性の組み合わせは、設定関数の中で種類を見れば判定できる。version との衝突は、`set_version` と option の追加の両方で判定する。
  - 成立した時点がそのまま呼び出しの順になるので、3.19 を構造的に満たす。凍結時の検査がなくなり、解析の入口が単純になる。
- **Trade-offs**: 衝突の検査を、追加関数と設定関数の両方に書く必要がある（例: 否定形の衝突、必須と既定値）。規則関数を Decl に集め、Build は呼ぶ順だけを持つ。

### Decision: program 直下を root という command ハンドルで表す
- **Context**: 1.3〜1.5、1.7。追加関数は、program と command のどちらも追加先として受け付ける必要がある。
- **Alternatives Considered**:
  1. program 用と command 用に、追加関数を 2 組用意する（10 関数）。
  2. 共通のスコープ型と、変換関数を設ける。
  3. program が root の command を内蔵し、`clopts_program_root` で取り出す。
- **Selected Approach**: 3。追加関数・説明文・handler は command のハンドルで受ける。version と サブコマンドの追加だけを program で受ける。
- **Rationale**: 公開関数の数が最小で、型の安全性も保てる。内部では初版の「グローバルのスロット / 選択コマンドのスロット」の区別がそのまま root / 選択コマンドに対応する。
- **Trade-offs**: 利用側は `clopts_program_root` を 1 回呼ぶ必要がある。結果やエラーで root を command として返すと紛らわしいので、要件どおり NULL で報告する。

### Decision: 公開関数の命名は `clopts_<ハンドル>_<動作>` に揃える
- **Context**: structure.md の命名規則。roadmap の例（`clopts_add_flag`、`clopts_opt_env`）は仮の名前である。
- **Selected Approach**: `clopts_program_add_command`、`clopts_command_add_value`、`clopts_option_set_env`、`clopts_result_error_token` など。初版の `clopts_parse`、`clopts_parse_with_env`、`clopts_result_*` は名前を保つ（consumer fixture の `decltype(&clopts_parse)` がそのまま通る）。

### Decision: 結果から得る文字列は argv を借用する
- **Context**: 11.8、12.8、13.3。
- **Alternatives Considered**:
  1. すべてを結果に複製する（寿命は「結果を解放するまで」だけになる）。
  2. argv と既定値を借用する（初版どおり）。
- **Selected Approach**: 2。
- **Rationale**: 位置引数や値の複製が要らず、確保点が減る（15.7 の網羅試験も短くなる）。13.3 で、アクセサは program を解放する前にだけ呼べると決めているので、既定値の借用と矛盾しない。argv は通常 `main` の間生きている。
- **Trade-offs**: 利用側が argv を一時バッファで作る場合（platform-argv の wargv 変換など）は、その寿命に注意が要る。API リファレンスに明記する。platform-argv は、取得した argv をライブラリが所有し、明示的に解放する方針なので、問題はない。

### Decision: OOM の番兵は `static const` の結果オブジェクトにする
- **Context**: 4.8、13.1。
- **Selected Approach**: `static const struct clopts_result` を `(clopts_result_t *)` にキャストして返す。全アクセサは `const` で受ける。`clopts_result_free` はアドレスで見分けて何もしない。`clopts_result_run` は `parsed` が偽なので `assert` で止まる。
- **Rationale**: C99 では、`const` オブジェクトのポインタから `const` を外すこと自体は未定義動作ではない。書き込まない限り問題はない。グローバルな可変状態を持たずに済み、スレッド間でも安全である。

### Decision: 失敗注入と漏れの検出は、program ごとの確保プローブで行う
- **Context**: 15.7。初版の「`malloc` の失敗は注入しない」を覆す必要がある。
- **Alternatives Considered**:
  1. グローバルな確保関数の差し替え（グローバルな可変状態が増える）。
  2. リンク時の `malloc` の差し替え（OS ごとに方法が違い、共有版では効かない）。
  3. サニタイザ（ASan / LeakSanitizer）の CI ジョブを足す（project-foundation の CI の境界。MSVC では漏れを検出できない）。
  4. program に付ける確保プローブ（失敗位置の指定と、生存ブロックの計数）。
- **Selected Approach**: 4。parser のすべての確保を `parser_alloc.c` の窓口に通す。プローブが NULL のときは、`malloc` などを直接呼ぶだけ。
- **Rationale**: グローバル状態がなく、全 OS・両形式で同じテストが動く。窓口は `malloc` / `free` を直接呼ぶので、tech.md の規約の範囲に収まる。
- **Trade-offs**: 確保点は「呼び出し」の単位で数えるので、実際には確保しない `vec_push` も確保点に含まれる。網羅のためには十分である。`clopts_program_new` は 1 ブロックで確保し、プローブの対象外にする。

### Decision: 環境変数のコールバックは `const char **value` に借用を返す 3 値の形にする
- **Context**: 8.6〜8.8、10。platform-argv の brief。
- **Selected Approach**: `clopts_env_status_t (*)(void *ctx, const char *name, const char **value)`。FOUND / UNSET / FAILED の 3 値。ライブラリは戻った直後に複製する。既定の取得は `getenv` の戻り値をそのまま返す。
- **Rationale**: コールバック側の確保をライブラリが `free` しないので、CRT の境界をまたがない（steering の ABI 規律）。platform-argv は、ctx が持つバッファに UTF-8 の値を書いて貸せばよい。

## Synthesis
- **一般化**:
  - root も command として扱い、追加・説明文・handler の関数を 1 組にした。
  - スロットの記録関数は初版と同じく種類ごとに 1 つで、明示指定・環境変数・既定値のどの出所からも呼ぶ。
- **Build vs. Adopt**: steering で外部依存ゼロと決まっている。ビルダー・失敗注入とも自前で作る。サニタイザは補助としてローカルで使えるが、要件の検証手段には採らない。
- **単純化**:
  - store と結果の殻を 1 つの構造体にした。
  - 凍結時の一括検査と通し番号をなくした。
  - 種類の列挙を公開ヘッダから外した（追加関数が種類を決めるため、利用側は要らない）。

## Risks & Mitigations
- 公開関数が約 48 個に増え、abi-compat の一覧の保守が増える。設計の宣言から一覧を作り、追加のたびに更新する（Revalidation Triggers）。
- 凍結フラグを書くので、同じ program を複数のスレッドから同時に解析できない。Non-Goals と API の説明に明記する。
- 即時検査の規則が追加関数と設定関数に分散する。規則表の行ごとにテストを置く（`test_decl_check.c`）。
- 初版のテストを書き換える際に観点を落とす。検証の部分は関数名の置換に留め、件数を初版と比べる（15.8）。
