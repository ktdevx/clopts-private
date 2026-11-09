# ABI の検査（保守者向け）

この文書は、CLOpts の ABI（バイナリ互換性）を守るための検査について、公開関数を足す・消すときの手順、各検査が何を検出しどの構成で動くか、失敗したときの読み方、手元での abidiff の実行方法を示す。何を保証するか（保証の範囲、SOVERSION の規則、互換を保つ変更と壊す変更）は利用者向けの章 `docs/abi.md` を参照する。

検査の実体は次のファイルにある。

| ファイル | 役割 |
|----------|------|
| `abi/clopts.symbols` | 公開シンボル一覧。共有ライブラリが export すべきシンボルの、OS によらない唯一の基準 |
| `tests/abi/check_symbols.cmake` | 一覧の形式の検査と、export されたシンボルとの照合 |
| `tests/abi/check_header_rules.cmake` | 公開ヘッダの静的検査（`CLOPTS_API` の付け漏れ） |
| `tests/abi/fixtures/headers/` | 公開ヘッダを C99 / C++11 でコンパイルし、C++ から全シンボルにリンクする子プロジェクト |
| `tests/abi/run_abidiff.cmake` | 直近のリリースタグとの abidiff による比較（CI の `abi-compare` ジョブ） |
| `tests/abi/CMakeLists.txt` | 上の検査を CTest のテスト（ラベル `abi`）として登録する |

## 公開関数を足す・消すときの手順

公開関数を足す・消す変更では、同じ変更（同じコミット）の中で公開シンボル一覧 `abi/clopts.symbols` を更新する。一覧と export がずれた状態のコミットを作らない。

### 足すとき

1. 公開ヘッダ（`include/clopts/*.h`）の宣言に `CLOPTS_API` を付ける。

   ```c
   CLOPTS_API int clopts_positional_abi_probe(const clopts_positional_t *positional);
   ```

   `CLOPTS_API` のない宣言は `abi.header_rules` が違反として報告する。宣言は `extern "C"` の中（各公開ヘッダの `#ifdef __cplusplus` で囲まれた範囲）に置く。
2. 定義を `src/<module>/` に書く。
3. `abi/clopts.symbols` の、宣言したヘッダの組（`# clopts/<header>.h` の注釈の下）に、ヘッダの中の宣言順に合わせた位置で名前を足す。
4. 互換を保つ変更なので、SOVERSION は変えない（`docs/abi.md` の「互換を保つ変更」）。

### 消すとき

1. 公開ヘッダの宣言と、`src/` の定義を消す。
2. `abi/clopts.symbols` から名前の行を消す。
3. 互換を壊す変更なので、major（0.x の間は minor）を上げる（`docs/abi.md` の「互換を壊すときの手順」）。1.0.0 以降で版を上げずに消すと、CI の `abi-compare` ジョブが `incompatible` で失敗する。

内部の関数（モジュール間で共有する `clopts_<module>_` の関数）には `CLOPTS_API` を付けない。付けると共有ライブラリから export され、`abi.exports` が余分なシンボルとして報告する。

### 公開シンボル一覧の形式

- UTF-8、LF。1 行に 1 つのシンボル名を書く。行頭と行末の空白は無視する。
- 空行と、`#` で始まる行（先頭の空白の後でもよい）は無視する（注釈）。
- 名前は `^clopts_[a-z0-9_]+$` に合うこと。重複してはいけない。
- 名前は OS によらず同じに書く（macOS の先頭の `_` は付けない）。
- 載せるのは `CLOPTS_API` の付いた関数だけ。公開ヘッダの `static inline` の関数（`clopts_file_write`）は export されないので載せない。
- 特定の OS でだけ宣言・export される関数は、名前の後に空白で区切って OS の注記を 1 つ書く。受け付ける注記は `windows` だけである。現在は `include/clopts/platform.h` の `#ifdef _WIN32` の中で宣言される `clopts_argv_from_wide` が該当する。

  ```text
  # clopts/platform.h
  clopts_argv_from_wide windows
  ```

  注記 `windows` の付いた名前は、Windows の照合（`FORMAT` が `dumpbin` / `objdump-pe`）でだけ export を要求し、Linux と macOS（`nm-elf` / `nm-macho`）では一覧に無いものとして扱う。C++ のリンク検査（`abi.headers_cxx`）では、その名前の参照を `#ifdef _WIN32` で囲んで生成する。`windows` 以外の語や、3 語以上の行は形式の誤りになる。Windows 専用の公開関数を足すときは、この注記を付ける。
