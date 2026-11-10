# PLAN

## 宣言の API を、利用者が書くコードから決め直す（破壊的変更）

状態: 検討中。実装はまだ始めていない。下の「未決」を決めてから始める。

### 要約

内部の作りと作法は今のまま残し、宣言の公開 API だけを変える。

- 作ることと付けることを、1 回の呼び出しにする。
- 宣言エラーは Program が最初の 1 件を覚え、利用者は宣言の後で 1 回だけ確かめる。setter は `void` にする。
- `--help` と `--version` のオプションを、1 行で足せるようにする。

parse、text、process、base は変えない。変えるのは decl と、そのテスト、サンプル、文書である。

### 背景

今の宣言の API（作る → 設定する → 付ける。呼び出しごとに `clopts_error_t` を返す）は、`58aabba` の直後に入れた。そのときの設計メモ（`664a588` で消した `TODO.md`）は、動機を「操作概念として自然」「親子関係として綺麗な形」と書き、代償を「呼び出しごとに戻り値を見る必要がある」と書いている。利用者の困りごとから始めた変更ではなかった。

この代償が、見積もりより大きかった。`examples/` の同じ 2 つのサンプルを、`58aabba` と今で比べる。

| サンプル | `58aabba` | 今 | `CHECK` の数（今） |
|---|---|---|---|
| textcount | 182 行 | 265 行（+46%） | 37 |
| taskbook | 228 行 | 367 行（+61%） | 61 |

textcount で増えた 83 行の内訳は、おおよそ次のとおりである（サンプルを読んで分けた数で、正確ではない）。

| 何の行か | 行数 |
|---|---|
| help と version を自分で作る（終端の FLAG を作る関数、オプション 2 つ、振り分け） | 約 34 |
| エラー処理の道具（`CHECK` マクロ、`fail:` の後始末、戻り値の受け渡し） | 約 25 |
| 持ち主の受け渡し（`add` と `option = NULL` が 9 組） | 約 18 |
| その他 | 約 6 |

原因は 2 つある。

1. **宣言の呼び出しが、すべてエラーを返す。** `clopts_error_t` の 7 種類のうち、実行時に起きうるのは `OUT_OF_MEMORY` だけで、残りの 6 種類はプログラマの誤りである。[設計の考え方](docs/dev/design.md) の 7 章も「宣言エラーは、プログラマの誤りである。開発中に一度出て、直せば二度と出ない」と書いている。それなのに API は、製品のコードに、すべての呼び出しでの検査を書かせている。持ち主の受け渡しも、ここから来ている。作ってから付けるまでの間に setter が失敗しうるので、まだ付けていない option を利用者が追いかけて後始末することになる。
2. **help と version を、利用者に作らせている。** 「利用者が公開 API で短く書けることは src に置かない」（同 8 章）を当てはめた結果である。ただ、ほぼすべての CLI が `--help` を持ち、全員が毎回 30 行あまりを書いている。

### 考え方

**概念モデルは、内部の構成と語彙を決める。公開 API の形は、利用者が書くコードから決める。**

今の公開 API は、内部の概念モデルの写しになっている。Option、Positional、Command、Program のそれぞれに `create` / `set_*` / `add_*` があり、利用者がその部品を組み立てる。「まだ付けていない Option」はモデルとしては正しい状態だが、利用者には要らない概念で、その管理が受け渡しのコードになる。

同じモデル（付けずに作り、設定してから付ける）を、clap（Rust）も採っている。Rust では、メソッドをつなげる書き方、所有権の移動、panic が、受け渡しとエラー処理を隠す。C にはどれもないので、同じモデルの手間がそのままコードに出る。

### ほかのライブラリとの比較

私たちの知識からまとめたもので、各ライブラリの最新の文書には当たっていない。実装の前に、手本にするもの（cairo、argtable3）は確かめる。

- **宣言の呼び出しごとにエラーコードを返して、利用者に確かめさせる引数ライブラリは、調べた中にない。** panic（clap、Go の flag）、例外（argparse、CLI11、cxxopts）、検査なし（getopt_long、argp、popt、cargs）のどれかである。argtable3 は、メモリ不足を `arg_nullcheck` で最後に 1 回確かめる。
- **メモリの上で木を組み立てる C のライブラリは、作って付けるのを 1 回でするか、入れ子で書けるようにしている。** cJSON（`cJSON_AddStringToObject`）、yyjson、libxml2（`xmlNewChild`）、jansson（`json_object_set_new` は失敗しても引き取る）、GTK。
- **エラーを溜める作りには、実績のある手本がある。** cairo は、描く関数がすべて `void` で、エラーは文脈に溜まり、エラーの状態では何をしても何も起きない。利用者は最後に `cairo_status()` を 1 回見る。
- **help は、自動か 1 行である。** 自動で持つライブラリ（argp、GOption、clap）は、どれも自分で出力して終了する。CLOpts は出力も終了もしないので、「1 行で足し、利用者が出力する」が合う。
- **CLOpts が進んでいる所**: 出力も終了もしない、グローバルな状態がない、アロケーターの差し替え、Windows の argv を UTF-8 で扱う、値がどこから来たかを読める、結果をハンドルで読む。ここは変えない。

