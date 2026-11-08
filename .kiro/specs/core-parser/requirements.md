# Requirements Document

## Introduction

core-parser は、CLOpts の中核となる解析機能を定める spec である。利用側はビルダー API で宣言を組み立てる。`clopts_program_new` から始め、サブコマンド・オプション・位置引数を関数で追加し、属性は設定関数で付ける。そのうえで解析関数を 1 回呼ぶと、状態・値・構造化エラーを読める結果ハンドルが返る。本 spec が定めるものは 2 つある。1 つは、他 spec が依存する契約で、公開ヘッダに置く不透明ハンドル型・状態とエラー種別の列挙・コールバック型・公開関数である。もう 1 つは挙動で、ビルダー・宣言検査・走査・値の解決・必須検査・エラー優先度を扱う。

2026-11-08 の改訂で、初版の `static const` 宣言テーブル方式を、ビルダー API と不透明ハンドルの方式に置き換えた。目的は、共有ライブラリとして提供し、1.0.0 以降に ABI 後方互換を保つことである（roadmap.md の Approach Decision）。初版の解析規則とテストの観点は引き継ぎ、API の形だけを置き換える。0.x の間は API / ABI を壊してよいので、初版の公開 API との互換は求めない。

出発点は Monde の内部モジュール `cli_args` だが、汎用ライブラリとしての歪みは取り除く。取り除くのは、整形済み文字列で返るエラー、文字列キーの線形探索、宣言と別に持つディスパッチ表、サブコマンド必須、`--no-x` / カウント / 環境変数フォールバックの欠落、foundation 型への依存である。

本書での用語:
- **ハンドル**: ライブラリが確保し、中身を公開しないオブジェクトへのポインタ。program / command / option / positional / result の 5 種類がある。
- **ビルダー呼び出し**: program の生成、宣言の追加、属性の設定を行う公開関数の呼び出し。
- **使い方エラー**: コマンドライン（argv）の内容に起因する誤り。ツールの利用者が直すもの。
- **宣言エラー**: ビルダー呼び出しで組み立てた宣言の内容に起因する誤り。ツールの開発者が直すもの。
- **出所**: 値がどこから来たか（明示指定 / 環境変数 / 既定値 / 未指定）。
- **グローバルオプション**: サブコマンドあり構成で program に直接宣言されたオプション。
- **予約名**: ライブラリが固定で解釈する名前。`--help`、`-h`、`help`（サブコマンド名）、および program がバージョン文字列を持つ場合の `--version`。
- **契約違反**: 利用側のプログラムの誤り（別の program のハンドルを渡すなど）。デバッグビルドで検出し、戻り値では報告しない。

## Boundary Context

- **In scope**:
  - 公開ヘッダの不透明ハンドル型、状態・使い方エラー・宣言エラー・出所・種類の列挙、環境変数の取得コールバック型、handler の関数ポインタ型。
  - ビルダー関数（program / command / option（FLAG / COUNT / VALUE / LIST）/ positional の追加、属性の設定、handler / userdata の設定、バージョン文字列の設定）。
  - ビルダーの失敗の扱い（最初のエラーの記録、以降の呼び出しは何もしない、NULL を渡しても安全）。
  - 宣言検査、解析関数、結果ハンドル（状態、選ばれたコマンド、値、出所、位置引数、構造化エラー）とアクセサ。
  - 走査器、環境変数フォールバックの解決順、必須検査、ハンドラ呼び出し、メモリの所有と解放。
  - help-and-messages 向けの内部の宣言モデルの読み出し口。
  - `tests/parser/` の単体テスト。初版の API を使っている統合テストの利用側コードを、新しい API に移すこと。
- **Out of scope**:
  - ヘルプ・使い方・バージョン・エラーの文面生成（help-and-messages）。
  - Windows での argv と環境変数の UTF-8 取得（platform-argv）。
  - 公開シンボル一覧の管理と ABI の検査（abi-compat）。
  - 利用者向け文書とサンプル（docs-and-release）。
  - 宣言を読み出す公開 API（イントロスペクション。名前や説明文をハンドルから読む公開関数を含む）。
  - 値の型変換・妥当性検査（整数・選択肢・範囲）、長いオプションの前方一致、未知のオプションに対する候補提示。
  - 入れ子のサブコマンド、オプションのグループ化・排他・依存、隠しオプション・別名。
  - 設定ファイル、レスポンスファイル。
  - 初版の `static const` 宣言型と、その解析関数との互換。
