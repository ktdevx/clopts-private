# Implementation Plan

- [ ] 1. 基盤: 公開シンボル一覧と検査ディレクトリの骨格
- [x] 1.1 公開シンボル一覧を作り、`tests/abi/` をテストツリーに登録する
  - リポジトリ直下の `abi/clopts.symbols` を作り、現在の公開ヘッダにある `CLOPTS_API` 付きの全関数を、ヘッダごとの `# clopts/<header>.h` の注釈でまとめ、ヘッダの宣言順に 1 行 1 名で並べる。`static inline` の `clopts_file_write` は載せない。ファイルの先頭に、形式（1 行 1 名、空行と `#` の注釈を無視）と更新の手順の要約を注釈で書く
  - `tests/abi/CMakeLists.txt` を、ラベル `abi` のテストをここで登録する旨の注釈だけを持つ骨格として作り、`tests/CMakeLists.txt` の末尾に `add_subdirectory(abi)` を足す。この時点ではテストを登録しない
  - 完了条件: 一覧の名前の集合が、`include/clopts/*.h` の `CLOPTS_API` 付きの宣言から抜き出した名前の集合と一致する（grep などで突き合わせた結果を示す）。静的ビルドと共有ビルドの構成・ビルドが `CLOPTS_WERROR=ON` で通り、既存の `ctest` がすべて通る
  - _Requirements: 2.1, 2.2, 2.4_

- [ ] 2. 検査スクリプトと子プロジェクト
- [x] 2.1 (P) シンボル照合スクリプトの一覧の検査と集合の比較・報告を、`nm-elf` の形式で実装する
  - 一覧を読み、空行と注釈の行を無視し、重複と規則（`clopts_` で始まる小文字・数字・`_`）に合わない名前と未知の OS の注記（設計の SymbolList の「OS の注記」）を、一覧のパスと行番号付きですべて集めてから失敗する。`LIBRARY` も `DUMP_FILE` もなければ、ここで成功して終わる
  - `DUMP_FILE` を読み、`nm-elf` の規則で名前を取り出し、除外リスト（`_init`、`_fini`、`_edata`、`_end`、`__bss_start`。理由の注釈付き）を取り除く。取り出した名前が 0 個なら、出力の形式が想定と違うものとして失敗する
  - 一覧と比べ、不足と余分を名前順ですべて報告し、一覧の場所と「一覧を更新するか export をやめるか」の案内と `docs/dev/abi-checks.md` への参照を添えて失敗する。一致すれば照合した名前の数を表示して成功する
  - `nm-elf` は Windows でないので、注記 `windows` の付いた名前を期待する集合から外す
  - 記録した出力 `tests/abi/fixtures/exports/nm-elf.txt`（Linux の `nm -D --defined-only` の実際の出力の形。除外リストの名前を含む）と、それに対応する `expected.symbols`（注記 `windows` の付いた名前を 1 つ含む）を用意する
  - 完了条件: `cmake -DSYMBOLS_FILE=... -DFORMAT=nm-elf -DDUMP_FILE=... -P check_symbols.cmake` が、`expected.symbols` で成功し、1 つ欠けて 1 つ余分な一覧で両方の名前と案内を出して失敗し、重複と規則外の名前を含む一覧で行番号付きの報告を出して失敗する。実際の `abi/clopts.symbols` を一覧だけの検査にかけると成功する
  - _Boundary: SymbolCheck_
  - _Depends: 1.1_
  - _Requirements: 2.3, 2.4, 3.4, 3.5, 3.6, 3.7, 3.9_

- [x] 2.2 シンボル照合スクリプトに残りの 3 形式とツールの実行を加える
  - `dumpbin` と `objdump-pe` では注記 `windows` の付いた名前も期待する集合に入れる
  - `nm-macho`（先頭の `_` を 1 つ除く）、`dumpbin`（`ordinal hint RVA name` の表の行から名前を取り、`=` 以降を捨てる）、`objdump-pe`（`[Ordinal/Name Pointer] Table` の節の `[` で始まる行の最後の語）の取り出し規則を加える
  - `LIBRARY` があれば、`DUMP_FILE` の代わりに `TOOL` を設計の表どおりの引数で実行する（`TOOL` が `link.exe` なら `/dump /exports`）。`TOOL` が空か存在しなければ `export dump tool not found for FORMAT <FORMAT>`、ツールが非 0 で終われば終了コードと出力を添えて失敗する
  - 3 形式の記録した出力（`nm-macho.txt`、`dumpbin.txt`、`objdump-pe.txt`）を、各ツールの文書にある出力の形に従って手で作り（Linux の開発機では採取できないため）、`expected.symbols` と同じ名前の集合（Windows の 2 形式は注記 `windows` の名前を含み、`nm-macho` は含まない）になるよう用意する
  - 完了条件: 4 形式すべての記録した出力が `expected.symbols` で成功する。Linux の共有ビルドの `libclopts.so` に `LIBRARY` と `TOOL=nm` を渡すと実際の `abi/clopts.symbols` で成功し、`TOOL` を空にすると、ツールが見つからない旨で失敗する
  - _Depends: 2.1_
  - _Requirements: 3.3, 3.4, 3.8, 3.9_

