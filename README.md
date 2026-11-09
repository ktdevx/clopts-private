# CLOpts

CLOpts は、C99 で書かれた、外部依存ゼロのコマンドライン引数パーサライブラリである。サブコマンド、オプション、位置引数を関数で宣言すると、同じ宣言から引数の解析、ヘルプや使い方の文面、使い方の誤りの報告が決まる。依存するのは C 標準ライブラリと、Windows では kernel32 の API だけである。

C++ の同名のプロジェクト [Sirraide/clopts](https://github.com/Sirraide/clopts) とは別のプロジェクトであり、API にも実装にも関係はない。

## 特徴

- **宣言からの解析と文面の生成**: `clopts_program_new` から始めて、サブコマンド、オプション（フラグ、カウント、単一値、リスト値）、位置引数を関数で足す。解析の規則とヘルプ・使い方・版の文面は、すべてこの宣言から決まる。宣言を 1 か所変えれば、挙動とヘルプが一緒に変わる。
- **構造化エラー**: 使い方の誤りは文字列ではなく、種別、該当するコマンド・オプション・トークン、argv の添字をアクセサで読めるエラーとして返る。宣言の誤り（名前の重複など）は使い方の誤りと区別して返る。
- **入出力も `exit` もしない**: ライブラリは標準出力にも標準エラーにも書かず、`exit` も呼ばない。ヘルプやエラーの文面は書き込みのコールバックに渡すので、表示の仕方と終了コードは利用側が決める。`FILE*` に出すなら、公開ヘッダの `clopts_file_write` を渡すだけでよい。
- **アロケーターの差し替え**: `clopts_set_allocator` で、ライブラリが使う `malloc` / `calloc` / `realloc` / `free` を差し替えられる。ライブラリの確保と解放は、例外なく設定した関数を通る。
- **Windows での UTF-8 の引数**: `clopts_argv_get` が Windows のコマンドラインを UTF-8 の argv に変換する。環境変数も UTF-8 で得られる。他の OS では何も変換しないので、同じソースがどの OS でも動く。
- **静的版と共有版の選択、1.0.0 以降の ABI 後方互換**: 静的ライブラリとしても共有ライブラリとしてもビルドでき、取り込み方によらず形式を選べる。公開 API は不透明なハンドルと関数、および配置を凍結した構造体（アロケーターの `clopts_allocator_t`）でできており、1.0.0 以降は共有ライブラリの ABI の後方互換を保証する。保証の範囲は [ABI の互換性](docs/abi.md) の章に示す。

## 最小のサンプル

名前を 1 つ受け取って挨拶するプログラム `hello` を示す。宣言し、UTF-8 の argv を得て解析し、状態に応じて結果を読むかヘルプ・版・エラーの文面を出し、最後に解放する。

<!-- clopts-source: tests/docs/readme_minimal.c -->
```c
#include <stdio.h>

#include "clopts/clopts.h"

int main(int argc, char **argv)
{
    clopts_program_t *program = clopts_program_new("hello");
    clopts_command_t *root;
    clopts_option_t *greeting;
    clopts_positional_t *name;
    clopts_argv_t *args;
    clopts_result_t *result;
    int code = 0;

    if (program == NULL)
    {
        fputs("hello: error: out of memory\n", stderr);
        return 1;
    }
    root = clopts_program_root(program);

    /* Declare: hello [-g TEXT] NAME */
    clopts_program_set_version(program, "1.0.0");
    clopts_command_set_description(root, "Greet someone.");
    greeting = clopts_command_add_value(root, "greeting", 'g');
    clopts_option_set_value_name(greeting, "TEXT");
    clopts_option_set_default(greeting, "Hello");
    clopts_option_set_help(greeting, "Word to greet with");
    name = clopts_command_add_positional(root, "NAME");
    clopts_positional_set_required(name, true);
    clopts_positional_set_help(name, "Who to greet");

    /* UTF-8 argv on every platform (on Windows, taken from the command line). */
    args = clopts_argv_get(argc, argv);
    if (clopts_argv_status(args) != CLOPTS_ARGV_OK)
    {
        fputs("hello: error: cannot read the command line arguments\n", stderr);
        clopts_argv_free(args);
        clopts_program_free(program);
        return 1;
    }

    result = clopts_parse(program, clopts_argv_count(args), clopts_argv_values(args));
    switch (clopts_result_status(result))
    {
    case CLOPTS_OK:
        printf("greeting: %s (%s)\n", clopts_result_value(result, greeting),
               clopts_result_source(result, greeting) == CLOPTS_SOURCE_DEFAULT ? "default" : "explicit");
        printf("name: %s\n", clopts_result_positional_at(result, 0));
        break;
    case CLOPTS_HELP:
        clopts_write_help(program, clopts_result_command(result), clopts_file_write, stdout);
        break;
    case CLOPTS_VERSION:
        clopts_write_version(program, clopts_file_write, stdout);
        break;
    case CLOPTS_USAGE_ERROR:
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        clopts_write_usage_hint(program, clopts_result_error_command(result), clopts_file_write, stderr);
        code = 2;
        break;
    case CLOPTS_NO_COMMAND:
        /* Only programs with subcommands get this; kept for the common pattern. */
        clopts_write_usage(program, NULL, clopts_file_write, stderr);
        clopts_write_usage_hint(program, NULL, clopts_file_write, stderr);
        code = 2;
        break;
    default: /* CLOPTS_DECL_ERROR, CLOPTS_OUT_OF_MEMORY */
        clopts_write_error(program, result, CLOPTS_PREFIX_PROGRAM | CLOPTS_PREFIX_ERROR, clopts_file_write, stderr);
        code = 1;
        break;
    }

    clopts_result_free(result);
    clopts_argv_free(args);
    clopts_program_free(program);
    return code;
}
```

`hello --help` を実行すると、宣言から作られた次のヘルプが標準出力に出る（終了コード 0）。

<!-- clopts-check: -- hello --help -->
```text
Usage: hello [<options>] <NAME>

Greet someone.

Arguments:
  <NAME>  Who to greet

Options:
  -g, --greeting <TEXT>  Word to greet with (default: Hello)
  -h, --help             Show this help
      --version          Show version information
```

`hello World` なら `greeting: Hello (default)` と `name: World` を出して終了コード 0 で終わり、`NAME` を渡さなければエラーの 1 行と使い方の案内を標準エラーへ出して終了コード 2 で終わる。関数ごとの説明は [API リファレンス](docs/api.md) の章に、引数の解釈の規則は [解析規則](docs/parsing.md) の章にある。

## 取り込み方

CMake 3.21 以上のプロジェクトに、次の 3 通りのどれかで取り込む。どの経路でも、利用側は `target_link_libraries(app PRIVATE CLOpts::clopts)` で `CLOpts::clopts` にリンクするだけでよい。取り込まれた CLOpts は、テストやサンプルをビルドせず、利用側のビルドの設定（`CMAKE_*` の変数、CRT の種別、警告）を変えない。手順の詳細、CMake のオプションの一覧、MSVC の CRT の注意は [取り込み手順](docs/integration.md) の章にある。

### add_subdirectory

CLOpts のソースを利用側のリポジトリの中（次の例では `external/clopts`）に置き、`add_subdirectory` で加える。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app C)