- 並び順は自由だが、公開ヘッダごとに `# clopts/<header>.h` の注釈でまとめ、ヘッダの中の宣言順に並べることを推奨する（並び順は検査しない）。

```text
# clopts/program.h
clopts_program_new
clopts_program_free
...

# clopts/platform.h
clopts_argv_from_wide windows
```

## 検査の一覧

### CTest のテスト（ラベル `abi`）

`ctest -L abi` で実行できる。既存の CI の `build-and-test` ジョブは `ctest` で全テストを流すので、5 環境（Linux の GCC と Clang、macOS の Apple Clang、Windows の MSVC と MinGW-w64）× 2 形式（静的・共有）でそのまま動く。

| テスト名 | 検出するもの | 登録の条件 | OS | 有効になる版 |
|----------|--------------|------------|----|--------------|
| `abi.symbol_list` | `abi/clopts.symbols` の形式の誤り（重複、規則に合わない名前、未知の OS の注記） | 常に（静的・共有の両方） | 全 OS | 版によらない |
| `abi.exports` | 共有ライブラリが export するシンボルと一覧の過不足（不足・余分）。ツールが見つからないか失敗したこと | 共有ビルドだけ（`CLOPTS_BUILD_SHARED=ON`） | 全 OS | 版によらない |
| `abi.symbols_selftest.nm-elf` | `check_symbols.cmake` の `nm-elf` の取り出しと除外の誤り（記録したツールの出力で） | 常に | 全 OS | 版によらない |
| `abi.symbols_selftest.nm-macho` | 同じく `nm-macho` の取り出しの誤り | 常に | 全 OS | 版によらない |
| `abi.symbols_selftest.dumpbin` | 同じく `dumpbin` の取り出しの誤り | 常に | 全 OS | 版によらない |
| `abi.symbols_selftest.objdump-pe` | 同じく `objdump-pe` の取り出しの誤り | 常に | 全 OS | 版によらない |
| `abi.symbols_selftest.mismatch` | 不足と余分の報告（名前、一覧の場所、案内）が出ないこと | 常に | 全 OS | 版によらない |
| `abi.symbols_selftest.bad_list` | 一覧の形式の誤りの報告（行と理由）が出ないこと | 常に | 全 OS | 版によらない |
| `abi.header_rules` | `include/clopts/` の公開ヘッダにある、`CLOPTS_API` のない関数の宣言 | 常に | 全 OS | 版によらない |
| `abi.header_rules_selftest.good` | 違反のないヘッダを違反と誤って報告すること | 常に | 全 OS | 版によらない |
| `abi.header_rules_selftest.missing_api` | `CLOPTS_API` のない関数を見逃すこと | 常に | 全 OS | 版によらない |
| `abi.header_rules_selftest.missing_api_multiline` | 複数行にまたがる宣言で `CLOPTS_API` の欠落を見逃すこと | 常に | 全 OS | 版によらない |
| `abi.headers_c` | 公開ヘッダを単独で include した翻訳単位が、C99 と CLOpts と同じ警告の設定（`CLOPTS_WERROR=ON`）でコンパイルできないこと | 常に | 全 OS | 版によらない |
| `abi.headers_cxx` | 公開ヘッダを単独で include した翻訳単位が C++11 でコンパイルできないこと。一覧の全シンボルを参照する `abi_link_check` が `CLOpts::clopts` にリンクできないこと（`extern "C"` の欠落、一覧にあってヘッダにない名前） | 常に。C++ のコンパイラが見つからなければ `DISABLED`（`Not Run (Disabled)` と表示され、失敗にはならない） | 全 OS | 版によらない |
| `abi.compare_selftest` | `run_abidiff.cmake` の基準のタグの選択と、abidiff の終了コードの判定の誤り | 常に | 全 OS | 版によらない |

