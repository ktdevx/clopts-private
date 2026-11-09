# TODO

## オプションを単独で作り、コマンドにアタッチする API へ変更する（破壊的変更）

状態: 実装済み。API・ABI の互換は考えず、親子関係として綺麗な形を前提に検討した。下の「未決」は実装で決めた内容に置き換えた。

### 動機

現在、`clopts_option_t` は `clopts_command_add_flag` / `_count` / `_value` / `_list` でしか作れず、生成と同時にスコープへ登録される（`src/parser/parser_build.c` の `build_add_option`）。「オプションを作る → 設定する → コマンドに渡す（所有権を移す）」の方が操作概念として自然なので、そちらへ寄せる。

### 方針 0: オプションは親を知らない

子が親へのポインタを持つと、生成時に親が必要になり、「単独で作ってから渡す」と相容れない。オプションは自分の属性（種類、名前、ヘルプ、既定値など）だけを持ち、`program` / `scope` / `index` を持たないようにする。

現状 `struct clopts_option`（`src/parser/parser_model.h`）の `program` / `scope` / `index` は次に使われている。それぞれ親なしで済ませる方法:

| 現状の用途 | 使っている場所 | 親なしでの扱い |
|------------|----------------|----------------|
| 宣言エラーの記録先（program に最初の 1 件だけ記録） | setter（`build_reject_option`） | 方針 2 で即座に戻り値で返すので、記録先が要らない |
| 凍結・エラー済みの判定 | setter の `build_begin(option->program)` | アタッチ後の setter を禁止する（下記 1）ので、program の凍結を見る必要がない。オプション自身の `attached` フラグ 1 つで判定する |
| スコープを見る検査（重複、予約名、否定形の衝突。root とサブコマンドの間もまたぐ） | 追加時と `set_negatable`（`build_has_negation_name`） | アタッチの関数が、受け取る側（command と program）の情報で判定する。オプションが親を知る必要はない |
| 結果のスロットを引く（`option->scope` で root かサブコマンドかを判定、`option->index` でスロット番号） | `find_slot`（`src/parser/parser_result.c`） | 結果がポインタで引く（下記） |
| 結果のアクセサの「別の program のオプション」の assert | `find_slot` | 失う。見つからないオプションは「値なし」として扱う |

解析中の参照（`src/parser/parser_decl.c` の `ref.index`）はすでにスコープの vec 内の位置から index を計算しているので、`option->index` に依存していない。

**結果のスロットをポインタで引く方法**: `clopts_result_flag(result, option)` などは、root と選ばれたコマンドのオプション vec から `option` と同じポインタを探し、その位置のスロットを返す。

- 単純には線形探索（1 回のアクセスで O(root のオプション数 + コマンドのオプション数)）。CLI のオプション数なら十分。
- 気になるなら、解析の終わりに結果がポインタでソートした表を作り、二分探索にする（結果の確保が増える）。

オプションに残る「状態」は `attached`（アタッチ済みか）だけになる。これは親を指すものではなく、二重アタッチとアタッチ後の setter を契約違反として止めるためのもの。

同じ考え方は位置引数にも適用する（`program` / `scope` / `index` を持たない）。コマンドについては「その他の決めること」の 3 を参照。

### 方針 1: 所有権は成功したときだけ移る（失敗時は副作用なし）

一般的な親子関係の API は、ほぼ「アタッチは成功したら所有権が移り、失敗したら何も変わらず呼び出し側が持ち続ける」という原子的な意味になっている。

- DOM `appendChild`: 失敗すると例外になり、木もノードも変わらない。
- C++ の `std::unique_ptr` を受け取る関数: 例外が出たら所有権は呼び出し側に残る（強い保証）。
- libxml2 `xmlAddChild`: 失敗すると NULL を返し、子の解放は呼び出し側の責任のまま。

これに合わせる。

