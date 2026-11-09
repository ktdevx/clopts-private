# API リファレンス

この章は、CLOpts の公開 API の型、関数、ハンドルの寿命、メモリの所有、エラーの扱いを示す。宣言の書き方、解析の呼び出し方、結果とエラーの読み方、ヘルプやエラーの文面の出し方を、公開ヘッダを読まずに確かめるための章である。

公開ヘッダは `include/clopts/` にあり、`#include "clopts/clopts.h"` ですべてを取り込める。ヘッダごとの内容は次のとおり。

| ヘッダ | 内容 |
|--------|------|
| `clopts/decl.h` | 宣言の API 全体（次の 5 つのヘッダをまとめて取り込む） |
| `clopts/error.h` | エラーの型（`clopts_error_t`）とその説明（`clopts_error_message`） |
| `clopts/decl_types.h` | ハンドルの型と、ビルダー関数に共通の規則 |
| `clopts/program.h` | プログラムの関数（root とサブコマンドからの作成、解放、読み取り） |
| `clopts/command.h` | コマンド（root を含む）の関数（作成、解放、説明、オプションと位置引数を渡す関数、読み取り） |
| `clopts/option.h` | オプションの種類（`clopts_option_kind_t`）と、オプションの関数（作成、解放、属性、読み取り） |
| `clopts/positional.h` | 位置引数の関数（作成、解放、属性、読み取り） |
| `clopts/parse.h` | 解析の関数、状態とエラー種別の列挙、結果とエラーのアクセサ |
| `clopts/env.h` | 環境変数を引くコールバックの型（`clopts_getenv_fn`）とその戻り値（`clopts_env_status_t`）。`clopts/parse.h` と `clopts/platform.h` の両方が取り込む |
| `clopts/help.h` | ヘルプ・使い方・版・エラーの文面を出す関数と、`--help`・`--version`・`help` コマンドを宣言する関数 |
| `clopts/writer.h` | 書き込みコールバックの型と、`FILE*` へ書く便宜関数 |
| `clopts/platform.h` | UTF-8 の argv と環境変数を得る関数 |
| `clopts/alloc.h` | ライブラリ全体で使うアロケーターを差し替える関数と、その構造体 |

引数がどの規則で解釈されるか（`--`、`-` 単体、並べ替え、`--no-<name>`、カウント、環境変数の優先順位など）は [解析規則](parsing.md) の章で、Windows での argv と環境変数の扱いは [Windows での注意](windows.md) の章で示す。共有ライブラリとしての互換性の方針は [ABI の互換性](abi.md) の章にある。

この章の出力例は、サブコマンドありのサンプル `examples/taskbook/taskbook.c`（版 1.2.3）を実行した結果である。

## 全体の流れ

CLOpts を使うプログラムは、次の順に API を呼ぶ。

1. 宣言する: 宣言は下から上へ組み立てる。オプションと位置引数を単独で作って設定し、コマンドに渡す（所有権を移す）。コマンド（root とサブコマンド）を単独で作って埋めたら、最後に `clopts_program_create` に root とサブコマンドをまとめて渡してプログラムを作る。誤った呼び出しは、その場で戻り値として宣言エラーを返す。
2. 解析する: `clopts_parse`（または `clopts_parse_with_env`）に argv を渡し、結果のハンドルを受け取る。
3. 状態で分岐する: `clopts_result_status` で解析の状態を読む。
4. 値を読む、または文面を出す: 成功なら `clopts_result_command` で選ばれたコマンドを見て処理を選び、結果のアクセサで値を読む。終端オプション（`--help`、`--version` など）で終わったなら `clopts_result_terminal` でどれかを見てヘルプや版を、エラーならエラーを `clopts_write_*` で出す。
5. 解放する: `clopts_result_free`、`clopts_argv_free` などで解放し、最後に `clopts_program_destroy` でプログラムを解放する。

taskbook の `run`、`finish`、`main` の抜粋を示す（宣言の部分と、各サブコマンドの処理は省いた）。ライブラリはコマンドに処理（ハンドラ）を持たせないので、選ばれたコマンドのハンドルを比べて処理を選ぶ。

```c
/* Runs the selected command. */
static int run(const clopts_program_t *program, const clopts_result_t *result, const struct taskbook_options *opts)
{
    const clopts_command_t *command = clopts_result_command(result);
    const clopts_command_t *target;

    if (command == opts->add)
    {
        return run_add(result, opts);
    }
    if (command == opts->list)
    {
        return run_list(result, opts);
    }
    if (command == opts->done)
    {
        return run_done(result, opts);
    }
    /* help [COMMAND] */
    if (!clopts_help_target(program, result, &target))
    {
        fprintf(stderr, "taskbook: error: unknown command '%s'\n", clopts_result_positional_at(result, 0));
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        return 2;
    }
    clopts_write_help(program, target, clopts_file_write, stdout);
    return 0;
}

/* Writes the text for result and returns the exit code (0, 1 or 2). */
static int finish(const clopts_program_t *program, const clopts_result_t *result,
                  const struct taskbook_options *opts)
{
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        return run(program, result, opts);
    case CLOPTS_TERMINATED:
        if (clopts_result_terminal(result) == opts->version)
        {
            clopts_write_version(program, "1.2.3", clopts_file_write, stdout);
        }
        else
        {
            clopts_write_help(program, clopts_result_command(result), clopts_file_write, stdout);
        }
        return 0;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_error_command(result), clopts_file_write, stderr);
        return 2;
    case CLOPTS_NO_COMMAND:
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        return 2;
    default:
        /* CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        return 1;
    }
}

int main(int argc, char **argv)
{
    struct taskbook_options opts = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    clopts_program_t *program = NULL;
    clopts_argv_t *args;
    clopts_env_t *env;
    clopts_result_t *result;
    clopts_error_t error;
    int code;

    error = declare(&program, &opts);
    if (error != CLOPTS_ERROR_NONE)
    {
        fprintf(stderr, "taskbook: error: invalid declaration: %s\n", clopts_error_message(error));
        return 1;
    }

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("taskbook: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_destroy(program);
        return 1;
    }

    env = clopts_env_new();
    result = clopts_parse_with_env(program, clopts_argv_count(args), clopts_argv_values(args), clopts_env_lookup, env);
    code = finish(program, result, &opts);

    clopts_result_free(result);
    clopts_env_free(env);
    clopts_argv_free(args);
    clopts_program_destroy(program);
    return code;
}
```

宣言の部分では、たとえば `add` サブコマンドを次のように宣言している。`CHECK` は呼び出しの戻り値を `e` に入れ、エラーなら関数の後始末（`fail:`）へ飛ぶマクロである（taskbook の中にある）。

```c
CHECK(clopts_command_create("add", &add));
CHECK(clopts_command_set_summary(add, "Add a task"));
CHECK(clopts_command_set_description(add, "Add a task with the given title."));
CHECK(clopts_option_create(CLOPTS_OPTION_LIST, "tag", 't', &option));
CHECK(clopts_option_set_value_name(option, "TAG"));
CHECK(clopts_option_set_delimiter(option, ','));
CHECK(clopts_option_set_help(option, "Tags of the task"));
CHECK(clopts_command_add_option(add, option));
opts->tag = option;
option = NULL; /* now owned by add */
CHECK(clopts_option_create(CLOPTS_OPTION_VALUE, "priority", 'p', &option));
CHECK(clopts_option_set_value_name(option, "LEVEL"));
CHECK(clopts_option_set_default(option, "normal"));
CHECK(clopts_option_set_help(option, "Priority: low, normal or high"));
CHECK(clopts_command_add_option(add, option));
opts->priority = option;
option = NULL; /* now owned by add */
CHECK(clopts_positional_create("TITLE", &positional));
clopts_positional_set_required(positional, true);
CHECK(clopts_positional_set_help(positional, "Title of the task"));
CHECK(clopts_command_add_positional(add, positional));
positional = NULL; /* now owned by add */
```

