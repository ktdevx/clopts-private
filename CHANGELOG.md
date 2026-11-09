# 変更履歴

CLOpts の版ごとの変更をこのファイルに記録する。版番号は [Semantic Versioning](https://semver.org/lang/ja/) に従い、`major.minor.patch` の形をとる。1.0.0 より前は、minor が上がるごとに互換を壊すことがある。共有ライブラリの ABI の保証の範囲は [ABI の互換性](docs/abi.md) の章にある。

書式は次のとおりである。

- 先頭の `## [Unreleased]` の節に、まだリリースしていない変更を集める。リリースのときに、この節を `## [X.Y.Z] - YYYY-MM-DD`（版番号とリリース日）の見出しに変え、その上に新しい `## [Unreleased]` の節を作る。
- 版の節は、新しい版から順に並べ、同じ版を 2 回書かない。
- 各節の変更は、`### 互換性のない変更`、`### 追加`、`### 変更`、`### 非推奨`、`### 削除`、`### 修正`、`### セキュリティ` の分類に分けて書く。使わない分類は書かなくてよい。
- 互換を壊す変更は、ほかの分類とは別に `### 互換性のない変更` に書き、その節の最初の分類として置く。利用側がしなければならない対応も一緒に書く。

## [Unreleased]

### 追加

- サブコマンド、オプション（フラグ、カウント、単一値、リスト値）、位置引数を関数で宣言する API。宣言は下から上へ組み立てる。オプションと位置引数は単独で作って設定し（`clopts_option_create`、`clopts_positional_new`）、コマンドに渡す（`clopts_command_add_option`、`clopts_command_add_positional`）。コマンドも単独で作り（`clopts_command_new`）、root とサブコマンドをまとめて `clopts_program_new` に渡してプログラムを作る。所有権は渡すのに成功したときだけ移る。オプションと位置引数は自分を渡したコマンドを、コマンドは自分を渡したプログラムを知らず、渡した後は読み取り専用になる。プログラムは作った後は変わらないので、同じプログラムを複数のスレッドから同時に解析できる。
- 宣言を読み取る関数（`clopts_program_command_at`、`clopts_command_option_at`、`clopts_option_long_name` など）。ヘルプの文面もこの関数だけで宣言を読む。
- 宣言の誤りは、誤った呼び出しの戻り値（`clopts_decl_error_t`）としてすぐに返り、失敗した呼び出しは何も変えない。そのため、組み立てたプログラムはつねに正しく、解析が宣言の誤りを返すことはない。オプションの名前の重なりは同じコマンドの中だけで検査し、root とサブコマンドで同じ名前を使うと、サブコマンドの名前が root の名前を隠す。予約された名前はない。宣言の誤りの説明は `clopts_decl_error_message` で得られる。
- 宣言に従った argv の解析（`clopts_parse`）。`--` と `-` 単体、オプションと位置引数の並べ替え、`--no-<name>`、カウント、環境変数と既定値を含む値の優先順位、値の出所の取得を扱う。解析器に組み込みのオプションやコマンドはなく、終端オプション（`clopts_option_set_terminal`）が現れるとそこで解析を止めて `CLOPTS_TERMINATED` を返す（`clopts_result_terminal`）。
- 種別、該当するコマンド・オプション・トークン、argv の添字をアクセサで読める、使い方の誤りの構造化エラー。
- 宣言から作るヘルプ、使い方、版、エラーの文面の出力（`clopts_write_help`、`clopts_write_usage`、`clopts_write_version`、`clopts_write_error`、`clopts_write_usage_hint`）と、それと組で使う `-h` / `--help`・`--version`・`help` コマンドを宣言する関数（`clopts_help_add_help_option`、`clopts_help_add_version_option`、`clopts_help_command_new`、`clopts_help_target`）。ライブラリ自身は入出力も `exit` もせず、文面は書き込みのコールバックに渡す。`FILE*` に出すための `clopts_file_write` を公開ヘッダに置く。
- ライブラリ全体で使うアロケーターを差し替える `clopts_set_allocator` と、その 4 つの関数（`malloc_fn`、`calloc_fn`、`realloc_fn`、`free_fn`）をまとめる構造体 `clopts_allocator_t`（`clopts/alloc.h`）。ライブラリの確保と解放は、すべてこのアロケーターを通る。`clopts_allocator_t` の配置は凍結し、拡張するときは別の型と関数を足す。
- Windows のコマンドラインから UTF-8 の argv を得る `clopts_argv_get`（他の OS では変換しない）と、`wmain` 向けの `clopts_argv_from_wide`。環境変数を UTF-8 で引く `clopts_env_new` と `clopts_env_lookup`。
- 静的ライブラリと共有ライブラリのどちらでもビルドできる CMake の構成（`CLOPTS_BUILD_SHARED`）。`add_subdirectory`、`FetchContent`、install してからの `find_package(CLOpts CONFIG)` の 3 通りで取り込め、install 経路では install した形式がそのまま使われ（1 つの prefix に 1 つの形式）、`CLOpts_LIBRARY_TYPE` で確かめられる。
- 共有ライブラリの ABI の検査（ラベル `abi`）。公開シンボル一覧 `abi/clopts.symbols` と export されるシンボルの照合、公開ヘッダの規則の検査、C と C++ からのヘッダの取り込みの検査、CI での前の版との abidiff による比較。版から SOVERSION を決める規則。
- 文書とサンプル。README、`docs/` の各章（API リファレンス、解析規則、取り込み手順、Windows での注意、ABI の互換性）、サンプルプログラム `examples/taskbook`（サブコマンドあり）と `examples/textcount`（サブコマンドなし）。文書に載せたコードと出力の例は、テスト（ラベル `docs`）で実際のソースと実行結果に照合する。
