# Requirements Document

## Introduction

core-parser は、CLOpts の中核となる解析機能を定める spec である。利用側は `static const` の宣言テーブル（program → commands → options / positionals、またはサブコマンドなしの program → options / positionals）を書き、解析関数を 1 回呼ぶだけで、解析結果か構造化エラーを受け取れる。本 spec は、公開ヘッダに置く宣言型・結果型・構造化エラー型・状態の列挙（他 spec が依存する契約）と、宣言検査・走査・値の解決・必須検査・エラー優先度の挙動を定める。

出発点は Monde の内部モジュール `cli_args` だが、汎用ライブラリとしての歪み（整形済み文字列で返るエラー、文字列キーの線形探索、宣言と別に持つディスパッチ表、サブコマンド必須、`--no-x` / カウント / 環境変数フォールバックの欠落、foundation 型への依存）を取り除く。

本書での用語:
- **使い方エラー**: コマンドライン（argv）の内容に起因する誤り。ツールの利用者が直すもの。
- **宣言エラー**: 宣言テーブルの内容に起因する誤り。ツールの開発者が直すもの。
- **出所**: 値がどこから来たか（明示指定 / 環境変数 / 既定値 / 未指定）。
- **グローバルオプション**: サブコマンドあり構成で program に直接宣言されたオプション。
- **予約名**: ライブラリが固定で解釈する名前。`--help`、`-h`、`help`（サブコマンド名）、および program がバージョン文字列を持つ場合の `--version`。

## Boundary Context

- **In scope**: 公開ヘッダの宣言型（program / command / option / positional、種類 FLAG / COUNT / VALUE / LIST、否定可能、環境変数名、必須、既定値、値名、説明文、グローバルオプション、handler / userdata、位置引数の個数制約）、宣言検査、構造化エラー型とエラー種別の列挙、解析状態の列挙、結果ストア（値・出所・位置引数）と参照 API、走査器、環境変数フォールバックの解決順、必須検査、ハンドラ呼び出し、メモリ所有、`tests/` の単体テスト。
- **Out of scope**: ヘルプ・使い方・バージョン・エラーの文面生成（help-and-messages）、Windows での argv と環境変数の UTF-8 取得（platform-argv）、利用者向け文書とサンプル（docs-and-release）、値の型変換・妥当性検査（整数・選択肢・範囲）、長いオプションの前方一致、未知のオプションに対する候補提示、入れ子のサブコマンド、オプションのグループ化・排他・依存、隠しオプション・別名、設定ファイル、レスポンスファイル。
- **Adjacent expectations**:
  - project-foundation が提供するビルド・テスト基盤と内部ユーティリティ（可変長配列、サイズ計算のオーバーフロー検査）の上に実装する。
  - help-and-messages は本 spec の公開ヘッダの型（宣言型・構造化エラー型・結果型・列挙）だけに依存して文面を生成する。エラー種別の列挙に追加があれば help-and-messages の文面表が追従する前提で、本 spec は列挙を公開ヘッダで確定させる。
  - platform-argv は環境変数を UTF-8 で返す取得関数を提供する。本 spec は取得関数を差し替えられる口を持ち、platform-argv の完了前は `getenv` 相当で動作・テストできる。
  - Monde の `src/cli_args/` と spec `cli-args` は参照元であり、API 互換は求めない。

## Requirements

### Requirement 1: 宣言テーブル

**Objective:** As a CLI ツールの開発者, I want サブコマンド・オプション・位置引数を `static const` の宣言テーブルで書きたい, so that 解析・ヘルプ・エラーが 1 か所の宣言から決まり、実行時の登録処理や並行のディスパッチ表を持たなくて済む

#### Acceptance Criteria

