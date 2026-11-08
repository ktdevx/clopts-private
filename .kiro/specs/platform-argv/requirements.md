# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts の利用者（コマンドラインツールの開発者）は、Windows でも他の OS と同じように UTF-8 の引数と環境変数を扱いたい。Windows の `main(argc, argv)` と `getenv` は ANSI コードページで値を渡すため、日本語のファイル名などの非 ASCII 文字が壊れる。ツールの利用者から見ても、同じ exe なのに引数の見え方が `main` の argv と食い違うとバグ報告の種になる。

**現状**: Monde の `src/cli_args/args_argv.{h,c}`（約 180 行）は、非 Windows ではポインタをそのまま返し、Windows では `CommandLineToArgvW(GetCommandLineW())` と `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS)` で変換している。`CommandLineToArgvW` は 2008 年以前の分割規則で動くため、現行 MSVC CRT が `main` に渡す argv と食い違う（引用符内の `""`、argv[0] の終端規則、先頭が空白のときの挙動）。shell32 にリンクし、変換失敗の理由を文字列比較で判定している箇所がある。テストは `tests/cli_args/argv_test.c` の 5 件。CLOpts では project-foundation がビルド基盤（静的／共有、`CLOPTS_API`）を、core-parser が環境変数の取得コールバック（借用した `const char *` を返し、ライブラリが複製する）を提供する。

**何を変えるか**: 不透明ハンドルとして argv を返す取得関数（非 Windows ではパススルーでできるだけ確保しない、Windows では `GetCommandLineW()` を現行 MSVC CRT の規則で自前分割して UTF-8 に変換し、`main` の argv と要素数・区切りが一致する）と、`wmain` 利用者向けの `wargv` からの変換関数を提供する。argv はライブラリが確保し、ライブラリの冪等な解放関数で解放する。環境変数を UTF-8 で返す取得関数を core-parser の環境変数コールバックの形で提供する（Windows では `GetEnvironmentVariableW` + UTF-8 変換）。変換失敗（不正な UTF-16）・取得失敗・メモリ不足は、該当する引数の添字を含む構造化エラー（列挙とアクセサ）で返す。Windows での依存は kernel32 のみとし、分割関数は純粋関数にして Windows 以外でも表駆動で単体テストできるようにする。

**範囲外**: `activeCodePage=UTF-8` マニフェスト（文書で言及するにとどめる）、コンソール出力のコードページ設定、stdio のバイナリモード設定、引数の解析そのもの（core-parser）、文面（help-and-messages）、利用者向けの手順書（docs-and-release）。

詳細は同ディレクトリの `brief.md` を参照。

## Introduction

platform-argv は、CLOpts の利用者（C で CLI ツールを書く開発者）が、Windows でも他の OS と同じように UTF-8 のコマンドライン引数と環境変数を受け取れるようにする。Windows では、プロセスのコマンドライン文字列（UTF-16）を、現行 MSVC CRT が `main` の argv を作るときと同じ規則で分割し、UTF-8 に変換した argv を返す。Windows 以外では、渡された argv をそのまま返す。あわせて、`wmain` を使う利用者向けに UTF-16 の引数列を UTF-8 に変換する関数と、core-parser の環境変数取得コールバックとして差し込める UTF-8 の環境変数取得関数を提供する。

取得した argv と環境変数取得の文脈はライブラリが確保し、ライブラリの関数で解放する。失敗は文字列ではなく、種別の列挙と該当する引数の添字などをアクセサで読む構造化エラーとして返る。

本文書の主語 the platform module は、本 spec が提供する公開関数群（argv の取得・変換・解放とそのアクセサ、環境変数の取得とその文脈）を指す。

## Boundary Context