- **Adjacent expectations**:
  - project-foundation（改訂後）が提供するものの上に実装する。ビルド・テスト基盤、内部ユーティリティ（可変長配列、サイズ計算のオーバーフロー検査）、`CLOPTS_API`、静的・共有の両形式のビルドがそれに当たる。
  - help-and-messages は、公開ヘッダの型と、本 spec が用意する内部の宣言モデルの読み出し口に依存して文面を生成する。読み出し口は公開しない。エラー種別の列挙に値を足したら、help-and-messages の文面表がそれに追従する前提である。
  - platform-argv は、本 spec が定めるシグネチャの環境変数取得コールバックとして、UTF-8 の実装を提供する。platform-argv の完了前も、本 spec は C 標準の環境変数取得と同等の既定の取得で動き、テストできる。
  - abi-compat は、本 spec が足す公開関数を公開シンボル一覧に載せて検査する。新しい公開関数を足すたびに一覧の更新が要る。
  - Monde の `src/cli_args/` と spec `cli-args`（参照用 submodule `ref/monde`）は参照元であり、API 互換は求めない。
- **初版（2026-11-07）からの意図的な差分**:
  - 宣言は `static const` の構造体配列ではなく、ビルダー関数で組み立てる。ハンドルの中身は公開しない。
  - 宣言に渡した文字列（名前、説明文、値名、環境変数名、既定値、バージョン文字列）はライブラリが複製する。利用側はビルダー呼び出しの後、文字列の寿命を気にしなくてよい。実行時に決まるバージョン文字列も、複製で扱える。
  - 宣言エラーはビルダー呼び出しの順で最初の 1 件を記録する（初版は宣言テーブルの先頭からの検査順）。
  - 結果はライブラリが確保する不透明な結果ハンドルで返す。エラーの各項目はアクセサで読む。
  - 環境変数の取得コールバックは、借用した文字列を返す形にする。ライブラリはそれを複製し、コールバックが確保したメモリを解放しない。
  - サブコマンドあり構成の program に位置引数や handler を宣言することは、宣言エラーにする（初版は黙って無視していた）。
- **Monde からの意図的な差分**（初版から引き継ぐもの）:
  - サブコマンドなし構成と、グローバルオプション（サブコマンドの前後どちらでも可）を持つ。Monde はサブコマンド必須で、サブコマンドの前に書けるのは `--help` / `-h` / `--version` だけだった。
  - 位置引数は、個数の範囲（min / max）ではなく、位置引数ごとの必須・可変長で宣言する。
  - リスト値の区切り文字は宣言で選ぶ。区切りで生じた空の項目はエラーにせず、項目として保持する。
  - バージョン文字列は省略でき、省略したときは `--version` を予約しない。
  - 出所は「明示 / 環境変数 / 既定値 / 未指定」の 4 区分。
  - 宣言エラーは最初の 1 件だけを返す。
  - 次の規則を引き継ぐ:
    - 走査規則（`--name=value` の `=` の後ろ全体が値、値トークンは `-` 始まりでも消費、`-ov` の残りは値、`--` と `-` の扱い）。
    - 最初の使い方エラーを保留し、残りのトークンから完全一致の `--help` / `-h` を探す優先規則。
    - `help <cmd>` の余分な引数の扱い。
    - 宣言検査の項目。

## Requirements

### Requirement 1: ビルダーによる宣言

**Objective:** As a CLI ツールの開発者, I want サブコマンド・オプション・位置引数を関数呼び出しで宣言したい, so that 構造体の配置や配列の要素数を意識せずに宣言でき、解析・ヘルプ・エラーが 1 か所の宣言から決まる

#### Acceptance Criteria

1. The parser shall 名前を受け取って program のハンドルを生成する関数と、program とそこから派生したすべてのハンドルをまとめて解放する関数を提供する。
2. The parser shall program に名前を付けてサブコマンドを追加し、command のハンドルを返す関数を提供する。
3. The parser shall FLAG / COUNT / VALUE / LIST の種類ごとにオプションを追加する関数を提供する。各関数は、長い名前（なしも可）と短い名前（1 文字、なしも可）を受け取り、option のハンドルを返す。
4. The parser shall オプションと位置引数の追加先として program と command のどちらも受け付ける。program に追加したオプションは、サブコマンドなし構成では program のオプション、サブコマンドあり構成ではグローバルオプションとして扱う。
5. The parser shall 名前を受け取って位置引数を追加し、positional のハンドルを返す関数を提供する。
6. The parser shall option の属性を設定する関数を提供する。対象の属性は、説明文、値名、環境変数名、既定値、必須、否定可能、リスト値の区切り文字、カウントの上限である。
7. The parser shall positional の属性（説明文、必須、可変長）、command の属性（説明文、handler、userdata）、program の属性（説明文、バージョン文字列、handler、userdata）を設定する関数を提供する。
8. The parser shall ビルダー関数に渡された文字列をすべて呼び出しの中で複製し、呼び出しが返った後は利用側の文字列を参照しない。
9. When 同じ属性を同じハンドルに 2 回以上設定した, the parser shall 最後に設定した値を採用する。
10. The parser shall サブコマンド・オプション・位置引数を追加した順に保持し、解析・宣言検査・内部の宣言モデルの読み出しでその順序を使う。
11. The parser shall ビルダー関数が返したハンドルを、program を解放するまで有効なまま保つ。後からほかの宣言を追加しても、既存のハンドルは無効にならない。
12. While program が解析または宣言検査に一度でも使われた, the parser shall その program に対するビルダー呼び出しを契約違反として扱い、デバッグビルドで検出する。リリースビルドでは宣言を変えずに何もしない（ハンドルを返す関数は NULL を返す）。