root と 4 つのサブコマンドを宣言し終えたら、最後にまとめてプログラムを作る。`clopts_program_create` が成功するとプログラムがすべてのコマンドを持ち、失敗すると何も変わらないので、呼び出し側がコマンドを解放する。

```c
CHECK(clopts_help_command_create(&help));

{
    clopts_command_t *commands[4];

    commands[0] = add;
    commands[1] = list;
    commands[2] = done;
    commands[3] = help;
    CHECK(clopts_program_create(root, commands, 4, out));
}
return CLOPTS_ERROR_NONE;

fail:
    /* Nothing was handed to a program: every handle that is not attached is still ours. */
    clopts_option_destroy(option);
    clopts_positional_destroy(positional);
    clopts_command_destroy(root);
    clopts_command_destroy(add);
    clopts_command_destroy(list);
    clopts_command_destroy(done);
    clopts_command_destroy(help);
    return e;
```

失敗したら NULL のハンドルを次の呼び出しに渡さず、その場で後始末して戻る理由は「[失敗の扱い](#失敗の扱い)」で示す。

`taskbook add "Write docs" -t docs,api -v` を実行すると、`add` の処理（`run_add`）が選ばれ、受け取った値とその出所を標準出力へ出して、終了コード 0 で終わる。

<!-- clopts-check: -- taskbook add "Write docs" -t docs,api -v -->
```text
command: add
title: Write docs
tags: docs, api (explicit)
priority: normal (default)
verbose: 1 (explicit)
color: false
```

使い方の誤り（ここでは存在しないサブコマンド）では、エラーの 1 行と使い方の案内を標準エラーへ出し、終了コード 2 で終わる（`taskbook remove`）。

<!-- clopts-check: exit=2 stream=stderr -- taskbook remove -->
```text
taskbook: error: unknown command 'remove'
Run 'taskbook --help' for usage.
```

## ハンドルと寿命

公開 API のハンドルはすべて不透明型（ヘッダでは名前だけを宣言し、中身を見せない型）へのポインタである。中身の配置はライブラリの内部にあり、利用側はハンドルを関数に渡すことでだけ扱う。

| ハンドル | 表すもの | 作る関数 | 解放する関数 | 有効な期間 | 依存するハンドル |
|----------|----------|----------|--------------|------------|------------------|
| `clopts_program_t` | プログラムの宣言全体（作った後は変わらない） | `clopts_program_create` | `clopts_program_destroy` | `clopts_program_destroy` まで | なし |
| `clopts_command_t` | サブコマンド、または root（プログラム自身の範囲） | `clopts_command_create` | プログラムに渡す前は `clopts_command_destroy`。`clopts_program_create` で渡した後は、なし（プログラムと一緒に解放される） | 渡す前は `clopts_command_destroy` まで。渡した後はプログラムの `clopts_program_destroy` まで（同じアドレスのまま） | 渡した後はプログラム |
| `clopts_option_t` | オプション | `clopts_option_create` | コマンドに渡す前は `clopts_option_destroy`。`clopts_command_add_option` で渡した後は、なし（プログラムと一緒に解放される） | 渡す前は `clopts_option_destroy` まで。渡した後はプログラムの `clopts_program_destroy` まで（同じアドレスのまま） | 渡した後はプログラム |
| `clopts_positional_t` | 位置引数 | `clopts_positional_create` | コマンドに渡す前は `clopts_positional_destroy`。`clopts_command_add_positional` で渡した後は、なし（プログラムと一緒に解放される） | 渡す前は `clopts_positional_destroy` まで。渡した後はプログラムの `clopts_program_destroy` まで（同じアドレスのまま） | 渡した後はプログラム |
| `clopts_result_t` | 解析の結果 | `clopts_parse`、`clopts_parse_with_env` | `clopts_result_free` | `clopts_result_free` まで。ただしアクセサを呼べるのはプログラムの解放前だけ | プログラム、argv（結果の文字列が argv を借用する） |
| `clopts_argv_t` | UTF-8 の argv | `clopts_argv_get`、`clopts_argv_from_wide`（Windows のみ） | `clopts_argv_free` | `clopts_argv_free` まで。Windows 以外では、渡した argv が有効な間だけ | Windows 以外では、呼び出し側の argv |
| `clopts_env_t` | 環境変数を引くための文脈 | `clopts_env_new` | `clopts_env_free` | `clopts_env_free` まで | なし |

オプションと位置引数は自分を渡したコマンドを知らず、コマンドは自分を渡したプログラムを知らない。渡した後は読み取り専用になり、同じものを 2 回渡すこともできない。ライブラリはこれを検査しないので、渡した後に属性を変える関数や渡す関数を呼ぶこと、同じオプション・位置引数を 2 回渡すこと、同じコマンドを `clopts_program_create` に 2 回並べること（root を `commands` にも並べることを含む）は契約違反で、動作は未定義になる。プログラムは `clopts_program_create` で作った後は変わらない。

root は `clopts_program_create` の最初の引数に渡すコマンドで、グローバルオプションを持つ。root の名前はプログラムの名前に、root の説明（description）はプログラムの説明になる。サブコマンドのないプログラムでは、位置引数も root に置く。

解放の順序は次のとおり。

- `clopts_result_free` は、`clopts_program_destroy` の前でも後でも呼べる。ただし、結果のアクセサは、結果を作ったプログラムを解放する前にだけ呼べる。
- 結果から得た文字列は argv を借用することがあるので、`clopts_argv_free` は結果を使い終えてから呼ぶ。
- `clopts_env_free` は、解析が終わった後ならいつ呼んでもよい（環境変数の値は解析中に複製される）。
- すべての解放関数は NULL を受け付け、何もしない。

## 宣言

宣言は、実行時にビルダー関数を呼んで組み立てる。宣言の関数は、ハンドルごとに `clopts/program.h`、`clopts/command.h`、`clopts/option.h`、`clopts/positional.h` に分かれており、`clopts/decl.h` で全部を取り込める。宣言は下から上へ組み立てる。オプションと位置引数は単独で作って設定してからコマンドに渡し、コマンドは単独で作って埋めてから、root とサブコマンドをまとめて `clopts_program_create` に渡す。

ビルダー関数（解放の関数、読み取りの関数、失敗しない `clopts_positional_set_required` と `clopts_positional_set_variadic` を除く）は、どれも `clopts_error_t` を返す。成功なら `CLOPTS_ERROR_NONE`、失敗ならその理由である（「[失敗の扱い](#失敗の扱い)」）。ハンドルを作る関数は、作ったハンドルを最後の引数 `out` に入れる。

### プログラム

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_program_create`（`root, commands, command_count, out`） | root と、`commands` の `command_count` 個のサブコマンド（`command_count` が 0 なら `commands` は NULL でよい）からプログラムを作り、`*out` に入れる。成功するとプログラムが root とすべてのコマンドを持ち、それらは読み取り専用になる。失敗すると何も変わらず、コマンドは呼び出し側が持ったままになる。NULL の `out`・root・コマンドは契約違反で、`assert` で検出される。失敗の理由は検査の順に、同じ名前のサブコマンド（`CLOPTS_ERROR_DUPLICATE_COMMAND`）、サブコマンドがあるのに root に位置引数がある（`CLOPTS_ERROR_MIXED_LAYOUT`） |
| `clopts_program_destroy`（`program`） | プログラムと、それが持つすべてのもの（root、コマンド、渡されたオプションと位置引数）を解放する。NULL なら何もしない |
| `clopts_program_root`（`program`） | root を返す。`program` が NULL なら NULL |
| `clopts_program_command_count`（`program`） | サブコマンドの数 |
| `clopts_program_command_at`（`program, index`） | `index` 番目のサブコマンド（`clopts_program_create` に渡した順）。`index` は `clopts_program_command_count` の値より小さくなければならない |
| `clopts_program_find_command`（`program, name`） | 名前が `name` と一致する（大文字と小文字を区別し、前方一致はしない）サブコマンド。なければ NULL |

プログラムは作った後は変わらないので、同じプログラムを複数のスレッドから同時に解析し、読んでよい。

### コマンド（root を含む）

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_command_create`（`name, out`） | `name` という名前（複製される）のコマンドを、どのプログラムにも渡していない状態で作り、`*out` に入れる。名前が不正（NULL、空、`-` で始まる、空白を含む）なら `CLOPTS_ERROR_INVALID_NAME`。root にするコマンドの名前はプログラムの名前になる |
| `clopts_command_destroy`（`command`） | プログラムに渡していないコマンドを、渡されたオプションと位置引数ごと解放する。NULL なら何もしない。プログラムに渡したコマンドを渡すのは契約違反で、`assert` で検出される（`assert` が無効なら何もしない） |
| `clopts_command_set_summary`（`command, summary`） | プログラムのヘルプのコマンド一覧に出す 1 行の説明。root には使われない |
| `clopts_command_set_description`（`command, description`） | コマンドのヘルプに出す説明。root ではプログラムの説明になる |
| `clopts_command_add_option`（`command, option`） | `option` をコマンドに渡す。成功するとコマンドが `option` を持ち、`option` はコマンドが解放されるまで読み取り専用で有効になる。失敗すると何も変わらず、`option` は呼び出し側が持ったままになる（直してもう一度渡すか、`clopts_option_destroy` で解放する）。失敗の理由は、同じコマンドの中での名前の重なり（`CLOPTS_ERROR_DUPLICATE_OPTION`）、同じコマンドの中での否定形との衝突（`CLOPTS_ERROR_NEGATION_CONFLICT`） |
| `clopts_command_add_positional`（`command, positional`） | `positional` を、すでに渡した位置引数の後ろに渡す。所有権は `clopts_command_add_option` と同じに移る。失敗の理由は、最後の位置引数が可変長（`CLOPTS_ERROR_VARIADIC_POSITION`）、必須の位置引数を省略できる位置引数の後ろに渡した（`CLOPTS_ERROR_POSITIONAL_ORDER`） |
| `clopts_command_name`（`command`） | 名前 |
| `clopts_command_summary`（`command`）、`clopts_command_description`（`command`） | 1 行の説明と説明。設定していなければ NULL |
| `clopts_command_option_count`（`command`）、`clopts_command_option_at`（`command, index`） | 渡したオプションの数と、`index` 番目のオプション（渡した順） |
| `clopts_command_positional_count`（`command`）、`clopts_command_positional_at`（`command, index`） | 渡した位置引数の数と、`index` 番目の位置引数（渡した順） |

root にするコマンドに渡したオプションはグローバルオプションに、サブコマンドに渡したオプションはそのコマンドのオプションになる。コマンドに渡す関数が検査するのは、そのコマンドの中の関係だけである。root とサブコマンドの間で名前が重なってもよく、そのときはサブコマンドの名前が root の名前を隠す（「[名前の規則](#名前の規則)」）。

読み取りの関数（`clopts_command_name` などの、`clopts_command_`・`clopts_program_`・`clopts_option_`・`clopts_positional_` で始まり、`_new`・`_free`・`_set_`・`_add_` を含まない関数）は、渡す前でも渡した後でも呼べる。NULL のハンドルは空（NULL、0、false、`'\0'`）として読まれる（オプションを除く。「[オプションの属性](#オプションの属性)」を参照）。添字が範囲外なのは契約違反で、`assert` で検出される。`clopts/help.h` の文面の関数も、宣言をこの読み取りの関数だけで読んでいる。

### オプション

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_option_create`（`kind, long_name, short_name, out`） | 種類 `kind` のオプションを、どのコマンドにも渡していない状態で作り、`*out` に入れる。`long_name` は先頭の `--` を除いた名前（なければ NULL）、`short_name` は 1 文字の名前（なければ `'\0'`）で、少なくとも一方が必要である（なければ `CLOPTS_ERROR_NO_NAME`）。種類が不正なら `CLOPTS_ERROR_INVALID_KIND`、名前が不正なら `CLOPTS_ERROR_INVALID_NAME` |
| `clopts_option_destroy`（`option`） | コマンドに渡していないオプションを解放する。NULL なら何もしない。渡したオプションを渡すのは契約違反で、`assert` で検出される（`assert` が無効なら何もしない） |

`clopts_option_kind_t` は次の 4 値をとる。値は ABI の一部である。

| 種類 | 意味 |
|------|------|
| `CLOPTS_OPTION_FLAG` | 値をとらない。指定されると true、否定できるとき `--no-<name>` で false |
| `CLOPTS_OPTION_COUNT` | 値をとらず、出現回数を数える。回数は上限で頭打ちになる |
| `CLOPTS_OPTION_VALUE` | 値を 1 つとる。繰り返すと最後の値が残る |
| `CLOPTS_OPTION_LIST` | 値の並びをとる。繰り返した値はつなげられ、区切り文字で分けられる |

### オプションの属性

属性はコマンドに渡す前に設定する。

| 関数 | 対象の種類 | 意味 |
|------|------------|------|
| `clopts_option_set_help`（`option, help`） | すべて | ヘルプに出す 1 行の説明 |
| `clopts_option_set_value_name`（`option, value_name`） | VALUE、LIST（ほかの種類にも設定できるが、使われない） | ヘルプに出す値の名前（taskbook の `<LEVEL>` など） |
| `clopts_option_set_env`（`option, env`） | VALUE、LIST | argv で値が与えられないときに読む環境変数の名前 |
| `clopts_option_set_default`（`option, default_value`） | VALUE、LIST | 既定値。LIST では区切り文字で分けられる |
| `clopts_option_set_required`（`option, required`） | VALUE、LIST | argv か環境変数から値を得なければならない |
| `clopts_option_set_negatable`（`option, negatable`） | FLAG | `--no-<long_name>` を受け付け、フラグを false にする |
| `clopts_option_set_delimiter`（`option, delimiter`） | LIST | 値の区切り文字。`'\0'` で元に戻す（このとき 1 回の指定が 1 つの値になる） |
| `clopts_option_set_count_max`（`option, count_max`） | COUNT | 回数の上限。0 で既定値（`UINT_MAX`）に戻す |
| `clopts_option_set_terminal`（`option, terminal`） | FLAG | 終端オプションにする。argv に現れたところで解析を止め、`CLOPTS_TERMINATED` で終える（「[解析と状態](#解析と状態)」）。`--help` や `--version` に使う |

文字列の属性は複製され、もう一度設定すると前の複製が置き換わる（最後の呼び出しが有効）。NULL、false、`'\0'`、0 を渡すと属性を元に戻す（元に戻す呼び出しは検査されない）。対象でない種類のオプションに環境変数・既定値・必須・否定・区切り文字・回数の上限・終端を設定すると、宣言エラーが返り、属性は変わらない（「[宣言エラーの種類](#宣言エラーの種類)」を参照）。値の名前は検査されず、FLAG と COUNT では使われない。

属性は、次の読み取りの関数で読める。NULL のオプションを渡すのは契約違反で、`assert` で検出される（`assert` が無効なときの動作は未定義）。

| 関数 | 戻り値 |
|------|--------|
| `clopts_option_kind_of`（`option`） | 種類 |
| `clopts_option_long_name`（`option`）、`clopts_option_short_name`（`option`） | 長い名前（先頭の `--` を除く。なければ NULL）と短い名前（なければ `'\0'`） |
| `clopts_option_help`、`clopts_option_value_name`、`clopts_option_env`、`clopts_option_default`（`option`） | 文字列の属性。設定していなければ NULL |
| `clopts_option_delimiter`（`option`） | 区切り文字。設定していなければ `'\0'` |
| `clopts_option_count_max`（`option`） | 回数の上限。設定していなければ 0（既定値の `UINT_MAX` を意味する） |
| `clopts_option_required`、`clopts_option_negatable`、`clopts_option_terminal`（`option`） | 必須、否定できる、終端かどうか |

### 位置引数

| 関数 | 意味 |
|------|------|
| `clopts_positional_create`（`name, out`） | `name` という名前（複製される。ヘルプと文面に使う）の位置引数を、どのコマンドにも渡していない状態で作り、`*out` に入れる。NULL か空なら `CLOPTS_ERROR_INVALID_NAME` |
| `clopts_positional_destroy`（`positional`） | コマンドに渡していない位置引数を解放する（`clopts_option_destroy` と同じ扱い） |
| `clopts_positional_set_help`（`positional, help`） | ヘルプに出す 1 行の説明。NULL で元に戻す |
| `clopts_positional_set_required`（`positional, required`） | 必須にする。必須の位置引数は、省略できる位置引数より前に渡す（渡すときに検査される）。失敗しないので戻り値はない |
| `clopts_positional_set_variadic`（`positional, variadic`） | 残りのトークンをすべて受け取る。可変長の位置引数の後ろには、位置引数を渡せない（渡すときに検査される）。失敗しないので戻り値はない |
| `clopts_positional_name`、`clopts_positional_help`（`positional`） | 名前と、ヘルプの説明（設定していなければ NULL） |
| `clopts_positional_required`、`clopts_positional_variadic`（`positional`） | 必須か、可変長か |

### 名前の規則

- コマンド（root を含む。root の名前はプログラムの名前）の名前: NULL、空、`-` で始まる、空白を含むものは不正である。
- オプションの長い名前: 上の規則に加えて、`=` を含むものは不正である。
- オプションの短い名前: `-`、`=`、空白は使えない。
- 位置引数の名前: NULL と空は不正である。
- 環境変数の名前: 空は不正である。
- 予約された名前はない。`help`、`h`、`version` もふつうの名前で、`--help` や `help` コマンドが要るなら `clopts/help.h` の関数で宣言する（「[ヘルプのための宣言](#ヘルプのための宣言)」）。
- オプションの名前は、同じコマンドの中では重ねられない。root とサブコマンドの間では重ねてよい。サブコマンドが選ばれた後は、そのコマンドの名前が先に探されるので、同じ名前の root のオプションは隠れる（[解析規則](parsing.md) の章）。
- サブコマンドの名前は、同じプログラムの中では重ねられない。

名前の形はコマンド・オプション・位置引数を作るときに、オプションの名前の重なりはコマンドに渡すときに、サブコマンドの名前の重なりは `clopts_program_create` が検査する。

### 失敗の扱い

ビルダー関数は、呼ばれるたびにその場で検査され、結果を戻り値で返す。

- 失敗した呼び出しは、何も変えない。ハンドルを作る関数は `*out` を NULL にする。そのため、成功した呼び出しだけで組み立てたプログラムは、つねに正しい。プログラムにエラーが記録されることはなく、解析が宣言エラーを返すこともない。
- オプションや位置引数だけで決まる検査（種類、名前の形、属性の組み合わせ）は、作る関数と属性の関数が行う。同じコマンドのほかのオプション・位置引数との関係で決まる検査（名前の重なり、否定形との衝突、位置引数の順序）は、コマンドに渡す関数が行う。コマンドの間の関係で決まる検査（サブコマンドの名前の重なり、root とサブコマンドの混在）は、`clopts_program_create` だけが行う。
- メモリの確保に失敗すると `CLOPTS_ERROR_OUT_OF_MEMORY` が返る。
- NULL のハンドルや NULL の `out` を渡すのは契約違反で、`assert` で検出される（`NDEBUG` のビルドでは検査されず、動作は未定義）。エラーは返らない。そのため、作る関数が失敗したら、NULL のハンドルを次の呼び出しに渡さず、その場で後始末して戻る（taskbook と textcount の書き方）。コマンドに渡せなかったオプションと位置引数は、呼び出し側が解放する。
- 渡した後のオプション・位置引数・コマンドに属性の関数や渡す関数を呼ぶことと、同じものを 2 回渡すことは検査しない（契約違反で、動作は未定義）。
- 宣言エラーの短い説明（英語）は、`clopts/error.h` の `clopts_error_message` で得られる。

`clopts_program_create`、`clopts_command_create`、`clopts_option_create`、`clopts_positional_create`、`clopts_command_add_option`、`clopts_command_add_positional` と `clopts/help.h` の宣言の関数の戻り値を捨てると、GCC と Clang では警告が出る（捨てるとハンドルを失うか、渡せなかったものが解放されないため）。

プログラムは `clopts_program_create` の後は変わらず、解析もプログラムに書き込まない。

## 解析と状態

解析の関数は `clopts/parse.h` にある。

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_parse`（`program, argc, argv`） | `argv[1]` から `argv[argc-1]` までを `program` の宣言に従って解析し、結果を返す（`argv[0]` は読み飛ばす）。`argc` は 0 以上で、`argv[0]` から `argv[argc-1]` は NUL で終わる文字列（`argc` が 0 なら argv は読まれない）。環境変数は `getenv()` で引く。宣言も argv も書き換えない。NULL は返さない。メモリの確保に失敗したとき（または `program` が NULL のとき）は、共有のメモリ不足の番兵を返す |
| `clopts_parse_with_env`（`program, argc, argv, getenv_fn, env_ctx`） | `clopts_parse` と同じだが、環境変数を `getenv_fn(env_ctx, name, &value)` で引く。`getenv_fn` が NULL なら既定の引き方（`getenv()`）になる |
| `clopts_result_status`（`result`） | 結果の状態（`clopts_status_t`）を返す |

`clopts_status_t` は次の 5 値をとる。表の「利用側の処理」は、taskbook と textcount のサンプルが従う終了コードの規約である。

| 状態 | 意味 | 利用側の処理 |
|------|------|--------------|
| `CLOPTS_OK` | 解析に成功した | `clopts_result_command` で選ばれたコマンドを見て処理を選び、値を読んで処理する。結果を標準出力へ出し、0 を返す。`help` コマンド（`clopts_help_command_create`）なら、`clopts_help_target` で対象を得てヘルプを出す |
| `CLOPTS_TERMINATED` | 終端オプション（`clopts_option_set_terminal`）が argv に現れ、そこで解析を止めた。`clopts_result_terminal` がそのオプションを返す。必須のオプションと位置引数は検査されていない | `--help` なら `clopts_write_help` に `clopts_result_command` の値を渡してヘルプを、`--version` なら `clopts_write_version` で版を標準出力へ出し、0 を返す |
| `CLOPTS_NO_COMMAND` | サブコマンドのあるプログラムで、サブコマンドが現れなかった。読めるのはグローバルオプションの値だけ | `clopts_write_usage` と `clopts_write_usage_hint` を標準エラーへ出し、2 を返す |
| `CLOPTS_USAGE_ERROR` | argv に使い方の誤りがある | `clopts_write_error`（`CLOPTS_PREFIX_PROGRAM` と `CLOPTS_PREFIX_ERROR`）と、`clopts_result_error_command` の値を渡した `clopts_write_usage_hint` を標準エラーへ出し、2 を返す |
| `CLOPTS_OUT_OF_MEMORY` | メモリの確保か、環境変数の引き方が失敗した | `clopts_write_error` を標準エラーへ出し、1 を返す |

解析は、ヘルプ・版・エラーを含めて何も出力せず、`exit()` も呼ばない。文面を出すかどうか、どの終了コードで終わるかは、利用側が状態を見て決める。解析器には組み込みのオプションもコマンドもない。`--help`、`--version`、`help` コマンドは、ふつうの宣言として `clopts/help.h` の関数で足す。

`CLOPTS_NO_COMMAND` の例（`taskbook` を引数なしで実行する）:

<!-- clopts-check: exit=2 stream=stderr -- taskbook -->
```text
Usage: taskbook [<options>] <command> [<arguments>]
Run 'taskbook --help' for usage.
```

`CLOPTS_TERMINATED` の例（`taskbook --version`。taskbook は `clopts_result_terminal` が `--version` を返したときに版を出す）:

<!-- clopts-check: -- taskbook --version -->
```text
taskbook 1.2.3
```

### 環境変数の引き方

`clopts_parse_with_env` に渡す `getenv_fn` は、`clopts/env.h` にある次の型のコールバックである。

```c
typedef clopts_env_status_t (*clopts_getenv_fn)(void *ctx, const char *name, const char **value);
```

コールバックは `clopts_env_status_t` を返す。

| 値 | 意味 |
|----|------|
| `CLOPTS_ENV_FOUND` | 見つかった。`*value` に、コールバックが所有する文字列を入れる |
| `CLOPTS_ENV_UNSET` | 設定されていない。`*value` は読まれない |
| `CLOPTS_ENV_FAILED` | 引き方が失敗した。解析は `CLOPTS_OUT_OF_MEMORY` で終わる |

`*value` に入れた文字列は、次にコールバックが呼ばれるまでか、解析の関数が戻るまでだけ有効であればよい。ライブラリはコールバックが戻った直後にそれを複製し、その後は読まず、解放もしない。UTF-8 の値を得るには、`clopts/platform.h` の `clopts_env_lookup` を `clopts_env_t` とともに渡す（「[プラットフォームの関数](#プラットフォームの関数)」を参照）。taskbook はこの方法で `TASKBOOK_FORMAT` を読む（`TASKBOOK_FORMAT=json taskbook list`）。

<!-- clopts-check: env=TASKBOOK_FORMAT=json -- taskbook list -->
```text
command: list
all: false
format: json (env)
verbose: 0
color: false
```

## 結果のアクセサ

値のアクセサは、状態が `CLOPTS_OK`、`CLOPTS_TERMINATED`（終端オプションまでに読んだ値と、環境変数と既定値）、`CLOPTS_NO_COMMAND`（グローバルオプションだけ）のときに意味を持つ。それ以外の状態のときと、選ばれなかったコマンドのオプションを渡したときは、「値なし」（false、0、NULL、0 個、`CLOPTS_SOURCE_NONE`）を返す。NULL のオプションを渡したときも「値なし」を返す。

| 関数 | 戻り値 |
|------|--------|
| `clopts_result_command`（`result`） | `CLOPTS_OK`、`CLOPTS_TERMINATED`、`CLOPTS_NO_COMMAND`、`CLOPTS_USAGE_ERROR` では選ばれたコマンド。なければ NULL（サブコマンドのないプログラムや、サブコマンドの前で止まったとき） |
| `clopts_result_terminal`（`result`） | `CLOPTS_TERMINATED` では解析を止めた終端オプション。ほかの状態では NULL |
| `clopts_result_flag`（`result, option`） | FLAG の値（`bool`） |
| `clopts_result_count`（`result, option`） | COUNT の回数（`unsigned`。上限で頭打ち） |
| `clopts_result_value`（`result, option`） | VALUE の値（文字列）。値がなければ NULL |
| `clopts_result_list_count`（`result, option`） | LIST の値の個数 |
| `clopts_result_list_at`（`result, option, index`） | LIST の `index` 番目の値。`index` は `clopts_result_list_count` の値より小さくなければならない |
| `clopts_result_source`（`result, option`） | 値の出所（`clopts_source_t`） |
| `clopts_result_positional_count`（`result`） | 位置引数として受け取ったトークンの個数 |
| `clopts_result_positional_at`（`result, index`） | `index` 番目の位置引数（argv の要素そのもの）。`index` は `clopts_result_positional_count` の値より小さくなければならない |
| `clopts_result_free`（`result`） | 結果を解放する。NULL とメモリ不足の番兵では何もしない。`clopts_program_destroy` の前でも後でも呼べる |

値はすべて文字列で、数値などへの変換は利用側が行う。

`clopts_source_t` は、オプションの値がどこから来たかを示す。

| 値 | 意味 |
|----|------|
| `CLOPTS_SOURCE_NONE` | 値を得なかった |
| `CLOPTS_SOURCE_DEFAULT` | 既定値から |
| `CLOPTS_SOURCE_ENV` | 環境変数から |
| `CLOPTS_SOURCE_EXPLICIT` | argv から（`--no-<name>` で false にしたときを含む） |

優先順位は argv、環境変数、既定値の順である（詳しくは [解析規則](parsing.md) の章）。

オプションの値は、そのオプションのコマンドの中での位置から一定の時間で探され、アドレスで確かめられる。そのため、別のプログラムのオプションや、選ばれなかったコマンドのオプション、コマンドに渡していないオプションを渡したときは「値なし」を返す。

次のものは契約違反で、`assert` で検出される: 範囲外の `index`。NULL の結果はメモリ不足の番兵と同じに読まれ（状態は `CLOPTS_OUT_OF_MEMORY`）、契約違反ではない。

## エラーのアクセサ

エラーのアクセサは、状態が `CLOPTS_USAGE_ERROR` のときに意味を持つ。それ以外の状態のときと、そのエラーがセットしない項目では、NONE、NULL、-1、0、`'\0'` を返す。

| 関数 | 戻り値 |
|------|--------|
| `clopts_result_usage_error`（`result`） | 使い方エラーの種別（`clopts_usage_error_t`） |
| `clopts_result_error_command`（`result`） | エラーの起きたコマンド。root なら NULL |
| `clopts_result_error_option`（`result`） | エラーに関わるオプション |
| `clopts_result_error_positional`（`result`） | エラーに関わる位置引数 |
| `clopts_result_error_token`（`result`） | エラーの原因になった argv の要素 |
| `clopts_result_error_argv_index`（`result`） | そのトークンの argv の添字（`argv[0]` が 0）。なければ -1 |
| `clopts_result_error_name_len`（`result`） | 長い形の未知のオプションで、トークンのうち `--name` の長さ（`=` の手前まで） |
| `clopts_result_error_short_name`（`result`） | 短いオプションのまとめ書きの中で、原因になった文字 |

## 使い方エラーの種類

`clopts_usage_error_t` は、argv の誤りの種類を示す（`CLOPTS_USAGE_NONE` はエラーなし）。表の「文面」は `clopts_write_error` が出す本文で、`<...>` の部分にトークンやオプションの名前が入る。「セットされるアクセサ」は `clopts_result_error_argv_index` のほかにセットされるもので、`clopts_result_error_command` はどの種別でもエラーの起きたコマンド（root なら NULL）になる。

| 種別 | 起きる入力の例（taskbook） | 文面 | セットされるアクセサ |
|------|----------------------------|------|----------------------|
| `CLOPTS_USAGE_UNKNOWN_COMMAND` | 宣言にないサブコマンド名（`taskbook remove`） | `unknown command '<名前>'` | token（そのコマンド名） |
| `CLOPTS_USAGE_UNKNOWN_OPTION` | 宣言にないオプション（`taskbook --bogus`、`taskbook add -vz x`） | `unknown option '<オプション>'` | token と、長い形なら name_len、短い形なら short_name |
| `CLOPTS_USAGE_MISSING_VALUE` | 値をとるオプションに値がない（`taskbook add --tag`） | `option '<オプション>' requires a value` | option、token |
| `CLOPTS_USAGE_UNEXPECTED_VALUE` | 値をとらないオプションに値を与えた（`taskbook --color=yes list`、`taskbook --help=x`） | `option '<オプション>' does not take a value` | option、token |
| `CLOPTS_USAGE_MISSING_OPTION` | 必須のオプションが argv からも環境変数からも値を得なかった（taskbook には必須のオプションがない） | `missing required option '<オプション>'` | option（argv_index は -1） |
| `CLOPTS_USAGE_MISSING_ARGUMENT` | 必須の位置引数がない（`taskbook add`） | `missing argument <名前>` | positional（argv_index は -1） |
| `CLOPTS_USAGE_UNEXPECTED_ARGUMENT` | 宣言した位置引数より多いトークン（`taskbook add a b`） | `unexpected argument '<トークン>'` | token（余った最初のトークン） |

argv に誤りが複数あっても、報告されるのは最初の 1 つだけである。ただし、誤りの後ろに終端オプション（`--help` や `--version`）がそのまま書かれていれば、そちらが優先される（[解析規則](parsing.md) の章を参照）。

taskbook での出力は次のとおり（いずれも標準エラー、終了コード 2）。

`taskbook --bogus`:

<!-- clopts-check: exit=2 stream=stderr -- taskbook --bogus -->
```text
taskbook: error: unknown option '--bogus'
Run 'taskbook --help' for usage.
```

`taskbook add -vz x`（まとめ書きの中の未知の文字。サブコマンドの後のエラーには、接頭辞と案内にコマンド名が入る）:

<!-- clopts-check: exit=2 stream=stderr -- taskbook add -vz x -->
```text
taskbook add: error: unknown option '-z'
Run 'taskbook add --help' for usage.
```

`taskbook add --tag`:

<!-- clopts-check: exit=2 stream=stderr -- taskbook add --tag -->
```text
taskbook add: error: option '--tag' requires a value
Run 'taskbook add --help' for usage.
```

`taskbook --color=yes list`:

<!-- clopts-check: exit=2 stream=stderr -- taskbook --color=yes list -->
```text
taskbook: error: option '--color' does not take a value
Run 'taskbook --help' for usage.
```

`taskbook add`:

<!-- clopts-check: exit=2 stream=stderr -- taskbook add -->
```text
taskbook add: error: missing argument <TITLE>
Run 'taskbook add --help' for usage.
```

`taskbook add a b`:

<!-- clopts-check: exit=2 stream=stderr -- taskbook add a b -->
```text
taskbook add: error: unexpected argument 'b'
Run 'taskbook add --help' for usage.
```

## 宣言エラーの種類

`clopts_error_t` は、ビルダー関数の結果を示す（`CLOPTS_ERROR_NONE` は成功）。表の「説明」は `clopts_error_message` が返す文字列である。

| 種別 | 返す関数と起きる例 | 説明 |
|------|--------------------|------|
| `CLOPTS_ERROR_INVALID_NAME` | 不正なコマンド名・オプション名・位置引数名（`clopts_command_create`、`clopts_option_create`、`clopts_positional_create`）、空の環境変数名（`clopts_option_set_env`（`option, ""`）） | `invalid name` |
| `CLOPTS_ERROR_NO_NAME` | 長い名前も短い名前もないオプション（`clopts_option_create`（`CLOPTS_OPTION_FLAG, NULL, '\0', &option`）） | `option has neither a long nor a short name` |
| `CLOPTS_ERROR_DUPLICATE_COMMAND` | 同じ名前のサブコマンドを 2 つ渡した（`clopts_program_create`） | `duplicate command name` |
| `CLOPTS_ERROR_DUPLICATE_OPTION` | 同じコマンドの中でオプションの名前が重なった（`clopts_command_add_option`） | `duplicate option name` |
| `CLOPTS_ERROR_NOT_NEGATABLE` | FLAG でないオプションに `clopts_option_set_negatable`（`option, true`） | `only a flag can be negatable` |
| `CLOPTS_ERROR_NEGATION_CONFLICT` | 否定できる FLAG `x` と `no-x` という名前のオプションを、同じコマンドに渡した（どちらの順で渡しても、後から渡した方の `clopts_command_add_option`） | `conflicts with the negated form of a flag` |
| `CLOPTS_ERROR_DEFAULT_NOT_ALLOWED` | FLAG か COUNT に `clopts_option_set_default` | `a flag or count cannot have a default value` |
| `CLOPTS_ERROR_ENV_NOT_ALLOWED` | FLAG か COUNT に `clopts_option_set_env` | `a flag or count cannot have an environment variable` |
| `CLOPTS_ERROR_REQUIRED_WITH_DEFAULT` | 必須と既定値の両方を設定した（どちらの順でも、後の `clopts_option_set_default` か `clopts_option_set_required`） | `a required option cannot have a default value` |
| `CLOPTS_ERROR_REQUIRED_NOT_ALLOWED` | FLAG か COUNT に `clopts_option_set_required`（`option, true`） | `a flag or count cannot be required` |
| `CLOPTS_ERROR_POSITIONAL_ORDER` | 省略できる位置引数の後ろに、必須の位置引数を渡した（`clopts_command_add_positional`） | `a required argument cannot follow an optional one` |
| `CLOPTS_ERROR_VARIADIC_POSITION` | 可変長の位置引数の後ろに、位置引数を渡した（`clopts_command_add_positional`） | `only the last argument can be variadic` |
| `CLOPTS_ERROR_ATTRIBUTE_KIND` | LIST でないオプションに区切り文字、COUNT でないオプションに回数の上限、FLAG でないオプションに終端を設定した | `attribute does not apply to this kind of option` |
| `CLOPTS_ERROR_MIXED_LAYOUT` | サブコマンドがあるのに、root に位置引数がある（`clopts_program_create`） | `a program with subcommands cannot have root arguments` |
| `CLOPTS_ERROR_INVALID_KIND` | `clopts_option_kind_t` にない種類で `clopts_option_create` | `invalid option kind` |
| `CLOPTS_ERROR_INVALID_ARGUMENT` | 返らない（NULL のハンドルや `out` は契約違反で、`assert` で検出される。値は変えないために残している） | `invalid argument` |
| `CLOPTS_ERROR_OUT_OF_MEMORY` | メモリの確保に失敗した | `out of memory` |

taskbook と textcount は、宣言エラーがあれば `<program>: error: invalid declaration: <説明>` の 1 行を標準エラーへ出し、終了コード 1 で終わる。

## 文面の出力

ヘルプ・使い方・版・エラーの文面を出す関数は `clopts/help.h` にある。どの関数も、文面を書き込みコールバック `write_fn(ctx, data, len)` にだけ渡し、ストリームへ直接は書かない。文面の関数は、宣言を公開の読み取りの関数だけで読む（ライブラリの内部の構造には頼らない）ので、同じ読み取りの関数を使えば、利用側が独自のヘルプを書くこともできる。

| 関数 | 出す文面 |
|------|----------|
| `clopts_write_help`（`program, command, write_fn, ctx`） | ヘルプ。`command` が NULL か root ならプログラム全体の、サブコマンドならそのコマンドのヘルプ。並ぶのは宣言したものだけで、行のない段落（`Options:` など）は出さない。サブコマンドのヘルプの `Global Options:` には、そのコマンドに隠された（書ける形がすべて、つまり長い名前、否定できるなら `--no-<name>`、短い名前がすべて、先にそのコマンドで見つかる）root のオプションを出さない |
| `clopts_write_usage`（`program, command, write_fn, ctx`） | `clopts_write_help` の Usage の行だけ |
| `clopts_write_version`（`program, version, write_fn, ctx`） | `<program> <version>` の 1 行。`version` が NULL なら `CLOPTS_WRITE_UNAVAILABLE` |
| `clopts_write_usage_hint`（`program, command, write_fn, ctx`） | `Run '<program> [<command>] --help' for usage.` の 1 行 |
| `clopts_write_error`（`program, result, flags, write_fn, ctx`） | 状態が `CLOPTS_USAGE_ERROR`、`CLOPTS_OUT_OF_MEMORY` の結果について、エラーの 1 行。それ以外の状態では `CLOPTS_WRITE_UNAVAILABLE` |

宣言エラーの種別の短い説明（英語、静的な文字列）は、`clopts/error.h` の `clopts_error_message`（`error`）が返す。`CLOPTS_ERROR_NONE` と、列挙にない値では NULL を返す。

`clopts_write_error` の `flags` は接頭辞を選ぶ。`CLOPTS_PREFIX_PROGRAM` は `<program>: ` または `<program> <command>: ` を、`CLOPTS_PREFIX_ERROR` は `error: ` を前に付ける。両方を指定するときは `CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR` と書く。定義されていないビットは無視される。メモリ不足の結果では、本文は `out of memory` になる。使い方の案内は `clopts_write_error` の行に含まれないので、必要なら `clopts_write_usage_hint` を続けて呼ぶ。

戻り値の `clopts_write_status_t` は次の 3 値をとる。

| 値 | 意味 |
|----|------|
| `CLOPTS_WRITE_OK` | 文面のすべてのバイトを、順にコールバックへ渡した |
| `CLOPTS_WRITE_FAILED` | コールバックが失敗した。失敗した呼び出しの後は、コールバックをもう呼んでいない |
| `CLOPTS_WRITE_UNAVAILABLE` | 出す文面がない。コールバックを 1 度も呼んでいない |

文面をコールバックの何回の呼び出しに分けるかは決まっていない。決まっているのは、つなげたバイト列だけである。

書き込みコールバックの型は `clopts/writer.h` の `clopts_write_fn` である。

```c
typedef int (*clopts_write_fn)(void *ctx, const char *data, size_t len);
```

`ctx` が示す出力先へ `data` の `len` バイトを書き、すべて書けたら 0 を、そうでなければ 0 以外を返す。`ctx` は呼び出し側が所有し、関数にはコールバックと別の引数として渡す。

`FILE*` へ書くときは、`clopts/writer.h` の便宜関数 `clopts_file_write` をコールバックに、`FILE*` を `ctx` に渡す。

```c
clopts_write_help(program, NULL, clopts_file_write, stdout);
```

`clopts_file_write` は `static inline` の関数で、利用側の翻訳単位でコンパイルされ、ライブラリには含まれない（公開シンボルの一覧にも載らない）。そのため、ライブラリと利用側で C ランタイムが違っても、`FILE*` がライブラリの境界を越えない。`fwrite()` がすべてのバイトを書けば 0 を、そうでなければ 1 を返し、`len` が 0 なら何も書かずに 0 を返す。`ctx` は有効な `FILE*` でなければならず、`len` が 0 より大きいときは `data` が NULL であってはならない（そうでなければ動作は未定義）。

文面の関数の前提（`assert` で検出される）は、`program` と `write_fn` が NULL でないこと、`command` が `program` のものであること、`result` が `program` から得たもので、`program` がまだ解放されていないことである。NULL の結果はメモリ不足の番兵と同じに読まれる。文面の関数は、宣言も結果も書き換えず、メモリも確保しない。

ヘルプの文面の例として、`taskbook help add`（`taskbook add --help` と同じ）の出力を示す。`-h, --help` と `--version` は root に宣言したグローバルオプションなので、`Global Options:` に並ぶ。

<!-- clopts-check: -- taskbook help add -->
```text
Usage: taskbook add [<options>] <TITLE>

Add a task with the given title.

Arguments:
  <TITLE>  Title of the task

Options:
  -t, --tag <TAG>[,<TAG>...]  Tags of the task
  -p, --priority <LEVEL>      Priority: low, normal or high (default: normal)

Global Options:
  -v, --verbose...  Print more details (up to 3 times)
      --[no-]color  Colorize the output
  -h, --help        Show this help
      --version     Show version information
```

プログラム全体のヘルプは、root に `help` という長い名前のオプションがあるときだけ、最後に `Run '<program> <command> --help' for help on a command.` の行を付ける。

### ヘルプのための宣言

解析器は `--help` や `help` コマンドを知らないので、ヘルプの文面と組で使う宣言を `clopts/help.h` の関数で足す。

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_help_add_help_option`（`command, out`） | 終端の FLAG `-h` / `--help`（説明は `Show this help`）を作って `command` に渡す。ふつうは root に渡す（グローバルオプションはサブコマンドの中でも見つかる）。`*out` に作ったオプションを入れる（失敗なら NULL。`out` は NULL でもよい）。失敗の理由は `clopts_option_create` と `clopts_command_add_option` と同じで、失敗したら何も渡さない |
| `clopts_help_add_version_option`（`command, out`） | 同じく、終端の FLAG `--version`（説明は `Show version information`） |
| `clopts_help_command_create`（`out`） | `help` という名前のサブコマンド（1 行の説明は `Show help for a command`、省略できる位置引数 `COMMAND`）を、どのプログラムにも渡していない状態で作り、`*out` に入れる。ほかのサブコマンドと一緒に `clopts_program_create` に渡す |
| `clopts_help_target`（`program, result, target`） | 状態が `CLOPTS_OK` の結果で、最初の位置引数をヘルプの対象のコマンドの名前として読む。位置引数がなければ `*target` を NULL（プログラム全体のヘルプ）に、その名前のサブコマンドがあればそのコマンドにして true を返す。その名前のサブコマンドがないとき、または状態が `CLOPTS_OK` でないときは `*target` を NULL にして false を返す |

taskbook は、`taskbook help bogus` のように知らない名前を渡されたとき、自分でエラーの 1 行を出す。

<!-- clopts-check: exit=2 stream=stderr -- taskbook help bogus -->
```text
taskbook: error: unknown command 'bogus'
Run 'taskbook --help' for usage.
```

## メモリの所有と寿命

- ビルダー関数に渡した文字列（名前、説明、値の名前、環境変数の名前、既定値）は、ライブラリが複製する。呼び出し側は、呼び出しの直後にその文字列を変更・解放してよい。`clopts_write_version` の `version` は複製されず、呼び出しの間だけ読まれる。
- ライブラリが確保したものは、ライブラリの関数で解放する: プログラムとそれが持つハンドルは `clopts_program_destroy`、プログラムに渡していないコマンドは `clopts_command_destroy`、コマンドに渡していないオプションと位置引数は `clopts_option_destroy` と `clopts_positional_destroy`、結果は `clopts_result_free`、argv のハンドルは `clopts_argv_free`、環境変数の文脈は `clopts_env_free`。ライブラリから得た文字列を `free()` してはならない。
- ライブラリの確保と解放は、すべて設定されたアロケーター（既定は標準の `malloc` / `calloc` / `realloc` / `free`）を通る。差し替え方は「[アロケーター](#アロケーター)」で示す。
- ライブラリは argv を書き換えない。宣言も、解析によって書き換えられない。
- 結果から得た文字列（値、LIST の各値、位置引数、エラーのトークン）は、結果を解放するまで有効で、しかも呼び出し側がプログラムと argv の両方を生かしている間だけ有効である。`clopts_result_positional_at` と `clopts_result_error_token` は argv の要素そのものを返す。argv を `clopts_argv_get` で得たときは、結果を使い終えてから `clopts_argv_free` を呼ぶ。
- 環境変数の値は、解析中にライブラリが複製する。`getenv_fn` が返す文字列は、次の呼び出しか解析の終わりまで有効であればよい。
- 解放の順序: 結果のアクセサは、プログラムを解放する前に呼ぶ。`clopts_result_free` そのものは、`clopts_program_destroy` の前でも後でも呼べる。
- メモリ不足の番兵（`clopts_parse` などが確保に失敗したときに返す共有の結果）は、`clopts_result_free` に渡しても何もしない。そのため、状態にかかわらず、得た結果は必ず `clopts_result_free` に渡してよい。
- 文面の関数（`clopts_write_*`）はメモリを確保しない。
- コマンドに渡す前のオプションと位置引数は、呼び出し側が持つ。`clopts_command_add_option` と `clopts_command_add_positional` が成功すると、コマンドが持つ。失敗したときは呼び出し側が持ったままなので、`clopts_option_destroy` か `clopts_positional_destroy` で解放する。
- プログラムに渡す前のコマンドは、呼び出し側が持つ。`clopts_program_create` が成功すると、プログラムが root とすべてのコマンドを持つ。失敗したときは呼び出し側が持ったままなので、`clopts_command_destroy` で解放する（コマンドに渡したオプションと位置引数も一緒に解放される）。

### スレッド

- 解析は、どのストリームにも書かず、`exit()` も呼ばない。
- 解析はプログラムに書き込まない（プログラムは `clopts_program_create` の後は変わらない）。そのため、同じプログラムを複数のスレッドから同時に解析してよい。
- 文面の関数と読み取りの関数も、同じプログラムを複数のスレッドから同時に読んでよい。
- 1 つの `clopts_env_t` を、複数のスレッドから同時に使ってはならない。
- `clopts_set_allocator` は同期しない。ほかのスレッドがライブラリを使っていない間に呼ぶ。設定した関数は、ライブラリを使うすべてのスレッドから呼ばれうる。

## アロケーター

`clopts/alloc.h` は、ライブラリがメモリの確保と解放に使う関数を差し替える。アロケーターはライブラリ全体で 1 つで、ハンドルごとには持たない。

```c
typedef struct clopts_allocator
{
    void *(*malloc_fn)(size_t size);
    void *(*calloc_fn)(size_t count, size_t size);
    void *(*realloc_fn)(void *ptr, size_t size);
    void (*free_fn)(void *ptr);
} clopts_allocator_t;
```

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_set_allocator`（`allocator`） | `allocator` の 4 つの関数を写して、以降のすべての確保と解放に使う。NULL なら標準の `malloc` / `calloc` / `realloc` / `free` に戻す。4 つとも NULL でないこと（1 つでも NULL なら契約違反） |

- 各関数の引数と意味は、標準の `malloc` / `calloc` / `realloc` / `free` と同じである。利用側の文脈を渡す引数はない。
- ライブラリは、大きさ 0 では呼ばない（1 バイトとして確保する）。そのため、`malloc_fn`、`calloc_fn`、`realloc_fn` の NULL は、つねに確保の失敗を意味する。失敗した `realloc_fn` は、標準の `realloc` と同じく、元のブロックを有効なまま残すこと。
- `calloc_fn` を呼ぶ前に、ライブラリが `count * size` のあふれを検査する。`free_fn` に NULL は渡さない。
- 呼び出しの後に `allocator` の指す構造体を変えても、解放しても、影響しない。
- 設定してよいのは、ライブラリが確保したもの（プログラム、結果、argv のハンドル、環境変数の文脈）が 1 つも生きていないときだけである。ブロックは、解放する時点で設定されている `free_fn` で解放されるので、生きている間に変えると、確保したときとは別の関数で解放されてしまう。ライブラリはこの誤りを検出しない。通常は `main` の最初で 1 回呼ぶ。
- メモリ不足の番兵（`clopts_parse` などや `clopts_argv_get` が確保に失敗したときに返す共有のオブジェクト）は、静的なオブジェクトで、アロケーターとは関係しない。
- 構造体 `clopts_allocator_t` の配置は凍結されている。メンバーを足したり、削ったり、並べ替えたり、型を変えたりはしない。拡張が要るときは、別の型と別の関数を足す（[ABI の互換性](abi.md)）。

## プラットフォームの関数

`clopts/platform.h` は、プロセスの argv と環境変数を UTF-8 で得て、`clopts_parse` / `clopts_parse_with_env` に渡すための関数を持つ。Windows でのふるまい（コマンドラインからの取得、`wmain`、`activeCodePage`）と使い分けは [Windows での注意](windows.md) の章で示す。ここでは関数の一覧だけを示す。列挙は、値を末尾に足すことでだけ拡張される。

### argv

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_argv_get`（`argc, argv`） | Windows では `argc` と `argv` を読まず（0 と NULL でよい）、プロセスのコマンドラインを分割して各要素を UTF-8 にしたハンドルを返す（文字列はハンドルが所有する）。それ以外では、渡された `argc` と同じ argv の文字列を、複製も変換もせずに返す（呼び出し側の argv が有効な間だけ有効。`argc` は 0 以上、`argv[argc]` は NULL で、`argc` が 0 でなければ `argv` は NULL でない）。NULL は返さず、確保に失敗するとメモリ不足の番兵を返す |
| `clopts_argv_from_wide`（`argc, wargv`） | Windows だけ（`_WIN32` のときに宣言される）。`wmain` の引数を UTF-8 にしたハンドルを返す。ハンドルは `clopts_argv_get` と同じもの |
| `clopts_argv_status`（`args`） | 取得の結果（`clopts_argv_status_t`） |
| `clopts_argv_count`（`args`） | 要素の数。状態が `CLOPTS_ARGV_OK` でなければ 0 |
| `clopts_argv_values`（`args`） | 要素の配列（`values[count]` は NULL）。NULL は返さない。`clopts_argv_free` まで有効 |
| `clopts_argv_error_index`（`args`） | `CLOPTS_ARGV_INVALID_UTF16` の要素の添字（`argv[0]` が 0）。ほかの状態では -1 |
| `clopts_argv_free`（`args`） | ハンドルを解放する。NULL と番兵は無視される |

NULL のハンドルは番兵と同じに読まれる（状態は `CLOPTS_ARGV_OUT_OF_MEMORY`、要素の数は 0、要素の配列は空、添字は -1）。`clopts_argv_status_t` の値は次のとおり。

| 値 | 意味 |
|----|------|
| `CLOPTS_ARGV_OK` | 取得できた |
| `CLOPTS_ARGV_INVALID_UTF16` | 正しい UTF-16 でない要素がある（対になっていないサロゲート）。`clopts_argv_error_index` がその要素を示す |
| `CLOPTS_ARGV_CMDLINE_UNAVAILABLE` | Windows で、プロセスのコマンドラインを得られなかった |
| `CLOPTS_ARGV_OUT_OF_MEMORY` | メモリの確保に失敗した |

### 環境変数

| 関数 | 引数と戻り値 |
|------|--------------|
| `clopts_env_new()` | 環境変数を引くための文脈を作って返す。確保に失敗すると NULL |
| `clopts_env_free`（`env`） | 文脈と、それが返したすべての値を解放する。NULL は無視される |
| `clopts_env_lookup`（`ctx, name, value`） | `clopts_getenv_fn` の型の関数。`clopts_env_t *` を `ctx` として `clopts_parse_with_env` に渡す。`name` の UTF-8 の値を `*value` に入れる（`ctx` から借用し、同じ `ctx` での次の呼び出しか `clopts_env_free` まで有効。Windows 以外ではプロセスの環境を指す）。`ctx` が NULL なら `name` を読まずに `CLOPTS_ENV_FAILED` を返す |
| `clopts_env_error`（`env`） | 最後の `clopts_env_lookup` が `CLOPTS_ENV_FAILED` を返した理由（`clopts_env_error_t`）。FOUND と UNSET の後は NONE。NULL は `CLOPTS_ENV_ERROR_OUT_OF_MEMORY` として読まれる |
| `clopts_env_error_name`（`env`） | 失敗した引き方に渡された名前（`env` が所有する複製）。失敗がないとき、または複製できなかったときは NULL |

`clopts_env_error_t` の値は、`CLOPTS_ENV_ERROR_NONE`、`CLOPTS_ENV_ERROR_INVALID_UTF16`（Windows で、値が正しい UTF-16 でない）、`CLOPTS_ENV_ERROR_INVALID_NAME`（Windows で、名前が正しい UTF-8 でない）、`CLOPTS_ENV_ERROR_OUT_OF_MEMORY` である。

`clopts_env_new` が NULL を返しても、そのまま `clopts_parse_with_env` に渡してよい。そのときは環境変数を引く時点で `CLOPTS_ENV_FAILED` になり、解析は `CLOPTS_OUT_OF_MEMORY` で終わる（taskbook はこの扱いをしている）。
