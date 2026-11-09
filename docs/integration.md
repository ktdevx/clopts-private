# 取り込み手順

この章は、CLOpts を自分の CMake プロジェクトに取り込む 3 通りの手順（`add_subdirectory`、`FetchContent`、install してからの `find_package`）と、CLOpts の CMake オプション、静的版と共有版の選び方を示す。どの手順でも、利用側は `CLOpts::clopts` というターゲットにリンクするだけでよい。

必要なものは、CMake 3.21 以上と C99 のコンパイラである。利用側は C でも C++ でもよい（公開ヘッダは C++ から取り込める）。公開 API の使い方は [API リファレンス](api.md) の章で、共有ライブラリとしての互換性の方針は [ABI の互換性](abi.md) の章で示す。

## 取り込み方の選び方

| 手順 | CLOpts のソース | CLOpts のビルド | 向いている状況 |
|------|-----------------|-----------------|----------------|
| `add_subdirectory` | 利用側のリポジトリに置く（git submodule、コピーなど） | 利用側のビルドの一部 | ソースを手元で管理したいとき |
| `FetchContent` | 構成時に版のタグを指定して取得する | 利用側のビルドの一部 | ソースを置かずに版を固定したいとき |
| install → `find_package` | 利用側とは別に置く | 利用側とは別にビルドして install する | 複数のプロジェクトで 1 つの install を使うとき、静的版と共有版を並べて置きたいとき |

`add_subdirectory` と `FetchContent` では、CLOpts は利用側と同じビルドの中でビルドされ、利用側のコンパイラ、ビルド構成（Debug / Release）、MSVC の CRT の設定がそのまま使われる。install 経路では、CLOpts のビルドの設定と利用側の設定を合わせるのは利用者の役目になる（[MSVC の CRT](#msvc-の-crt) を参照）。

## add_subdirectory

CLOpts のソースを利用側のリポジトリの中（次の例では `external/clopts`）に置き、`add_subdirectory` で加える。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app C)

add_subdirectory(external/clopts)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
```

CLOpts のソースが利用側のソースツリーの外にあるときは、`add_subdirectory(<CLOpts のソース> clopts)` のように 2 つ目の引数でビルドディレクトリを指定する。

取り込まれた CLOpts（トップレベルでない CLOpts）は、利用側の設定を変えない。

- テスト、サンプル、install の規則、警告のエラー化は、既定で OFF になる（[CMake のオプション](#cmake-のオプション) を参照）。テストが OFF なので、CLOpts は `include(CTest)` を呼ばず、利用側に `BUILD_TESTING` や `enable_testing()` を持ち込まない。テスト用のフレームワーク（Naos）も取得しない。
- CLOpts の警告のフラグと内部用の定義は CLOpts 自身のターゲットだけに付き（`PRIVATE`）、`CLOpts::clopts` を通して利用側のターゲットに伝わらない。
- `CMAKE_*` の変数、`BUILD_SHARED_LIBS`、ディレクトリのプロパティは変更しない。

同じビルドに CLOpts が 2 回加えられたとき（2 つの依存先がそれぞれ CLOpts を取り込むときなど）は、最初に加えたものが使われ、2 回目の `add_subdirectory` は何もしない。

## FetchContent

`FetchContent` で、版のタグを指定して CLOpts を取得する。タグは版番号の前に `v` を付けた形（版 0.1.0 なら `v0.1.0`）である。

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

`FetchContent_MakeAvailable` は、取得したソースを `add_subdirectory` と同じ方法で加える。そのため、オプションの既定、親からの上書きの方法、利用側の設定を変えないことは、`add_subdirectory` と同じである。

ブランチ名（`main` など）ではなく版のタグを指定すること。ブランチを指定すると、構成し直すたびに違うソースが使われることがある。

## install と find_package

CLOpts を利用側とは別にビルドして、ある場所（prefix）に install し、利用側は `find_package` で探す。

まず CLOpts をビルドして install する（`/opt/clopts` は install 先の例）。

```sh
cmake -S clopts -B build-clopts -DCMAKE_BUILD_TYPE=Release -DCLOPTS_BUILD_TESTING=OFF
cmake --build build-clopts
cmake --install build-clopts --prefix /opt/clopts
```

CLOpts のソースを直接構成するとトップレベルの扱いになり、テストが既定で ON になる。テストを ON にすると、構成時にテスト用のフレームワーク（Naos）を取得するので、install するだけなら `-DCLOPTS_BUILD_TESTING=OFF` を付ける。Visual Studio のようなマルチ構成のジェネレータでは、`CMAKE_BUILD_TYPE` の代わりに `cmake --build` と `cmake --install` に `--config Release` を付ける。

install されるのは、ライブラリ、公開ヘッダ（`include/clopts/`）、package config（`<libdir>/cmake/CLOpts/`）である。サンプルやテストは install されない。

利用側では、`find_package` で版を指定して探す。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app C)

find_package(CLOpts 0.1.0 CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLOpts::clopts)
```