### Requirement 2: ビルダーの失敗の扱い

**Objective:** As a CLI ツールの開発者, I want ビルダー呼び出しのたびに戻り値を検査しなくても安全に宣言を組み立てたい, so that 宣言のコードが短くなり、誤りやメモリ不足を解析時にまとめて知ることができる

#### Acceptance Criteria

1. If ビルダー呼び出しで宣言エラーが見つかった、またはメモリ確保に失敗した, then the parser shall その最初のエラーを program に記録する。
2. While program にエラーが記録されている, the parser shall その program に対する以降のビルダー呼び出しで宣言を変えず、ハンドルを返す関数は NULL を返す。
3. When ハンドルを受け取るビルダー関数に NULL が渡された, the parser shall 何もせず、ハンドルを返す関数であれば NULL を返す。
4. If program の生成でメモリ確保に失敗した, then the parser shall NULL を返す。
5. When NULL の program が解析関数または宣言検査関数に渡された, the parser shall 状態 OUT_OF_MEMORY を返す。
6. When エラーが記録された program が解析関数または宣言検査関数に渡された, the parser shall argv の内容にかかわらず、記録されたエラーに応じた状態（宣言エラーなら DECL_ERROR、メモリ不足なら OUT_OF_MEMORY）を返す。
7. When program を解放する関数に NULL が渡された, the parser shall 何もしない。
8. If ビルダー関数に、別の program から派生したハンドルが渡された, then the parser shall これを契約違反としてデバッグビルドで検出する。

### Requirement 3: 宣言検査

**Objective:** As a CLI ツールの開発者, I want 宣言の誤りを使い方エラーとは別の宣言エラーとして検出したい, so that 開発中のミスをすぐに特定でき、ツールの利用者に開発者の誤りが使い方の誤りとして見えることがない

#### Acceptance Criteria

1. When 解析関数が呼ばれた, the parser shall argv の走査に先立って宣言全体を検査し、宣言エラーがあれば argv の内容にかかわらず宣言エラーを返す。
2. The parser shall argv なしで宣言だけを検査する関数を提供する。この関数は、状態と、宣言エラーの内容を読めるエラー情報を返す。
3. If 同じスコープ（program のオプション、または 1 つの command）の中で、またはグローバルオプションと command のオプションの間で、長い名前か短い名前が重複している, then the parser shall 宣言エラー「オプション名の重複」を返す。
4. If 同じ program の中でサブコマンド名が重複している, then the parser shall 宣言エラー「コマンド名の重複」を返す。
5. If オプション名またはサブコマンド名が予約名と衝突している, then the parser shall 宣言エラー「予約名との衝突」を返す。`--version` との衝突は、program がバージョン文字列を持つ場合に限る。
6. If option が長い名前も短い名前も持たない, then the parser shall 宣言エラー「名前がない」を返す。
7. If 名前の形式が不正である, then the parser shall 宣言エラー「名前の形式が不正」を返す。不正とするのは次の場合である: 長い名前が空、または `=`、空白、先頭の `-` を含む。短い名前が `-` または空白である。環境変数名が空文字列である。program・command・positional の名前が空または NULL である。
8. If FLAG 以外の種類の option が否定可能に設定されている, then the parser shall 宣言エラー「否定できない種類」を返す。
9. If 否定可能な FLAG `x` と同じスコープに、長い名前 `no-x` の option が宣言されている, then the parser shall 宣言エラー「否定形との衝突」を返す。
10. If FLAG または COUNT の option に既定値が設定されている, then the parser shall 宣言エラー「既定値を持てない種類」を返す。
11. If FLAG または COUNT の option に環境変数名が設定されている, then the parser shall 宣言エラー「環境変数を持てない種類」を返す。
12. If option が必須に設定され、既定値も設定されている, then the parser shall 宣言エラー「必須と既定値の同時指定」を返す。
13. If FLAG または COUNT の option が必須に設定されている, then the parser shall 宣言エラー「必須にできない種類」を返す。
14. If 同じスコープの位置引数で、任意の位置引数の後ろに必須の位置引数がある, then the parser shall 宣言エラー「位置引数の順序が不正」を返す。
15. If 可変長の位置引数がスコープの末尾以外にある、または可変長の位置引数が 2 つ以上ある, then the parser shall 宣言エラー「可変長位置引数の位置が不正」を返す。
16. If 区切り文字が LIST 以外の option に設定されている、またはカウントの上限が COUNT 以外の option に設定されている, then the parser shall 宣言エラー「種類に合わない属性」を返す。
17. If サブコマンドを持つ program に位置引数または handler が宣言されている, then the parser shall 宣言エラー「構成の混在」を返す。
18. When 宣言エラーを返す, the parser shall エラー情報から次のものを読めるようにする: エラー種別、該当する command のハンドル（program 直下なら NULL）、該当する option または positional のハンドル（該当があれば）。
19. If 宣言エラーが複数ある, then the parser shall ビルダー呼び出しの順で最初に成立したものだけを返す。後のビルダー呼び出しによって初めて成立する誤り（たとえば、後から設定したバージョン文字列との衝突）は、その時点で成立したものとして順序を決める。

