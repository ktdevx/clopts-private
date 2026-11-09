# 解析規則

この章は、CLOpts が argv（コマンドライン引数の並び）をどの規則で解釈するかを示す。オプションの書き方、`--` と `-` 単体、サブコマンドとの並べ方、名前の照合、繰り返し指定、値の優先順位、ヘルプと版の要求、値の型を、規則ごとに実行例とともに説明する。CLOpts で書いたツールの利用者に挙動を説明するときと、想定外の解釈がないかを確かめるときに使う。

規則は宣言の内容によらず、すべてのプログラムで同じである。プログラムの側で規則を切り替える設定はない。関数の使い方は [API リファレンス](api.md) の章で、Windows での argv と環境変数の扱いは [Windows での注意](windows.md) の章で示す。

## 実行例に使うプログラム

この章の実行例は、次の 2 つのサンプルを実行した結果である。どちらもファイルを読んだりタスクを保存したりはせず、解析で得た値を報告するだけである。

- `examples/textcount/textcount.c`（版 0.4.0）: サブコマンドのないフィルタ型のツール。`textcount [<options>] <FILE>...` の形で使う。
- `examples/taskbook/taskbook.c`（版 1.2.3）: サブコマンド `add`、`list`、`done`、`help` を持つツール。グローバルオプションと並べ替えの例に使う。

解析に成功すると、サンプルは値を `<key>: <value> (<出所>)` の形で 1 行ずつ標準出力へ出し、終了コード 0 で終わる。出所は `explicit`（argv から）、`env`（環境変数から）、`default`（既定値から）のどれかで、値を得なかったオプションと位置引数には出所を付けない。リストの値は `, ` で結ぶ。textcount は `files:` の行から始め、taskbook は `command:` の行から始めて、最後にグローバルオプション（`verbose`、`color`）を出す。使い方の誤りでは、エラーの 1 行と使い方の案内を標準エラーへ出し、終了コード 2 で終わる。

textcount のヘルプ（`textcount --help`）:

<!-- clopts-check: -- textcount --help -->
```text
Usage: textcount [<options>] <FILE>...

Count the lines and words of text files.

Arguments:
  <FILE>  Files to count (- for standard input)

Options:
  -l, --lines                             Count lines
  -w, --words                             Count words
      --[no-]total                        Print a total line
  -q, --quiet...                          Print less (repeat for even less)
  -e, --encoding <NAME>                   Text encoding of the files (default: utf-8) [env: TEXTCOUNT_ENCODING]
  -x, --exclude <PATTERN>[,<PATTERN>...]  Skip files matching the patterns
  -h, --help                              Show this help
      --version                           Show version information
```

`-l` と `-w` と `--total` はフラグ（指定したかどうかだけを持つ）、`-q` はカウント（指定した回数を持つ）、`-e` は単一値、`-x` は `,` で区切るリスト値、`FILE` は 1 個以上を受け取る必須の位置引数である。`--total` は `--no-total` で否定できる。

taskbook のヘルプ（`taskbook --help`）:

<!-- clopts-check: -- taskbook --help -->
```text
Usage: taskbook [<options>] <command> [<arguments>]

Keep a small list of tasks.

Commands:
  add   Add a task
  list  List tasks
  done  Mark tasks as finished
  help  Show help for a command

Options:
  -v, --verbose...  Print more details (up to 3 times)
      --[no-]color  Colorize the output
  -h, --help        Show this help
      --version     Show version information

Run 'taskbook <command> --help' for help on a command.
```

`-v` は上限 3 のカウント、`--color` は否定できるフラグで、どちらもグローバルオプションである。`add` は位置引数 `TITLE` とオプション `-t/--tag`（リスト値）・`-p/--priority`（既定値 `normal`）を、`list` は `-a/--all` と `--format`（環境変数 `TASKBOOK_FORMAT`、既定値 `text`）を、`done` は 1 個以上の位置引数 `ID` を持つ。