- `clopts_command_add_option(command, option)` は成功したら `CLOPTS_DECL_NONE` を返し、以降 `option` は command が所有する（呼び出し側の手元のポインタは program が生きている間の借用参照になる）。
- 失敗したら、エラーの種類を返し、command も option も変わらない。option は呼び出し側が持ち続けるので、直して再アタッチするか `clopts_option_free` で解放する。
- 「失敗しても必ず移る」（前案）は、壊れたオブジェクトを木に入れることになり、木の不変条件（「つながっているものはすべて正しい」）が崩れるのでやめる。

### 方針 2: エラーは即座に返す。オプション・program にエラー状態を持たせない

宣言エラーは利用者の入力ではなくプログラマの誤りなので、溜めておいて後で解析結果として返す理由は薄い。各呼び出しの戻り値で即座に返す。

宣言エラーは、何を見れば判定できるかで 2 つに分かれる。

| 分類 | 種別 | 判定する場所 |
|------|------|--------------|
| オプション単体で決まる（コマンドの使い方に関係しない） | `INVALID_NAME`（名前の形式）、`NO_NAME`、`NOT_NEGATABLE`、`DEFAULT_NOT_ALLOWED`、`ENV_NOT_ALLOWED`、`REQUIRED_WITH_DEFAULT`、`REQUIRED_NOT_ALLOWED`、`ATTRIBUTE_KIND` | `clopts_option_new` / `clopts_option_set_*` が即座に返す |
| 兄弟・親との関係で決まる（コマンドの使い方＝CLI の形に関係する） | `DUPLICATE_OPTION`、`RESERVED_NAME`、`NEGATION_CONFLICT`、（位置引数）`POSITIONAL_ORDER`、`VARIADIC_POSITION`、（コマンド）`DUPLICATE_COMMAND`、`MIXED_LAYOUT` | アタッチ（`clopts_command_add_*` / `clopts_program_add_command`）が即座に返す |

後者もアタッチの時点で判定でき、しかも方針 1（失敗したら何も変えない）を守るにはアタッチの戻り値で返すしかない。したがって後者も溜めずに即座に返す。

結果として:

- オプションにも program にもエラー状態は要らない（`program->error` と `build_begin` のエラー済み判定は廃止）。
- 宣言が通った program は常に正しいので、`clopts_parse` は宣言エラーを返さなくなる。`CLOPTS_DECL_ERROR`、`clopts_validate`、宣言エラー用の `clopts_result_error_*`、`clopts_write_error` の `invalid declaration` の文面は削除できる。
- 凍結後の変更は今と同じく契約違反（assert）のまま。

代償: 呼び出しごとに戻り値を見る必要がある。見なければ、そのオプションが黙って無いだけになる。`CLOPTS_NODISCARD`（`warn_unused_result` / `[[nodiscard]]`）を付けてコンパイラに警告させる。

### 方針 3: 生成関数は種類を引数で受ける 1 つにする

種類ごとに `clopts_option_new_flag` / `_count` / `_value` / `_list` を分けず、種類を公開の enum で渡す 1 つの関数にする。

- 公開の関数が 4 つから 1 つになる（ABI の記号も減る）。
- 種類を値として扱えるので、表（配列）からオプションをまとめて宣言する、といった書き方ができる。
- 代償: 範囲外の種類を渡せてしまう。`CLOPTS_DECL_INVALID_KIND`（仮）を返す。
- 種類の enum は ABI の一部になるので、値を明示して固定する。内部の `clopts_parser_kind_t` はこの公開 enum に置き換える。
- 位置引数は名前の規則（短い名前なし、`<NAME>` 表記）も属性も違うので、オプションの種類には含めず別の生成関数のままにする。

### 提案 API

