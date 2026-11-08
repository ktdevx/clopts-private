# Requirements Document

## Project Description (Input)
**誰が困っているか**: コマンドラインツールの利用者は、マニュアルを開かずに `--help` で使い方を確認し、誤った指定をしたときには何を直せばよいか分かる英語のメッセージを読みたい。ツールの開発者は、ヘルプの文面を手で書くとオプションの追加・変更のたびに実際の挙動とずれるため、宣言から自動生成したい。

**現状**: Monde の `src/cli_args/args_help.c`（約 360 行）は、全体ヘルプ・サブコマンドヘルプ・バージョン行・使い方の案内を生成するが、出力先が foundation の `mnd_buf_t` に固定され、エラーは走査器が文字列に整形してしまうため、利用側が接頭辞を剥がして自分の書式（`monde: error: ...`）に合わせ直していた。桁揃えの幅はバイト数で数えている。CLOpts では core-parser が宣言（不透明ハンドル）と構造化エラー（結果ハンドルのアクセサ）を、project-foundation が書き込みコールバック型 `clopts_write_fn` と公開ヘッダ内の `static inline` の `FILE*` 用コールバックを提供する。

**何を変えるか**: 宣言と構造化エラーから、決定的な英語の文面（全体ヘルプ、サブコマンドヘルプ、サブコマンドなし構成のヘルプ、Usage 行、バージョン行、使い方の案内、エラー文面）を書き込みコールバックへ書く公開関数を提供する。引数は不透明ハンドル・関数ポインタ・整数のみで構造体を値渡ししない。グローバルオプション・カウントフラグ・否定可能フラグ（`--[no-]color`）・環境変数名（`[env: NAME]`）・既定値・必須をヘルプ上で表現する。エラー文面は種別ごとに 1 行で該当トークンとコマンド名を含み、接頭辞（プログラム名、`error:`）の有無をビットフラグで選べる。文面は LF 固定・端末幅非依存・バイト単位で決定的で、桁揃えは UTF-8 のコードポイント数で数える。ライブラリ本体は `stdio` に依存せず、走査器の実装ではなく内部の宣言モデルの読み出し口と結果のアクセサだけに依存する。書き込みコールバックの失敗は呼び出し側へ返す。

**範囲外**: 端末幅に合わせた折り返し、複数行の説明、epilog / examples、色付け、候補提示（did you mean）、多言語化、東アジア幅の考慮、出力先と終了コードの決定（利用側）、宣言型・エラー型の定義と解析の挙動（core-parser）、文書・サンプルでの説明（docs-and-release）。

詳細は同ディレクトリの `brief.md` を参照。

## Introduction

help-and-messages は、core-parser で組み立てた宣言（program / command / option / positional のハンドル）と、解析の結果ハンドルが持つ構造化エラーから、利用者向けの英語の文面を生成し、呼び出し側が渡す書き込みコールバックへ書く。生成する文面は、全体ヘルプ、サブコマンドのヘルプ、サブコマンドなし構成のヘルプ、Usage 行、バージョン行、使い方の案内、使い方エラー・宣言エラー・メモリ不足の各 1 行である。

文面は同じ宣言と同じ指定からは常に同じバイト列になり、テストでバイト単位に検査できる。レイアウト（セクション構成、`(required)` / `(default: X)` の注記、左列と説明の桁揃え）は Monde の `args_help.c` を踏襲し、CLOpts で増えた属性（グローバルオプション、カウントフラグ、否定可能フラグ、環境変数、短い名前だけ・長い名前だけのオプション、サブコマンドなし構成）の表記を加える。

本文書の主語 the help module は、本 spec が提供する文面生成の公開関数群を指す。

## Boundary Context
- **In scope**:
  - 全体ヘルプ、サブコマンドのヘルプ、サブコマンドなし構成のヘルプ、Usage 行、バージョン行、使い方の案内の生成
  - 使い方エラー（全種別）、宣言エラー（全種別）、メモリ不足の 1 行の文面の生成と、接頭辞の選択
  - 左列と説明の桁揃え（UTF-8 のコードポイント数で幅を数える）
  - 書き込みコールバックへの出力と、書き込み失敗の検出・報告
  - `tests/help/` の単体テスト（バイト単位の比較、全エラー種別の網羅）