利用側を構成するときに、install 先を `CMAKE_PREFIX_PATH` で渡す。

```sh
cmake -S app -B build-app -DCMAKE_PREFIX_PATH=/opt/clopts
```

指定した版と install されている版の照合は、ABI の互換の単位に合わせている。0.x の間は同じ minor の中で指定した版以上のもの（`0.1.0` を指定すれば 0.1.x）、1.0.0 以降は同じ major の中で指定した版以上のものが見つかる。版の規則と互換の単位は [ABI の互換性](abi.md) の章を参照する。また、ポインタの大きさが違う構成（32 ビットと 64 ビット）でビルドした install は見つからない。

`find_package` が成功すると、次の変数がセットされる。

| 変数 | 値 |
|------|----|
| `CLOpts_LIBRARY_TYPE` | 選ばれた形式。`STATIC` または `SHARED` |
| `CLOpts_static_FOUND`、`CLOpts_shared_FOUND` | 選ばれた形式の方が `TRUE`、他方が `FALSE` |

## CMake のオプション

CLOpts は次のオプションを持つ。「トップレベル」は CLOpts のソースを直接構成したとき、「取り込まれたとき」は `add_subdirectory` や `FetchContent` で加えられたときを指す。

| オプション | 意味 | 既定（トップレベル） | 既定（取り込まれたとき） |
|------------|------|----------------------|--------------------------|
| `CLOPTS_BUILD_SHARED` | ON なら共有版、OFF なら静的版をビルドする | `BUILD_SHARED_LIBS` の値（未定義なら OFF） | `BUILD_SHARED_LIBS` の値（未定義なら OFF） |
| `CLOPTS_BUILD_TESTING` | CLOpts のテストをビルドし、`ctest` に登録する。ON のときは `include(CTest)` を呼び、`BUILD_TESTING` も ON（その既定）ならテスト用に Naos を取得する | ON | OFF |
| `CLOPTS_BUILD_EXAMPLES` | `examples/` のサンプル（taskbook、textcount）をビルドする。サンプルは install されない | ON | OFF |
| `CLOPTS_INSTALL` | install の規則と package config を作る | ON | OFF |
| `CLOPTS_WERROR` | CLOpts 自身のターゲットの警告をエラーにする。利用側のターゲットには影響しない | ON | OFF |

`CLOPTS_BUILD_SHARED` の既定は、最初に構成したときの `BUILD_SHARED_LIBS` の値で決まり、キャッシュに残る。あとから `BUILD_SHARED_LIBS` を変えても CLOpts の形式は変わらないので、形式を変えるときは `CLOPTS_BUILD_SHARED` を直接指定するか、ビルドディレクトリを作り直す。

### 親プロジェクトからの上書き

取り込む側（親プロジェクト）は、`add_subdirectory` や `FetchContent_MakeAvailable` より前に、通常の変数としてオプションをセットすれば上書きできる。

```cmake
set(CLOPTS_BUILD_SHARED ON)
add_subdirectory(external/clopts)
```

`FetchContent` でも同じである。

```cmake
set(CLOPTS_BUILD_SHARED ON)
FetchContent_MakeAvailable(clopts)
```