1. The parser shall program、command、option、positional の 4 つの宣言型を公開ヘッダで提供し、利用側がそれらを `static const` の初期化子だけで記述できるようにする。
2. The parser shall program 宣言に、名前、説明文、バージョン文字列へのポインタ（NULL 可）、グローバルオプションの配列、サブコマンドの配列を持たせる。
3. Where program がサブコマンドを 1 つも持たない（サブコマンドなし構成）, the parser shall program 宣言に直接オプションの配列、位置引数の配列、handler と userdata を持たせ、それらを解析の対象にする。
4. Where program がサブコマンドを 1 つ以上持つ（サブコマンドあり構成）, the parser shall program 直下の位置引数を解析の対象にせず、サブコマンド名の後ろの位置引数だけを各 command の宣言に従って解析する。
5. The parser shall command 宣言に、名前、説明文、オプションの配列、位置引数の配列、handler（関数ポインタ、NULL 可）、userdata（`void *`）を持たせる。
6. The parser shall option 宣言に、長い名前（NULL 可）、短い名前（1 文字、省略可）、種類（FLAG / COUNT / VALUE / LIST）、否定可能フラグ、環境変数名（NULL 可）、必須フラグ、既定値（NULL 可）、値名、説明文、リスト値の区切り文字（LIST 用、省略可）、カウントの上限（COUNT 用、省略可）を持たせる。
7. The parser shall positional 宣言に、名前、説明文、必須フラグ、可変長フラグを持たせる。
8. The parser shall 宣言テーブルと argv を読み取り専用として扱い、解析の前後でその内容を変更しない。
9. The parser shall バージョン文字列を program 宣言内の固定文字列としてではなくポインタとして参照し、実行時に決まる文字列を `static const` の宣言から指せるようにする。

### Requirement 2: 宣言検査

**Objective:** As a CLI ツールの開発者, I want 宣言テーブルの誤りを使い方エラーとは別の宣言エラーとして検出したい, so that 開発中のミスをすぐに特定でき、ツールの利用者に開発者の誤りが使い方の誤りとして見えることがない

#### Acceptance Criteria

1. When 解析関数が呼ばれた, the parser shall argv の走査に先立って宣言テーブル全体を検査し、宣言エラーがあれば argv の内容にかかわらず宣言エラーを返す。
2. The parser shall 宣言だけを検査する手段を提供し、argv なしで宣言エラーの有無を確認できるようにする。
3. If 同じスコープ（program のグローバルオプション、または 1 つの command）で、またはグローバルオプションと command のオプションの間で、長い名前か短い名前が重複している, then the parser shall 宣言エラー「オプション名の重複」を返す。
4. If 同じ program の中でサブコマンド名が重複している, then the parser shall 宣言エラー「コマンド名の重複」を返す。
5. If オプション名またはサブコマンド名が予約名と衝突している（`--version` との衝突は program がバージョン文字列を持つ場合に限る）, then the parser shall 宣言エラー「予約名との衝突」を返す。
6. If option が長い名前も短い名前も持たない, then the parser shall 宣言エラー「名前がない」を返す。
7. If 長い名前が空、または `=`、空白、先頭の `-` を含むなど形式が不正である、あるいは短い名前が `-` である, then the parser shall 宣言エラー「名前の形式が不正」を返す。
8. If FLAG 以外の種類の option に否定可能フラグが立っている, then the parser shall 宣言エラー「否定できない種類」を返す。
9. If 否定可能な FLAG `x` と同じスコープに長い名前 `no-x` の option が宣言されている, then the parser shall 宣言エラー「否定形との衝突」を返す。
10. If FLAG または COUNT の option に既定値が指定されている, then the parser shall 宣言エラー「既定値を持てない種類」を返す。
11. If FLAG または COUNT の option に環境変数名が指定されている, then the parser shall 宣言エラー「環境変数を持てない種類」を返す。
12. If option に必須フラグと既定値の両方が指定されている, then the parser shall 宣言エラー「必須と既定値の同時指定」を返す。
13. If FLAG または COUNT の option に必須フラグが立っている, then the parser shall 宣言エラー「必須にできない種類」を返す。
14. If 位置引数の配列で、任意の位置引数の後ろに必須の位置引数がある, then the parser shall 宣言エラー「位置引数の順序が不正」を返す。
15. If 可変長の位置引数が配列の末尾以外にある、または可変長の位置引数が 2 つ以上ある, then the parser shall 宣言エラー「可変長位置引数の位置が不正」を返す。
16. If program または command の名前が空である, then the parser shall 宣言エラー「名前の形式が不正」を返す。
17. When 宣言エラーを返す, the parser shall エラー種別と、該当する command（program 直下なら NULL）、該当する option または positional の記述子へのポインタ（該当があれば）を構造体に含める。
18. If 宣言エラーが複数ある, then the parser shall 宣言テーブルの先頭から順に検査して最初に見つけたものだけを返す。