### 残すもの

- モジュールの分け方、名前の付け方、構成のテスト（[設計の考え方](docs/dev/design.md)）。
- Scope と、名前の比べ方が 1 か所にあること（`scope.c`、`lookup.c`、`clopts_scope_shadows_option`）。
- 終端の FLAG というしくみ（`clopts_option_set_terminal`、`CLOPTS_PARSE_TERMINATED`）。
- 7 種類の、ドメインを知らない `clopts_error_t`。
- `clopts_parse` が環境変数を自分で読むこと。`CLOPTS_PARSE_*`、`clopts_usage_error_t`、matches と result の分離。
- 結果をハンドルで読むこと。サブコマンドをハンドルの比較で振り分けること。
- 渡された文字列を複製すること。
- 凍結を契約にすること（検出しない）。
- 種類を `clopts_option_kind_t` で渡すこと。
- 失敗した呼び出しは何も変えないこと。宣言エラーがあっても、Program の不変条件は破れない。

### 変えるもの

#### 公開 API（案）

```c
/* 作って付ける。失敗したら NULL を返し、program が最初のエラーを覚える。 */
clopts_option_t     *clopts_program_add_option(clopts_program_t *program, clopts_option_kind_t kind,
                                               const char *long_name, char short_name);
clopts_positional_t *clopts_program_add_positional(clopts_program_t *program, const char *name);
clopts_command_t    *clopts_program_add_command(clopts_program_t *program, const char *name);
clopts_option_t     *clopts_command_add_option(clopts_command_t *command, clopts_option_kind_t kind,
                                               const char *long_name, char short_name);
clopts_positional_t *clopts_command_add_positional(clopts_command_t *command, const char *name);

/* setter は void。NULL の option には何もしない。失敗したら何も変えず、program がエラーを覚える。 */
void clopts_option_set_help(clopts_option_t *option, const char *help);
/* ほかの setter も同じ */

/* 終端の FLAG を作って付ける。 */
clopts_option_t *clopts_program_add_help_option(clopts_program_t *program);
clopts_option_t *clopts_program_add_version_option(clopts_program_t *program);

/* 宣言の後で 1 回確かめる。最初のエラーの種類。 */
clopts_error_t clopts_program_get_error(const clopts_program_t *program);
```

なくなるもの: `clopts_option_create` / `clopts_option_destroy`、`clopts_positional_create` / `clopts_positional_destroy`、`clopts_command_create` / `clopts_command_destroy`、作ったものを受け取る形の `clopts_*_add_*`。

#### 利用者が書くコード

```c
clopts_program_t *p = clopts_program_create("textcount");
clopts_program_set_description(p, "Count the lines and words of text files.");

opts.lines = clopts_program_add_option(p, CLOPTS_OPTION_FLAG, "lines", 'l');
clopts_option_set_help(opts.lines, "Count lines");

opts.encoding = clopts_program_add_option(p, CLOPTS_OPTION_VALUE, "encoding", 'e');
clopts_option_set_env(opts.encoding, "TEXTCOUNT_ENCODING");
clopts_option_set_default(opts.encoding, "utf-8");

opts.help    = clopts_program_add_help_option(p);
opts.version = clopts_program_add_version_option(p);

if (clopts_program_get_error(p) != CLOPTS_ERROR_NONE)
{
    /* 宣言の誤りか、メモリ不足 */
}

result = clopts_parse(p, argc, argv);
```

オプション 1 つあたり 2〜4 行で、今の 5〜8 行の半分ほどになる。textcount は、`58aabba` の 182 行から数行の増減に収まる見込みである（まだ書いて確かめていない）。

#### 内部（decl だけ）

1. **Program が、最初のエラーの種類を覚える。** 失敗した呼び出しは何も変えない（今の Scope の決まりのまま）。
2. **Option と Positional が、自分の Scope と Program を知る。** 付けてから設定するので、`set_negatable(true)` のときに、同じ範囲の名前とぶつからないかを確かめる必要がある。今の「Option は自分のコマンドを知らない」は手放す。検査は `scope.c` の関数を呼ぶので、名前の比べ方が 1 か所という点は保つ。
3. **NULL の扱いが変わる。** 今は「NULL の option を setter に渡すのは契約違反（assert）」である。変えた後は、足す関数が失敗すると NULL を返すので、setter は NULL を受け取ったら何もしない。

### 失うもの

- **エラーの場所。** 分かるのは最初のエラーの種類だけで、どの呼び出しかは分からない（未決 1）。
- **Option が持ち主を知らないという単純さ。** 使いやすさのために、モデルを崩す所である。
- **付けていない Option を扱うこと。** 作ったオプションを条件によって付けない、という書き方はできなくなる。足す呼び出しを条件で囲めば、同じことは書ける。
- **宣言を実行時に組み立てる使い方での細かい制御。** 最初のエラーの後の呼び出しは何もしないので、後のエラーは最初の 1 つに隠れる。