### Requirement 4: 解析の起動と状態

**Objective:** As a CLI ツールの開発者, I want 解析関数を 1 回呼ぶだけで結果を受け取りたい, so that 表示と終了コードを自分で決めながら、解析の流れを短いコードで書ける

#### Acceptance Criteria

1. The parser shall program、`argc`、`argv` を受け取り、ライブラリが確保した結果ハンドルを返す解析関数を提供する。解析関数は `argv[0]` をプログラム名として扱い、`argv[1]` から走査する。
2. The parser shall 結果ハンドルから、OK、HELP、VERSION、NO_COMMAND、USAGE_ERROR、DECL_ERROR、OUT_OF_MEMORY の 7 つの状態のいずれかを読めるようにする。
3. The parser shall 標準出力・標準エラー出力・その他のストリームへ一切書き込まず、プロセスを終了させない。
4. When 解析が状態 OK で終わった, the parser shall 結果ハンドルから次のものを参照できるようにする: 選択されたサブコマンド（サブコマンドなし構成では NULL）、各オプションの値と出所、位置引数の列。
5. When 解析が HELP を返す, the parser shall ヘルプの対象（program 全体か、特定の command か）を結果ハンドルから参照できるようにする。
6. While program がサブコマンドあり構成である, when argv にサブコマンド名が現れずに走査が終わった（引数なし、またはグローバルオプションだけ）, the parser shall 状態 NO_COMMAND を返し、走査済みのグローバルオプションの値を結果ハンドルから参照できるようにする。
7. If 解析の途中でメモリ確保に失敗した, then the parser shall 状態 OUT_OF_MEMORY の結果ハンドルを返す。
8. If 結果ハンドル自体の確保に失敗した, then the parser shall NULL ではなく、状態 OUT_OF_MEMORY を読める結果ハンドルを返す。このハンドルは解放関数に渡しても安全なものにする。
9. If `argc` が 0 である, then the parser shall 引数なしとして扱う。サブコマンドあり構成では NO_COMMAND を、サブコマンドなし構成では必須検査の結果に応じた状態を返す。
10. The parser shall 同じ program を何度でも解析に使えるようにし、各解析の結果ハンドルを互いに独立させる。
11. The parser shall program と argv を読み取り専用として扱い、解析の前後でその内容を変更しない。

### Requirement 5: サブコマンドの選択とグローバルオプション

**Objective:** As a CLI ツールの利用者, I want `tool [global options] <command> [options] [args]` の形で呼び出したい, so that 複数のサブコマンドを持つツールを一般的な慣習どおりに使える

#### Acceptance Criteria

1. While program がサブコマンドあり構成である, when `-` で始まらない最初のトークンがサブコマンド名に一致した, the parser shall そのサブコマンドを選択し、以降のトークンをそのサブコマンドの宣言に従って解析する。
2. While program がサブコマンドあり構成である, if `-` で始まらない最初のトークンがどのサブコマンド名にも一致しない, then the parser shall 使い方エラー「不明なコマンド」を返す。
3. While program がサブコマンドあり構成である, when サブコマンド名より前にオプションが現れた, the parser shall それをグローバルオプションとしてだけ照合する。サブコマンド固有のオプション名であっても、使い方エラー「不明なオプション」として扱う。
4. When サブコマンドが選択された後にオプションが現れた, the parser shall そのサブコマンドのオプションとグローバルオプションの両方を照合の対象にする。
5. The parser shall サブコマンド名の照合で大文字小文字を区別し、前方一致による省略を受け付けない。
6. While program がサブコマンドなし構成である, the parser shall `-` で始まらないトークンをすべて位置引数として扱う。

### Requirement 6: 走査規則（長いオプション・短いオプション・終端・位置引数）

**Objective:** As a CLI ツールの利用者, I want 一般的なコマンドライン慣習（`--name value`、`--name=value`、`-o v`、`-ov`、`-abc`、`--`）で引数を書きたい, so that 他のツールと同じ感覚で使える

#### Acceptance Criteria