- **In scope**: Windows でのコマンドライン文字列の分割（現行 MSVC CRT の規則）、UTF-16 から UTF-8 への変換、`wmain` の引数列（UTF-16）からの変換、取得した argv の所有と解放、Windows 以外でのパススルー、環境変数の UTF-8 での取得、構造化エラー（不正な UTF-16、コマンドライン文字列の取得失敗、メモリ不足、不正な UTF-8 の変数名）、分割規則の表駆動テスト。
- **Out of scope**: 実行ファイルのマニフェストによる `activeCodePage=UTF-8` の設定（文書で言及するにとどめる）、コンソール出力のコードページ設定、stdio のバイナリモード設定、ワイルドカード（`*`、`?`）の展開、引数の解析そのもの、ヘルプやエラーの文面、利用者向けの手順書。
- **Adjacent expectations**:
  - core-parser は環境変数取得コールバックのシグネチャ（`clopts_getenv_fn`。借用した文字列を返し、ライブラリが複製する。`CLOPTS_ENV_FAILED` を返すと解析は `CLOPTS_OUT_OF_MEMORY` で終わる）を定めている。本 spec はそれに合わせ、core-parser の契約は変えない。
  - `clopts_parse`（取得関数を指定しない形）の既定の環境変数取得は、Windows でも `getenv` 相当のまま変えない。UTF-8 で読みたい利用者は、本 spec の取得関数を `clopts_parse_with_env` に渡す。この使い方の説明は docs-and-release が担う。
  - project-foundation が提供するビルド基盤（静的／共有の選択、`CLOPTS_API`）の上に載る。公開シンボル一覧と ABI の検査は abi-compat が所有し、本 spec が追加する公開関数はその一覧に載る。
  - 本 spec の関数は stdout / stderr に書かず、`exit` しない。

## Requirements

### Requirement 1: argv の取得とアクセサ
**Objective:** As a CLI ツールの開発者, I want `main` の argc / argv を渡すだけで、どの OS でも UTF-8 の argv を取得したい, so that OS ごとの分岐を書かずに同じコードで引数を扱える

#### Acceptance Criteria
1. When 利用者が `main` の argc と argv を渡して argv の取得を要求したとき, the platform module shall 取得結果を不透明ハンドルとして返し、要素数と文字列の配列をアクセサで読めるようにする。
2. The platform module shall 取得結果の文字列の配列を、要素数の位置に NULL を置いた形（`argv[argc] == NULL`）で返す。
3. When argv の取得が成功したとき, the platform module shall 構造化エラーの種別として成功を表す値を返す。
4. If argv の取得が失敗したとき, the platform module shall 要素数として 0 を返し、途中まで変換した要素を利用者に見せない。
5. The platform module shall 公開ヘッダに取得結果の構造体の定義を置かず、取得結果の中身をアクセサ経由でのみ読ませる。
6. The platform module shall 利用者が渡した argv の内容を書き換えない。

### Requirement 2: Windows でのコマンドライン文字列の分割
**Objective:** As a Windows 向けに CLI ツールを配布する開発者, I want 取得した argv が `main` の argv と同じ要素数・同じ区切りになってほしい, so that 「同じ exe なのに引数の見え方が違う」という食い違いが起きない

#### Acceptance Criteria
1. While Windows で動作しているとき, when argv の取得を要求されたとき, the platform module shall 利用者が渡した argc / argv の内容によらず、プロセスのコマンドライン文字列から argv を作る。
2. While Windows で動作しているとき, the platform module shall コマンドライン文字列を現行 MSVC CRT が `main` の argv を作るときと同じ規則で分割し、要素数と各要素の内容を一致させる。
3. The platform module shall 先頭の要素（argv[0]）を次の規則で切り出す: 引用符（`"`）は出現するたびに引用モードを切り替えて出力に含めず、バックスラッシュは常にそのまま出力し、引用モードの外の空白またはタブで要素を終える。
4. If コマンドライン文字列が空白またはタブで始まるとき, the platform module shall argv[0] を空文字列とする。
5. If コマンドライン文字列が空のとき, the platform module shall 要素数を 1、argv[0] を空文字列とする。
6. The platform module shall argv[0] より後の要素を、引用モードの外にある 1 個以上の空白またはタブの並びで区切り、末尾の空白やタブから空の要素を作らない。
7. The platform module shall argv[0] より後の要素で、引用符の直前にある 2n 個のバックスラッシュを n 個のバックスラッシュとして出力し、その引用符で引用モードを切り替える。
8. The platform module shall argv[0] より後の要素で、引用符の直前にある 2n+1 個のバックスラッシュを n 個のバックスラッシュと 1 個の引用符として出力し、引用モードを切り替えない。
9. The platform module shall argv[0] より後の要素で、引用符が続かないバックスラッシュをそのまま出力する。
10. While 引用モードにあるとき, when 引用符が 2 個続いたとき, the platform module shall 引用符 1 個を出力し、引用モードを維持する。
11. When 区切りの後に `""` だけからなる要素が現れたとき, the platform module shall 空文字列の要素を 1 個作る。
12. If 引用モードのままコマンドライン文字列が終わったとき, the platform module shall その位置で最後の要素を終える。
13. The platform module shall 空白とタブ以外の文字（改行など）を区切りとして扱わず、ワイルドカード（`*`、`?`）を展開しない。