```c
/* 種類。値は ABI の一部 */
typedef enum clopts_option_kind
{
    CLOPTS_OPTION_FLAG = 0,   /* 値を取らない。与えられたら true、否定可能なら --no-<name> で false */
    CLOPTS_OPTION_COUNT = 1,  /* 値を取らない。出現回数を数える */
    CLOPTS_OPTION_VALUE = 2,  /* 値を 1 つ取る。繰り返したら最後が勝つ */
    CLOPTS_OPTION_LIST = 3    /* 値を取り、繰り返しを連結して区切り文字で分ける */
} clopts_option_kind_t;

/* 生成。不正な名前・種類と OOM を区別するため、戻り値はエラー、オブジェクトは out で返す */
CLOPTS_API clopts_decl_error_t clopts_option_new(clopts_option_kind_t kind, const char *long_name, char short_name,
                                                 clopts_option_t **out);

/* 未アタッチのオプションだけを解放する。アタッチ済みを渡すのは契約違反（assert）。NULL は何もしない */
CLOPTS_API void clopts_option_free(clopts_option_t *option);

/* setter はすべてエラーを返す（失敗時は属性を変えない）。アタッチ後に呼ぶのは契約違反 */
CLOPTS_API clopts_decl_error_t clopts_option_set_default(clopts_option_t *option, const char *default_value);
/* ...他の setter も同様 */

/* 成功時のみ所有権が command に移る。失敗時は何も変わらない */
CLOPTS_API clopts_decl_error_t clopts_command_add_option(clopts_command_t *command, clopts_option_t *option);
```

`clopts_decl_error_t` にはメモリ不足を表す種別（例: `CLOPTS_DECL_OUT_OF_MEMORY`）を足す。あるいは、戻り値の型を宣言用の別の enum にする（下の「未決」参照）。

使用例:

```c
clopts_option_t *verbose;

if (clopts_option_new(CLOPTS_OPTION_COUNT, "verbose", 'v', &verbose) != CLOPTS_DECL_NONE) { /* ... */ }
clopts_option_set_help(verbose, "Print more details");
clopts_option_set_count_max(verbose, 3);
if (clopts_command_add_option(root, verbose) != CLOPTS_DECL_NONE)
{
    clopts_option_free(verbose);   /* 失敗時は呼び出し側が持ったまま */
    /* ... */
}
/* 以降 verbose は借用参照（結果の読み出しに使う） */
```

### その他の決めること（推奨案）

1. **アタッチ後の setter → 禁止する（設定 → アタッチ → 読み取り専用）**
   - 許すと、`set_negatable` などの setter がアタッチ後は兄弟との検査も要るようになり、オプションが親を知る必要が戻ってくる（方針 0 と矛盾する）。
   - 禁止すれば、関係の検査はアタッチの 1 か所だけになる。
2. **二重アタッチ → `CLOPTS_DECL_ALREADY_ATTACHED`（仮）を返し、何も変えない**
   - DOM のような付け替え（元の親から外して移す）はしない。オプションは親を知らないので、元の親から外せない。
   - 同じ定義を複数のコマンドで使いたい場合は、作る関数を呼び分ける。必要なら `clopts_option_clone` を後で検討する。
3. **位置引数・コマンドへの展開**
   - 位置引数: 揃える（`clopts_positional_new` + `clopts_command_add_positional(cmd, pos)`）。親を持たない点も同じ。
   - コマンド: 方針 2 の即時エラーは `clopts_program_add_command` などにも適用する。`clopts_command_new` + アタッチの形にし、コマンドからも `program` を外すかは別途判断（root の扱い、`MIXED_LAYOUT`、`clopts_write_help` などが program からコマンドをたどる作りとの関係を見る）。

### 実装で決めたこと（旧「未決」）