1. When `--` で始まる 3 文字以上のトークンが現れた, the parser shall `=` の手前（`=` がなければトークン末尾）までを長い名前として照合する。
2. When 長い名前に一致する VALUE または LIST の option に `--name=value` 形式が使われた, the parser shall `=` の後ろ全体（空文字列を含む）を値として採用する。
3. When 長い名前に一致する VALUE または LIST の option に `--name value` 形式が使われた, the parser shall 次のトークンを、その内容にかかわらずそのまま値として消費する。`-` で始まるトークンや `--` も値になる。
4. If VALUE または LIST の option が値を要するのに次のトークンがない, then the parser shall 使い方エラー「値がない」を返す。
5. If FLAG または COUNT の option に `--name=value` 形式で値が与えられた, then the parser shall 使い方エラー「値を取らないオプションに値が与えられた」を返す。
6. If 長い名前がどの option にも一致しない, then the parser shall 使い方エラー「不明なオプション」を返す。
7. The parser shall 長いオプション名の照合で大文字小文字を区別し、前方一致による省略を受け付けない。
8. When `-` で始まり `--` では始まらない 2 文字以上のトークンが現れた, the parser shall 2 文字目以降の各文字を短い名前として順に照合する（まとめ書き）。
9. When まとめ書きの途中の文字が VALUE または LIST の option に一致した, the parser shall その文字より後ろの残りの文字列が空でなければそれを値として採用する。残りがなければ、次のトークンをそのまま値として消費する。
10. The parser shall `-o=v` のような短いオプションの残り文字列を、`=` を含めてそのまま値として扱う（`=v` が値になる）。
11. If まとめ書きの中の文字がどの option の短い名前にも一致しない, then the parser shall 使い方エラー「不明なオプション」を返し、該当する 1 文字を特定できる情報を含める。
12. When トークン `--` が現れた, the parser shall それ以降のすべてのトークンを、その内容にかかわらず位置引数として扱う。
13. The parser shall トークン `-` 単体を位置引数として扱う。
14. When サブコマンド選択後（またはサブコマンドなし構成で）`-` で始まらないトークンが現れた, the parser shall それを宣言された位置引数の順に割り当てる。オプションと位置引数が交互に並んでいても受け付ける。
15. If 宣言された位置引数をすべて割り当てた後にさらに位置引数が現れ、末尾の位置引数が可変長でない, then the parser shall 使い方エラー「余分な引数」を返す。

### Requirement 7: 値の種類と繰り返しの規則

**Objective:** As a CLI ツールの開発者, I want フラグ・カウント・単一値・リスト値の 4 種類と `--no-<name>` による否定を宣言だけで使いたい, so that よくあるオプションの形を自前の後処理なしに表現できる

#### Acceptance Criteria

1. When FLAG の option が指定された, the parser shall その値を真とする。同じ FLAG が繰り返し指定されても使い方エラーにしない。
2. When 否定可能な FLAG `x` に対して `--no-x` が指定された, the parser shall その値を偽とし、出所を明示指定として記録する。
3. When 同じ FLAG に対して `--x` と `--no-x` が混在して指定された, the parser shall argv 上で最後に現れたものを採用する。
4. If 否定可能でない FLAG、または FLAG 以外の option に対して `--no-<name>` が指定された, then the parser shall 使い方エラー「不明なオプション」を返す。
5. If `--no-x=value` の形で否定形に値が与えられた, then the parser shall 使い方エラー「値を取らないオプションに値が与えられた」を返す。
6. When COUNT の option が指定された, the parser shall 指定された回数を値とする。`-vvv` のまとめ書き、`-v -v`、`--verbose --verbose` のいずれも 1 回ずつ数える。
7. If COUNT の指定回数が上限を超えた, then the parser shall 値を上限にとどめ（クランプし）、使い方エラーにしない。上限は、宣言で設定した値、設定がなければライブラリ既定の値とする。
8. When VALUE の option が複数回指定された, the parser shall argv 上で最後に指定された値を採用する（後勝ち）。
9. When LIST の option が複数回指定された, the parser shall 指定された順にすべての値を 1 つのリストとして連結する。
10. Where LIST の option に区切り文字が設定されている, the parser shall 1 回の指定の値をその区切り文字で分割し、分割で生じた空の項目も項目として保持する。
11. Where LIST の option に区切り文字が設定されていない, the parser shall 1 回の指定の値をそのまま 1 項目として扱う。
12. The parser shall 値を文字列（および FLAG の真偽、COUNT の回数）として返し、整数変換・選択肢・範囲などの妥当性検査を行わない。

### Requirement 8: 既定値・環境変数フォールバック・出所

**Objective:** As a CLI ツールの開発者, I want 明示指定 > 環境変数 > 既定値の順で値を解決し、各値の出所を知りたい, so that 設定の由来に応じた挙動（例: 環境変数由来なら警告する）を利用側で決められる

#### Acceptance Criteria