- 静的ビルドでは `abi.exports` だけが登録されない。他のテストは両形式で同じものが登録される。
- `abi.exports` が使うツールは構成から決まる: MSVC は `dumpbin`（なければ `link.exe /dump`）、MinGW は `CMAKE_OBJDUMP`（`objdump -p`）、macOS は `CMAKE_NM`（`nm -gU`）、それ以外は `CMAKE_NM`（`nm -D --defined-only`）。ツールが見つからなくてもテストは登録され、実行時に失敗として報告される。
- `abi.headers_c` と `abi.headers_cxx` は、このビルドツリーと同じ生成器・C コンパイラ・構成・形式（静的か共有か）で子プロジェクトを構成・ビルドする。共有のツリーでは、C++ のリンク検査は共有版の CLOpts にリンクする。
- C++ のコンパイラは CLOpts 本体には要らない。`check_language(CXX)` で探すだけで、ルートの `project()` に `CXX` は足していない。

### CI のジョブ `abi-compare`

CTest のテストではなく、`.github/workflows/ci.yml` の独立したジョブである（`build-and-test` に依存せず、依存もされない）。

| ジョブ | 検出するもの | 動く条件 | OS | 有効になる版 |
|--------|--------------|----------|----|--------------|
| `abi-compare` | 直近のリリースとの間の、互換のない ABI の変更（公開関数の削除、シグネチャや型の配置の変化など、シンボル名の照合では分からないもの） | main への push と pull request | Linux（`ubuntu-latest`、gcc、`abigail-tools` の abidiff） | 1.0.0 以降で、現在の版と同じ SOVERSION（同じ major）の、現在の版以下のリリースタグ `vX.Y.Z` があるとき。それ以外は理由を示して成功する |

ジョブの出力の最初の行が状態を示す。

| 最初の行 | 意味 | ジョブ |
|----------|------|--------|
| `abi-compare: skipped: version <v> is before 1.0.0` | 1.0.0 より前なので比較しない | 成功 |
| `abi-compare: skipped: no release tag with SOVERSION <major>` | 基準にできるタグがない | 成功 |
| `abi-compare: compatible: ...` | abidiff の終了コードが 0 または 4（変更なし、または互換を保つ変更だけ） | 成功 |
| `abi-compare: incompatible: ...` | abidiff の終了コードにビット 8（互換のない変更）が立った | 失敗 |
| `abi-compare: tool error: ...` | ビルド、git、abidiff のいずれかが失敗した（abidiff の終了コードのビット 1 か 2、未知のビットを含む） | 失敗 |

基準は、`^v([0-9]+)\.([0-9]+)\.([0-9]+)$` に合うタグ（`-rc` などは除く）のうち、現在の major と同じで現在の版以下の最大の版（数値で比べるので `v1.10.0` は `v1.9.0` より新しい）。基準と現在のソースを同じ設定（`CLOPTS_BUILD_SHARED=ON`、`CMAKE_BUILD_TYPE=Debug`、同じ C コンパイラと生成器）でその場でビルドし、両方の公開ヘッダを `--headers-dir1` / `--headers-dir2` で渡して abidiff を実行する。基準のデータはリポジトリに置かない。

## 失敗したときの読み方

### `abi.exports`: 不足と余分

報告の形は次のとおりである（名前は自己テスト `abi.symbols_selftest.mismatch` のもの。`<build>` と `<repo>` は実際には絶対パスで出る）。

```text
 abi symbol check failed (nm-elf, <build>/libclopts.so.0.1.0)
   missing (in the list but not exported):
     clopts_result_take
   extra (exported but not in the list):
     clopts_write_help
 The public symbol list is <repo>/abi/clopts.symbols.
 If the change is intended, update the list; otherwise stop exporting the symbol
 (internal functions must not carry CLOPTS_API). See docs/dev/abi-checks.md.
```

- `missing`（一覧にあるが export されていない）: 一覧に足したが、宣言に `CLOPTS_API` を付け忘れたか、定義がない。あるいは関数を消したのに一覧から消し忘れた。宣言と定義を確かめ、消したのなら一覧から消す。
- `extra`（export されているが一覧にない）: 公開関数を足したのに一覧に足し忘れたか、内部の関数に `CLOPTS_API` を付けてしまった。公開する意図なら一覧に足し、そうでなければ `CLOPTS_API` を外す。
- 不足と余分は、それぞれ名前順にすべて出る。

一覧の形式の誤り（`abi.symbol_list` と、一覧を読むすべての照合）は、`<一覧のパス>:<行>: <理由>` の形で、すべての誤りがまとめて出る。次は自己テスト `abi.symbols_selftest.bad_list` の fixture を対象にした実際の出力である（`abi.symbol_list` では対象が `<repo>/abi/clopts.symbols` になる）。