## 長いオプションと短いオプション

オプションには、長い名前（`--name`）と 1 文字の短い名前（`-n`）がある。宣言によっては片方だけを持つ（textcount の `--total` には短い名前がない）。

長いオプションは次のように書く。

- `--name`: フラグとカウントは、これだけで指定になる。
- `--name=value`: 値を取るオプション（単一値とリスト値）に値を渡す。名前は最初の `=` で終わり、その後ろはすべて値になる（値に `=` を含めてよい）。
- `--name value`: 値を取るオプションは、`=` がなければ次の要素を値にする。

フラグとカウントに `=` で値を付けると、使い方エラーになる。

短いオプションは次のように書く。

- `-o value`: 値を取るオプションは、次の要素を値にする。
- `-ovalue`: 同じ要素の残りがあれば、それを値にする。
- `-abc`: フラグとカウントは 1 つの要素にまとめて書ける（まとめ書き）。左から 1 文字ずつ読み、値を取るオプションが現れたところで、その要素の残り（なければ次の要素）が値になる。

長い書き方（`textcount --lines --encoding=latin1 notes.txt`）:

<!-- clopts-check: -- textcount --lines --encoding=latin1 notes.txt -->
```text
files: notes.txt
lines: true (explicit)
words: false
total: false
quiet: 0
encoding: latin1 (explicit)
exclude:
```

短い書き方で、`-l` と `-w` をまとめ、`-e` の値を同じ要素に続けたもの（`textcount -lw -elatin1 notes.txt`）:

<!-- clopts-check: -- textcount -lw -elatin1 notes.txt -->
```text
files: notes.txt
lines: true (explicit)
words: true (explicit)
total: false
quiet: 0
encoding: latin1 (explicit)
exclude:
```

まとめ書きの最後に値を取るオプションを置き、値を次の要素で渡したもの（`textcount -lwe latin1 notes.txt`）:

<!-- clopts-check: -- textcount -lwe latin1 notes.txt -->
```text
files: notes.txt
lines: true (explicit)
words: true (explicit)
total: false
quiet: 0
encoding: latin1 (explicit)
exclude:
```

値は、書かれたとおりに受け取る。短いオプションの残りは `=` も含めて値になるので、`-e=latin1` の値は `=latin1` である（`textcount -e=latin1 notes.txt`）。

<!-- clopts-check: -- textcount -e=latin1 notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: =latin1 (explicit)
exclude:
```

次の要素を値にするときは、その要素が `-` で始まっていても、オプションとしてではなく値として受け取る（`textcount --encoding -l notes.txt`）。

<!-- clopts-check: -- textcount --encoding -l notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: -l (explicit)
exclude:
```

フラグに値を付けた例（`textcount --lines=yes notes.txt`）:

<!-- clopts-check: exit=2 stream=stderr -- textcount --lines=yes notes.txt -->
```text
textcount: error: option '--lines' does not take a value
Run 'textcount --help' for usage.
```

値を取るオプションの後ろに要素がないときも、使い方エラーになる（`textcount notes.txt -e`）。

<!-- clopts-check: exit=2 stream=stderr -- textcount notes.txt -e -->
```text
textcount: error: option '-e' requires a value
Run 'textcount --help' for usage.
```

## `--` と `-` 単体

`--` より後ろの要素は、`-` で始まっていても、すべて位置引数になる。`--` そのものは位置引数にならない。`-` 単体（標準入力を表すのによく使う）は、`--` の前でも位置引数である。

`-l` という名前のファイルと標準入力を渡す例（`textcount -- -l - notes.txt`）:

<!-- clopts-check: -- textcount -- -l - notes.txt -->
```text
files: -l, -, notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: utf-8 (default)
exclude:
```

`--` の前の `-` 単体（`textcount -l -`）:

<!-- clopts-check: -- textcount -l - -->
```text
files: -
lines: true (explicit)
words: false
total: false
quiet: 0
encoding: utf-8 (default)
exclude:
```

サブコマンドのあるプログラムでは、サブコマンド名より前に `--` を置くと、その後ろの要素もサブコマンド名としては扱われない。位置引数はサブコマンドを選んだ後でしか受け取れないので、サブコマンド名と同じ文字列でも未知のコマンドのエラーになる（`taskbook -- add`）。

<!-- clopts-check: exit=2 stream=stderr -- taskbook -- add -->
```text
taskbook: error: unknown command 'add'
Run 'taskbook --help' for usage.
```

## グローバルオプションとサブコマンド

サブコマンドのあるプログラムでは、プログラムに渡したオプションがグローバルオプションになる。argv は次のように読まれる。

- サブコマンド名より前には、グローバルオプションだけを置ける。サブコマンドのオプションをそこに置くと、未知のオプションのエラーになる。
- 最初のオプションでない要素が、サブコマンド名である。
- サブコマンド名より後ろでは、そのサブコマンドのオプション、グローバルオプション、位置引数を、どの順序で並べてもよい。位置引数どうしの順序は保たれる。

サブコマンドのないプログラム（textcount）では、最初の要素から、オプションと位置引数を自由に並べられる。

オプションの名前は、次の順に探される。

1. 選ばれたサブコマンドのオプション（サブコマンド名より前では、まだ選ばれていないので飛ばす）。長い形では、まず名前がそのまま一致するもの、次に `--no-<name>` を否定できる FLAG の `<name>` の否定として探す。
2. プログラムのオプション（グローバルオプション）。同じく、そのままの一致、次に否定の順で探す。

そのため、プログラムとサブコマンドに同じ名前のオプションを宣言してよく、サブコマンドが選ばれた後はサブコマンドの方が見つかる（グローバルオプションの方は隠れる）。サブコマンド名より前では、グローバルオプションの方が見つかる。どちらのオプションが値を受け取ったかは、結果をオプションのハンドルで読めば区別できる。サブコマンドのヘルプの `Global Options:` には、書ける形（長い名前、否定できるなら `--no-<name>`、短い名前）がすべてそのサブコマンドに隠されたグローバルオプションは出ない。

グローバルオプションをサブコマンド名の前後に置き、サブコマンドのオプションと位置引数を混ぜた例（`taskbook -v add -p high "Write docs" --no-color -t docs`）:

<!-- clopts-check: -- taskbook -v add -p high "Write docs" --no-color -t docs -->
```text
command: add
title: Write docs
tags: docs (explicit)
priority: high (explicit)
verbose: 1 (explicit)
color: false (explicit)
```

`list` のオプション `-a` をサブコマンド名の前に置いた例（`taskbook -a list`）:

<!-- clopts-check: exit=2 stream=stderr -- taskbook -a list -->
```text
taskbook: error: unknown option '-a'
Run 'taskbook --help' for usage.
```

同じ位置でも、サブコマンド名の後ろなら受け取る（`taskbook list -a`）。

<!-- clopts-check: -- taskbook list -a -->
```text
command: list
all: true (explicit)
format: text (default)
verbose: 0
color: false
```

textcount で、位置引数の後ろにオプションを置いた例（`textcount notes.txt -w todo.txt -l`）:

<!-- clopts-check: -- textcount notes.txt -w todo.txt -l -->
```text
files: notes.txt, todo.txt
lines: true (explicit)
words: true (explicit)
total: false
quiet: 0
encoding: utf-8 (default)
exclude:
```

## 名前の照合（前方一致と大文字小文字）

オプション名とサブコマンド名は、宣言した名前と完全に一致するときだけ認識される。

- 長いオプションの前方一致による省略は行わない。`--lines` を `--lin` と書くことはできない（候補が 1 つに決まる場合でも同じ）。
- 大文字と小文字を区別する。`--Lines` は `--lines` と別の名前で、`-L` は `-l` と別の名前である。
- サブコマンド名も同じで、省略形や大文字小文字の違う名前は未知のコマンドになる。