- **Out of scope**:
  - 端末幅に合わせた折り返し、複数行の説明の整形、epilog / examples、色付け、候補提示（did you mean）、多言語化・文面の差し替え、東アジア幅（全角文字の 2 桁扱い）の考慮
  - 文面を stdout / stderr のどちらに出すか、どの状態でどの文面を出すか、終了コードの決定（利用側）
  - 宣言の組み立て、宣言検査、argv の解析、HELP / VERSION の検出とヘルプ対象の決定、エラー種別の定義（core-parser）
  - 公開シンボル一覧の検査の仕組み（abi-compat。本 spec は新しい公開関数を一覧に足すだけ）
  - README・サンプルでのヘルプ出力例の説明（docs-and-release）
- **Adjacent expectations**:
  - core-parser が、宣言の内容（名前、説明、要約、値の表示名、既定値、環境変数名、必須、否定可能、種類、位置引数の必須・可変長、バージョン文字列、サブコマンドの有無）を読み取り専用で参照できる内部の読み出し口と、結果ハンドルのエラーのアクセサ（種別、該当 command / option / positional、トークン、長い名前の長さ、まとめ書き中の 1 文字）を提供すること。
  - core-parser が使い方エラーまたは宣言エラーの種別を列挙に追加したときは、本 spec の文面表を同じ変更で追従させること（追従漏れは本 spec のテストで検出する）。
  - project-foundation が、書き込みコールバックの型と、利用側の翻訳単位でコンパイルされる `FILE*` 用のコールバックを公開ヘッダで提供すること。

## Requirements

### Requirement 1: サブコマンドあり構成の全体ヘルプ
**Objective:** As a CLI ツールの利用者, I want プログラム全体の使い方とサブコマンドの一覧を確認したい, so that どのサブコマンドを使えばよいかがマニュアルなしで分かる

#### Acceptance Criteria
1. When サブコマンドあり構成の program について全体ヘルプが要求された, the help module shall 次の段落をこの順で書く: プログラム名とサブコマンドの位置を示す Usage 行、プログラムの説明（設定されている場合のみ）、`Commands:` の一覧、`Options:` の一覧、サブコマンドのヘルプの求め方を示す末尾の案内（`Run '<program> <command> --help' for help on a command.` の形）。
2. The help module shall `Commands:` の一覧に、宣言の追加順で各サブコマンドの名前と要約を 1 行ずつ並べ、最後に予約されたサブコマンド `help` とその説明の行を置く。
3. If サブコマンドに要約が設定されていない, then the help module shall その行に名前だけを書き、行末に空白を残さない。
4. The help module shall 全体ヘルプの `Options:` の一覧に、宣言の追加順でグローバルオプションを並べ、続けて `-h, --help` の行を置く。
5. Where program がバージョン文字列を持つ, the help module shall 全体ヘルプの `Options:` の一覧の `-h, --help` の後に `--version` の行を置く。
6. Where program がバージョン文字列を持たない, the help module shall 全体ヘルプに `--version` の行を書かない。

### Requirement 2: サブコマンドのヘルプ
**Objective:** As a CLI ツールの利用者, I want 個々のサブコマンドの引数とオプションを確認したい, so that そのサブコマンドを正しく指定できる

#### Acceptance Criteria
1. When サブコマンドのヘルプが要求された, the help module shall 次の段落をこの順で書く: プログラム名・サブコマンド名・オプションの位置・位置引数の書き方を含む Usage 行、サブコマンドの説明、`Arguments:` の一覧（位置引数がある場合のみ）、`Options:` の一覧、`Global Options:` の一覧（該当する行がある場合のみ）。
2. If サブコマンドに説明が設定されていない, then the help module shall 要約を説明の代わりに書き、要約もなければ説明の段落を書かない。
3. The help module shall `Arguments:` の一覧に、宣言の追加順で各位置引数の名前（`<NAME>` の形）と説明を 1 行ずつ並べる。
4. The help module shall サブコマンドのヘルプの `Options:` の一覧に、宣言の追加順でそのサブコマンドのオプションを並べ、続けて `-h, --help` の行を置く。
5. The help module shall サブコマンドのヘルプの `Global Options:` の一覧に、宣言の追加順でグローバルオプションを並べ、program がバージョン文字列を持つ場合は続けて `--version` の行を置く。
6. If グローバルオプションがなく、program がバージョン文字列も持たない, then the help module shall サブコマンドのヘルプに `Global Options:` の段落を書かない。

### Requirement 3: サブコマンドなし構成のヘルプ
**Objective:** As a 単純なフィルタ型ツールの利用者, I want サブコマンドのないツールでも引数とオプションの一覧を確認したい, so that `tool [options] FILE...` 型のツールも同じ形式のヘルプで使い方が分かる