### Requirement 3: UTF-8 への変換と wmain の引数列からの変換
**Objective:** As a Windows 向けに CLI ツールを書く開発者, I want 日本語のファイル名などの非 ASCII の引数を UTF-8 で受け取りたい, so that ANSI コードページによる文字化けを避けられる

#### Acceptance Criteria
1. While Windows で動作しているとき, the platform module shall 分割した各要素を UTF-16 から UTF-8 に変換して返す。
2. Where Windows でビルドされているとき, the platform module shall `wmain` の argc と UTF-16 の引数列を受け取り、各要素を UTF-8 に変換した argv を Requirement 1 と同じ形の取得結果として返す変換関数を提供する。
3. Where Windows 以外でビルドされているとき, the platform module shall 2. の変換関数を公開ヘッダで宣言せず、共有ライブラリから export しない。
4. If 変換する要素に不正な UTF-16（対になっていないサロゲート）が含まれるとき, the platform module shall 変換を失敗とし、その要素の添字と「不正な UTF-16」の種別を構造化エラーとして返す。
5. If 不正な UTF-16 を含む要素が複数あるとき, the platform module shall 最も小さい添字の要素を報告する。
6. The platform module shall 不正な UTF-16 を置換文字などに置き換えて変換を続けない。

### Requirement 4: Windows 以外でのパススルー
**Objective:** As a macOS / Linux 向けにも同じソースで配布する開発者, I want Windows 以外では `main` の argv がそのまま使われてほしい, so that 余計な確保や変換の費用がかからない

#### Acceptance Criteria
1. While Windows 以外で動作しているとき, when argv の取得を要求されたとき, the platform module shall 利用者が渡した argc を要素数とし、利用者が渡した argv の各文字列と同じ文字列（同じアドレス）を返す。
2. While Windows 以外で動作しているとき, the platform module shall 引数の文字列と配列を複製せず、文字コードの変換もしない。
3. The platform module shall Windows 以外では取得結果が返す文字列の寿命が利用者の渡した argv の寿命に従うことを、公開ヘッダの説明に明記する。

### Requirement 5: 所有と解放
**Objective:** As a 共有ライブラリとして CLOpts を使う開発者, I want ライブラリが確保した argv をライブラリの関数で解放したい, so that 利用側とライブラリで CRT が異なっても壊れない

#### Acceptance Criteria
1. The platform module shall 取得結果（成功・失敗のどちらも）を解放する公開関数を提供し、取得結果が持つすべてのメモリをその関数で解放する。
2. The platform module shall 利用者に対し、取得結果やその文字列を利用側の `free` で解放することを求めない。
3. When 解放関数に NULL が渡されたとき, the platform module shall 何もせずに戻る。
4. If 取得中にメモリ確保が失敗したとき, the platform module shall それまでに確保したメモリを残さず、「メモリ不足」の種別を読める取得結果を返す。
5. When メモリ不足で返された取得結果が解放関数に渡されたとき, the platform module shall 異常終了せずに戻る。
6. While 取得結果が解放されていないとき, the platform module shall 取得結果が返す要素数と文字列を変えない。

### Requirement 6: argv 取得の構造化エラー
**Objective:** As a CLI ツールの開発者, I want 取得の失敗理由と該当する引数を列挙とアクセサで判定したい, so that 文字列比較なしに自分の書式でエラーを表示できる

#### Acceptance Criteria
1. The platform module shall argv 取得の結果の種別を、成功、不正な UTF-16、コマンドライン文字列の取得失敗、メモリ不足を区別する列挙で返す。
2. When 種別が不正な UTF-16 のとき, the platform module shall 該当する要素の添字（argv[0] を 0 とする）をアクセサで返す。
3. When 種別が不正な UTF-16 以外のとき, the platform module shall 添字のアクセサで、該当する添字がないことを表す負の値を返す。
4. If Windows でプロセスのコマンドライン文字列を取得できないとき, the platform module shall 「コマンドライン文字列の取得失敗」の種別を返す。
5. The platform module shall 失敗の種別を文字列に整形せず、文面の生成を利用者に委ねる。
6. The platform module shall 列挙の既存の値を変えず、新しい種別は末尾に追加する。

### Requirement 7: 環境変数の UTF-8 での取得
**Objective:** As a 環境変数フォールバックを使う開発者, I want Windows でも環境変数の値を UTF-8 で受け取りたい, so that 非 ASCII の値を含む環境変数が文字化けせずに解析に使われる