長いオプションの省略を試した例（`textcount --lin notes.txt`）:

<!-- clopts-check: exit=2 stream=stderr -- textcount --lin notes.txt -->
```text
textcount: error: unknown option '--lin'
Run 'textcount --help' for usage.
```

大文字小文字を変えた例（`textcount --Lines notes.txt` と `textcount -L notes.txt`）:

<!-- clopts-check: exit=2 stream=stderr -- textcount --Lines notes.txt -->
```text
textcount: error: unknown option '--Lines'
Run 'textcount --help' for usage.
```

<!-- clopts-check: exit=2 stream=stderr -- textcount -L notes.txt -->
```text
textcount: error: unknown option '-L'
Run 'textcount --help' for usage.
```

サブコマンド名の省略（`taskbook li`）:

<!-- clopts-check: exit=2 stream=stderr -- taskbook li -->
```text
taskbook: error: unknown command 'li'
Run 'taskbook --help' for usage.
```

## 繰り返し指定、否定、カウント

同じオプションを複数回指定したときの扱いは、オプションの種類で決まる。

| 種類 | 繰り返したとき |
|------|----------------|
| 単一値 | 後勝ち。最後に指定した値だけが残る |
| リスト値 | 連結。指定した順に、区切り文字で分けた値をすべて並べる |
| フラグ | 重複を許す。何回指定しても true |
| カウント | 指定した回数を数える。上限を宣言していれば、上限で頭打ちになる |

否定できるフラグ（ヘルプで `--[no-]name` と表示されるもの）は、`--no-<name>` で false にできる。`--name` と `--no-<name>` を両方書いたときは、最後に書いたほうが残る。`--no-<name>` で false にした値の出所は `explicit` である。否定できないフラグに `--no-<name>` を書くと、未知のオプションになる。

カウントは、短い名前を繰り返して `-vvv` のように書ける。`-q -q --quiet` のように、書き方を混ぜて数えてもよい。サブコマンドのあるプログラムでは、サブコマンド名の前後に書いたグローバルオプションの回数が合算される。

単一値とリスト値とフラグの繰り返し（`textcount -e latin1 -e sjis -x draft.txt --exclude="*.bak,*.tmp" -l -l notes.txt`）:

<!-- clopts-check: -- textcount -e latin1 -e sjis -x draft.txt --exclude="*.bak,*.tmp" -l -l notes.txt -->
```text
files: notes.txt
lines: true (explicit)
words: false
total: false
quiet: 0
encoding: sjis (explicit)
exclude: draft.txt, *.bak, *.tmp (explicit)
```

否定とカウント（`textcount --total --no-total -qqq notes.txt`）:

<!-- clopts-check: -- textcount --total --no-total -qqq notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false (explicit)
quiet: 3 (explicit)
encoding: utf-8 (default)
exclude:
```

否定できないフラグに `--no-` を付けた例（`textcount --no-lines notes.txt`）:

<!-- clopts-check: exit=2 stream=stderr -- textcount --no-lines notes.txt -->
```text
textcount: error: unknown option '--no-lines'
Run 'textcount --help' for usage.
```

textcount の `-q` には上限がない。上限で頭打ちになる例は、上限 3 の taskbook の `-v` で示す（`taskbook -vvvv list`）。

<!-- clopts-check: -- taskbook -vvvv list -->
```text
command: list
all: false
format: text (default)
verbose: 3 (explicit)
color: false
```

サブコマンド名の前後に書いたグローバルオプションの回数の合算（`taskbook -v list -v`）:

<!-- clopts-check: -- taskbook -v list -v -->
```text
command: list
all: false
format: text (default)
verbose: 2 (explicit)
color: false
```

## 値の優先順位と出所

値を取るオプション（単一値とリスト値）の値は、次の優先順位で決まる。

1. 明示指定: argv で指定した値。
2. 環境変数: argv で指定がなく、オプションに環境変数が宣言されていて、その環境変数が設定されているとき、その値。空の文字列が設定されているときも、空の文字列が値になる。
3. 既定値: どちらもないとき、宣言した既定値。
4. どれもなければ、値なし。

フラグとカウントは argv からだけ値を得る（環境変数と既定値は使わない）。

どの経路で値が決まったかは、オプションごとに `clopts_result_get_source` で判別できる（[API リファレンス](api.md#結果のアクセサ) の章を参照）。サンプルの報告の括弧の中が、その出所である。

textcount の `-e/--encoding` は、環境変数 `TEXTCOUNT_ENCODING` と既定値 `utf-8` を持つ。どちらも指定しないとき（`textcount notes.txt`）:

<!-- clopts-check: -- textcount notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: utf-8 (default)
exclude:
```