### Requirement 3: 解析の起動と状態

**Objective:** As a CLI ツールの開発者, I want 解析関数を 1 回呼ぶだけで解析結果か状態を受け取りたい, so that 表示と終了コードを自分で決めながら、解析の流れを短いコードで書ける

#### Acceptance Criteria

1. The parser shall program 宣言、`argc`、`argv`、結果の出力先を受け取る解析関数を提供し、`argv[0]` をプログラム名として扱って `argv[1]` から走査する。
2. The parser shall 解析関数の結果として、OK、HELP、VERSION、NO_COMMAND、USAGE_ERROR、DECL_ERROR、OUT_OF_MEMORY の 7 つの状態のいずれかを返す。
3. The parser shall 標準出力・標準エラー出力・その他のストリームへ一切書き込まず、プロセスを終了させない。
4. When 解析が状態 OK で終わった, the parser shall 選択されたサブコマンド（サブコマンドなし構成では NULL）、各オプションの値と出所、位置引数の列を結果から参照できるようにする。
5. When 解析が HELP を返す, the parser shall ヘルプの対象（program 全体か、特定の command か）を結果から参照できるようにする。
6. While program がサブコマンドあり構成である, when argv にサブコマンド名が現れずに走査が終わった（引数なし、またはグローバルオプションだけ）, the parser shall 状態 NO_COMMAND を返し、走査済みのグローバルオプションの値を結果から参照できるようにする。
7. If 解析の途中でメモリ確保に失敗した, then the parser shall 状態 OUT_OF_MEMORY を返す。
8. If `argc` が 0 である, then the parser shall 引数なしとして扱い、サブコマンドあり構成では NO_COMMAND、サブコマンドなし構成では必須検査の結果に応じた状態を返す。

### Requirement 4: サブコマンドの選択とグローバルオプション

**Objective:** As a CLI ツールの利用者, I want `tool [global options] <command> [options] [args]` の形で呼び出したい, so that 複数のサブコマンドを持つツールを一般的な慣習どおりに使える

#### Acceptance Criteria

1. While program がサブコマンドあり構成である, when `-` で始まらない最初のトークンがサブコマンド名に一致した, the parser shall そのサブコマンドを選択し、以降のトークンをそのサブコマンドの宣言に従って解析する。
2. While program がサブコマンドあり構成である, if `-` で始まらない最初のトークンがどのサブコマンド名にも一致しない, then the parser shall 使い方エラー「不明なコマンド」を返す。
3. While program がサブコマンドあり構成である, when サブコマンド名より前にオプションが現れた, the parser shall それをグローバルオプションとしてのみ照合し、サブコマンド固有のオプション名であっても使い方エラー「不明なオプション」として扱う。
4. When サブコマンドが選択された後にオプションが現れた, the parser shall そのサブコマンドのオプションとグローバルオプションの両方を照合の対象にする。
5. The parser shall サブコマンド名の照合で大文字小文字を区別し、前方一致による省略を受け付けない。
6. While program がサブコマンドなし構成である, the parser shall `-` で始まらないトークンをすべて位置引数として扱う。

### Requirement 5: 走査規則（長いオプション・短いオプション・終端・位置引数）

**Objective:** As a CLI ツールの利用者, I want 一般的なコマンドライン慣習（`--name value`、`--name=value`、`-o v`、`-ov`、`-abc`、`--`）で引数を書きたい, so that 他のツールと同じ感覚で使える

#### Acceptance Criteria