#### Acceptance Criteria
1. The platform module shall core-parser の環境変数取得コールバック（`clopts_getenv_fn`）と同じ形の取得関数と、その取得関数に渡す文脈を作る関数・解放する関数を提供する。
2. When 利用者が取得関数と文脈を `clopts_parse_with_env` に渡したとき, the platform module shall 解析中の環境変数の参照に UTF-8 の値を返す。
3. While Windows で動作しているとき, when UTF-8 の変数名で取得を要求されたとき, the platform module shall その変数の値を UTF-16 で読み、UTF-8 に変換して「見つかった」を返す。
4. While Windows 以外で動作しているとき, when 取得を要求されたとき, the platform module shall プロセスの環境変数の値を変換せずに返す。
5. If 変数が設定されていないとき, the platform module shall 「未設定」を返す。
6. When 変数が空の値で設定されているとき, the platform module shall 空文字列を値として「見つかった」を返す。
7. The platform module shall 返す値を文脈が持つ領域（Windows 以外ではプロセスの環境）からの借用とし、同じ文脈での次の取得呼び出しまたは文脈の解放まで有効に保つ。
8. If Windows で変数の値に不正な UTF-16 が含まれるとき、変数名が不正な UTF-8 のとき、またはメモリ確保が失敗したとき, the platform module shall 「失敗」（`CLOPTS_ENV_FAILED`）を返す。
9. When 取得関数が「失敗」を返したとき, the platform module shall 失敗の種別（不正な UTF-16 の値、不正な UTF-8 の変数名、メモリ不足）と対象の変数名を文脈に記録し、利用者がアクセサで読めるようにする。
10. When 取得関数が「見つかった」または「未設定」を返したとき, the platform module shall 文脈のアクセサで失敗がないことを返す。
11. When 文脈の解放関数に NULL が渡されたとき, the platform module shall 何もせずに戻る。
12. If 文脈の作成でメモリ確保が失敗したとき, the platform module shall 文脈の代わりに NULL を返す。

### Requirement 8: 依存・ビルド・公開 API の規律
**Objective:** As a CLOpts を取り込む開発者, I want 追加の依存なしに静的版・共有版のどちらでも本機能を使いたい, so that 取り込みのためにビルド設定を変えなくてよい

#### Acceptance Criteria
1. Where Windows でビルドされているとき, the platform module shall Windows の標準のシステムライブラリ（kernel32）以外のライブラリを利用側にリンクさせない（shell32 を含めない）。
2. Where Windows で MSVC または MinGW-w64 でビルドされているとき, the platform module shall 利用者に `wmain` や `-municode` を求めず、`main` から呼べば Requirement 2 と 3 の挙動を提供する。
3. The platform module shall 静的ライブラリ版と共有ライブラリ版のどちらでも同じ挙動を提供する。
4. The platform module shall すべての公開関数に `CLOPTS_API` を付け、公開ヘッダには不透明型の前方宣言・列挙・関数だけを置く。
5. When 共有ライブラリとしてビルドされたとき, the platform module shall 本 spec が追加する公開関数を export し、公開シンボル一覧と過不足なく一致させる。
6. The platform module shall stdout / stderr に書き込まず、`exit` を呼ばない。

### Requirement 9: 検証
**Objective:** As a CLOpts の保守者, I want 分割規則と変換を自動テストで確かめたい, so that CRT の規則との食い違いや退行を CI で検出できる

#### Acceptance Criteria
1. The platform module shall コマンドライン文字列の分割規則を、Windows 以外の OS を含む CI の全環境で表駆動テストにより検査できるようにする。
2. The platform module shall 分割規則のテストに、現行 MSVC CRT の既知の例（`"foo""bar"`、末尾のバックスラッシュ、奇数個・偶数個のバックスラッシュと引用符、先頭の空白、空の引数 `""`、閉じられない引用符、引用符を含む argv[0]）を含める。
3. While Windows で動作しているとき, when テスト実行ファイルが ASCII だけの引数で起動されたとき, the platform module shall 取得した argv の要素数と各要素が `main` の argv と一致することをテストで示す。
4. While Windows で動作しているとき, the platform module shall 非 ASCII の引数の UTF-8 変換、不正な UTF-16 の検出と添字の報告、環境変数の UTF-8 取得と失敗の記録をテストで検査する。
5. While Windows 以外で動作しているとき, the platform module shall パススルー（同じ文字列のアドレスを返すこと）と環境変数の取得をテストで検査する。