1. When VALUE または LIST の option が argv で指定された, the parser shall その値を採用し、出所を「明示指定」とする。
2. While VALUE または LIST の option が argv で指定されていない, when その option に環境変数名が設定されており、その環境変数が設定されている（空文字列を含む）, the parser shall 環境変数の値を採用し、出所を「環境変数」とする。
3. While VALUE または LIST の option が argv でも環境変数でも与えられていない, when その option に既定値が設定されている, the parser shall 既定値を採用し、出所を「既定値」とする。
4. While option が argv・環境変数・既定値のいずれからも値を得ていない, the parser shall 出所を「未指定」とする。値は、VALUE なら NULL、LIST なら空のリスト、FLAG なら偽、COUNT なら 0 とする。
5. When LIST の option の値が環境変数または既定値から採用された, the parser shall 明示指定と同じ規則で、設定された区切り文字で分割する（区切り文字がなければ 1 項目とする）。
6. The parser shall 環境変数の取得コールバックと、その文脈ポインタを解析ごとに渡せるようにする。渡されない場合は、C 標準の環境変数取得と同等に動作する。
7. The parser shall 取得コールバックに、環境変数が設定されている・設定されていない・取得に失敗した、の 3 つを区別して報告させる。設定されている場合、コールバックはコールバック側が所有する文字列を貸す（借用）。
8. When 取得コールバックが文字列を貸した, the parser shall コールバックから戻った直後、次にコールバックを呼ぶ前にその文字列を複製する。以降はコールバックの文字列を参照せず、解放もしない。
9. The parser shall 環境変数の参照を、該当する option が属するスコープ（選択されたサブコマンドとグローバル、またはサブコマンドなし構成の program）に限る。選択されなかったサブコマンドの option の環境変数は参照しない。
10. If 取得コールバックが取得の失敗を報告した, then the parser shall 状態 OUT_OF_MEMORY を返し、黙って既定値にフォールバックしない。

### Requirement 9: 必須検査と位置引数の個数制約

**Objective:** As a CLI ツールの利用者, I want 必須のオプションや引数が足りないときに何が足りないかを特定したエラーを受け取りたい, so that 指定を直してやり直せる

#### Acceptance Criteria

1. When 走査が完了し、HELP / VERSION / 使い方エラーのいずれも発生していない, the parser shall 選択されたスコープの必須オプションと必須位置引数を検査する。スコープは、グローバルと選択されたサブコマンド、またはサブコマンドなし構成の program である。
2. If 必須の VALUE または LIST の option が、明示指定からも環境変数からも値を得ていない, then the parser shall 使い方エラー「必須オプションがない」を返し、該当する option のハンドルを含める。
3. If 必須の位置引数に対応するトークンがない, then the parser shall 使い方エラー「必須引数がない」を返し、該当する positional のハンドルを含める。
4. When 末尾の位置引数が可変長かつ必須である, the parser shall 1 個以上のトークンを要求する。可変長かつ任意であれば、0 個以上を受け付ける。
5. When 位置引数の個数が宣言の範囲内である, the parser shall 各位置引数にトークンを宣言順に対応づけ、可変長の位置引数には残りのトークンすべてを対応づける。
6. While 状態が NO_COMMAND である, the parser shall サブコマンドの必須検査を行わず、グローバルオプションの必須検査だけを行う。
7. The parser shall 必須検査を宣言順（グローバルオプション、サブコマンドのオプション、位置引数の順）に行い、最初に見つかった不足だけを報告する。

### Requirement 10: ヘルプ・バージョンの検出とエラー優先度

**Objective:** As a CLI ツールの利用者, I want 他の指定が間違っていても `--help` を付ければヘルプが出てほしい, so that 使い方を確認してから直せる

#### Acceptance Criteria

1. When argv のどこかに `--help` または `-h` が現れた, the parser shall 状態 HELP を返す。サブコマンドが選択済みならそのサブコマンドを、未選択なら program 全体をヘルプの対象とする。サブコマンドの前後は問わず、まとめ書きの中の `h` も含む。ただし、`--` より後ろのトークンと、他のオプションの値として消費されたトークンは除く。
2. While program がサブコマンドあり構成である, when `-` で始まらない最初のトークンが `help` である, the parser shall 状態 HELP を返す。次のトークンが既存のサブコマンド名ならそれを、次のトークンがなければ program 全体をヘルプの対象とする。
3. While program がサブコマンドあり構成である, if `help` の次のトークンがどのサブコマンド名にも一致しない, then the parser shall 使い方エラー「不明なコマンド」を返す。
4. While program がサブコマンドあり構成である, if `help <cmd>` の後ろにさらにトークンがある, then the parser shall 使い方エラー「余分な引数」を返し、最初の余分なトークンを示す。
5. Where program がバージョン文字列を持つ, when argv のどこかに `--version` が現れた, the parser shall 状態 VERSION を返す。ただし、`--` より後ろのトークンと、他のオプションの値として消費されたトークンは除く。
6. Where program がバージョン文字列を持たない, when `--version` が現れた, the parser shall それを通常の長いオプションとして照合する。宣言になければ、使い方エラー「不明なオプション」として扱う。
7. When 使い方エラーを検出した後にまだトークンが残っている, the parser shall 残りのトークン（`--` より前）から、`--help`、`-h`、（バージョン文字列があれば）`--version` に完全一致するものだけを探す。見つかれば HELP または VERSION を、見つからなければ最初に検出した使い方エラーを返す。
8. If HELP または VERSION の要求と使い方エラーが同じ argv に含まれている, then the parser shall 使い方エラーの位置が要求より前であっても、HELP または VERSION を優先して返す。
9. If HELP の要求と VERSION の要求が同じ argv に含まれている, then the parser shall argv 上で先に現れたほうを返す。
10. The parser shall HELP または VERSION を返す場合に必須検査を行わない。
11. If 宣言エラーがある, then the parser shall HELP / VERSION の要求があっても宣言エラーを優先して返す。
12. If 使い方エラーが複数ある, then the parser shall argv 上で最初に見つけたものだけを報告する。