取り込まれたときの既定は OFF なので、通常はテストや install の規則を OFF にする記述はいらない。明示したいときは、同じように書く。

```cmake
set(CLOPTS_BUILD_TESTING OFF)
set(CLOPTS_INSTALL OFF)
set(CLOPTS_WERROR OFF)
add_subdirectory(external/clopts)
```

通常の変数でセットした値は、キャッシュの値より優先される。利用側を構成するコマンドラインで、キャッシュの値として渡してもよい（`cmake -S app -B build-app -DCLOPTS_BUILD_SHARED=ON` など）。

取り込まれた CLOpts の `CLOPTS_INSTALL` を ON にすると、利用側の install 先に CLOpts のライブラリ、公開ヘッダ、package config も install される。共有版を取り込んで利用側を install するときは、共有ライブラリも一緒に配る必要がある。

## 形式の選択（install 経路）

`add_subdirectory` と `FetchContent` では、形式は `CLOPTS_BUILD_SHARED` で決まる。install 経路では、どの形式を install するかと、`find_package` でどの形式を選ぶかの 2 つを決める。

### 静的版と共有版を並べて install する

静的版と共有版は、同じ prefix に並べて install できる。同じ版のソースを、`CLOPTS_BUILD_SHARED` だけを変えて 2 回ビルドし、同じ prefix に install する。

```sh
cmake -S clopts -B build-static -DCMAKE_BUILD_TYPE=Release -DCLOPTS_BUILD_TESTING=OFF -DCLOPTS_BUILD_SHARED=OFF
cmake --build build-static
cmake --install build-static --prefix /opt/clopts

cmake -S clopts -B build-shared -DCMAKE_BUILD_TYPE=Release -DCLOPTS_BUILD_TESTING=OFF -DCLOPTS_BUILD_SHARED=ON
cmake --build build-shared
cmake --install build-shared --prefix /opt/clopts
```

2 つの形式は、互いのファイルを上書きしない。package config の中では、形式ごとのターゲットのファイル（`CLOptsStaticTargets.cmake`、`CLOptsSharedTargets.cmake`）が別々に置かれ、`CLOptsConfig.cmake` と `CLOptsConfigVersion.cmake` は 2 つの形式で共通である。MSVC では、静的ライブラリは `clopts_static.lib`、共有版のインポートライブラリは `clopts.lib` という別の名前になる。

### COMPONENTS で形式を指定する

`find_package` の `COMPONENTS` に `static` または `shared` を書くと、その形式を選ぶ。

```cmake
find_package(CLOpts 0.1.0 CONFIG REQUIRED COMPONENTS static)
```

```cmake
find_package(CLOpts 0.1.0 CONFIG REQUIRED COMPONENTS shared)
```

### COMPONENTS がないときの既定の順序

`COMPONENTS` を書かないときは、次の順に形式を決める。

1. 変数 `CLOPTS_SHARED_LIBS` が定義されていれば、その値（ON なら共有版、OFF なら静的版）
2. そうでなければ、`BUILD_SHARED_LIBS` の値（ON なら共有版、未定義か OFF なら静的版）
3. 1 か 2 で決めた形式が install されていなければ、install されている方の形式

たとえば、両方を install した prefix では、何も指定しなければ静的版が、`BUILD_SHARED_LIBS` が ON の利用側では共有版が選ばれる。`BUILD_SHARED_LIBS` が ON でも、`CLOPTS_SHARED_LIBS` を OFF にすれば静的版が選ばれる。静的版だけを install した prefix では、`CLOPTS_SHARED_LIBS` を ON にしても静的版が選ばれる（3 による）。既定の順序で形式が見つからなくても失敗しないので、決まった形式が必要なときは `COMPONENTS` で指定する。

```cmake
set(CLOPTS_SHARED_LIBS ON)
find_package(CLOpts 0.1.0 CONFIG REQUIRED)
```

どの形式が選ばれたかは、`CLOpts_LIBRARY_TYPE`（`STATIC` または `SHARED`）で確かめられる。

### 形式を選べなかったとき

