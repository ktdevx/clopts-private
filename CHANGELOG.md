# 変更履歴

CLOpts の版ごとの変更をこのファイルに記録する。版番号は [Semantic Versioning](https://semver.org/lang/ja/) に従い、`major.minor.patch` の形をとる。1.0.0 より前は、minor が上がるごとに互換を壊すことがある。共有ライブラリの ABI の保証の範囲は [ABI の互換性](docs/abi.md) の章にある。

書式は次のとおりである。

- 先頭の `## [Unreleased]` の節に、まだリリースしていない変更を集める。リリースのときに、この節を `## [X.Y.Z] - YYYY-MM-DD`（版番号とリリース日）の見出しに変え、その上に新しい `## [Unreleased]` の節を作る。
- 版の節は、新しい版から順に並べ、同じ版を 2 回書かない。
- 各節の変更は、`### 互換性のない変更`、`### 追加`、`### 変更`、`### 非推奨`、`### 削除`、`### 修正`、`### セキュリティ` の分類に分けて書く。使わない分類は書かなくてよい。
- 互換を壊す変更は、ほかの分類とは別に `### 互換性のない変更` に書き、その節の最初の分類として置く。利用側がしなければならない対応も一緒に書く。

## [Unreleased]

### 追加

- サブコマンド、オプション（フラグ、カウント、単一値、リスト値）、位置引数を関数で宣言する API。プログラムを名前で作り（`clopts_program_create`）、グローバルオプション・位置引数・サブコマンドを渡して埋める（`clopts_program_add_option`、`clopts_program_add_positional`、`clopts_program_add_command`）。オプションと位置引数は単独で作って設定し（`clopts_option_create`、`clopts_positional_create`）、プログラムかコマンドに渡す（`clopts_command_add_option`、`clopts_command_add_positional`）。サブコマンドも単独で作って埋めてから（`clopts_command_create`）プログラムに渡す。所有権は渡すのに成功したときだけ移る。オプションと位置引数は自分を渡したプログラムやコマンドを、コマンドは自分を渡したプログラムを知らず、渡した後は読み取り専用になる。最初に解析するか文面を出した後はプログラムを変えない契約なので、同じプログラムを複数のスレッドから同時に解析できる。
- 宣言を読み取る関数（`clopts_program_get_command_at`、`clopts_option_get_long_name` など）。プログラム自身とサブコマンドが持つオプションと位置引数の並びは、Scope（`clopts_program_get_scope`、`clopts_command_get_scope`、`clopts_scope_get_option_at` など）として同じ関数で読む。サブコマンドがグローバルオプションを隠すかは `clopts_scope_shadows_option` で分かる。ヘルプの文面もこの関数だけで宣言を読む。
- 宣言の誤りは、誤った呼び出しの戻り値（`clopts_decl_error_t`）としてすぐに返り、失敗した呼び出しは何も変えない。そのため、組み立てたプログラムはつねに正しく、解析が宣言の誤りを返すことはない。オプションの名前の重なりはグローバルオプションの中と同じコマンドの中だけで検査し、グローバルオプションとサブコマンドで同じ名前を使うと、サブコマンドの名前がグローバルオプションを隠す。予約された名前はない。宣言の誤りの説明は `clopts_decl_error_get_message` で得られる。
- 宣言に従った argv の解析（`clopts_parse`）。`--` と `-` 単体、オプションと位置引数の並べ替え、`--no-<name>`、カウント、環境変数と既定値を含む値の優先順位、値の出所の取得を扱う。解析器に組み込みのオプションやコマンドはなく、終端オプション（`clopts_option_set_terminal`）が現れるとそこで解析を止めて `CLOPTS_PARSE_TERMINATED` を返す（`clopts_result_get_terminal_option`）。
- 使い方の誤りを表す使い方エラー（`clopts_usage_error_t`、`clopts_result_get_usage_error`）。種別（`clopts_usage_error_get_kind`）、該当するオプション・位置引数・トークン、argv の添字をアクセサで読める。エラーの起きたコマンドは、結果の選ばれたコマンド（`clopts_result_get_command`）である。
- 宣言から作るヘルプ、使い方、版、エラーの文面の出力（`clopts_write_help`、`clopts_write_usage`、`clopts_write_version`、`clopts_write_error`、`clopts_write_usage_hint`）。`-h` / `--help` と `--version` は終端の FLAG として、`help` コマンドはサブコマンドとして、ふつうに宣言する。ライブラリ自身は入出力も `exit` もせず、文面は書き込みのコールバックに渡す。`FILE*` に出すための `clopts_file_write` を公開ヘッダに置く。
- ライブラリ全体で使うアロケーターを差し替える `clopts_set_allocator` と、その 4 つの関数（`malloc_fn`、`calloc_fn`、`realloc_fn`、`free_fn`）をまとめる構造体 `clopts_allocator_t`（`clopts/base/mem.h`）。ライブラリの確保と解放は、すべてこのアロケーターを通る。
- Windows のコマンドラインから UTF-8 の argv を得る `clopts_argv_create`（他の OS では変換しない）と、`wmain` 向けの `clopts_argv_create_from_wide`（`clopts/process/argv.h`）。解析は、宣言した環境変数をどの OS でもプロセスの環境から UTF-8 で読み（Windows では `GetEnvironmentVariableW` で読んで変換する）、UTF-8 にできない値は使い方の誤り（`CLOPTS_USAGE_ERROR_INVALID_ENV_VALUE`）として報告する。プロセスの環境の代わりに値を渡す `clopts_parse_with_env`。
- 静的ライブラリと共有ライブラリのどちらでもビルドできる CMake の構成（`CLOPTS_BUILD_SHARED`）。`add_subdirectory`、`FetchContent`、install してからの `find_package(CLOpts CONFIG)` の 3 通りで取り込め、install 経路では install した形式がそのまま使われ（1 つの prefix に 1 つの形式）、`CLOpts_LIBRARY_TYPE` で確かめられる。
- 共有ライブラリの ABI の検査（ラベル `abi`）。公開シンボル一覧 `abi/clopts.symbols` と export されるシンボルの照合、公開ヘッダの規則の検査、C と C++ からのヘッダの取り込みの検査、CI での前の版との abidiff による比較。版から SOVERSION を決める規則。
- 文書とサンプル。README、`docs/` の各章（API リファレンス、解析規則、取り込み手順、Windows での注意、ABI の互換性）、サンプルプログラム `examples/taskbook`（サブコマンドあり）と `examples/textcount`（サブコマンドなし）。文書に載せたコードと出力の例は、テスト（ラベル `docs`）で実際のソースと実行結果に照合する。