- [x] 2.3 (P) 公開ヘッダの静的検査スクリプトと、その良い例・悪い例を実装する
  - `HEADER_DIR` の直下の `*.h` を毎回名前順に列挙し、コメント（複数行の `/* */` と `//`）と前処理指令の行（継続行を含む）を、行番号を保ったまま取り除く
  - 波括弧の深さを追って最上位の文に区切る。`extern "C" {` とその `}` は透過的に扱い、`static` で始まる文は本体が閉じたところで終える
  - 設計の分類表どおりに判定する: 構造体・共用体の定義（`typedef` の有無によらない）は違反、前方宣言・`typedef` の列挙・関数ポインタ型は許可、`static` の文は `CLOPTS_API` の検査の対象外、それ以外の関数の宣言は先頭が `CLOPTS_API` でなければ違反（名前は最初の `(` の直前の識別子）。違反をすべて集めて `<file>:<line>: <reason>` で出してから失敗し、なければヘッダと関数の数を表示して成功する。扱わない構文をスクリプトの先頭の注釈に書く
  - `tests/abi/fixtures/header_rules/good/`（前方宣言、`typedef enum`、関数ポインタ型、`static inline` の定義、`extern "C"`、コメント中の `struct x { }`、引数が次の行に続く `CLOPTS_API` 付きの宣言）と、`bad/<case>/<case>.h`（`struct_def`、`union_def`、`missing_api`、`missing_api_multiline`）を用意する
  - 完了条件: `cmake -DHEADER_DIR=... -P check_header_rules.cmake` が、実際の `include/clopts` と `good` で成功し、`bad` の各ケースで期待するファイル・行・理由を出して失敗する
  - _Boundary: HeaderRulesCheck_
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.8_

- [x] 2.4 (P) 公開ヘッダの C99 / C++ コンパイル検査の子プロジェクトを実装する
  - 構成時に `${CLOPTS_SOURCE_DIR}/include/clopts/*.h` を列挙し、ヘッダごとに `check_<header>.c` または `check_<header>.cpp`（その 1 つのヘッダの include と、空の翻訳単位を避ける `typedef` 1 つ）をビルドツリーに生成する
  - `HEADER_CHECK_LANG=C`: C だけのプロジェクトで、生成した `.c` のオブジェクトライブラリに CLOpts の `clopts_apply_compiler_options` を適用する（`CLOPTS_WERROR=ON`）
  - `HEADER_CHECK_LANG=CXX`: C と C++ のプロジェクトで、生成した `.cpp` を C++11（拡張なし）と設計の警告設定（GCC / Clang は `-Wall -Wextra -Wpedantic -Werror`、MSVC は `/W4 /WX /utf-8 /Zc:preprocessor /Zc:__cplusplus /EHsc`）でコンパイルする。CLOpts を `add_subdirectory` で取り込み、一覧の全シンボルのアドレスを取る `link_check.cpp` を一覧から生成して（注記 `windows` の名前は `#ifdef _WIN32` で囲む）`CLOpts::clopts` にリンクする実行ファイルを作る
  - 完了条件: `tests/cmake/run_fixture.cmake` を `EXPECT=build_ok` で直接呼び、C と C++ の両方が Linux の静的・共有の両形式で成功する。一時的に公開ヘッダの 1 つで `extern "C"` を外す（または C99 で通らない記述を入れる）と、該当する `check_<header>` を含む出力で失敗することを確かめ、変更を戻す
  - _Boundary: HeaderCompileFixture_
  - _Depends: 1.1_
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.7_

