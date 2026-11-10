# Windows での注意

この章は、Windows で CLOpts を使うときの注意を示す。非 ASCII の引数と環境変数を UTF-8 で受け取る方法、`wmain` を使うときの変換、実行ファイルのマニフェストの `activeCodePage` との関係、共有版の DLL の置き場所、コンソールへ UTF-8 の文面を出すときのコードページを扱う。

ここで使う関数は `clopts/platform.h` にある。関数ごとの引数と戻り値の一覧は [API リファレンス](api.md#プラットフォームの関数) の章にある。解析の関数と結果の読み方は同じ章の残りの節で、引数の解釈の規則は [解析規則](parsing.md) の章で示す。

CLOpts は、引数と環境変数の値を UTF-8 の文字列として扱う。Windows 以外の OS では、`clopts/platform.h` の関数は何も変換しない（後で述べるパススルー）。そのため、この章の手順どおりに書いたプログラムは、同じソースのまま Linux や macOS でもビルドでき、動く。サンプルの `examples/taskbook/taskbook.c` と `examples/textcount/textcount.c` は、この章の手順で書かれている。

## UTF-8 の argv を得る

Windows では、`main` が受け取る `argv` は UTF-8 ではない。C の実行時ライブラリ（CRT）が、プロセスのコマンドライン（UTF-16）を ANSI のコードページ（日本語の環境なら CP932 など）に変換したものである。そのコードページで表せない文字は、`?` などの別の文字に置き換わり、元には戻せない。

そこで、`main` の `argv` をそのまま `clopts_parse` に渡すのではなく、`clopts_argv_create` で UTF-8 の argv を得てから渡す。手順は次のとおり。

1. `clopts_argv_create`（`argc, argv`）で argv のハンドルを得る。
2. `clopts_argv_get_status` が `CLOPTS_ARGV_OK` でなければ、エラーを出して終わる。このときもハンドルは `clopts_argv_destroy` に渡す。
3. `clopts_argv_get_count` と `clopts_argv_get_values` を、`clopts_parse` か `clopts_parse_with_env` の `argc` と `argv` に渡す。
4. 結果を使い終えて `clopts_result_destroy` を呼んだ後で、`clopts_argv_destroy` でハンドルを解放する。

textcount の `main` から抜き出すと、次のようになる（宣言と、結果を使う `finish` は省いている）。

```c
    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_create(argc, argv);
    if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("textcount: error: cannot read the command line arguments\n", stderr);
        clopts_argv_destroy(args);
        clopts_program_destroy(program);
        return 1;
    }

    /* If clopts_env_create failed, looking up TEXTCOUNT_ENCODING fails and the
       parse reports out of memory. */
    env = clopts_env_create();
    result = clopts_parse_with_env(program, clopts_argv_get_count(args), clopts_argv_get_values(args),
                                   clopts_env_lookup, env);
    code = finish(program, result);

    clopts_result_destroy(result);
    clopts_env_destroy(env);
    clopts_argv_destroy(args);
    clopts_program_destroy(program);
    return code;
```

### Windows での取得

Windows の `clopts_argv_create` は、渡された `argc` と `argv` を読まない（0 と NULL を渡してもよい）。プロセスのコマンドライン（`GetCommandLineW()` が返す UTF-16 の文字列）を、現在の MSVC の CRT と同じ規則で要素に分け、各要素を UTF-8 に変換する。CLOpts はこの分割に shell32 の `CommandLineToArgvW` を使わない。

変換した文字列は、ハンドルが所有する。呼び出し側の `argv` とは別のメモリにあり、`clopts_argv_destroy` を呼ぶまで有効である。

### 他の OS ではパススルー

Windows 以外の OS では、`clopts_argv_create` は渡された `argc` と、`argv` の文字列そのものを返す。配列も文字列も複製せず、変換もしない。そのため、次の点に注意する。

- 文字列は、呼び出し側の `argv` が有効な間だけ有効である。`main` の `argv` を渡したときは、プログラムの終わりまで有効である。
- `argc` は 0 以上で、`argv[argc]` は NULL でなければならない。`argc` が 0 でなければ `argv` は NULL であってはならない。`main` の引数をそのまま渡せば、この条件を満たす。
- 文字列の符号化は変わらない。端末やロケールが UTF-8 であれば、UTF-8 の文字列が渡る。
- パススルーのときもハンドルは確保されるので、`clopts_argv_destroy` を呼ぶ。`clopts_argv_destroy` は、呼び出し側の `argv` の文字列を解放しない。

Windows で `clopts_argv_create` に 0 と NULL を渡すと、同じソースを他の OS でビルドしたときに引数が空になる。どの OS でもビルドするソースでは、`main` の `argc` と `argv` を渡す。

### 状態と失敗の扱い

`clopts_argv_create` は NULL を返さない。確保に失敗したときは、共有のメモリ不足の番兵を返す。状態が `CLOPTS_ARGV_OK` でないときは、`clopts_argv_get_count` は 0、`clopts_argv_get_values` は空の配列（先頭が NULL）を返す。状態の値は次のとおり。

| 状態 | 起きるとき | 扱い |
|------|------------|------|
| `CLOPTS_ARGV_OK` | 取得できた | `clopts_parse` に渡す |
| `CLOPTS_ARGV_INVALID_UTF16` | Windows で、正しい UTF-16 でない要素がある（対になっていないサロゲート） | `clopts_argv_get_error_index` で何番目の要素か（`argv[0]` が 0）がわかる。その要素は UTF-8 にできないので、文面には番号だけを出す |
| `CLOPTS_ARGV_CMDLINE_UNAVAILABLE` | Windows で、プロセスのコマンドラインを得られなかった | エラーを出して終わる |
| `CLOPTS_ARGV_OUT_OF_MEMORY` | メモリの確保に失敗した | エラーを出して終わる |

正しくない UTF-16 は、置換文字などに置き換えずに `CLOPTS_ARGV_INVALID_UTF16` として報告される。どの要素が原因かを示すには、次のように書く。

```c
    args = clopts_argv_create(argc, argv);
    if (clopts_argv_get_status(args) == CLOPTS_ARGV_INVALID_UTF16)
    {
        fprintf(stderr, "textcount: error: argument %d is not valid UTF-16\n", clopts_argv_get_error_index(args));
    }
    else if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("textcount: error: cannot read the command line arguments\n", stderr);
    }
```

どの状態のハンドルも `clopts_argv_destroy` に渡してよい（番兵と NULL は無視される）。失敗したときに返るハンドルも確保されていることがあるので、状態にかかわらず必ず解放する。

### 解放の順序

`clopts_parse` の結果は、argv の文字列を借用している。`clopts_result_get_positional_at` と `clopts_result_get_error_token` は argv の要素そのものを返し、ほかの値も argv の文字列を指すことがある。そのため、argv のハンドルは、結果を使い終えてから解放する。

1. 結果から値を読み、文面を出す（`clopts_write_*`）。
2. `clopts_result_destroy` で結果を解放する。
3. `clopts_argv_destroy` で argv のハンドルを解放する。

環境変数の文脈（`clopts_env_t`）は、この順序に入らない。次の節で述べるとおり、環境変数の値は解析中に複製されるので、`clopts_env_destroy` は解析の後ならいつ呼んでもよい。所有と寿命の規則の全体は、[API リファレンス](api.md) の章の「メモリの所有と寿命」の節にある。

## `wmain` を使うとき

プログラムが `main` ではなく `wmain` を入口にしているときは、`wmain` が受け取る UTF-16 の `argv` を `clopts_argv_create_from_wide` で UTF-8 に変換する。この関数は Windows でだけ（`_WIN32` が定義されているときだけ）宣言される。

```c
#include <stdio.h>
#include <wchar.h>

#include "clopts/clopts.h"

int wmain(int argc, wchar_t **wargv)
{
    clopts_program_t *program = NULL;
    clopts_argv_t *args;
    clopts_result_t *result;
    int code = 0;

    if (clopts_program_create("app", &program) != CLOPTS_DECL_ERROR_NONE)
    {
        fputs("app: error: out of memory\n", stderr);
        return 1;
    }
    /* ... declare options and positionals on program ... */

    args = clopts_argv_create_from_wide(argc, wargv);
    if (clopts_argv_get_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("app: error: cannot read the command line arguments\n", stderr);
        clopts_argv_destroy(args);
        clopts_program_destroy(program);
        return 1;
    }

    result = clopts_parse(program, clopts_argv_get_count(args), clopts_argv_get_values(args));
    /* ... use the result ... */

    clopts_result_destroy(result);
    clopts_argv_destroy(args);
    clopts_program_destroy(program);
    return code;
}
```

- `argc` は 0 以上で、`wargv` と `wargv[0]` から `wargv[argc - 1]` までは NULL であってはならない。`wmain` の引数をそのまま渡せば、この条件を満たす。
- 返すハンドルは `clopts_argv_create` と同じものである。状態の確かめ方（`CLOPTS_ARGV_INVALID_UTF16` と `clopts_argv_get_error_index` を含む）、文字列をハンドルが所有すること、`clopts_argv_destroy` を呼ぶ時期も同じである。
- `clopts_argv_create_from_wide` はコマンドラインを分割しない。CRT が `wmain` のために分割した要素を、そのまま変換する。
- `wmain` は Windows だけの入口である。MSVC では `wmain` をそのまま書けばよい。MinGW-w64 では、リンクに `-municode` を加える。

`main` を入口にしたまま `clopts_argv_create` を使っても、`wmain` と同じく UTF-16 のコマンドラインから UTF-8 の argv が得られる。どの OS でもビルドするプログラムでは、`main` と `clopts_argv_create` を使う方が、`#ifdef _WIN32` で入口を分けずに済む。

## 環境変数を UTF-8 で得る

Windows では、CRT の `getenv` も、値を ANSI のコードページで返す。環境変数のフォールバック（`clopts_option_set_env` で宣言したもの）で UTF-8 の値を得るには、`clopts_env_lookup` を `clopts_env_t` の文脈とともに `clopts_parse_with_env` に渡す。

1. `clopts_env_create()` で文脈を作る。
2. `clopts_parse_with_env` に、`getenv_fn` として `clopts_env_lookup` を、`ctx` としてその文脈を渡す。
3. 解析の後で `clopts_env_destroy` を呼ぶ。

前の節の textcount の抜粋が、この手順の例である。taskbook も同じ方法で `TASKBOOK_FORMAT` を読む。

- Windows の `clopts_env_lookup` は、変数の値を `GetEnvironmentVariableW` で UTF-16 のまま読み、UTF-8 に変換する。変数の名前（`clopts_option_set_env` で宣言した名前）は UTF-8 として UTF-16 に変換してから引く。Windows では、環境変数の名前の大文字と小文字は区別されない。
- Windows 以外の OS では、`getenv` で読んだ値をそのまま返す（パススルー）。値は変換されず、プロセスの環境の中を指す。
- 返した値は文脈から借用したもので、同じ文脈での次の呼び出しか `clopts_env_destroy` まで有効である。解析の関数は値を受け取った直後に複製するので、結果の値は文脈を解放した後も読める。そのため、`clopts_env_destroy` は解析の後ならいつ呼んでもよい。argv のハンドルと違い、結果を使い終えるまで待つ必要はない。
- `clopts_env_create` が確保に失敗して NULL を返しても、そのまま `clopts_parse_with_env` に渡してよい。そのときは環境変数を引く時点で `CLOPTS_ENV_FAILED` になり、解析は `CLOPTS_PARSE_OUT_OF_MEMORY` で終わる。`clopts_env_destroy` は NULL を無視する。
- 1 つの文脈を、複数のスレッドから同時に使ってはならない。

### 引き方の失敗

`clopts_env_lookup` が `CLOPTS_ENV_FAILED` を返すと、解析は `CLOPTS_PARSE_OUT_OF_MEMORY` で終わる。メモリ不足のほかに、Windows では次の場合にも失敗する。

| `clopts_env_get_error` | 起きるとき |
|--------------------|------------|
| `CLOPTS_ENV_ERROR_INVALID_UTF16` | Windows で、変数の値が正しい UTF-16 でない |
| `CLOPTS_ENV_ERROR_INVALID_NAME` | Windows で、引こうとした名前が正しい UTF-8 でない |
| `CLOPTS_ENV_ERROR_OUT_OF_MEMORY` | メモリの確保に失敗した（Windows では、`GetEnvironmentVariableW` が変数がない以外の理由で失敗したときも） |

解析が `CLOPTS_PARSE_OUT_OF_MEMORY` で終わったときに、本当のメモリ不足か環境変数の問題かを区別したければ、`clopts_env_get_error`（`env`）を見る。最後の引き方が失敗していれば、その理由を返し、`clopts_env_get_error_name`（`env`）が引こうとした名前を返す（名前を複製できなかったときは NULL）。最後の引き方が成功していれば `CLOPTS_ENV_ERROR_NONE` である。文脈が NULL のときは `CLOPTS_ENV_ERROR_OUT_OF_MEMORY` として読まれる。

## `activeCodePage` との関係

Windows 10 version 1903 以降では、実行ファイルのマニフェストに `activeCodePage` を `UTF-8` と書くと、そのプロセスの ANSI のコードページが UTF-8 になる。このとき、`main` の `argv` や CRT の `getenv` も UTF-8 の文字列を返す。マニフェストは次のような形である（埋め込み方はツールチェーンの文書を参照する）。

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
  <application>
    <windowsSettings>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>
```

CLOpts の関数は、このマニフェストを必要としない。`clopts_argv_create`、`clopts_argv_create_from_wide`、`clopts_env_lookup` は、ANSI のコードページを経由せずに UTF-16 から直接 UTF-8 に変換するので、マニフェストがなくても、それより前の版の Windows でも、UTF-8 の引数と値が得られる。

マニフェストがあっても、CLOpts の関数のふるまいは変わらない。両方を使ってかまわない。マニフェストで `main` の `argv` が UTF-8 になるからといって `clopts_argv_create` を省くと、マニフェストのない環境（埋め込み忘れ、古い Windows）で文字が失われる。CLOpts の関数を使っておけば、マニフェストの有無に左右されない。

マニフェストは、CLOpts が扱わない部分（ファイル名を受け取る `fopen` など、ANSI のコードページを使う API）にも効く。そうした部分にも UTF-8 を使いたいかは、プログラムの側で決める。

## 共有版の DLL 名と PATH

共有版の CLOpts の DLL のファイル名には、版が入らない（MSVC では `clopts.dll`、MinGW-w64 では `libclopts.dll`）。そのため、PATH の通ったディレクトリに別の版の CLOpts の DLL があると、それが読み込まれることがある。互換のない版が読み込まれると、起動時に失敗するか、誤った動作をする。

これを避けるには、次のどちらかにする。

- DLL を実行ファイルの横に置く。Windows は、PATH より先に実行ファイルのディレクトリから DLL を探す。ビルドの後に DLL を実行ファイルの横にコピーする CMake の記述は、[取り込み手順](integration.md#静的と共有の選び方) の章にある。
- 静的版を使う（既定の形式）。DLL がないので、取り違えは起きない。

どの版の DLL に差し替えてよいか（SOVERSION の規則）と、DLL 名の詳細は、[ABI の互換性](abi.md#windows-の-dll-名) の章の「Windows の DLL 名」を参照する。

## コンソールのコードページ

ヘルプやエラーの文面には、宣言した説明や、利用者が渡した引数（エラーのトークン）が入る。argv と環境変数を UTF-8 で得ると、これらの文面も UTF-8 になる。

CLOpts の文面の関数（`clopts_write_*`）は、書き込みのコールバックを通してだけ書き、コンソールに直接は出力しない。`clopts/writer.h` の `clopts_file_write` は、受け取ったバイト列を `fwrite` でそのまま書き、符号化を変換しない。ライブラリは、コンソールのコードページも変えない。

Windows のコンソールは、既定では UTF-8 ではないコードページ（OEM のコードページ。日本語の環境なら CP932 など）で文字を表示する。そのままでは、非 ASCII の文字を含む UTF-8 の文面が文字化けする。マニフェストの `activeCodePage` も、コンソールのコードページは変えない。

コンソールに UTF-8 の文面を出すかは、プログラムの側で決める。出すときは、文面を出す前に `SetConsoleOutputCP(CP_UTF8)`（`<windows.h>`）を呼ぶ。コンソールのコードページはコンソールの設定なので、プログラムが終わった後も同じコンソールに残る。元に戻したいときは、`GetConsoleOutputCP()` で前の値を取っておき、終わる前に戻す。

```c
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
    UINT previous_cp = GetConsoleOutputCP();

    SetConsoleOutputCP(CP_UTF8);
#endif
    /* ... declare, parse, write the help and errors ... */
#ifdef _WIN32
    SetConsoleOutputCP(previous_cp);
#endif
    return 0;
}
```

出力をファイルやパイプにリダイレクトしたときは、コンソールのコードページは関係せず、UTF-8 のバイト列がそのまま書かれる。