```text
 check_symbols: 5 error(s) in the public symbol list <repo>/tests/abi/fixtures/exports/bad_list.symbols:
   <repo>/tests/abi/fixtures/exports/bad_list.symbols:9: duplicate symbol 'clopts_program_new'
   <repo>/tests/abi/fixtures/exports/bad_list.symbols:10: invalid symbol name 'parse_args'
   <repo>/tests/abi/fixtures/exports/bad_list.symbols:11: invalid symbol name 'Clopts_Parse'
   <repo>/tests/abi/fixtures/exports/bad_list.symbols:12: invalid platform tag 'linux'
   <repo>/tests/abi/fixtures/exports/bad_list.symbols:13: invalid platform tag 'windows extra'
 Names must match ^clopts_[a-z0-9_]+$, appear once, and carry at most the
 platform tag 'windows'. See docs/dev/abi-checks.md.
```

理由は `duplicate symbol`（重複）、`invalid symbol name`（`^clopts_[a-z0-9_]+$` に合わない）、`invalid platform tag`（`windows` 以外の注記か、3 語以上の行）のどれかである。

検査の前提が満たされないときは、違反とは別の文言で失敗する。

- `export dump tool not found for FORMAT <FORMAT>`: export を読むツールが見つからない。構成時の `CMAKE_NM` / `CMAKE_OBJDUMP`（MSVC では `dumpbin` か `link.exe`）を確かめる。
- ツールの終了コードと出力が添えられた失敗: ツールが実行に失敗した。
- `no symbol names found in the <FORMAT> output ...`: ツールの出力から名前が 1 つも取れない。ツールの出力の形式が想定と違う（`dumpbin` は英語の出力の `ordinal hint RVA name` の表と `Summary` に依存する。`objdump-pe` は `[Ordinal/Name Pointer] Table` の節の `[` で始まる行に依存する）。報告にはツールの出力の先頭 80 行が付くので、それを見て `check_symbols.cmake` の取り出し規則を直し、CI のログの出力で `tests/abi/fixtures/exports/` の記録を置き換える。

### 除外リストに足すときの判断基準

`check_symbols.cmake` の先頭に、形式ごとの除外リスト（ツールチェーンが自動で加える、CLOpts のソースに由来しないシンボル）がある。現在の項目は `nm-elf` の `_init`、`_fini`、`_edata`、`_end`、`__bss_start` だけで、他の形式は空である。除外リストの名前は「あれば取り除く」もので、無くても失敗しない。

CI で見慣れない名前が `extra` に出たら、次の順に判断する。

1. 名前が `clopts_` で始まる、または CLOpts のソースに定義がある: CLOpts 由来である。除外リストには足さず、一覧に足すか export をやめる。
2. 名前がリンカやランタイム（`crti.o`、リンカスクリプト、コンパイラのランタイムなど）によって加えられ、CLOpts のソースに由来しない: その形式の除外リストに、理由の注釈を付けて足す。
3. どちらか分からない: 足さない。ツールの出力全体を見て、どこから来たかを確かめてから決める。

除外リストに足した名前は照合の対象から外れるので、CLOpts 由来の名前を足すと、その名前の出し漏れや漏れを検出できなくなる。

### `abi.header_rules`: ヘッダの違反

報告の形は次のとおりである（自己テスト `abi.header_rules_selftest.missing_api` の fixture を対象にした実際の出力。`abi.header_rules` では対象が `<repo>/include/clopts` になる）。

```text
 check_header_rules: 1 violation(s) in the public headers under <repo>/tests/abi/fixtures/header_rules/bad/missing_api (CLOPTS_API on every function; see requirement 4):
 <repo>/tests/abi/fixtures/header_rules/bad/missing_api/missing_api.h:10: function 'bad_unmarked' lacks CLOPTS_API
```

- `<file>:<line>: function '<name>' lacks CLOPTS_API`: その行から始まる関数の宣言に `CLOPTS_API` がない。公開関数なら付け、公開しないなら公開ヘッダから内部ヘッダ（`src/<module>/<module>_internal.h`）へ移す。
- 行は文の開始行である。違反はすべてまとめて出る。