#### Acceptance Criteria
1. When サブコマンドなし構成の program についてヘルプが要求された, the help module shall 次の段落をこの順で書く: プログラム名・オプションの位置・位置引数の書き方を含む Usage 行、プログラムの説明（設定されている場合のみ）、`Arguments:` の一覧（位置引数がある場合のみ）、`Options:` の一覧。
2. The help module shall サブコマンドなし構成のヘルプに、`Commands:` の一覧、予約されたサブコマンド `help` の行、サブコマンドのヘルプの求め方を示す末尾の案内を書かない。
3. The help module shall サブコマンドなし構成のヘルプの `Options:` の一覧に、宣言の追加順で program のオプションを並べ、続けて `-h, --help` の行と、program がバージョン文字列を持つ場合は `--version` の行を置く。
4. If サブコマンドあり構成の program に対してサブコマンドを指定せずにヘルプが要求された, then the help module shall Requirement 1 の全体ヘルプを書く。

### Requirement 4: オプションの行の表記と注記
**Objective:** As a CLI ツールの利用者, I want 各オプションの名前・値・既定値・必須・環境変数をヘルプの 1 行で読みたい, so that オプションの指定方法と省略したときの挙動が分かる

#### Acceptance Criteria
1. The help module shall 短い名前と長い名前の両方を持つオプションの左列を `-c, --name` の形で書き、長い名前だけを持つオプションは短い名前の位置を空白で埋めて `--name` の開始位置を揃え、短い名前だけを持つオプションは `-c` の形で書く。
2. Where オプションが単一の値を取る, the help module shall 左列の名前の後に値の表示名を `<VALUE>` の形で書く。値の表示名が設定されていなければ、既定の表示名を使う。
3. Where オプションがリスト値で区切り文字を持つ, the help module shall 左列で、区切り文字で区切って複数の値を指定できることを示す（`<VALUE>[,<VALUE>...]` の形。`,` の位置には宣言された区切り文字を書く）。
4. Where オプションがリスト値で区切り文字を持たない, the help module shall 左列で、繰り返し指定できることを示す。
5. Where オプションが否定可能なフラグである, the help module shall 左列の長い名前を `--[no-]name` の形で書く。
6. Where オプションがカウントフラグである, the help module shall 左列で、繰り返し指定できることを示す。
7. Where オプションが必須である, the help module shall 説明の後に `(required)` を書く。
8. Where オプションが既定値を持つ, the help module shall 説明の後に `(default: <既定値>)` を書く。
9. Where オプションが環境変数を持つ, the help module shall 説明の後に `[env: <環境変数名>]` を書く。
10. If オプションに説明がなく注記もない, then the help module shall 左列だけを書き、行末に空白を残さない。
11. If オプションに説明がなく注記がある, then the help module shall 注記を説明の開始位置から書き、注記の前に余分な空白を置かない。
12. The help module shall 複数の注記を持つオプションについて、注記を設計で定めた固定の順で、半角空白 1 個で区切って書く。

### Requirement 5: Usage 行と位置引数の表記
**Objective:** As a CLI ツールの利用者, I want Usage 行で引数の並べ方と必須・任意・可変長が一目で分かってほしい, so that 何をいくつ渡せばよいかがすぐ分かる

#### Acceptance Criteria
1. The help module shall Usage 行を `Usage: ` で始め、プログラム名（サブコマンドのヘルプではプログラム名とサブコマンド名）を含める。
2. The help module shall Usage 行で、必須の位置引数を `<NAME>`、任意の位置引数を `[<NAME>]`、必須の可変長の位置引数を `<NAME>...`、任意の可変長の位置引数を `[<NAME>...]` の形で、宣言の追加順に書く。
3. When Usage 行だけが要求された, the help module shall ヘルプの先頭と同じ Usage 行 1 行だけを書く。
4. The help module shall Usage 行の生成を、program 全体（サブコマンドあり構成・なし構成）とサブコマンドのどちらに対しても提供する。

### Requirement 6: バージョン行と使い方の案内
**Objective:** As a CLI ツールの利用者, I want バージョンを 1 行で確認し、エラーの後に詳しい使い方の求め方を知りたい, so that 報告や問い合わせに版を添えられ、誤りを直す手がかりが得られる

#### Acceptance Criteria
1. When バージョン行が要求された, the help module shall `<プログラム名> <バージョン文字列>` の 1 行を書く。
2. If バージョン文字列を持たない program についてバージョン行が要求された, then the help module shall 何も書かず、文面を生成できなかったことを呼び出し側に返す。
3. When サブコマンドを指定せずに使い方の案内が要求された, the help module shall `Run '<プログラム名> --help' for usage.` の 1 行を書く。
4. When サブコマンドを指定して使い方の案内が要求された, the help module shall `Run '<プログラム名> <サブコマンド名> --help' for usage.` の 1 行を書く。