1. When `--` で始まり 3 文字以上のトークンが現れた, the parser shall `=` の手前（`=` がなければトークン末尾）までを長い名前として照合する。
2. When 長い名前に一致する VALUE または LIST の option に `--name=value` 形式が使われた, the parser shall `=` の後ろ全体（空文字列を含む）を値として採用する。
3. When 長い名前に一致する VALUE または LIST の option に `--name value` 形式が使われた, the parser shall 次のトークンをその内容にかかわらず（`-` で始まる場合や `--` も含めて）そのまま値として消費する。
4. If VALUE または LIST の option が値を要するのに次のトークンがない, then the parser shall 使い方エラー「値がない」を返す。
5. If FLAG または COUNT の option に `--name=value` 形式で値が与えられた, then the parser shall 使い方エラー「値を取らないオプションに値が与えられた」を返す。
6. If 長い名前がどの option にも一致しない, then the parser shall 使い方エラー「不明なオプション」を返す。
7. The parser shall 長いオプション名の照合で大文字小文字を区別し、前方一致による省略を受け付けない。
8. When `-` で始まり `--` では始まらない 2 文字以上のトークンが現れた, the parser shall 2 文字目以降の各文字を短い名前として順に照合する（まとめ書き）。
9. When まとめ書きの途中の文字が VALUE または LIST の option に一致した, the parser shall その文字より後ろの残りの文字列（空でなければ）を値として採用し、残りがなければ次のトークンをそのまま値として消費する。
10. The parser shall `-o=v` のような短いオプションの残り文字列を `=` を含めてそのまま値として扱う（`=v` が値になる）。
11. If まとめ書きの中の文字がどの option の短い名前にも一致しない, then the parser shall 使い方エラー「不明なオプション」を返し、該当する 1 文字を特定できる情報を含める。
12. When トークン `--` が現れた, the parser shall それ以降のすべてのトークンをその内容にかかわらず位置引数として扱う。
13. The parser shall トークン `-` 単体を位置引数として扱う。
14. When サブコマンド選択後（またはサブコマンドなし構成）で `-` で始まらないトークンが現れた, the parser shall それを宣言された位置引数の順に割り当て、オプションと位置引数が交互に並んでいても受け付ける。
15. If 宣言された位置引数をすべて割り当てた後にさらに位置引数が現れ、末尾の位置引数が可変長でない, then the parser shall 使い方エラー「余分な引数」を返す。

### Requirement 6: 値の種類と繰り返しの規則

**Objective:** As a CLI ツールの開発者, I want フラグ・カウント・単一値・リスト値の 4 種類と `--no-<name>` による否定を宣言だけで使いたい, so that よくあるオプションの形を自前の後処理なしに表現できる

#### Acceptance Criteria

1. When FLAG の option が指定された, the parser shall その値を真とし、同じ FLAG が繰り返し指定されても使い方エラーにしない。
2. When 否定可能な FLAG `x` に対して `--no-x` が指定された, the parser shall その値を偽とし、出所を明示指定として記録する。
3. When 同じ FLAG に対して `--x` と `--no-x` が混在して指定された, the parser shall argv 上で最後に現れたものを採用する。
4. If 否定可能でない FLAG、または FLAG 以外の option に対して `--no-<name>` が指定された, then the parser shall 使い方エラー「不明なオプション」を返す。
5. If `--no-x=value` の形で否定形に値が与えられた, then the parser shall 使い方エラー「値を取らないオプションに値が与えられた」を返す。
6. When COUNT の option が指定された, the parser shall 指定された回数（`-vvv` のまとめ書き、`-v -v`、`--verbose --verbose` のいずれも 1 回ずつ数える）を値とする。
7. If COUNT の指定回数が上限（宣言で指定した上限、未指定ならライブラリ既定の上限）を超えた, then the parser shall 値を上限にとどめ（クランプ）、使い方エラーにしない。
8. When VALUE の option が複数回指定された, the parser shall argv 上で最後に指定された値を採用する（後勝ち）。
9. When LIST の option が複数回指定された, the parser shall 指定された順にすべての値を 1 つのリストとして連結する。
10. Where LIST の option に区切り文字が宣言されている, the parser shall 1 回の指定の値をその区切り文字で分割し、分割で生じた空の項目も項目として保持する。
11. Where LIST の option に区切り文字が宣言されていない, the parser shall 1 回の指定の値をそのまま 1 項目として扱う。
12. The parser shall 値を文字列（および FLAG の真偽、COUNT の回数）として返し、整数変換・選択肢・範囲などの妥当性検査を行わない。

