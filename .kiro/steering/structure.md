# Project Structure

> 2026-11-07 時点、実装未着手。ディレクトリ構成とターゲット作成関数は spec `project-foundation` で確定する。ここでは後続のすべての spec に共通させる規約を先に決めておく。

## Organization Philosophy

**1 ライブラリ・内部モジュール分割**。利用者に見せるのは 1 つのライブラリターゲット `CLOpts::clopts` と `include/clopts/` の公開ヘッダだけにし、内部は責務ごとのモジュール（`src/<module>/`）に分けてソースとテストを同じ名前で対にする。依存は下位モジュールへ向かう一方向とし、公開ヘッダを除いて内部モジュールのヘッダは install しない。

## Directory Patterns

### 公開ヘッダ
**Location**: `include/clopts/`  
**Purpose**: 利用者が include するヘッダ。`clopts/clopts.h` を入口（傘ヘッダ）とし、必要なら責務ごとに分けたヘッダ（例: `clopts/argv.h`）をそこから include する。公開ヘッダは内部モジュールのヘッダを include しない。  
**Example**: `#include <clopts/clopts.h>`

### 内部モジュールのソース
**Location**: `src/<module>/`  
**Purpose**: 1 つの責務の実装と内部ヘッダ。別々のライブラリターゲットにはせず、`clopts` ターゲットに `target_sources` で加える（ヘルパ関数 `clopts_add_module` 相当で統一する）。モジュール間で共有する内部ヘッダは `<module>_internal.h` と名付け、install しない。  
**Example**: `src/core/`（可変長配列、書き込みコールバック）、`src/parser/`（宣言検査、走査器、結果ストア）、`src/help/`（文面生成）、`src/platform/`（Windows の argv と環境変数）

### モジュールのテスト
**Location**: `tests/<module>/`  
**Purpose**: 対応する内部モジュールの Naos テスト。`clopts_add_module_tests` 相当で CTest に登録する。CTest のラベルは `<module>`。  
**Example**: `tests/parser/` は `src/parser/` をテストする  
**Exception**: `src/` に対応するモジュールを持たないテストディレクトリは次の 3 つに限る。
- `tests/toolchain/`: ビルド構成そのもの（警告のエラー化、VLA の禁止、グローバル設定を置いていないこと）を検証する。ラベルは `toolchain`。
- `tests/integration/`: 利用側を模したプロジェクトで `add_subdirectory` / `FetchContent` と install → `find_package(CLOpts CONFIG)` の両経路を検証する。ラベルは `integration`。
- `tests/docs/`: README と `docs/` のサンプル出力をビルドしたサンプルプログラムと照合する。Naos を使わず `cmake -P` で起動する。ラベルは `docs`。

### サンプルプログラム
**Location**: `examples/`  
**Purpose**: 利用者向けの最小の実行ファイル。サブコマンドあり・なしの 2 つを置き、`CLOPTS_BUILD_EXAMPLES` が ON のとき（既定はトップレベルのときだけ）ビルドする。文書中のヘルプ出力の出典でもあり、`tests/docs/` が照合する。

### ビルド構成
**Location**: `CMakeLists.txt`、`cmake/`  
**Purpose**: コンパイラオプション、モジュール追加・テスト登録・サンプル作成の関数、Naos の取得、install / export と package config。オプションやターゲットの作り方はここに集約し、各モジュールの `CMakeLists.txt` では個別に設定しない。ファイル名は `cmake/Clopts<Topic>.cmake`。バージョン番号を書く場所はルートの `project(clopts VERSION ...)` だけ。

### ドキュメント
**Location**: `docs/`、`README.md`  
**Purpose**: 日本語の利用者向け文書。README は概要（C++ の同名プロジェクト Sirraide/clopts との区別を含む）、最小のサンプル、取り込み方 3 通り、ライセンスを持ち、詳細は `docs/` に委ねる。  
**User chapters**: `docs/` 直下の `.md` が利用者向けの章（API リファレンス、解析規則、取り込み手順、Windows での注意）。章の間は相対リンクで結び、`docs/dev/` へはリンクしない。サンプル出力の引用には照合用のマーカー（`<!-- clopts-check: ... -->`）を付ける。  
**Maintainer docs**: `docs/dev/` は保守者向け（`release.md`: 版番号の規則とリリース手順と Naos の更新手順、`naos-feedback.md`: Naos への要望の記録）。

### CI とリリースのワークフロー
**Location**: `.github/workflows/`  
**Purpose**: `ci.yml` は main への push と pull request で動く 5 環境のビルドとテスト。`release.yml` は `vX.Y.Z` のタグで動き、タグとバージョンの一致を検査して GitHub Release を作る。リリースの変更で `ci.yml` を変えない。