次のときは、`find_package` が CLOpts を見つからなかったものとして扱う（`CLOpts_FOUND` が FALSE になる）。理由は `CLOpts_NOT_FOUND_MESSAGE` にセットされ、CMake が「Reason given by package:」に続けて表示する。

| 状況 | 表示される理由 |
|------|----------------|
| `COMPONENTS` で指定した形式が install されていない | `CLOpts: the shared form (component 'shared') is not installed in <package config のディレクトリ>` |
| `static` と `shared` を両方指定した | `CLOpts: components 'static' and 'shared' cannot be used together; request only one of them` |
| `static`、`shared` 以外の必須のコンポーネントを指定した | `CLOpts: unknown component(s): <名前> (available components: static, shared)` |
| 同じビルドで、すでに別の形式の `CLOpts::clopts` が読み込まれている | `CLOpts: the shared form was requested, but CLOpts::clopts has already been loaded as the static form; only one form can be used in a build` |
| どちらの形式も install されていない | `CLOpts: neither the static nor the shared form is installed in <package config のディレクトリ>` |

`REQUIRED` を付けていれば、構成がこの理由を示すエラーで止まる。`REQUIRED` を付けていなければ警告が出て構成は続くので、`CLOpts_FOUND` を見て処理を分ける。

1 つのビルドで使える形式は 1 つだけである。2 回目の `find_package` は、1 回目と同じ形式なら成功し、違う形式を `COMPONENTS` で求めると上の表のとおり失敗する。`COMPONENTS` を書かない 2 回目の `find_package` は、すでに読み込まれている形式を使う。

## 静的と共有の選び方

既定は静的版である。迷ったら静的版を使う。

静的版が向いているのは、次のようなときである。

- 実行ファイル 1 つで配りたいとき。共有ライブラリの置き場所や、別の版の共有ライブラリとの取り違えを考えなくてよい。
- Windows で DLL を配りたくないとき。
- 静的版は位置独立コード（PIC）でビルドされるので、利用側の共有ライブラリにリンクしてもよい。

静的版は ABI の互換の対象外である。CLOpts の版を上げるときは、利用側を再ビルドする。

共有版が向いているのは、次のようなときである。

- 複数のプログラムで 1 つの CLOpts を共有したいとき。
- 利用側を再ビルドせずに、同じ SOVERSION の新しい版（不具合の修正など）に差し替えたいとき。

共有版を使うときは、次の点に注意する。