### Requirement 7: 既定値・環境変数フォールバック・出所

**Objective:** As a CLI ツールの開発者, I want 明示指定 > 環境変数 > 既定値の順で値を解決し、各値の出所を知りたい, so that 設定の由来に応じた挙動（例: 環境変数由来なら警告する）を利用側で決められる

#### Acceptance Criteria

1. When VALUE または LIST の option が argv で指定された, the parser shall その値を採用し、出所を「明示指定」とする。
2. While VALUE または LIST の option が argv で指定されていない, when その option に環境変数名が宣言されており、その環境変数が設定されている（空文字列を含む）, the parser shall 環境変数の値を採用し、出所を「環境変数」とする。
3. While VALUE または LIST の option が argv でも環境変数でも与えられていない, when その option に既定値が宣言されている, the parser shall 既定値を採用し、出所を「既定値」とする。
4. While option が argv・環境変数・既定値のいずれからも値を得ていない, the parser shall 出所を「未指定」とし、VALUE の値を NULL、LIST を空のリスト、FLAG を偽、COUNT を 0 として返す。
5. When LIST の option の値が環境変数から採用された, the parser shall 宣言された区切り文字で分割する（区切り文字がなければ 1 項目とする）。
6. The parser shall 環境変数の取得関数を利用側またはプラットフォーム層が差し替えられるようにし、差し替えがない場合は C 標準の環境変数取得と同等に動作する。
7. When 環境変数の値を採用した, the parser shall その文字列を結果の中に複製して保持し、取得関数が返したバッファをそれ以降参照しない。
8. The parser shall 環境変数の参照を、該当する option が属するスコープ（選択されたサブコマンドとグローバル）に限って行い、選択されなかったサブコマンドの option の環境変数は参照しない。
9. If 環境変数の取得関数が「未設定」以外の失敗（メモリ不足など）を報告した, then the parser shall 状態 OUT_OF_MEMORY を返し、黙って既定値にフォールバックしない。

### Requirement 8: 必須検査と位置引数の個数制約

**Objective:** As a CLI ツールの利用者, I want 必須のオプションや引数が足りないときに何が足りないかを特定したエラーを受け取りたい, so that 指定を直してやり直せる

#### Acceptance Criteria

1. When 走査が完了し、HELP / VERSION / 使い方エラーのいずれも発生していない, the parser shall 選択されたスコープ（グローバルと選択されたサブコマンド、またはサブコマンドなし構成の program）の必須オプションと必須位置引数を検査する。
2. If 必須の VALUE または LIST の option が明示指定からも環境変数からも値を得ていない, then the parser shall 使い方エラー「必須オプションがない」を返し、該当する option の記述子を含める。
3. If 必須の位置引数に対応するトークンがない, then the parser shall 使い方エラー「必須引数がない」を返し、該当する positional の記述子を含める。
4. When 末尾の位置引数が可変長かつ必須である, the parser shall 1 個以上のトークンを要求し、可変長かつ任意であれば 0 個以上を受け付ける。
5. When 位置引数の個数が宣言の範囲内である, the parser shall 各位置引数のトークンを宣言順に対応づけ、可変長の位置引数には残りのトークンすべてを対応づける。
6. While 状態が NO_COMMAND である, the parser shall サブコマンドの必須検査を行わず、グローバルオプションの必須検査のみを行う。
7. The parser shall 必須検査を宣言順（グローバルオプション、サブコマンドのオプション、位置引数の順）に行い、最初に見つかった不足だけを報告する。

### Requirement 9: ヘルプ・バージョンの検出とエラー優先度