環境変数を設定したとき（`TEXTCOUNT_ENCODING=sjis textcount notes.txt`）:

<!-- clopts-check: env=TEXTCOUNT_ENCODING=sjis -- textcount notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: sjis (env)
exclude:
```

環境変数を設定したうえで、argv でも指定したとき（`TEXTCOUNT_ENCODING=sjis textcount -e latin1 notes.txt`）:

<!-- clopts-check: env=TEXTCOUNT_ENCODING=sjis -- textcount -e latin1 notes.txt -->
```text
files: notes.txt
lines: false
words: false
total: false
quiet: 0
encoding: latin1 (explicit)
exclude:
```

サブコマンドのオプションでも同じである。taskbook の `list --format` は環境変数 `TASKBOOK_FORMAT` と既定値 `text` を持つ（`TASKBOOK_FORMAT=json taskbook list --format text`）。

<!-- clopts-check: env=TASKBOOK_FORMAT=json -- taskbook list --format text -->
```text
command: list
all: false
format: text (explicit)
verbose: 0
color: false
```

## 終端オプション、ヘルプと版、使い方エラー

解析器には、組み込みのオプションもコマンドもない。`--help`、`-h`、`--version` は終端の FLAG として、`help` コマンドは `clopts/help.h` の `clopts_help_command_create` で足す、ふつうの宣言である。taskbook と textcount は、プログラムに `-h` / `--help` と `--version` を、taskbook はさらに `help` コマンドを宣言している。

`--help` と `--version` は終端オプション（`clopts_option_set_terminal` を設定した FLAG）である。終端オプションは次のように扱われる。

- argv に現れると、値を記録し、解析はそこで終わる。後ろの要素は読まない。環境変数と既定値は読むが、必須のオプションと位置引数は検査しない。状態は `CLOPTS_TERMINATED` で、`clopts_result_get_terminal_option` がそのオプションを、`clopts_result_get_command` がそのとき選ばれているサブコマンド（なければ NULL）を返す。taskbook と textcount は、それが `--help` ならそのサブコマンドの（なければプログラム全体の）ヘルプを、`--version` なら版を出す。
- 短い形は、まとめ書きの中（`-lh`）でも終端になる。そこまでの文字は読まれ、後ろの文字は読まれない。
- 値をとらないので、`--help=x` は値をとらないオプションのエラーになる。
- 使い方エラーを見つけても、その要素から `--` の手前までに、その範囲（選ばれたサブコマンドとグローバルオプション）の終端オプションが `--<name>` か `-<c>` のそのままの形であれば、最初に見つけたものが優先される（ここでは、まとめ書きの中の文字は数えない）。

`help` コマンドは、`help` という名前のふつうのサブコマンドで、省略できる位置引数 `COMMAND` を 1 つとる。解析器にとっては `taskbook help add` も `taskbook help remove` も成功（`CLOPTS_OK`）で、`help` コマンドが選ばれ、位置引数が 1 つ得られる。名前をサブコマンドとして引くのは `clopts_help_find_target` で、名前がサブコマンドでなければ false を返す。taskbook は、そのときエラーの 1 行と使い方の案内を出す。`help` の後ろに 2 つ以上の要素があれば、予期しない引数のエラーになる。

使い方エラーが複数あっても、報告されるのは最初に見つけた 1 つだけである。argv の読み取り中のエラー（未知のオプションなど）は、必須のオプションや位置引数がないことの検査より先に見つかる。

未知のオプションの後ろの `--version`（`textcount --bogus --version`）:

<!-- clopts-check: -- textcount --bogus --version -->
```text
textcount 0.4.0
```

サブコマンドを選んだ後の、未知のオプションの後ろの `-h` は、そのサブコマンドのヘルプになる（`taskbook done --bogus -h`）。

<!-- clopts-check: -- taskbook done --bogus -h -->
```text
Usage: taskbook done [<options>] <ID>...