### `abi.headers_c` / `abi.headers_cxx`: コンパイルとリンクの失敗

失敗すると、`tests/cmake/run_fixture.cmake` が子プロジェクトの出力全体を表示する。

- コンパイルの失敗は、ビルドの出力の `check_<header>.c`（C99）か `check_<header>.cpp`（C++11）の名前で、どのヘッダがどちらの言語で通らないかが分かる。ヘッダは単独で include されるので、そのヘッダが必要な別の公開ヘッダを自分で include していないことも、ここで分かる。
- 警告もエラーになる（C は CLOpts と同じ設定で `CLOPTS_WERROR=ON`、C++ は GCC / Clang で `-Wall -Wextra -Wpedantic -Werror`、MSVC で `/W4 /WX`）。
- `extern "C"` の欠落は、コンパイルではなく `abi_link_check` のリンクで、C++ の修飾された名前（引数の型付きの名前）の未定義参照として出る。たとえば `include/clopts/help.h` の `extern "C" {` と `}` を外すと、Linux の GCC では次のようになる。

  ```text
  [100%] Linking CXX executable abi_link_check
  /usr/bin/ld: CMakeFiles/abi_link_check.dir/link_check.cpp.o:(.data.rel.ro+0xc0): undefined reference to `clopts_write_help(clopts_program const*, clopts_command const*, int (*)(void*, char const*, unsigned long), void*)'
  /usr/bin/ld: CMakeFiles/abi_link_check.dir/link_check.cpp.o:(.data.rel.ro+0xc8): undefined reference to `clopts_write_usage(clopts_program const*, clopts_command const*, int (*)(void*, char const*, unsigned long), void*)'
  ...
  ```

  未定義の名前が引数の型を伴っていれば、そのヘッダの `extern "C"` の範囲を確かめる。
- 一覧にあって公開ヘッダに宣言のない名前は、`link_check.cpp` のコンパイルで未宣言の識別子として失敗する。一覧から消し忘れていないかを確かめる。

### `abi-compare`: abidiff の報告

`compatible` と `incompatible` では、abidiff の出力が `---- abidiff <tag> (...) -> current <version> (...): exit status <code> ----` と `---- end of abidiff output ----` の間に常に表示される。

- `Removed function`（`[D]` の行）: 公開関数が消えた。互換を壊す。
- `Changed function`: シグネチャや、引数・戻り値から届く型の配置が変わった。内容によっては互換を壊す（abidiff がビット 8 を立てれば `incompatible`）。
- `Added function`（`[A]` の行）: 公開関数が増えた。互換を保つので、ビット 4 だけが立ち `compatible` になる。

`incompatible` のときは、変更を取り消すか、互換を壊す変更として major を上げる（`docs/abi.md`）。`tool error` のときは、表示された子のビルドの出力か abidiff の出力で原因を確かめる。

#### 注意: 古いタグのビルドと `CLOPTS_WERROR`

比較では、基準のタグと現在のソースの両方を最上位のプロジェクトとしてビルドするので、`CLOPTS_WERROR` が ON になる。CI のランナーの gcc が新しくなり、古いタグのソースが新しい警告を出すと、基準のビルドが失敗して `abi-compare: tool error` になる。これは ABI の非互換ではない。出力の基準のビルドのログで警告を確かめ、比較スクリプトでの基準のビルドの設定を見直す（現在のスクリプトには `CLOPTS_WERROR` を切る仕組みはない）。

## 自己テストについて

失敗を期待する自己テスト（`abi.symbols_selftest.mismatch`、`abi.symbols_selftest.bad_list`、`abi.header_rules_selftest.<case>`）は、CTest の `PASS_REGULAR_EXPRESSION` で報告の全体（件数、名前、行、案内の文）を照合し、終了コードは見ない。そのため、`check_symbols.cmake` や `check_header_rules.cmake` の報告の文言、または `tests/abi/fixtures/` の内容を変えたら、`tests/abi/CMakeLists.txt` の正規表現も合わせて直す。

## ヘッダの走査器が扱わない構文

`check_header_rules.cmake` は CMake の文字列処理だけでヘッダを走査するので、次の構文は扱わない（スクリプトの先頭の注釈と同じ）。公開ヘッダにこれらが現れたら、走査器を広げる。