### Requirement 7: 使い方エラーの文面
**Objective:** As a CLI ツールの利用者, I want 指定の誤りを、どの引数のどこが悪いのか分かる 1 行の英語で知りたい, so that 何を直せばよいかをすぐに判断できる

#### Acceptance Criteria
1. When 状態が使い方エラーの結果について文面が要求された, the help module shall エラー種別ごとに定めた英語の 1 行を書く。
2. When エラー種別が「不明なコマンド」である, the help module shall 該当するトークンを含む文面を書く。
3. When エラー種別が「不明なオプション」で、長い形式のトークンである, the help module shall トークンのうち `=` より前の部分（`--name`）を含む文面を書き、`=` 以降の値を含めない。
4. When エラー種別が「不明なオプション」で、短いオプションのまとめ書きの中の 1 文字である, the help module shall その 1 文字を `-c` の形で含む文面を書く。
5. When エラー種別が「値がない」である, the help module shall 利用者が書いた形のオプション名（`--name` または `-c`）を含む文面を書く。
6. When エラー種別が「値を取らないオプションに値が与えられた」である, the help module shall 該当するオプションの長い名前（`--name`）を含む文面を書く。該当するオプションが予約されたオプション（`--help`、`--version`）であれば、利用者が書いたトークンのうち `=` より前の部分を含める。
7. When エラー種別が「必須オプションがない」である, the help module shall 該当するオプションの長い名前（`--name`。長い名前がなければ短い名前 `-c`）を含む文面を書く。
8. When エラー種別が「必須引数がない」である, the help module shall 該当する位置引数の名前（`<NAME>` の形）を含む文面を書く。
9. When エラー種別が「余分な引数」である, the help module shall 該当するトークンを含む文面を書く。
10. The help module shall 使い方エラーの文面に、使い方の案内（`Run '...' for usage.`）を含めない。案内は Requirement 6 の関数で別に書く。

### Requirement 8: 宣言エラーとメモリ不足の文面
**Objective:** As a CLI ツールの開発者, I want 宣言の誤りやメモリ不足も、使い方エラーと同じ関数で 1 行の英語にしたい, so that 開発中に宣言の誤りの原因を突き止められ、どの状態でも同じ方法でエラーを表示できる

#### Acceptance Criteria
1. When 状態が宣言エラーの結果について文面が要求された, the help module shall 宣言エラーであることと、エラー種別ごとに定めた理由を含む英語の 1 行を書く。
2. Where 宣言エラーに該当する command、option、または positional がある, the help module shall その名前（サブコマンド名、`--name` または `-c`、`<NAME>`）を文面に含める。
3. If 宣言エラーの該当する command、option、または positional が名前を持たない（名前の誤りそのものが宣言エラーである場合を含む）, then the help module shall 名前の部分を省いた文面を書き、文面の生成を失敗させない。
4. When 状態がメモリ不足の結果について文面が要求された, the help module shall メモリ不足であることを示す英語の 1 行を書く。
5. If 状態がエラー（使い方エラー、宣言エラー、メモリ不足）でない結果について文面が要求された, then the help module shall 何も書かず、文面を生成できなかったことを呼び出し側に返す。

### Requirement 9: エラー文面の接頭辞の選択
**Objective:** As a CLI ツールの開発者, I want エラー文面の接頭辞の有無を選びたい, so that 自分のツールの書式（`tool: error: ...`、ログへの転送、独自の装飾）に合わせるために文字列を加工しなくて済む

#### Acceptance Criteria
1. The help module shall エラー文面の接頭辞の選択を、独立に組み合わせられるビットフラグで受け取る。選択肢は少なくとも「プログラム名の接頭辞」と「`error: ` の接頭辞」の 2 つとする。
2. Where プログラム名の接頭辞が選択され、エラーに該当するサブコマンドがある, the help module shall 文面を `<プログラム名> <サブコマンド名>: ` で始める。
3. Where プログラム名の接頭辞が選択され、エラーに該当するサブコマンドがない, the help module shall 文面を `<プログラム名>: ` で始める。
4. Where `error: ` の接頭辞が選択された, the help module shall 本文の直前（プログラム名の接頭辞も選択されていればその後ろ）に `error: ` を書く。
5. Where どの接頭辞も選択されていない, the help module shall 本文だけを書く。
6. If 定義されていないビットがフラグに含まれている, then the help module shall そのビットを無視し、定義済みのビットだけに従って文面を書く。