**Objective:** As a CLI ツールの利用者, I want 他の指定が間違っていても `--help` を付ければヘルプが出てほしい, so that 使い方を確認してから直せる

#### Acceptance Criteria

1. When argv のどこか（サブコマンドの前後を問わず、まとめ書きの中の `h` を含み、ただし `--` より前で、他のオプションの値として消費されたトークンを除く）に `--help` または `-h` が現れた, the parser shall 状態 HELP を返し、サブコマンドが選択済みならそのサブコマンドを、未選択なら program 全体をヘルプの対象とする。
2. While program がサブコマンドあり構成である, when `-` で始まらない最初のトークンが `help` である, the parser shall 状態 HELP を返し、次のトークンが既存のサブコマンド名ならそれを、次のトークンがなければ program 全体をヘルプの対象とする。
3. While program がサブコマンドあり構成である, if `help` の次のトークンがどのサブコマンド名にも一致しない, then the parser shall 使い方エラー「不明なコマンド」を返す。
4. Where program 宣言がバージョン文字列（非 NULL）を持つ, when argv のどこか（`--` より前で、他のオプションの値として消費されたトークンを除く）に `--version` が現れた, the parser shall 状態 VERSION を返す。
5. Where program 宣言がバージョン文字列を持たない（NULL）, when `--version` が現れた, the parser shall それを通常の長いオプションとして照合し、宣言になければ使い方エラー「不明なオプション」として扱う。
6. If HELP または VERSION の要求と使い方エラーが同じ argv に含まれている, then the parser shall 使い方エラーの位置が要求より前であっても HELP または VERSION を優先して返す。
7. If HELP の要求と VERSION の要求が同じ argv に含まれている, then the parser shall argv 上で先に現れたほうを返す。
8. The parser shall HELP または VERSION を返す場合に必須検査を行わない。
9. If 宣言エラーがある, then the parser shall HELP / VERSION の要求があっても宣言エラーを優先して返す。
10. If 使い方エラーが複数ある, then the parser shall argv 上で最初に見つけたものだけを報告する。

### Requirement 10: 構造化エラー

**Objective:** As a CLI ツールの開発者, I want エラーを文字列ではなく構造体で受け取りたい, so that 自分の書式・言語・出力先・終了コードでエラーを表示でき、ライブラリが付けた接頭辞を剥がす処理が不要になる

#### Acceptance Criteria

1. The parser shall 使い方エラーの種別を公開ヘッダの列挙として定義し、少なくとも「不明なコマンド」「不明なオプション」「値がない」「値を取らないオプションに値が与えられた」「必須オプションがない」「必須引数がない」「余分な引数」を含める。
2. The parser shall 宣言エラーの種別を公開ヘッダの列挙として定義し、Requirement 2 で挙げたすべての種別を含める。
3. When 使い方エラーを返す, the parser shall エラー構造体に、種別、該当する command（サブコマンド選択前または選択なしなら NULL）、該当する option の記述子（該当があれば）、該当する positional の記述子（該当があれば）、該当するトークン（argv の要素を指すポインタ。該当があれば）、該当する argv の添字（該当がなければ負の値）を含める。
4. When まとめ書きの中の 1 文字で使い方エラーが起きた, the parser shall トークン全体に加えて該当する短い名前の 1 文字を構造体から判別できるようにする。
5. The parser shall エラー構造体の中に整形済みの文面を含めず、文面の生成を本 spec の外（help-and-messages）に委ねる。
6. The parser shall 宣言エラーと使い方エラーを状態（DECL_ERROR / USAGE_ERROR）で区別できるようにする。
7. The parser shall エラー構造体のポインタ型フィールドが宣言テーブルと argv を指すようにし、利用側がそれらを保持している限り、解析後も参照できるようにする。

### Requirement 11: 結果の参照 API とハンドラ呼び出し

**Objective:** As a CLI ツールの開発者, I want 値を option 記述子へのポインタで参照し、選択されたサブコマンドのハンドラをそのまま呼びたい, so that 名前の打ち間違いがコンパイル時または開発時に見つかり、並行のディスパッチ表を持たなくて済む