- 関数ポインタを返す関数（例: `int (*f(void))(int);`。戻り値の型が名前として取られる）と、関数ポインタ型の `extern` 変数（`CLOPTS_API` のない関数として報告される）。
- `extern` 変数。まったく検査しない。
- マクロで包んだ宣言（例: `DECLARE_FN(name);`。`DECLARE_FN` という名前の関数として扱われる）。
- 文字列リテラルや文字リテラルの中の `{`、`}`、`;`、`/*`、`//`（文字列リテラルを持つのは `extern "C"` だけという前提）。
- `//` コメントを継続させる、またはトークンを分割する行末の `\`。トライグラフとダイグラフ。
- `#if` / `#else` の枝をまたいでだけ釣り合う波括弧。

扱う範囲は次のとおりである。`/* */` と `//` のコメントと前処理指令の行を取り除き（行番号は保つ）、`extern "C" {` と対応する `}` を透過的に扱いながら最上位の文に区切る。`typedef` で始まる文（不透明型の前方宣言、構造体・共用体・列挙の定義、関数ポインタ型）は許し、`static` で始まる文（`static inline` の関数定義）は `CLOPTS_API` の検査から外す。

## 手元での abidiff の実行

### 必要なもの

Linux で、`abidiff`（Ubuntu / Debian では `sudo apt-get install -y abigail-tools`）、`git`、C コンパイラ、CMake 3.21 以上。

### 実行のしかた

CI の `abi-compare` ジョブと同じ引数で、比較スクリプトを `cmake -P` で実行する。

```sh
cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<work> -DC_COMPILER=gcc -DGENERATOR="Unix Makefiles" \
      -P <repo>/tests/abi/run_abidiff.cmake
echo "exit=$?"
```

- `SOURCE_DIR`（必須）: 比較するリポジトリ。タグは `git -C <SOURCE_DIR> tag --list "v*"` で探すので、タグを取得しておく（`git fetch --tags`）。
- `WORK_DIR`（必須）: 作業ディレクトリ。実行のたびに消して作り直す。`SOURCE_DIR` と同じ場所や、`SOURCE_DIR` を含む場所（その親など）は指定できない（`must not contain or be SOURCE_DIR` の `tool error` で失敗する）。
- `C_COMPILER`（省略可、既定は環境の `CC`）と `GENERATOR`（省略可）。
- 現在の側は `SOURCE_DIR` の作業ツリーをそのまま使う（コミットしていない変更も入る）。基準の側は、タグから `git archive` で `<WORK_DIR>/baseline-src` に取り出す。リポジトリの作業ツリーとタグは変えない。

1.0.0 より前の版では、比較せずに次の行を出して成功する。

```text
abi-compare: skipped: version 0.1.0 is before 1.0.0
```

スクリプトは、基準と現在の側をそれぞれ `<WORK_DIR>/{baseline,current}-prefix` に install し、次の形で abidiff を呼ぶ。手元で abidiff だけをやり直すときにも使える。

```sh
abidiff --headers-dir1 <work>/baseline-prefix/include \
        --headers-dir2 <work>/current-prefix/include \
        --drop-private-types \
        <work>/baseline-prefix/lib/libclopts.so.1.0.0 <work>/current-prefix/lib/libclopts.so.1.0.0
```

比較の規則の自己テスト（基準の選択と終了コードの判定）だけなら、git もビルドも abidiff も使わずに実行できる（`abi.compare_selftest` と同じ）。

```sh
cmake -DMODE=self_test -P tests/abi/run_abidiff.cmake
```

### 1.0.0 前に比較の経路を確かめる方法

1.0.0 より前は、CI の `abi-compare` ジョブは常に `skipped` になり、比較の経路（基準のビルド、abidiff の実行、終了コードの判定）が CI で通らない。比較スクリプトを変えたときは、手元の Linux で、一時的な clone に仮の `v1.0.0` を付けて確かめる。本物のリポジトリにはタグを付けず、変更もしない。

以下は、2026-11-08 に abidiff 2.4.0、gcc 13.3.0、CMake 3.28.3、`Unix Makefiles` で実際に行った手順と結果である（出力の中のパスは実際には絶対パスで出る。`<clone>`、`<work>` に置き換えた）。

#### 準備: 一時的な clone と仮のタグ

`<clone>` は clone の場所、`<work>` は比較の作業ディレクトリ（`<clone>` の外）。