### Requirement 11: 構造化エラー

**Objective:** As a CLI ツールの開発者, I want エラーを整形済みの文字列ではなく、項目ごとに読める形で受け取りたい, so that 自分の書式・言語・出力先・終了コードでエラーを表示でき、ライブラリが付けた接頭辞を剥がす処理が不要になる

#### Acceptance Criteria

1. The parser shall 使い方エラーの種別を公開ヘッダの列挙として定義する。少なくとも「不明なコマンド」「不明なオプション」「値がない」「値を取らないオプションに値が与えられた」「必須オプションがない」「必須引数がない」「余分な引数」を含める。
2. The parser shall 宣言エラーの種別を公開ヘッダの列挙として定義し、Requirement 3 で挙げたすべての種別を含める。
3. When 使い方エラーを返す, the parser shall 結果ハンドルのアクセサで次の項目を読めるようにする。該当がない項目は、NULL または負の値で表す。
   - 種別
   - 該当する command のハンドル（サブコマンド選択前、または選択なしなら NULL）
   - 該当する option のハンドル
   - 該当する positional のハンドル
   - 該当するトークン
   - 該当する argv の添字
4. When まとめ書きの中の 1 文字で使い方エラーが起きた, the parser shall トークン全体に加えて、該当する短い名前の 1 文字をアクセサで判別できるようにする。
5. The parser shall エラー情報に整形済みの文面を含めず、文面の生成を本 spec の外（help-and-messages）に委ねる。
6. The parser shall 宣言エラーと使い方エラーを状態（DECL_ERROR / USAGE_ERROR）で区別できるようにする。
7. When 状態が USAGE_ERROR / DECL_ERROR 以外の結果ハンドルに対してエラーのアクセサが呼ばれた, the parser shall 種別として「エラーなし」を表す値を返し、ほかの項目は該当なしとして返す。
8. The parser shall エラー情報が返すトークンの文字列が有効な期間を定め、API の説明に書く。

### Requirement 12: 結果の参照 API とハンドラ呼び出し

**Objective:** As a CLI ツールの開発者, I want 値を宣言時に受け取った option のハンドルで参照し、選択されたサブコマンドのハンドラをそのまま呼びたい, so that 名前の打ち間違いが起こらず、並行のディスパッチ表を持たなくて済む

#### Acceptance Criteria

1. The parser shall option の値を、宣言時に返された option のハンドルをキーとして参照する関数群を提供し、文字列キーによる参照を公開しない。
2. The parser shall 次のものを結果ハンドルから参照する手段を提供する: FLAG の真偽、COUNT の回数、VALUE の文字列（未指定なら NULL）、LIST の項目数と各項目、各 option の出所。
3. If 結果ハンドルに対して、解析に使った program に属さない option のハンドルが渡された, then the parser shall これを契約違反としてデバッグビルドで検出して止め、黙って未指定として返すことはしない（リリースビルドでは未指定の値を返す）。
4. The parser shall 選択されたサブコマンドの command ハンドル（サブコマンドなし構成では NULL）と、位置引数の個数および各トークンを、結果ハンドルから参照する手段を提供する。
5. The parser shall 選択されたサブコマンド（サブコマンドなし構成では program）の handler を呼び出す関数を提供する。この関数は、結果ハンドルと宣言で設定した userdata を handler に渡し、handler の戻り値をそのまま返す。
6. If handler を呼び出す関数が、状態 OK 以外の結果ハンドル、または handler が設定されていないスコープに対して呼ばれた, then the parser shall これを契約違反としてデバッグビルドで検出する。
7. While 結果ハンドルが状態 OK 以外である, the parser shall 値の参照関数に対して、未指定のときと同じ値を返す。ただし、NO_COMMAND のときのグローバルオプションの値（Requirement 4.6）は除く。
8. The parser shall 結果ハンドルから得た文字列（値、リストの項目、位置引数のトークン）が有効な期間を定め、API の説明に書く。少なくとも、結果ハンドルを解放するまで、かつ利用側が argv を保持している間は有効とする。