- [x] 2.5 (P) abidiff 比較スクリプトの基準の選択と終了コードの判定を、自己テストとともに実装する
  - `select_baseline`: `^v([0-9]+)\.([0-9]+)\.([0-9]+)$` に合うタグだけを候補にし、現在の major と同じで現在の版以下のものから最大の版を数値の比較で選ぶ。現在の版が 1.0.0 未満なら空を返す
  - `classify_exit_code`: ビット 1 か 2 は `tool_error`、それ以外でビット 8 は `incompatible`、0 と 4 は `compatible`、未知のビットは `tool_error`
  - `MODE=self_test` で両関数を表駆動で検査する（0.x、同じ major の最大、`v1.10.0` と `v1.9.0`、現在より新しいタグ・別の major・`-rc` の除外、候補なし。0、4、12、1、2、3、13、16）。git、ビルド、abidiff を使わない
  - 完了条件: `cmake -DMODE=self_test -P run_abidiff.cmake` が成功し、表の 1 行の期待を一時的に変えると、その行を示して失敗する
  - _Boundary: AbiCompareScript_
  - _Requirements: 6.2, 6.3, 6.6, 6.7, 6.8_

- [ ] 2.6 abidiff 比較スクリプトの比較の経路を実装する
  - `MODE=compare`: `project(clopts VERSION ...)` から版を読み（読めなければ失敗）、1.0.0 未満または基準がなければ `abi-compare: skipped: <理由>` を出して成功する
  - 基準のタグを `git archive` と `cmake -E tar` で `<WORK_DIR>/baseline-src` に取り出し、基準と現在のそれぞれを共有・テストなし・install あり・`CMAKE_BUILD_TYPE=Debug`・同じ生成器と C コンパイラで構成・ビルド・install する。失敗したらその出力を添えて失敗する。`<WORK_DIR>` は毎回作り直し、リポジトリの状態を変えない
  - install 先の `lib*/` からシンボリックリンクでない `libclopts.so.*` の実体を 1 つ見つけ（0 個か複数なら失敗）、`abidiff --headers-dir1 --headers-dir2 --drop-private-types` を実行する。`abidiff` がなければ失敗する。結果を `classify_exit_code` で判定し、最初の行に状態を出し、abidiff の出力を常に表示する
  - 完了条件: 現在のリポジトリ（0.1.0）で `MODE=compare` が `abi-compare: skipped: version 0.1.0 is before 1.0.0` を出して成功する。比較の経路そのものの確認は 3.3 で行う
  - _Depends: 2.5_
  - _Requirements: 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [ ] 3. 統合: CTest への登録と CI
- [ ] 3.1 `abi.*` のテストを CTest に登録する
  - 設計の登録表どおりに、`abi.symbol_list`、`abi.exports`（共有ビルドのときだけ）、`abi.symbols_selftest.<format>` の 4 件と `.mismatch`・`.bad_list`、`abi.header_rules`、`abi.header_rules_selftest.good` と各 `bad` のケース（`PASS_REGULAR_EXPRESSION` で期待するファイル・行・理由）、`abi.headers_c`、`abi.headers_cxx`、`abi.compare_selftest` を、すべてラベル `abi` で登録する
  - `abi.exports` の `FORMAT` と `TOOL` を設計の表どおりに選ぶ（MSVC は `CMAKE_LINKER` のディレクトリの `dumpbin`、なければ `link.exe` を `/dump` で、MinGW は `CMAKE_OBJDUMP`、macOS と Linux は `CMAKE_NM`）。見つからなくても登録し、空の `TOOL` を渡す
  - `check_language(CXX)` で C++ コンパイラを判定し、なければ `abi.headers_cxx` を `DISABLED` にする。Visual Studio 生成器以外では見つかった C++ コンパイラを子に渡す。子プロジェクトの起動引数は `tests/toolchain/CMakeLists.txt` と同じにし、このビルドツリーの `CLOPTS_BUILD_SHARED` を渡す。ルートの `project()` に `CXX` を足さない
  - 完了条件: Linux の静的ビルドと共有ビルドの両方で `ctest -L abi` がすべて通る。`ctest -N -L abi` に、共有ビルドでは `abi.exports` があり静的ビルドではない。新しいビルドツリーを `-DCMAKE_CXX_COMPILER=CMAKE_CXX_COMPILER-NOTFOUND` で構成したとき（`check_language(CXX)` は値が定義済みだと検出を飛ばすので、存在しないパスではなくこの値を使う。効かなければ `CXX` 環境変数に存在しないパスを入れた新しいツリーで確かめ、使った方法を記録する）、`abi.headers_cxx` が `Not Run (Disabled)` と表示され、失敗に数えられない
  - _Depends: 2.2, 2.3, 2.4, 2.5_
  - _Requirements: 3.1, 3.2, 3.3, 4.7, 5.5, 5.6, 5.8_