```sh
git clone <repo> <clone>
cd <clone>
sed -i 's/^project(clopts VERSION 0.1.0 /project(clopts VERSION 1.0.0 /' CMakeLists.txt
git commit -am "tmp: version 1.0.0"
git tag v1.0.0
```

各ケースの変更はコミットせず、作業ツリーのまま比較する（基準はタグから取り出すので、作業ツリーの変更は現在の側にだけ入る）。ケースの間は `git checkout -- .` で作業ツリーを戻す。比較のコマンドは上の「実行のしかた」と同じ（`SOURCE_DIR=<clone>`）。

#### 結果

| ケース | 変更 | 最初の行（状態） | abidiff の終了コード | スクリプトの終了コード |
|--------|------|------------------|----------------------|------------------------|
| (a) | なし | `compatible` | 0 | 0 |
| (b) | 公開関数を 1 つ追加 | `compatible` | 4（ABI の変更あり） | 0 |
| (c) | 公開関数を 1 つ削除 | `incompatible` | 12（4 + 8: 互換のない変更） | 1 |

(a) 変更なし（`v1.0.0` のコミットそのもの）:

```text
abi-compare: compatible: no incompatible ABI change from v1.0.0 to 1.0.0 (abidiff exit status 0)
---- abidiff v1.0.0 (<work>/baseline-prefix/lib/libclopts.so.1.0.0) -> current 1.0.0 (<work>/current-prefix/lib/libclopts.so.1.0.0): exit status 0 ----
---- end of abidiff output ----
```

(b) 公開関数を 1 つ追加。手順どおり 3 つのファイルを同じ変更で更新した。

- `include/clopts/positional.h`: `clopts_positional_set_help` の宣言の後に `CLOPTS_API int clopts_positional_abi_probe(const clopts_positional_t *positional);` を追加
- `src/parser/parser_positional.c`: 定義（`return positional != NULL;`）を追加
- `abi/clopts.symbols`: `# clopts/positional.h` の組の `clopts_positional_set_help` の次の行に `clopts_positional_abi_probe` を追加

```text
abi-compare: compatible: no incompatible ABI change from v1.0.0 to 1.0.0 (abidiff exit status 4)
---- abidiff v1.0.0 (<work>/baseline-prefix/lib/libclopts.so.1.0.0) -> current 1.0.0 (<work>/current-prefix/lib/libclopts.so.1.0.0): exit status 4 ----
Functions changes summary: 0 Removed, 0 Changed, 1 Added function
Variables changes summary: 0 Removed, 0 Changed, 0 Added variable

1 Added function:

  [A] 'function int clopts_positional_abi_probe(const clopts_positional_t*)'    {clopts_positional_abi_probe}

---- end of abidiff output ----
```

abidiff は追加を ABI の変更（ビット 4）として報告するが、互換のない変更（ビット 8）ではないので `compatible` と判定され、報告もジョブの出力に残る。

(c) 公開関数を 1 つ削除。(b) を戻してから、`include/clopts/positional.h` の `clopts_positional_set_help` の宣言、`src/parser/parser_positional.c` の定義、`abi/clopts.symbols` の行を消した（比較のビルドは `CLOPTS_BUILD_TESTING=OFF` なので、この関数を呼ぶテストはビルドされない）。

```text
abi-compare: incompatible: incompatible ABI change from v1.0.0 to 1.0.0 (abidiff exit status 12)
---- abidiff v1.0.0 (<work>/baseline-prefix/lib/libclopts.so.1.0.0) -> current 1.0.0 (<work>/current-prefix/lib/libclopts.so.1.0.0): exit status 12 ----
Functions changes summary: 1 Removed, 0 Changed, 0 Added function
Variables changes summary: 0 Removed, 0 Changed, 0 Added variable

1 Removed function:

  [D] 'function void clopts_positional_set_help(clopts_positional_t*, const char*)'    {clopts_positional_set_help}

---- end of abidiff output ----
```

この後に `CMake Error` として同じ状態行がもう一度出て、スクリプトは 0 以外で終わる。報告は削除された関数を名前とシグネチャで示す。

#### 後片付け

```sh
rm -rf <clone> <work>
git -C <repo> tag      # 仮のタグが本物のリポジトリにないこと
git -C <repo> status   # 変更がないこと
```