### 仕様とプロジェクトメモリ
**Location**: `.kiro/specs/<feature>/`、`.kiro/steering/`  
**Purpose**: spec 駆動開発の仕様（brief → requirements → design → tasks）と、プロジェクト全体の方針。spec 名は kebab-case（例: `core-parser`、`help-and-messages`）。spec と内部モジュールの対応は roadmap.md に従う（`core-parser` → `parser`、`help-and-messages` → `help`、`platform-argv` → `platform`、`project-foundation` → `core` とビルド構成）。

## Naming Conventions

- **モジュール名**: 小文字の snake_case、1 語を基本とする（`core`、`parser`、`help`、`platform`）。ディレクトリ `src/<module>/`、`tests/<module>/` に使う。
- **CMake**: ライブラリターゲットは `clopts`、別名 `CLOpts::clopts`（リンクには別名を使う）。オプションは `CLOPTS_<NAME>`。テスト実行ファイルは `<module>_tests`、CTest のテスト名は `<module>_tests.<suite>.<test>`。
- **ファイル**: 小文字の snake_case（`.c` / `.h`）。モジュール内のファイルには `<module>_` の接頭辞を付ける（例: `src/parser/parser_scan.c`）。公開ヘッダは接頭辞なし（`include/clopts/clopts.h`）。
- **公開シンボル**: 接頭辞 `clopts_`。
  - 関数: `clopts_<名詞>_<動作>`（例: `clopts_parse`、`clopts_result_free`、`clopts_write_help`）。型名の `_t` は関数名に含めない。
  - 型: `typedef` で定義し末尾に `_t`。構造体・列挙のタグ名は `_t` を除いた同じ名前（例: `typedef struct clopts_option { ... } clopts_option_t;`）。
  - マクロ・列挙値: `CLOPTS_<名詞>_`（例: `CLOPTS_UNLIMITED`、`CLOPTS_ERROR_UNKNOWN_OPTION`）。`_T` は含めない。
- **内部シンボル**: 翻訳単位内で閉じるものは `static`。モジュール間で共有する内部シンボルは `clopts_<module>_` の接頭辞（例: `clopts_core_vec_push`）を付け、`<module>_internal.h` で宣言する。利用者のシンボルと衝突しないよう、接頭辞なしの外部リンケージは作らない。
- **ヘッダガード**: 公開ヘッダは `CLOPTS_<FILE>_H`（例: `CLOPTS_CLOPTS_H`）、内部ヘッダは `CLOPTS_<MODULE>_<FILE>_H`（例: `CLOPTS_PARSER_INTERNAL_H`）。

## Include Organization

```c
#include "clopts/clopts.h"          /* 公開ヘッダ: include/ を基点 */
#include "parser/parser_internal.h" /* 内部ヘッダ: src/ を基点（ライブラリ内部だけ） */
#include <stdio.h>                  /* 標準ライブラリ */
```

- 公開ヘッダの include 基点は `include/`（`PUBLIC`、install 後も同じパスで使える）。内部ヘッダの基点は `src/`（`PRIVATE`）。
- 内部モジュールは、依存する下位モジュールの `<module>_internal.h` だけを include する。横や上への依存を作らない。
- Windows 専用コード（`<windows.h>`）は `src/platform/` の Windows 専用の翻訳単位に閉じ込め、他のモジュールから `#ifdef _WIN32` を使わない。

## Code Organization Principles

- **依存は一方向**: core ← parser ← help、core ← platform。`help` は `parser` の公開型（公開ヘッダ）に依存するが実装には依存しない。`platform` は `parser` を知らず、`parser` は環境変数の取得関数を差し替え口として受け取る。
- **公開ヘッダに置くのは契約だけ**: 宣言型、結果型、構造化エラー型、状態の列挙、公開関数。内部のデータ構造（結果ストアの実装、可変長配列）は公開ヘッダに出さない。
- **I/O は境界の外**: ライブラリは stdout / stderr に書かず `exit` しない。文面は書き込みコールバックへ、エラーは構造体で返し、表示と終了コードの決定は利用側が行う。`stdio` への依存は `FILE*` 便宜関数の翻訳単位に閉じ込める。
- **宣言は読み取り専用**: ライブラリは宣言テーブルと argv を書き換えない。実行時に決まる値（バージョン文字列）はポインタとして宣言に持たせる。
- **テストは架空の宣言で**: 実在のツールの宣言をテストに持ち込まず、ライブラリの機能ごとに架空のプログラム宣言を用意する。

---
_Document patterns, not file trees. New files following patterns shouldn't require updates_