- [ ] 3.2 CI に独立した `abi-compare` ジョブを足す
  - `.github/workflows/ci.yml` に、既存のトリガで動き、既存の `build-and-test` ジョブと互いに `needs` で依存しないジョブ `abi-compare`（`ubuntu-latest`）を足す。手順は `actions/checkout@v4`（`fetch-depth: 0`）、`abigail-tools` の apt でのインストール、`run_abidiff.cmake` の `MODE=compare` 実行（`WORK_DIR` は `RUNNER_TEMP` の下、`C_COMPILER=gcc`、`GENERATOR="Unix Makefiles"`）
  - 既存の `build-and-test` ジョブとマトリクスは変えない
  - 完了条件: ワークフローの YAML が構文として正しく（手元で YAML として読み込めることを確かめる）、既存のジョブとの差分が新しいジョブの追加だけである
  - _Depends: 2.6_
  - _Requirements: 6.1, 6.9_

- [ ] 3.3 abidiff 比較の経路を、手元の Linux の仮のタグで確かめる
  - 手元の Linux に `abigail-tools` を入れ、`abidiff --version` が動くことを確かめる
  - リポジトリの一時的な複製（`git clone` によるもの。タグを付けられる）で、版を 1.0.0 にしたコミットにローカルのタグ `v1.0.0` を付け、(a) 変更なし、(b) 公開関数を 1 つ足した変更（一覧も更新）、(c) 公開関数を 1 つ消した変更、のそれぞれで `MODE=compare` を実行する。複製は検証の後に消し、本物のリポジトリにタグを残さない
  - 実行したコマンドと結果を、4.2 が文書に載せられるよう作業メモ（スクラッチの場所）に残す。このタスクでは文書を編集しない
  - 完了条件: (a) と (b) が `compatible` で成功し、(c) が `incompatible` で失敗して abidiff の報告に消した関数の名前が出る
  - _Depends: 2.6_
  - _Requirements: 6.4, 6.5, 6.6, 6.7_

- [ ] 4. 文書
- [ ] 4.1 (P) ABI 方針の文書 `docs/abi.md` を書く
  - 設計の AbiPolicyDocs の `docs/abi.md` の項目（保証の範囲、版と SOVERSION、Windows の DLL 名、macOS の互換版と現在版と前方互換を保証しないときの挙動、互換を保つ変更、互換を壊す変更、壊すときの手順）を日本語で書く。識別子・コマンド・ファイル名は英語のまま。`docs/dev/` へはリンクしない
  - 書いた SOVERSION の規則と macOS の版が、`cmake/CloptsLibrary.cmake` の実装と一致することを確かめる
  - 完了条件: `docs/abi.md` があり、要件 1.1〜1.8 の各項目に対応する節がある（節と要件の対応を示す）
  - _Boundary: AbiPolicyDocs（docs/abi.md）_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.11_

- [ ] 4.2 (P) 保守者向けの文書 `docs/dev/abi-checks.md` を書き、structure.md を更新する
  - 公開関数を足す・消すときの手順（`CLOPTS_API` を付け、同じ変更で `abi/clopts.symbols` を更新する。形式と推奨する並べ方）、各検査の表（テスト名、検出するもの、登録の条件、OS、有効になる版）、失敗の読み方と除外リストに足す判断基準、ヘッダの走査器が扱わない構文、手元での abidiff の実行方法と 1.0.0 前に比較の経路を確かめる方法（3.3 で実際に行った手順と結果を例として載せる）を、日本語で書く
  - `.kiro/steering/structure.md` の `tests/abi/` の項に公開シンボル一覧の場所 `abi/clopts.symbols` を追記し、リポジトリ直下の `abi/` を Directory Patterns に加える
  - 完了条件: 文書の検査の表のテスト名が `ctest -N -L abi` の出力と一致する。structure.md に `abi/clopts.symbols` が載っている
  - _Boundary: AbiPolicyDocs（docs/dev/abi-checks.md）、structure.md_
  - _Depends: 3.1, 3.2, 3.3_
  - _Requirements: 1.9, 1.10, 1.11_