add_subdirectory(external/clopts)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
```

### FetchContent

版のタグ（版番号の前に `v` を付けた形）を指定して、構成時に取得する。ブランチ名ではなく版のタグを指定すること。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app C)

include(FetchContent)
FetchContent_Declare(clopts
    GIT_REPOSITORY https://github.com/ktdevx/clopts-private.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(clopts)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
```

### install と find_package

CLOpts を利用側とは別にビルドして install し（`/opt/clopts` は install 先の例）、利用側は `find_package(CLOpts CONFIG)` で探す。

```sh
cmake -S clopts -B build-clopts -DCMAKE_BUILD_TYPE=Release -DCLOPTS_BUILD_TESTING=OFF
cmake --build build-clopts
cmake --install build-clopts --prefix /opt/clopts
```

```cmake
cmake_minimum_required(VERSION 3.21)
project(app C)

find_package(CLOpts 0.1.0 CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
```

利用側を構成するときに、install 先を `CMAKE_PREFIX_PATH` で渡す（`cmake -S app -B build-app -DCMAKE_PREFIX_PATH=/opt/clopts`）。

### 静的版と共有版の選び方

既定は静的版である。迷ったら静的版を使う。

`add_subdirectory` と `FetchContent` では、`CLOPTS_BUILD_SHARED` で選ぶ（ON なら共有版、OFF なら静的版）。既定は `BUILD_SHARED_LIBS` の値（未定義なら OFF）である。CLOpts を加えるより前に、通常の変数としてセットする。