- 1.0.0 より前は、minor の版が上がるごとに ABI が変わる（SOVERSION が変わる）。0.1.x から 0.2.0 に上げるときは利用側を再ビルドする。何を保証するかは [ABI の互換性](abi.md) の章を参照する。
- Windows には rpath がないので、実行ファイルから見える場所（実行ファイルと同じディレクトリ、または PATH の通ったディレクトリ）に DLL を置く必要がある。install 経路では DLL は prefix の `bin/` に install される。
- Windows の DLL のファイル名には版が入らない。PATH 上に別の版の DLL があると、それが読み込まれることがある。DLL は実行ファイルの横に置くこと。詳しくは [ABI の互換性](abi.md#windows-の-dll-名) の章の「Windows の DLL 名」を参照する。

Windows で共有版を使うときは、ビルドした実行ファイルをビルドディレクトリからそのまま動かせるように、ビルドの後に DLL を実行ファイルの横にコピーする。次の記述は `add_subdirectory`、`FetchContent`、install のどの経路でも、`CLOpts::clopts` を定義した後に書けば使える。コピーするのは `CLOpts::clopts` が共有版のときだけである。静的版では `$<TARGET_RUNTIME_DLLS:app>` が空になり、コピーのコマンドが引数の不足で失敗するので、形式を確かめずに `if(WIN32)` だけで書かないこと。

```cmake
get_target_property(clopts_type CLOpts::clopts TYPE)
if(WIN32 AND clopts_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET app POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:app> $<TARGET_FILE_DIR:app>
        COMMAND_EXPAND_LISTS
    )
endif()
```

## MSVC の CRT

CLOpts は、MSVC の CRT（ランタイムライブラリ。`/MT`、`/MD` など）の種別を自分では決めない。`CMAKE_MSVC_RUNTIME_LIBRARY` も、ターゲットの `MSVC_RUNTIME_LIBRARY` プロパティもセットしない。

`add_subdirectory` と `FetchContent` では、CLOpts は利用側の設定を引き継ぐ。CRT の種別を変えるときは、利用側で `CMAKE_MSVC_RUNTIME_LIBRARY` を、CLOpts を加えるより前にセットする。そうすれば、CLOpts と利用側が同じ CRT でビルドされる。

```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_subdirectory(external/clopts)
```

install 経路では、CLOpts は CLOpts をビルドしたときの CRT でビルドされている。利用側と CRT の種別が違う静的版をリンクすると、リンクに失敗するか、誤った動作をすることがある。CLOpts をビルドするときに、利用側と同じ `CMAKE_MSVC_RUNTIME_LIBRARY` の値を渡すこと。

```sh
cmake -S clopts -B build-clopts -DCLOPTS_BUILD_TESTING=OFF "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>"
```

静的版を install すると、package config に、ビルドに使った `CMAKE_MSVC_RUNTIME_LIBRARY` の値が記録される。`find_package` で静的版を選んだとき、MSVC の利用側の `CMAKE_MSVC_RUNTIME_LIBRARY` が記録した値と違えば、次の警告が出る（構成は止まらない）。

```text
CLOpts was built with MSVC runtime library '<記録した値>', but this project uses '<利用側の値>'. Mixing runtime libraries may fail to link or misbehave.
```

実際の表示では、CMake がこの文を適当な位置で折り返す。

この確認は簡易なものである。

- 2 つの値は文字列として比べる。ジェネレータ式は評価しないので、意味が同じでも書き方が違えば警告が出る。
- どちらかの値が空（CMake の既定の CRT を使う）なら確かめない。たとえば、どちらも既定のまま、Release でビルドした静的版を Debug の利用側からリンクすると、この警告なしに Release 用と Debug 用の CRT が混ざる。利用側と同じビルド構成の CLOpts を install すること。
- 共有版については確かめない。

## CI で検査している経路

CI では、5 つの環境（Linux の GCC と Clang、macOS の Apple Clang、Windows の MSVC と MinGW-w64）それぞれで、静的版と共有版の 2 通り、計 10 通りの構成で CLOpts をビルドし、次の統合テストを実行している。統合テストでは、利用側のプロジェクト（`tests/integration/fixtures/` の下）を CLOpts とは別に構成し、ビルドして実行する。

| 経路 | テスト | 確かめていること |
|------|--------|------------------|
| `add_subdirectory` | `integration.subdirectory` | その構成の形式の CLOpts を `add_subdirectory` で加え、C と C++ の利用側がビルドでき、実行できること。CLOpts の警告のフラグと内部用の定義が利用側に伝わらないこと |
| install → `find_package` | `integration.install_find_package` | その構成の CLOpts を install し、`find_package(CLOpts <版> CONFIG REQUIRED)` で見つけ、`CLOPTS_SHARED_LIBS` で求めた形式が選ばれ、C と C++ の利用側がビルドでき、実行できること |
| 形式の選択 | `integration.package_selection` | 静的版と共有版を同じ prefix に並べて install でき、`COMPONENTS`、`CLOPTS_SHARED_LIBS`、`BUILD_SHARED_LIBS`、既定、install されている方への切り替えで正しい形式が選ばれること。`static` と `shared` の同時指定と、install されていない形式の指定が、理由を示して失敗すること |
| 内部関数を隠すこと | `integration.internal_hidden` | 共有版の構成だけ。利用側が CLOpts の内部関数を呼ぶとリンクに失敗すること |

`FetchContent` の経路は、CI で直接は検査していない。`FetchContent_MakeAvailable` は取得したソースを `add_subdirectory` と同じ方法で加えるので、取り込んだ後の扱いは `add_subdirectory` の経路の検査で確かめられている。

この章の記述例のターゲット名（`CLOpts::clopts`）、`find_package` の呼び方、形式の選び方は、これらのテストの利用側のプロジェクトと同じ形である。