Mark the given tasks as finished.

Arguments:
  <ID>  Numbers of the tasks

Global Options:
  -v, --verbose...  Print more details (up to 3 times)
      --[no-]color  Colorize the output
  -h, --help        Show this help
      --version     Show version information
```

`--` より後ろの `--help` は位置引数なので、使い方エラーが残る（`textcount --bogus -- --help`）。

<!-- clopts-check: exit=2 stream=stderr -- textcount --bogus -- --help -->
```text
textcount: error: unknown option '--bogus'
Run 'textcount --help' for usage.
```

誤りが 2 つあるときは、最初の 1 つだけが報告される。次の例では、未知のオプションが 2 つあり、必須の `FILE` もないが、報告は `--bogus` だけである（`textcount --bogus -L`）。

<!-- clopts-check: exit=2 stream=stderr -- textcount --bogus -L -->
```text
textcount: error: unknown option '--bogus'
Run 'textcount --help' for usage.
```

`help <cmd>` でサブコマンドのヘルプを出す例（`taskbook help add`）。`taskbook add --help` と同じ出力である。

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

`help` の後ろがサブコマンドでない例（`taskbook help remove`）:

<!-- clopts-check: exit=2 stream=stderr -- taskbook help remove -->
```text
taskbook: error: unknown command 'remove'
Run 'taskbook --help' for usage.
```

## 値は文字列

CLOpts は、オプションと位置引数の値を、argv に書かれた（または環境変数と既定値の）文字列のまま返す。数値への変換、選択肢の中にあるかの確認、範囲の検査など、値の型変換と妥当性の検査は利用側が行う。CLOpts がこれらの理由で使い方エラーを出すことはない。

taskbook の `done` の `ID` はタスクの番号を表すが、数値でない値もそのまま文字列として受け取る（`taskbook done abc 1.5`）。

<!-- clopts-check: -- taskbook done abc 1.5 -->
```text
command: done
ids: abc, 1.5
verbose: 0
color: false
```

`add` の `--priority` はヘルプで `low, normal or high` と説明しているが、それ以外の値も受け取る（`taskbook add "Write docs" -p urgent`）。

<!-- clopts-check: -- taskbook add "Write docs" -p urgent -->
```text
command: add
title: Write docs
tags:
priority: urgent (explicit)
verbose: 0
color: false
```

`-` で始まる要素はオプションとして読まれるので、負の数を位置引数として渡すには `--` の後ろに置く。`taskbook done -3` は未知のオプション `-3` のエラーになる。

<!-- clopts-check: exit=2 stream=stderr -- taskbook done -3 -->
```text
taskbook done: error: unknown option '-3'
Run 'taskbook done --help' for usage.
```

`--` の後ろに置けば、位置引数として受け取る（`taskbook done -- -3`）。

<!-- clopts-check: -- taskbook done -- -3 -->
```text
command: done
ids: -3
verbose: 0
color: false
```