### Requirement 10: 桁揃えと文面の決定性
**Objective:** As a CLI ツールの開発者, I want ヘルプの列が揃い、同じ宣言からは常に同じバイト列が出てほしい, so that 読みやすいヘルプを配布でき、ヘルプの文面をテストでバイト単位に検査できる

#### Acceptance Criteria
1. The help module shall 各一覧（`Commands:`、`Arguments:`、`Options:`、`Global Options:`）の行を 2 桁の字下げで始め、一覧ごとに説明の開始位置を「字下げ + その一覧の左列の最大幅 + 2 桁」に揃える。
2. The help module shall 左列の幅を UTF-8 のコードポイント数で数える。
3. If 左列に UTF-8 として正しくないバイト列が含まれている, then the help module shall そのバイトを 1 桁として数え、文面の生成を失敗させない。
4. The help module shall 改行を LF（`\n`）だけで書き、すべての文面（ヘルプ、Usage 行、バージョン行、使い方の案内、エラー文面）を改行で終える。
5. The help module shall 段落の間に空行を 1 行だけ置き、どの行の末尾にも空白を書かない。
6. The help module shall 端末の幅、ロケール、環境変数、実行環境によらず、同じ宣言と同じ指定からは同じバイト列を書く。
7. The help module shall 宣言と結果の文字列（名前、説明、要約、既定値、トークンなど）を、折り返し・切り詰め・エスケープをせずにそのまま書く。

### Requirement 11: 出力先と書き込みの失敗
**Objective:** As a CLI ツールの開発者, I want 文面を任意の出力先へ書き、書き込みの失敗を知りたい, so that stdout / stderr、ログ、GUI、テスト用のバッファのどれにでも同じ関数で出せ、出力の失敗を終了コードに反映できる

#### Acceptance Criteria
1. The help module shall すべての文面を、呼び出し側が渡す書き込みコールバックと、その文脈（呼び出し側が所有する値）へ書く。
2. The help module shall 標準出力・標準エラー出力・ファイルに直接書かず、`FILE*` を引数に取らない。
3. When 利用側が `FILE*` へ文面を出したい, the help module shall project-foundation が公開ヘッダで提供する `FILE*` 用のコールバックと `FILE*` を渡すだけで出力できるようにする。
4. If 書き込みコールバックが失敗を返した, then the help module shall それ以降の書き込みを行わずに、書き込みに失敗したことを呼び出し側に返す。
5. When 文面をすべて書き終えた, the help module shall 成功を呼び出し側に返し、成功・書き込みの失敗・文面を生成できなかった（Requirement 6.2、8.5、11.6）の 3 つを呼び出し側が区別できるようにする。
6. If 宣言エラーまたはメモリ不足が記録された program についてヘルプ、Usage 行、バージョン行、または使い方の案内が要求された, then the help module shall 何も書かず、文面を生成できなかったことを呼び出し側に返す。
7. The help module shall 文面の生成で、宣言（program とその配下のハンドル）と結果ハンドルの内容を変更しない。
8. The help module shall 公開関数の引数と戻り値を、不透明ハンドル、関数ポインタ、整数（列挙・ビットフラグを含む）、文脈のポインタだけで構成し、構造体を値で受け渡さない。

### Requirement 12: 網羅性とテスト
**Objective:** As a CLOpts の保守者, I want エラー種別の追加に文面の追従漏れがあればテストで気づきたい, so that 新しい種別のエラーが空の文面や誤った文面で利用者に届くことを防げる

#### Acceptance Criteria
1. The help module shall core-parser が定義する使い方エラーと宣言エラーのすべての種別について、それぞれ固有の文面を持つ。
2. If 文面を持たない使い方エラーまたは宣言エラーの種別が列挙に追加された, then the help module のテスト shall 自動テストの失敗としてそれを検出する。
3. The help module のテスト shall 全体ヘルプ、サブコマンドのヘルプ、サブコマンドなし構成のヘルプ、Usage 行、バージョン行、使い方の案内、全エラー種別の文面について、期待するバイト列との完全一致を検査する。
4. The help module のテスト shall Requirement 4 の各表記（短い名前だけ、長い名前だけ、値、リスト値、否定可能、カウント、必須、既定値、環境変数、説明なし）と、非 ASCII の名前を含む一覧の桁揃えを、それぞれ少なくとも 1 つの架空の宣言で検査する。
5. The help module のテスト shall 書き込みコールバックが途中で失敗する場合に、失敗が返り、失敗後にコールバックが呼ばれないことを検査する。