### Requirement 13: メモリの所有と解放

**Objective:** As a CLI ツールの開発者, I want 宣言と結果の所有と解放の規則が単純で、失敗後も安全であってほしい, so that エラー経路でも二重解放やリークを気にせずに済み、共有ライブラリの境界を越えたメモリの受け渡しで問題が起きない

#### Acceptance Criteria

1. The parser shall 結果ハンドルを解放する関数を提供し、解析がどの状態で終わった場合でも呼び出せるようにする。OUT_OF_MEMORY の場合や、Requirement 4.8 の結果ハンドルの場合も含む。
2. When 結果ハンドルを解放する関数に NULL が渡された, the parser shall 何もしない。
3. The parser shall 結果ハンドルと program を、どちらの順で解放しても安全にする。結果ハンドルの参照関数は、program を解放する前にだけ呼べるものとする。
4. The parser shall ライブラリが確保したメモリをライブラリの関数でだけ解放させる。利用側が確保したメモリ（argv、userdata、コールバックが貸した文字列）は、解放もせず、所有もしない。
5. The parser shall 確保するメモリのサイズ計算でオーバーフローを検査する。オーバーフローする場合は確保せず、メモリ不足として扱う。
6. If 解析の途中でメモリ確保に失敗した, then the parser shall それまでに確保したメモリを結果ハンドルに保持したままにして、解放関数で漏れなく解放できるようにする。
7. If ビルダー呼び出しの途中でメモリ確保に失敗した, then the parser shall それまでに確保したメモリを program に保持したままにして、program の解放関数で漏れなく解放できるようにする。

### Requirement 14: 共有ライブラリとしての公開 API

**Objective:** As a CLI ツールの開発者, I want 静的ライブラリでも共有ライブラリでも同じ公開 API を使いたい, so that 形式を選び直しても利用側のコードを変えずに済み、将来の版で共有ライブラリを差し替えられる

#### Acceptance Criteria

1. The parser shall 公開ヘッダにハンドルの構造体の定義を置かず、ハンドル型は中身の見えない型としてだけ公開する。
2. The parser shall 公開する関数のすべてを、静的版と共有版の両方の形式で利用側から呼べるようにする。
3. The parser shall 公開する関数の引数と戻り値に、構造体の値渡しと、C 標準ライブラリのストリーム型を使わない。
4. The parser shall 公開する列挙の各値を、ヘッダに明示した数値で定義する。
5. The parser shall 内部の宣言モデルの読み出し口と内部の関数を、共有版の公開シンボルとして外に出さない。
6. Where CLOpts が共有ライブラリとしてビルドされている, the parser shall 利用側のプログラムが本 spec の公開関数だけを使って、宣言・解析・結果の参照・解放を最後まで行えるようにする。

### Requirement 15: 単体テスト

**Objective:** As a CLOpts の保守者, I want 解析の挙動を架空の宣言で網羅的に検証するテストを持ちたい, so that 走査規則やエラー優先度の変更が既存の挙動を壊したときに検出できる

#### Acceptance Criteria

1. The parser shall 解析機能の単体テストを `tests/parser/` に置き、プロジェクトのテスト基盤から静的版・共有版のどちらのビルドでも実行できるようにする。
2. The parser shall テストの宣言に、架空のプログラム（実在のツールではない）だけを使う。
3. The parser shall 次のそれぞれを、少なくとも 1 件のテストで到達させる: 7 つの状態、使い方エラーの各種別、宣言エラーの各種別。
4. The parser shall サブコマンドあり構成とサブコマンドなし構成の両方について、次の挙動をテストで検証する。
   - 長いオプション、短いオプションのまとめ書き、`--name=value`
   - `--`、`-` 単体、位置引数とオプションの混在
   - `--no-x`、カウントの上限、リストの連結と区切り
   - 既定値、環境変数フォールバックと出所
   - 必須検査、HELP / VERSION の優先度
5. The parser shall 環境変数フォールバックのテストで取得コールバックを差し替え、実際のプロセス環境に依存せずに検証する。
6. The parser shall ビルダーについて、次の挙動をテストで検証する。
   - NULL のハンドルを渡しても安全であること
   - 最初のエラーが記録され、以降の呼び出しが何もしないこと
   - 渡した文字列が複製されること（呼び出し後に利用側の文字列を書き換えても宣言が変わらない）
   - 解析に使った後のビルダー呼び出しが宣言を変えないこと
7. The parser shall メモリ確保の失敗を、ビルダー呼び出し・解析・結果ハンドル自体の確保のそれぞれで再現する。各場合について、状態 OUT_OF_MEMORY が返ること、program と結果ハンドルを漏れなく解放できることをテストで検証する。
8. The parser shall 初版の単体テストが検証していた解析規則の観点を、新しい API のテストでもすべて検証する。