### 採らないもの

| 案 | 採らない理由 |
|----|--------------|
| `58aabba` に戻す | 内部の作り（`root` を Command で表す、予約した名前を走査が知っている、解析の結果に `CLOPTS_DECL_ERROR` が混ざる、コマンドのハンドラ、16 種類の宣言エラー、環境変数を利用者がつなぐ）は、今のほうが良い。静的ライブラリのコード（Release、x86-64、gcc 13.3）も、37,841 バイトから 32,711 バイトに減っている |
| 今の API を中心として残し、その上に書きやすい層を載せる（Lua の `lauxlib` の形） | 同じことをする書き方が 2 つになる。宣言を実行時に細かく組み立てる使い方を、今は重く見ない |
| 付ける関数が、失敗しても option を引き取るだけにする（jansson の形） | 失敗したときの考えごとは減るが、行数はほとんど減らない |
| すべての関数に、エラーを溜める引数 `clopts_error_t *` を足す | 効果は同じだが、どの呼び出しにも引数が 1 つ増える。Program が覚えれば要らない |
| プログラマの誤りを assert にする | 宣言を実行時に組み立てると、名前の重なりは実行時に起きうる。溜める形なら、どちらの使い方でも同じコードで済む |
| 表や構造体で宣言する（getopt_long、argp、GOption の形） | 公開する構造体の並びが ABI になる |
| help で自動的に出力して終了する | 出力も終了もしない方針とぶつかる |
| 文字列を借りる（複製しない） | エラーを溜めるなら、setter が失敗しうることは利用者の負担にならない。利点は速さとメモリだけになる |
| コマンドにハンドラを持たせる | Command に、宣言でないものが入る。ハンドルの比較で足りる |

### 文書の変更

実装より先に、[設計の考え方](docs/dev/design.md) を直す。

1. **考え方を 1 つ足す。** 上の「考え方」である。今の文書には、公開 API の形の決め方がない。
2. **7 章（エラー）。** 「宣言エラーはプログラマの誤り」と、呼び出しごとに確かめさせる API の食い違いを直す。
3. **8 章（src に置かないもの）。** 「利用者が短く書けるか」ではなく、「ほぼ全員が毎回書くか」で線を引き直す。
4. **10 章（採らなかった案）。** 「作る → 設定する → 付ける」の形を、理由と一緒に残す。「概念として自然」という理由で、同じ変更をもう一度しないためである。
5. **6 章。** 「読み取りだけを共通の型にし、足す関数は持ち主に残す」は変わらない。足す関数の形だけが変わる。

[アーキテクチャ](docs/dev/architecture.md) の宣言モデルの図と説明、`docs/api.md`、`README.md` のサンプルも、同じ変更で直す。

### 未決

1. **エラーのときに、原因まで覚えるか。** 種類だけにするか、原因のオプションの名前なども覚えるか。`58aabba` は、原因のハンドルを覚えていた。
2. **説明文を、足す呼び出しの引数に入れるか。** ほぼすべてのオプションに説明があるので、入れれば単純なフラグが 1 行になる。入れなければ、属性が増えても関数の形は変わらない。
3. **`clopts_program_create` の形。** 今は `clopts_error_t` を返し、ハンドルは引数で返す。名前の誤りとメモリ不足を区別するためである。ほかの関数とそろえてハンドルを返す形にすると、Program を作れなかったときのエラーの置き場所がない。
4. **エラーのある Program を `clopts_parse` に渡したとき。** 契約違反にするか、何かを返すか。
5. **help と version のオプションの名前と説明文を、変えられるようにするか。** 足す関数が返すハンドルに、setter を呼べば変えられる。短い名前（`-h`、`-V`）を付けるかは決める必要がある。
6. **宣言を実行時に組み立てる使い方を、どこまで支えるか。** 1 と 4 の答えが、これで変わる。

### 手順

1. `examples/textcount` と `examples/taskbook` を、変えた後の API で書いてみる（ライブラリは触らない）。行数と読みやすさを、今の版と `58aabba` の版と比べる。失敗したときの後始末が、`clopts_program_destroy` だけで済むことを確かめる。
2. 未決を決める。
3. cairo と argtable3 の、エラーの状態の決まりを文書とソースで確かめる。
4. [設計の考え方](docs/dev/design.md) を直す。
5. decl を書き換える（`option.c`、`positional.c`、`command.c`、`program.c`、`scope.c`、公開ヘッダ）。`abi/clopts.symbols` を同じ変更で直す。
6. `tests/decl` を書き直す。ほかのテストは、宣言を作る所だけを直す。
7. サンプル、`docs/`、`README.md`、`CHANGELOG.md` を直す。
8. サイズと行数を測り直して、この文書の数字と比べる。
9. この文書を消す。残す判断は、[設計の考え方](docs/dev/design.md) の 10 章に移してある。