- [x] 宣言 API の戻り値の型: `clopts_decl_error_t` をそのまま使い、`decl.h` へ移した（ビルダー関数が返すため）。値を 5 つ足した: `CLOPTS_DECL_INVALID_KIND`（16）、`CLOPTS_DECL_ALREADY_ATTACHED`（17）、`CLOPTS_DECL_INVALID_ARGUMENT`（18、NULL のハンドルか out）、`CLOPTS_DECL_FROZEN`（19、凍結後の変更。assert が無効なときだけ返る）、`CLOPTS_DECL_OUT_OF_MEMORY`（20）。
- [x] `CLOPTS_NODISCARD`（GCC / Clang の `warn_unused_result`）は、ハンドルを作る関数とアタッチの関数（`clopts_program_new`、`clopts_program_add_command`、`clopts_option_new`、`clopts_positional_new`、`clopts_command_add_option`、`clopts_command_add_positional`）だけに付けた。setter には付けない。
- [x] 宣言エラーの文面: `clopts_decl_error_message(kind)`（`help.h`）として残した。`clopts_write_error` の `invalid declaration: ...` は削除。
- [x] 結果のスロットの引き方: 線形探索（`find_slot`）。
- [x] アタッチ後の setter と二重アタッチは assert にせず、`CLOPTS_DECL_ALREADY_ATTACHED` を返すだけにした。
- [x] コマンドは今回は `clopts_program_add_command(program, name, &out)` のまま（program が作る）。`program` を持つのもそのまま。
- [x] NULL のハンドルを渡した呼び出しは何もせず `CLOPTS_DECL_INVALID_ARGUMENT` を返す。これを使うと「最初のエラーを 1 つ残し、後の呼び出しの結果は無視する」書き方ができる（examples の `check`）。
- [x] `CLOPTS_DECL_ERROR` を除いた `clopts_status_t` は値を詰めた（`CLOPTS_OUT_OF_MEMORY` は 5）。

### 影響範囲

- [x] `src/parser/parser_build.c`: new とアタッチの分離、setter の戻り値化、関係の検査をアタッチへ集約、`build_begin` / `build_record_*` の整理
- [x] `src/parser/parser_model.h`: `struct clopts_option` / `struct clopts_positional` から `program` / `scope` / `index` を外して `attached` を足す、`program->error` の削除、`clopts_parser_kind_t` を公開 enum に置き換え
- [x] `src/parser/parser_result.c`: `find_slot` をポインタでの探索に変える
- [x] `src/parser/parser_scan.c`: 宣言エラーを結果で返す経路の削除
- [x] `src/help/help_message.c`・`help_layout.c`: `CLOPTS_DECL_ERROR` の文面と `program->error.status` 参照の削除
- [x] `include/clopts/decl.h`・`parse.h`・`help.h`: API の差し替え、`clopts_option_kind_t` の追加、`CLOPTS_DECL_ERROR` / `clopts_validate` の削除
- [x] `abi/clopts.symbols`
- [x] `docs/api.md`: 宣言、メモリの所有と寿命、宣言エラーの表
- [x] `README.md`、`examples/taskbook/taskbook.c`、`examples/textcount/textcount.c`
- [x] `tests/parser/`（`test_build.c`、`test_decl_check.c`、`test_errors.c`、`test_oom.c`、`test_result.c` など）、`tests/help/`
- [x] `CHANGELOG.md`: まだリリースしていないので、「追加」の節の記述を新しい API に書き換えた

### 後続: ファイルの分割（実装済み）

親を知らなくなったことで責務が分かれたので、メンテしやすいようにハンドルごとにファイルを分けた。

- 公開ヘッダ: `clopts/decl.h` を、`decl_types.h`（ハンドルの型、`clopts_decl_error_t`、`clopts_handler_fn`、共通の規則）、`program.h`、`command.h`、`option.h`（`clopts_option_kind_t` を含む）、`positional.h` に分けた。`decl.h` はこの 5 つをまとめて取り込むだけのヘッダとして残し、`parse.h` と `help.h` は `decl_types.h` だけを取り込む。`abi/clopts.symbols` の組もヘッダごとにした。
- 内部モデル: `parser_model.h` から `struct clopts_option` を `parser_option.h` に、`struct clopts_positional` を `parser_positional.h` に分けた。`parser_model.h` には program と command だけが残る。
- 実装: `parser_build.c` を `parser_program.c`、`parser_command.c`（アタッチと兄弟との検査を含む）、`parser_option.c`、`parser_positional.c` に分けた。option と positional のファイルは program と command を読まない。文字列の差し替えは `clopts_core_replace_string`（core）に移した。
- テスト: オプション単体・位置引数単体の規則を `test_decl_option.c`（suite `decl_option`）と `test_decl_positional.c`（suite `decl_positional`）に分けた。どちらも program を作らない。