```cmake
set(CLOPTS_BUILD_SHARED ON)
add_subdirectory(external/clopts)
```

install 経路では、静的版と共有版を同じ prefix に並べて install でき、`find_package` の `COMPONENTS static` または `COMPONENTS shared` で形式を指定する。`COMPONENTS` を書かないときは、`CLOPTS_SHARED_LIBS`、`BUILD_SHARED_LIBS` の順に見て形式を決める。

```cmake
find_package(CLOpts 0.1.0 CONFIG REQUIRED COMPONENTS shared)
```

共有版は、複数のプログラムで 1 つの CLOpts を共有したいときや、利用側を再ビルドせずに不具合の修正版へ差し替えたいときに向く。ただし、1.0.0 より前は minor の版が上がるごとに ABI が変わる。Windows では、DLL を実行ファイルから見える場所（実行ファイルと同じディレクトリなど）に置く必要がある。ビルドの後に共有版の DLL を実行ファイルの横にコピーする記述、並べて install する手順、`COMPONENTS` がないときの順序の詳細は [取り込み手順](docs/integration.md#静的と共有の選び方) の章に、ABI の保証の範囲は [ABI の互換性](docs/abi.md) の章にある。

## 対応環境

- CMake 3.21 以上
- C99 のコンパイラ（公開ヘッダは C++ からも取り込める）
- CI では、次の 5 つの環境それぞれで、静的版と共有版の両方（計 10 通り）をビルドしてテストしている。
  - Linux の GCC
  - Linux の Clang
  - macOS の Apple Clang
  - Windows の MSVC
  - Windows の MinGW-w64

## 文書

詳しい説明は `docs/` の各章にある。章の一覧と読む順序は [docs/README.md](docs/README.md) にある。

- [API リファレンス](docs/api.md): 公開 API の型と関数、ハンドルの寿命、メモリの所有、エラーの種類と文面の出力
- [解析規則](docs/parsing.md): 引数の解釈の規則（`--`、`-` 単体、並べ替え、`--no-<name>`、カウント、値の優先順位など）と実行例
- [取り込み手順](docs/integration.md): 取り込み方 3 通り、CMake のオプション、静的版と共有版の選び方、MSVC の CRT
- [Windows での注意](docs/windows.md): UTF-8 の argv と環境変数、`wmain`、`activeCodePage`、DLL と PATH、コンソールのコードページ
- [ABI の互換性](docs/abi.md): 共有ライブラリの ABI の保証、版と SOVERSION、互換を保つ変更と壊す変更

サンプルプログラムは `examples/` にある。`examples/taskbook/taskbook.c` はサブコマンドありの、`examples/textcount/textcount.c` はサブコマンドなしのプログラムである。

## ライセンス

MIT License、Copyright (c) The CLOpts Authors。全文は [LICENSE](LICENSE) にある。