#### Acceptance Criteria

1. The parser shall option の値を、宣言テーブルの要素のアドレス（記述子ポインタ）をキーとして参照する関数群を提供し、文字列キーによる参照を公開しない。
2. The parser shall FLAG の真偽、COUNT の回数、VALUE の文字列（未指定なら NULL）、LIST の項目数と各項目、および各 option の出所を参照する手段を提供する。
3. If 結果に対して、解析に使った宣言テーブルに属さない記述子ポインタが渡された, then the parser shall これを契約違反としてデバッグビルドで検出できるようにする（黙って未指定として返さない）。
4. The parser shall 選択されたサブコマンドの宣言（サブコマンドなし構成では NULL）と、位置引数の個数および各トークンを結果から参照する手段を提供する。
5. The parser shall 結果と userdata を引数にして選択されたサブコマンド（サブコマンドなし構成では program）の handler を呼び出す関数を提供し、handler の戻り値をそのまま返す。
6. If handler を呼び出す関数が、状態 OK 以外の結果、または handler が NULL のスコープに対して呼ばれた, then the parser shall これを契約違反としてデバッグビルドで検出できるようにする。
7. The parser shall 結果から参照される VALUE の文字列と位置引数のトークンが、出所が明示指定なら argv を、既定値なら宣言テーブルを指すようにし、複製を作らない。
8. The parser shall LIST の項目と環境変数由来の値を結果の中に複製して保持し、結果の解放とともに解放する。

### Requirement 12: メモリ所有と解放

**Objective:** As a CLI ツールの開発者, I want 結果の解放が単純で失敗後も安全であってほしい, so that エラー経路でも二重解放やリークを気にせずに済む

#### Acceptance Criteria

1. The parser shall 結果を解放する関数を提供し、解析がどの状態で終わった場合（OUT_OF_MEMORY を含む）でも呼び出せるようにする。
2. The parser shall 結果の解放を冪等にし、同じ結果に対して 2 回以上呼んでも安全であるようにする。
3. When 解析関数が結果を初期化する前に失敗した場合でも, the parser shall 呼び出し側がゼロ初期化した結果に対する解放関数の呼び出しを安全に受け付ける。
4. The parser shall 宣言テーブル、argv、userdata、バージョン文字列を解放の対象とせず、利用側の所有物として扱う。
5. The parser shall 結果が保持する確保済みメモリのサイズ計算でオーバーフローを検査し、オーバーフローする場合は確保せずに OUT_OF_MEMORY として扱う。
6. If 解析の途中でメモリ確保に失敗した, then the parser shall それまでに確保したメモリを結果に保持したままにして、解放関数で漏れなく解放できるようにする。

### Requirement 13: 単体テスト

**Objective:** As a CLOpts の保守者, I want 解析の挙動を架空の宣言で網羅的に検証するテストを持ちたい, so that 走査規則やエラー優先度の変更が既存の挙動を壊したときに検出できる

#### Acceptance Criteria

1. The parser shall 解析機能の単体テストを `tests/` 配下の対応するモジュールのディレクトリに置き、プロジェクトのテスト基盤から実行できるようにする。
2. The parser shall テストの宣言に架空のプログラム（実在のツールではない）だけを使う。
3. The parser shall 7 つの状態のそれぞれ、使い方エラー種別のそれぞれ、宣言エラー種別のそれぞれを少なくとも 1 件のテストで到達させる。
4. The parser shall サブコマンドあり構成とサブコマンドなし構成の両方について、長いオプション、短いオプションのまとめ書き、`--name=value`、`--`、`-` 単体、位置引数とオプションの混在、`--no-x`、カウントの上限、リストの連結と区切り、既定値、環境変数フォールバックと出所、必須検査、HELP / VERSION の優先度をテストで検証する。
5. The parser shall 環境変数フォールバックのテストで取得関数を差し替え、実際のプロセス環境に依存せずに検証する。
6. The parser shall 結果の解放の冪等性、ゼロ初期化した結果の解放、OUT_OF_MEMORY 後の解放をテストで検証する。