- [ ] 5. 検証
- [ ] 5.1 検査が実際の違反を捕まえることを Linux で確かめる
  - 一時的な変更で、(a) 内部関数に `CLOPTS_API` を付けると共有ビルドの `abi.exports` が余分として報告する、(b) 一覧から 1 つ消すと不足として報告する、(c) 公開ヘッダに構造体の定義を足すと `abi.header_rules` が報告する、(d) 公開関数の `CLOPTS_API` を外すと `abi.header_rules` と `abi.exports` が報告する、ことを確かめ、すべての変更を戻す
  - 完了条件: 各ケースの失敗の出力（該当する名前・ファイル・行を含む）を記録し、変更を戻した後に静的・共有の両方で `ctest` 全体が通る
  - _Depends: 3.1_
  - _Requirements: 3.5, 3.6, 4.2, 4.4_

- [ ] 5.2 CI の全環境で ABI の検査が通ることを確かめる
  - 変更を push し、CI の 10 ジョブすべてで `ctest` が通ること、共有の 5 ジョブのログに `abi.exports` の成功が、静的の 5 ジョブのログに `abi.exports` がないこと、`abi-compare` ジョブが skipped の行を出して成功することを確かめる。いずれかの環境で除外リストに足すべきツールチェーン由来のシンボルが出たら、理由付きで足して再実行する。本物の出力を取り出し規則が読めない（名前が 0 個、列の取り違え）ときは、`check_symbols.cmake` の取り出し規則を直し、CI のログから採取した出力で該当する記録ファイルを置き換えて再実行する。取り出し規則か除外リストを変えたときは、`docs/dev/abi-checks.md` の該当する記述も合わせて直す
  - 完了条件: CI の全ジョブ（10 + `abi-compare`）が成功している
  - _Depends: 3.1, 3.2_
  - _Requirements: 3.2, 3.3, 5.5, 6.1, 6.3_

## Implementation Notes
- 1.1: `clopts_argv_from_wide` は `platform.h` の `#ifdef _WIN32` の中だけで宣言されるため、設計の SymbolList に OS の注記（`<name> windows`）を追記した（ユーザー承認済み）。注記付きの名前は `dumpbin` / `objdump-pe` の照合でだけ要求し、`link_check.cpp` では `#ifdef _WIN32` で囲む。Linux の `libclopts.so` の export は注記なしの 64 個
- 2.1: `check_symbols.cmake` は形式ごとに `_check_symbols_extract_<FORMAT>` と `_check_symbols_exclude_<FORMAT>` を持つ（2.2 はここに足す）。データ由来のリストの空判定は必ず `list(LENGTH)` で行う（`if(<var>)` だと `off` などの名前が偽になり見逃す）。FATAL_ERROR の報告行は先頭に空白 1 つを付けて CMake の折り返しを防ぐ。`nm-elf.txt` は実出力の 12 名の部分集合で、他形式の記録も同じ 12 名（Windows の 2 形式は加えて `clopts_argv_from_wide`）にそろえる
- 2.2: `TOOL` は PATH 上の名前も `find_program` で解決する（3.1 ではフルパスを渡す）。dumpbin の取り出しは英語の出力（`ordinal hint RVA name` と `Summary`）に依存する。objdump-pe は binutils 2.42 の実出力（旧形式）と、記録した新形式の両方で通る
- 2.3: `check_header_rules.cmake` は `static` でない本体付きの関数定義も本体の閉じで文を終え、`CLOPTS_API` なしとして報告する。波括弧の不整合も違反として報告する。扱わない構文はスクリプト先頭の注釈にあり、4.2 で `docs/dev/abi-checks.md` に写す
- 2.4: 子プロジェクトは CLOpts を `EXCLUDE_FROM_ALL` で取り込み、ヘッダごとの検査は `$<TARGET_PROPERTY:clopts,INTERFACE_*>` で include と `CLOPTS_STATIC` を受け取る（リンクすると CLOpts 本体が先に失敗して `check_<header>` が出ないため）。CXX では `add_dependencies(clopts abi_header_check)` でヘッダ検査を先にコンパイルする。`extern "C"` の欠落は `abi_link_check` の未定義参照（修飾名）で報告される。MSVC / AppleClang / MinGW は CI で確かめる
- 2.5: `MODE` を省くと compare（設計どおり）。終了コードの判定は設計の表の順に評価するので、8 と未知のビットが同時に立つ 24 は incompatible になる（どちらでもジョブは失敗する）。`select_baseline` はタグを CMake のリストで受け取るので、2.6 で `git tag --list` の行を分けて渡す
