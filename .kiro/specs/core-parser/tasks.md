# Implementation Plan

- [x] 1. 基盤: 公開契約とモジュール骨格
- [x] 1.1 公開ヘッダに宣言型と解析契約を置き、傘ヘッダから include する
  - 宣言型（program / command / option / positional、種類 FLAG / COUNT / VALUE / LIST、ハンドラの関数ポインタ型、カウント上限の既定値）を宣言専用の公開ヘッダに置く。利用側が `static const` の初期化子だけで書けるよう、フィールドは文字列ポインタ・1 文字・真偽値・符号なし整数・サイズ・列挙・関数ポインタ・`void *` に限る
  - 解析契約（7 つの状態、使い方エラー 7 種別、宣言エラー 13 種別、エラー構造体、出所の 4 区分、結果の殻、環境変数取得関数の型と 3 つの戻り値、公開関数の宣言）を解析用の公開ヘッダに置く。各フィールドが「いつ有効か」をコメントで明記する
  - 両ヘッダに `extern "C"` ガードとヘッダガードを付け、依存する標準ヘッダだけを include する。傘ヘッダから両方を include する
  - 完了条件: 傘ヘッダだけを include した C ファイルが、宣言テーブル 1 つを `static const` で定義して警告ゼロでコンパイルできる。GCC / Clang の `-Wall -Wextra -Wpedantic -Werror`、`-std=c99` で確認する。install 規則は `include/clopts/` ごとのため変更不要であることを確認する
  - _Boundary: PublicHeaders_
  - _Requirements: 1.1, 1.2, 1.3, 1.5, 1.6, 1.7, 1.9, 3.2, 7.6, 10.1, 10.2, 10.6, 11.1_

- [x] 1.2 parser モジュールとテストの骨格（内部契約、全テストファイルの登録、架空の宣言フィクスチャ）を用意する
  - `src/parser/` をモジュール追加ヘルパで `clopts` ターゲットに登録し、ソース 4 ファイル（宣言検査、結果ストア、解決、走査）を作る。各ファイルの中身は、内部ヘッダまたは公開ヘッダで宣言された外部リンケージの関数のスタブ（固定値を返し、未使用の引数は `(void)` で明示的に捨てる）に限り、未使用の `static` 関数は置かない。結果ストアのファイルには結果の解放関数を本実装で置く（`impl` が NULL なら何もしない）。`src/CMakeLists.txt` に追加する
  - 内部ヘッダに、設計の内部契約（照合結果の構造体、スロット、ストア、DeclCheck / ResultStore / Resolver の内部関数）をすべて宣言する。include してよいのは公開ヘッダと `core/core_internal.h` だけ
  - `tests/parser/` をテスト登録ヘルパで登録し、設計のファイル計画にある 9 つのテストファイル（decl、dispatch、long、short、values、positional、resolve、errors、result）をそれぞれ 1 スイートとして作り、エントリファイルの `NAOS_DECLARE_SUITE` / `NAOS_ADD_SUITE` と `tests/CMakeLists.txt`・テストの CMake への登録をここで一度に済ませる。以降のタスクは自分のテストファイルだけを編集する。宣言エラーのテストなど、特定のスイートだけが使う宣言はそのスイートのファイル内に `static const` で置いてよい（フィクスチャは共通の妥当な宣言だけを持つ）
  - フィクスチャ: 架空のプログラム 2 つ（サブコマンドあり `tool 1.2.3`: グローバル COUNT `-v/--verbose`、否定可能 FLAG `--color`、環境変数付き VALUE `--config`；サブコマンド `build`（必須 VALUE `-o/--output`、カンマ区切り LIST `-k/--keys`、上限 8 の COUNT `-j`、必須位置引数 `SRC`、可変長 `EXTRA`）、`copy`（必須 `SRC` / `DST`）、`cat`（任意の可変長 `FILE`）；`cat` には区切り `,`・既定値 `"a,b"`・環境変数 `TOOL_TAGS` 付きの LIST `--tags` も持たせる；サブコマンドなし `filt`: 既定値 "10" と環境変数付き VALUE `-n/--lines`、任意の可変長 `FILE`、ハンドラあり、バージョンなし；必須のグローバル VALUE `--token` とサブコマンド 1 つだけを持つ小さな `strict`）の宣言と、環境変数のモック取得関数（名前 → 値の表、次回 FAILED を返すフラグ、照会した名前の記録）
  - 各スイートに最低 1 件の実テストを置く: result スイートにはゼロ初期化した結果の解放、他のスイートにはフィクスチャの宣言が設計どおり（オプション名・種類・必須・可変長・区切り・上限）であることの確認
  - 完了条件: `cmake -S . -B build && cmake --build build` がプロジェクトのコンパイラオプション（`CLOPTS_WERROR=ON`）で警告ゼロで成功し、`ctest -N` に 9 スイートの `parser_tests.<suite>.<test>` が並び、`ctest -L parser` が全件通る
  - _Boundary: ParserModule, ParserTests_
  - _Requirements: 12.3, 13.1, 13.2_

- [x] 2. コア: 宣言検査、結果ストア、値の解決
- [x] 2.1 (P) 宣言検査と宣言の照合を実装し、宣言エラー種別ごとにテストする
  - 設計の規則表 14 行を宣言テーブルの先頭から順に検査し、最初の違反で種別・該当 command・該当 option / positional をエラー構造体に入れて止める
  - 予約名は長い名前 `help`、短い名前 `h`、コマンド名 `help`、バージョン文字列があるときの長い名前 `version`。サブコマンドあり構成では program 直下の位置引数とハンドラを検査しない
  - 照合関数（コマンド名、長い名前、否定形 `no-<name>`、短い名前）をバイト単位の完全一致・大文字小文字区別で実装し、選択コマンド → グローバルの順に探す
  - 公開関数「宣言だけを検査する」を実装する
  - テスト（スイート decl のみ編集）: 規則表の各行で宣言エラーと対応する種別・指し先、妥当な宣言 2 つで OK、バージョンなしなら `--version` を自前で宣言できる、2 つの違反を含む宣言で先頭側だけが返る
  - 完了条件: `ctest -L parser -R decl` が全件通り、宣言エラー 13 種別すべてがテストから到達している
  - 2.2 とはファイルを共有しない（内部ヘッダ・テスト登録・エントリは 1.2 で確定済み）
  - _Boundary: DeclCheck, ParserTests_
  - _Requirements: 1.4, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 2.10, 2.11, 2.12, 2.13, 2.14, 2.15, 2.16, 2.17, 2.18, 4.5, 5.7_

- [x] 2.2 (P) 結果ストアと参照 API、解放、ハンドラ呼び出しを実装し、記録規則と所有を内部 API でテストする
  - ストアの確保（グローバルのスロット配列）、サブコマンド選択時のコマンド側スロット配列の確保、各スロットのリスト用可変長配列の初期化。サイズ計算はオーバーフロー検査を通し、失敗時は既存の内容を変えずに失敗を返す
  - 記録関数: FLAG は真偽、COUNT は +1 と上限（0 なら既定上限）でのクランプ、VALUE は置換、LIST は区切り文字で分割して複製を追加（空の項目も追加、区切りなしは 1 項目）。出所を引数で受けて一緒に記録する。環境変数の複製の所有権移動、位置引数の追加
  - 公開アクセサ（FLAG / COUNT / VALUE / LIST 項目数と項目 / 出所 / 位置引数の個数と項目）。記述子ポインタ → スロットは選択コマンド → グローバルの順に要素アドレスの等値比較で解決し、見つからなければ `assert`（`NDEBUG` では未指定相当）
  - 解放: 複製・リスト項目・可変長配列・スロット配列・ストア本体を解放して `impl` を NULL に戻す。ゼロ初期化・解放済みに対して安全。ハンドラ呼び出しは OK かつハンドラ非 NULL を `assert` し戻り値をそのまま返す
  - テスト（スイート values と result のみ編集。解析関数は使わず内部 API で直接記録する）: 後勝ち・連結・空項目の保持・区切りなし・クランプ（上限 8 と既定上限）・否定で偽・出所の上書き、アクセサが記述子ポインタで正しいスロットを返す、確保後の解放と 2 回解放、ハンドラ呼び出しが userdata と戻り値を伝える
  - 設計のファイル計画では values スイートに argv 経由の繰り返し規則も含めているが、本タスクでは内部 API の検証に限り、argv 経由のケースは 4.1 で同じスイートに追加する（意図的な分担）
  - 完了条件: `ctest -L parser -R "values|result"` が通る。`result->impl` から到達できるメモリの確保と解放が結果ストアのファイルだけで行われている
  - _Boundary: ResultStore, ParserTests_
  - _Requirements: 3.4, 6.1, 6.6, 6.7, 6.8, 6.9, 6.10, 6.11, 6.12, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8, 12.1, 12.2, 12.4, 12.5, 12.6_

- [x] 2.3 環境変数・既定値の解決と必須検査、既定の環境変数取得関数を実装する
  - 既定の取得関数: `getenv` の戻り値を即座に複製して FOUND、NULL なら UNSET、確保失敗なら FAILED
  - 解決: スロットがある範囲（グローバル + 選択コマンド、または program。フラグでグローバルのみに限定可）を宣言順に走り、未指定の VALUE / LIST について環境変数 → 既定値の順で記録する。VALUE の環境変数値は所有権をスロットへ、LIST は分割後に複製を解放。取得失敗・確保失敗は即座に失敗を返す
  - 必須検査: 宣言順（グローバル → コマンド → 位置引数）に最初の不足をエラー構造体に入れる。位置引数は必須の個数と記録数で判定し、可変長かつ必須は 1 個以上
  - テスト（スイート resolve のみ編集。内部 API でストアを用意して呼ぶ）: モック取得関数で ENV / DEFAULT / NONE の出所、空文字列の環境変数で ENV、LIST の既定値と環境変数の分割、FAILED で失敗、グローバルのみ限定で選択外のコマンドの環境変数名を照会しない、必須の不足が宣言順に 1 件、可変長かつ必須で 0 個は不足
  - 完了条件: `ctest -L parser -R resolve` が通る。実際のプロセス環境に依存するテストがない
  - ストアの記録関数の実装を使うため 2.2 の後に順次実行する
  - _Boundary: Resolver, ParserTests_
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7_

- [x] 3. 走査器: 解析関数と走査規則
- [x] 3.1 解析関数の骨格を作り、宣言検査・結果ストア・解決・必須検査を接続する（結合タスク）
  - 公開の解析関数 2 つ（既定の取得関数 / 差し替え）。手順: 前回の `impl` を解放 → 宣言検査（違反なら DECL_ERROR、argv を読まない）→ ストア確保 → 走査（前段と本走査の入口は次のタスクで埋める。ここでは「トークンがなければ終端」だけ）→ 保留エラーがあれば返す（回復走査は 3.6）→ HELP / VERSION なら即返却 → 解決 → 必須検査 → OK / NO_COMMAND。`argc == 0` を引数なしとして扱う。失敗は OUT_OF_MEMORY
  - 走査の状態（現在位置、選択コマンド、保留エラー、`--` 到達、HELP / VERSION 要求）を 1 つの構造体にまとめ、以降のタスクが小関数で表の各行を足せる形にする
  - 標準出力・標準エラー・`exit` を使わない
  - テスト（スイート dispatch に追加）: `tool`（引数なし）が NO_COMMAND、`filt`（引数なし）が OK で既定値 `10` と出所 DEFAULT、宣言エラーの宣言で DECL_ERROR、モックの FAILED で OUT_OF_MEMORY とその後の解放、モックで `FILT_LINES=5` を与えた `filt` が OK で値 `5`・出所 ENV
  - 完了条件: `ctest -L parser -R dispatch` が通り、OK / NO_COMMAND / DECL_ERROR / OUT_OF_MEMORY が解析関数から到達している
  - _Boundary: Scanner, ParserTests_
  - _Depends: 2.1, 2.2, 2.3_
  - _Requirements: 3.1, 3.2, 3.3, 3.7, 3.8, 7.2, 7.3, 7.9, 8.1, 8.6_

- [x] 3.2 前段（サブコマンド前）の表と help / version の各形を実装する
  - 前段の表: トークン終端で NO_COMMAND、トークン全体が `--help` / `-h` に完全一致すれば全体 HELP、`--version`（バージョンあり）に完全一致すれば VERSION（まとめ書き内の `h` は 3.5 の短いオプション処理が扱う）、`help` / `help <cmd>` / `help <cmd> extra`（余分な引数）/ `help nope`（不明なコマンド）、サブコマンド名で選択してコマンド側スロットを確保、`--` の次は不明なコマンド、`-` 始まりは「オプション 1 トークンの処理」へ（本体は 3.4 / 3.5。ここでは入口だけ用意し、暫定的に不明なオプションを返す。この暫定挙動は 3.4 で置き換えるため、本タスクのテストにサブコマンド前のグローバルオプションのケースを含めない）、それ以外は不明なコマンド
  - サブコマンドなし構成は前段を経ずに本走査へ入る。NO_COMMAND ではグローバルのみ解決・必須検査する
  - HELP / VERSION ではヘルプの対象（program 全体か選択コマンドか）を結果に入れ、以降のトークンを見ない。HELP / VERSION では解決と必須検査を行わない
  - テスト（スイート dispatch に追加）: `tool build`（必須欠落で MISSING_OPTION だが選択は成功し `command` が非 NULL）、`tool bulid`、`tool Build`、`tool --help`、`tool -h`、`tool help`、`tool help copy`、`tool help nope`、`tool help copy x`、`tool --version`、`tool --version build`（後続を見ない）、`tool -- build`、`tool -`
  - 完了条件: `ctest -L parser -R dispatch` が通り、HELP / VERSION / USAGE_ERROR が解析関数から到達して 7 状態がそろう
  - _Boundary: Scanner, ParserTests_
  - _Requirements: 1.4, 3.5, 3.6, 4.1, 4.2, 4.5, 8.6, 9.1, 9.2, 9.3, 9.4, 9.5, 9.8, 9.9, 9.11_

- [x] 3.3 終端、`-` 単体、位置引数の割り当てと余分な引数を実装する
  - `--` 以降と `-` 単体は位置引数。`-` で始まらないトークンは宣言された位置引数の順に割り当て、宣言数を超えて末尾が可変長でなければ「余分な引数」。可変長の位置引数には残りのトークンをすべて対応づける。必須の不足は 2.3 の必須検査が報告する
  - オプションの処理はまだないため、テストはオプションを含まない argv に限る
  - テスト（スイート positional に追加）: `tool copy a b` が OK で位置引数 2、`tool copy a` が MISSING_ARGUMENT（`DST`）、`tool copy a b c` が UNEXPECTED_ARGUMENT（`c`）、`tool cat` が OK で 0 個、`tool cat x y z` で 3 個、`tool cat -- -x --y` で位置引数 2、`tool cat -` で `-`、`filt a b` で位置引数 2、`strict`（引数なし）がサブコマンド未選択のままグローバルの必須検査に進み、最終状態が USAGE_ERROR・種別 MISSING_OPTION（NO_COMMAND のままにならない）。各テストで解析後に argv の各要素が変わっていないことを確認する
  - 完了条件: `ctest -L parser -R positional` が通る
  - _Boundary: Scanner, ParserTests_
  - _Requirements: 1.8, 4.6, 5.12, 5.13, 5.14, 5.15, 8.3, 8.4, 8.5, 8.6, 8.7_

- [x] 3.4 長いオプション（`=`、値の消費、否定形、グローバルとコマンドの照合）を実装する
  - `=` の手前までを名前にして照合 → なければ否定形。FLAG / COUNT に `=` 付きは「値を取らない」、VALUE / LIST は `=` の後ろ全体か次のトークンをそのまま消費、なければ「値がない」、見つからなければ「不明なオプション」と名前の長さ。`--help=x` / `--version=x` は option NULL の「値を取らない」。`--help` / `--version` の完全一致は選択後も HELP（そのコマンド）/ VERSION
  - サブコマンド前はグローバルのみ、選択後はコマンドとグローバルの両方を照合する。3.2 の暫定の入口をこの処理で置き換える
  - テスト（スイート long に追加）: 設計の Testing Strategy の long の各項目、`--output a --output b` で `b`、`--keys a,b --keys c` で 3 項目、`--keys a,,b` の空項目、`tool --verbose build x --output y` と `tool build --verbose x --output y` でグローバル COUNT が 1、`tool --output y build` が不明なオプション、`tool build x --help` が `build` の HELP、サブコマンドなし構成の `filt --lines=5 a` と `filt --lines 5` で値 `5`。各テストで解析後に argv の各要素が変わっていないことを確認する
  - 完了条件: `ctest -L parser -R long` が通る
  - _Boundary: Scanner, ParserTests_
  - _Requirements: 4.3, 4.4, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 6.1, 6.2, 6.3, 6.4, 6.5, 6.8, 6.9, 6.10, 6.11, 6.12, 9.1, 9.4_

- [x] 3.5 短いオプションのまとめ書きと値の連結を実装する
  - 1 文字ずつ照合。`h` で HELP（サブコマンド前なら全体、選択後はそのコマンド）、不明なら該当 1 文字付きの「不明なオプション」、FLAG / COUNT は次の文字へ、VALUE / LIST は残りの文字列（`=` を含めてそのまま）か次のトークンをそのまま消費、なければ「値がない」
  - テスト（スイート short に追加）: 設計の Testing Strategy の short の各項目、`-vvv` で 3、`-j` を 9 回で 8（クランプ）、`-k a,b -k c` で 3 項目、`tool -v build x -o y` と `tool build -v x -o y` でグローバル COUNT が 1、`tool -vv` の NO_COMMAND で COUNT 2、`tool -vh` が全体 HELP、サブコマンドなし構成の `filt -n5 a` と `filt -n 5` で値 `5`。各テストで解析後に argv の各要素が変わっていないことを確認する
  - 完了条件: `ctest -L parser -R short` が通る
  - _Boundary: Scanner, ParserTests_
  - _Requirements: 4.3, 4.4, 5.8, 5.9, 5.10, 5.11, 6.6, 6.7, 6.9, 9.1_

- [x] 3.6 回復走査とエラー優先度、構造化エラーの各フィールドを完成させる
  - 保留した最初の使い方エラーのトークンの次から `--` の手前まで、`--help` / `-h` / `--version`（バージョンあり）に完全一致するトークンだけを探し、見つかれば HELP（選択済みならそのコマンド）/ VERSION（先に現れたほう）、見つからなければ保留エラーを返す。宣言エラーは常に優先
  - すべての使い方エラーで、種別・command（選択後は非 NULL）・option / positional・token（argv の要素を指す）・argv 添字・名前の長さ・まとめ書きの該当 1 文字が設計どおりに埋まる
  - テスト（スイート errors に追加）: `--bad --worse` で最初のみ、`--bad -h` が HELP、必須欠落を含む `--help`、値として消費された `-h`、`--bad --version`、HELP と VERSION の先勝ち、宣言エラー + `--help`、7 つの使い方エラー種別それぞれの token / argv_index / name_len / short_name / command / option / positional
  - 完了条件: `ctest -L parser -R errors` が通り、使い方エラー 7 種別すべてがテストから到達している
  - _Boundary: Scanner, ParserTests_
  - _Requirements: 9.6, 9.7, 9.10, 9.12, 10.3, 10.4, 10.5, 10.7_

- [x] 4. 検証: 解析関数経由の結合テストと全体確認
- [x] 4.1 解析関数を通した値の解決と結果参照のテストを追加し、モジュール全体を検証する
  - スイート resolve に解析関数経由のケースを追加: `--config` の ENV / EXPLICIT / NONE、`tool build ...` で選択外コマンドの環境変数名を照会しない、環境変数で必須が満たされる、`tool build x` の MISSING_OPTION、`filt` の既定値、FAILED で OUT_OF_MEMORY とその後の解放
  - スイート values に解析関数経由の繰り返し規則を追加: `-k a,b -k c` と `--keys=a --keys b,c` の連結、`-k a,,b` と `-k ,` の空項目、`--output a --output b` の後勝ち、`-j` を 9 回で 8、`--color --no-color` と `--no-color --color`、`cat` の `--tags` が既定値・環境変数・明示指定のそれぞれで分割されること
  - スイート result に解析関数経由のケースを追加: 記述子ポインタで全種類の値と出所、選択コマンドと位置引数、`filt` と `build` のハンドラ呼び出しと userdata・戻り値の伝達、OUT_OF_MEMORY 後の 2 回の解放、同じ結果変数で 2 回解析しても漏れない
  - 7 つの状態・使い方エラー 7 種別・宣言エラー 13 種別がテストから到達していることを一覧で確認し、不足があればテストを追加する
  - 完了条件: `cmake --build build && ctest --test-dir build --output-on-failure` が全件通り、GCC / Clang の `-Wall -Wextra -Wpedantic -Werror -Werror=vla` で警告ゼロ。`-DCMAKE_C_FLAGS="-fsanitize=address,undefined"` で構成したビルドの `ctest -L parser` にリーク・不正アクセス・未定義動作の報告がない。`src/parser/` が include する内部ヘッダが `core/core_internal.h` だけである
  - _Boundary: ParserTests_
  - _Requirements: 3.4, 6.3, 6.7, 6.8, 6.9, 6.10, 7.1, 7.2, 7.3, 7.5, 7.8, 7.9, 8.2, 11.1, 11.2, 11.4, 11.5, 11.7, 11.8, 12.1, 12.2, 13.3, 13.4, 13.5, 13.6_

## 改訂（2026-11-08: ビルダー API と不透明ハンドル）

タスク 1〜4 は初版（`static const` 宣言テーブル）の実装で、完了済み。5 以降は design.md（2026-11-08 改訂）に従って、宣言をビルダー API と不透明ハンドルに作り直す。走査規則とテストの観点は初版から引き継ぐ。初版のソースとテストは、5.2 で登録しないディレクトリ `src/parser/legacy/` と `tests/parser/legacy/` に `git mv` し、移植の参照元にする（8.3 で削除する）。

- [ ] 5. 基盤: 確保の窓口と、新しい公開契約への切り替え
- [x] 5.1 parser の確保の窓口とテスト用の確保プローブを作る
  - 確保・複製・可変長配列への追加・解放を、プローブを受け取る窓口の関数に通す。プローブがないときは標準の確保関数を直接呼ぶだけにする
  - プローブは「指定した回数目以降の確保点をすべて失敗させる」ことと、「生存している確保ブロック（オブジェクト、文字列、バッファを持つ可変長配列）を数える」ことを行う。確保の要素数の掛け算と文字列長の足し算はオーバーフローを検査し、オーバーフローなら確保せずに失敗を返す
  - 初版のコードはまだ書き換えない（この時点では窓口を誰も使わない）。窓口の新しいソースをモジュールの一覧に足す。プローブの型と窓口の関数は現在の内部ヘッダに宣言し、5.2 の書き直しでもそのまま残す。program にプローブを取り付ける関数は、新しいモデルが要るので 5.2（スタブ）と 5.3（本実装）が担当する
  - oom スイートをここで登録し（テストのエントリと CMake）、窓口単体のテストを置く
  - 完了条件: 静的版・共有版の両方のビルドで、メモリ不足のスイートが通る。そこでは、n 回目以降の失敗、計数が 0 に戻ること、オーバーフローで失敗することを確かめる。既存の `ctest -L parser` も全件通ったままである
  - _Requirements: 13.5_

- [x] 5.2 公開ヘッダと内部モデルを新しい契約に置き換え、モジュールとテストの骨格を切り替える
  - 宣言用の公開ヘッダは、不透明ハンドル型、handler の関数ポインタ型、ビルダー関数の宣言だけにする。解析用の公開ヘッダは、値を明示した列挙（状態、使い方エラー、宣言エラー、出所、環境変数の状態）、借用を返す環境変数の取得コールバック型、解析・検査関数、結果ハンドルのアクセサと解放の宣言だけにする。すべての公開関数に `CLOPTS_API` を付ける
  - 内部の宣言モデル（program / command / option / positional の構造体、種類の列挙、不変条件のコメント）と、内部契約のヘッダ（結果の構造体、スロット、照合結果、確保プローブ、各ファイルの内部関数）を設計どおりに置く
  - 初版の 4 つのソース（宣言検査、結果ストア、解決、走査）を `src/parser/legacy/` に `git mv` し、モジュールの一覧から外す。設計のファイル計画どおりの新しいファイル（ビルダー、名前の規則と照合、走査、解決、結果ハンドル）を、設計の内部関数のスタブ（固定値を返す）で置く。プローブを取り付ける関数もスタブで置く。結果ハンドルのファイルには、OOM の番兵と、NULL・番兵を無視する解放だけを本実装で置く。解析・検査関数は常に番兵を返し、ビルダー関数は NULL を返す
  - 初版のテストファイル 9 つとフィクスチャ（`test_fixtures.{h,c}`）を `tests/parser/legacy/` に `git mv` し、登録から外す。設計のファイル計画にある残りの 10 スイート（build、decl、dispatch、long、short、values、positional、resolve、errors、result）を新しいファイルでここで一度に登録する。oom スイートは 5.1 のテストを維持する。移植前のスイートには、番兵の状態を確かめる 1 件だけを置く。フィクスチャは 7.1 で作り直すので、この時点では登録しない
  - 内部ヘッダ、内部モデル、テストのエントリと CMake は、ここで確定させる。以降のタスクは自分の担当ファイルと自分のスイートのファイルだけを編集する。変更が要る場合は、そのタスクを直列にする
  - 完了条件: 静的版・共有版の両方で、警告ゼロ（`CLOPTS_WERROR=ON`）でビルドが通る。公開ヘッダに構造体の定義がない（`struct` の本体を grep して 0 件）。`ctest -L parser` が登録した全スイートで通る。統合テストの consumer fixture（`decltype(&clopts_parse)`）もビルドが通る
  - _Requirements: 4.8, 11.1, 11.2, 11.6, 13.2, 14.1, 14.3, 14.4, 14.5_

- [x] 5.3 ビルダーの前処理・複製・組み立てと program の解放、検査関数の入口を実装する
  - 全ビルダー関数で共通の前処理を行う: ハンドルが NULL なら何もしない。凍結済みなら `assert` で止め、`NDEBUG` では何もしない。エラーを記録済みなら何もしない。ハンドルを返す関数は、何もしない場合 NULL を返す
  - program の生成（構造体と名前を 1 ブロックで確保）、root の取り出し、サブコマンド・4 種類のオプション・位置引数の追加、全属性の設定を実装する。文字列はすべて複製し、設定し直したときは古い複製を解放して入れ替える。子は個別に確保して親の一覧に繋いでから名前を複製し、途中の失敗でも program から辿れるようにする。確保の失敗は OUT_OF_MEMORY として program に記録する
  - 宣言の規則の検査はまだ入れない（6.1）。program の解放は、全スコープの子・文字列・一覧を解放する。検査関数の入口は設計の順（NULL → 凍結 → OOM なら番兵 → 結果を確保 → 記録済みの宣言エラーを写す → OK）で、設計どおり走査のファイルに実装する（解析関数と入口の処理を共有するため。走査の本体は 7.2 が担当）。結果の確保と宣言エラーの写しに要る最小限の関数は、結果ハンドルのファイルに置く。プローブを取り付ける関数も本実装にする
  - テスト（build スイートのみ編集）:
    - 全ビルダー関数への NULL
    - 最初の OOM の後は何もしないこと（プローブで 1 回目を失敗させる）
    - 呼び出し後に元の文字列を書き換えても宣言が変わらないこと
    - 同じ属性の再設定は後勝ち
    - 100 個追加しても最初のハンドルが有効であること
    - `NDEBUG` のときだけ: 検査関数の後の追加が NULL を返すこと
    - NULL の program の解放
    - NULL の program の検査で OUT_OF_MEMORY
  - 完了条件: `ctest -L parser -R build` が両形式で通る。検査関数が、記録済みの OOM で番兵を、何もないときに OK を返す
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 3.2, 13.7, 15.6_

- [ ] 6. コア: 宣言の即時検査、結果ハンドル、値の解決
- [x] 6.1 (P) 宣言の即時検査を実装し、規則表の各行をテストする
  - 名前の形式の規則と、照合範囲での重複・否定形の検出を、名前の規則のファイルに置く。初版の照合関数（コマンド名、長い名前、否定形、短い名前）もここでモデルに合わせて移植し、テストする（走査器は 7.2 でそれを使う）。照合範囲は、root への追加ならすべての command も、command への追加なら root も含む。異なる command 同士の重複は許す
  - 設計の即時検査の規則表（28 行）を、各ビルダー関数の中で表の順に検査する。最初に成立したものを、種別と報告するハンドル（root は NULL として報告）とともに記録する。追加関数では、エラーでも新しいオブジェクトを繋いでハンドルを返す。設定関数では、属性を変えずにエラーだけを記録する。解除の設定では規則を検査しない（位置引数の必須の解除だけは、後ろとの順序を検査する）
  - テスト（decl スイートのみ編集）:
    - 規則表の各行で、検査関数が DECL_ERROR、対応する種別、報告するハンドルを返すこと
    - version と `--version` の option は、どちらの順で宣言しても予約名との衝突になること
    - 2 つの誤りがあれば先の呼び出しのものが返ること
    - version がなければ `--version` を宣言できること
    - 妥当なフィクスチャ相当の宣言で OK
  - 完了条件: `ctest -L parser -R decl` が通る。宣言エラーの全 15 種別がテストから到達し、照合関数のテスト（完全一致、大文字小文字の区別、選択コマンド → root の順）も通る
  - 内部ヘッダ・内部モデル・テストのエントリ・フィクスチャは 5.2 で確定済みなので、変更しない（6.2 と並行に進めるため）
  - _Boundary: Build, Decl_
  - _Requirements: 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 3.10, 3.11, 3.12, 3.13, 3.14, 3.15, 3.16, 3.17, 3.18, 3.19, 5.5, 6.7_

- [x] 6.2 (P) 結果ハンドルの記録・参照・実行・解放を実装し、内部 API でテストする
  - 初版の結果ストアを移植する。対象は、スロット配列の確保（root 分、選択コマンド分）、種類ごとの記録関数（FLAG 後勝ち、COUNT のクランプ、VALUE 後勝ち、LIST の分割と連結、空の項目を保持）、環境変数の複製の所有、位置引数の追加である。確保はすべて窓口を通す
  - 値のアクセサ: 別の program の option なら `assert`。選択されなかった command の option、NULL の option、状態が OK / NO_COMMAND 以外の結果では、未指定の値を返す。エラーのアクセサは、状態が USAGE_ERROR / DECL_ERROR 以外なら該当なしを返す。NULL の結果は番兵と同じに扱う
  - handler の実行は、解析の結果・状態 OK・handler ありを `assert` し、userdata を渡して戻り値を返す。解放は、program を参照せずに結果の持つものだけを窓口経由で解放する
  - テスト（result スイートのみ編集。解析関数は使わず、内部 API で記録する）:
    - 記録規則（後勝ち、連結、空の項目、区切りなし、上限 8 と既定の上限）
    - 出所の上書き
    - アクセサがハンドルから正しいスロットを返すこと
    - 状態ごとの既定値
    - 番兵と NULL の各アクセサ
    - handler が userdata と戻り値を伝えること
    - program を先に解放してから結果を解放すること
  - 完了条件: `ctest -L parser -R result` が両形式で通る
  - 内部ヘッダ・内部モデル・テストのエントリ・フィクスチャは 5.2 で確定済みなので、変更しない（6.1 と並行に進めるため）
  - _Boundary: Result_
  - _Requirements: 4.2, 7.12, 11.3, 11.4, 11.5, 11.7, 11.8, 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8, 13.1, 13.3, 13.4, 13.6_

- [x] 6.3 環境変数・既定値の解決と必須検査を移植し、借用コールバックに合わせる
  - 既定の取得は、`getenv` の戻り値を借用として返すだけにする。FOUND を受けたら、次にコールバックを呼ぶ前に窓口経由で複製する。VALUE は複製をスロットが所有し、LIST は複製を分割する。FAILED と複製の失敗は OUT_OF_MEMORY として返し、既定値に進まない
  - 照会する環境変数は、選択されたスコープ（root と選択コマンド）の option だけにする。既定値はモデルの複製を借用する。必須の判定は、出所が明示指定か環境変数であること。不足は宣言順の最初の 1 件を記録する
  - テスト（resolve スイートのみ編集。モデルと結果を内部 API で用意して解決を呼ぶ。借用を返す模擬関数は、このスイートのファイル内にローカルに置く）:
    - 優先順位
    - 空文字列の環境変数
    - LIST の既定値と環境変数の分割
    - 選択されなかった command の環境変数を照会しないこと
    - FAILED で失敗すること
    - 模擬関数が返した直後にバッファを書き換えても値が変わらないこと
    - 必須が環境変数で満たされること
  - 完了条件: `ctest -L parser -R resolve` が両形式で通る
  - 6.2 の記録関数を使うので、6.2 の後に行う（(P) ではない）
  - _Depends: 6.2_
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9, 8.10, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7_

- [ ] 7. 走査器: 解析関数の移植と、初版テストの観点の移植
- [x] 7.1 フィクスチャをビルダーで作り直す
  - 初版と同じ架空の program 3 つ（`tool 1.2.3`、`filt`、`strict`）を、ビルダーで組み立てる関数にする。各関数は、ハンドルをまとめた構造体を埋め、プローブを受け取って生成の直後に取り付ける。解放関数も用意する
  - 環境変数の模擬関数は、名前 → 値の表、「次の呼び出しで FAILED を返す」フラグ、照会された名前の記録を持ち、借用を返す形にする。返した直後に表のバッファを書き換えるモードも持たせる
  - フィクスチャのファイルを、テストの CMake の共通ソースとして登録する（スイートのファイルは編集しない）
  - 完了条件: 3 つのフィクスチャを検査関数に通すと OK になる。build スイートに、宣言の内容（名前、種類、必須、可変長、区切り、上限）を確かめるケースを足し、それが両形式で通る
  - _Requirements: 15.2_

- [x] 7.2 走査器を内部モデルに移植して解析関数を本実装にし、ディスパッチのスイートを移植する
  - 解析関数の入口を、設計の順で実装する。順序は、NULL → 凍結 → OOM なら番兵 → 結果の確保（失敗なら番兵）→ 記録済みの宣言エラーを写す（argv は読まない）→ サブコマンドの有無で前段または本走査、である。入口の処理は、5.3 の検査関数と共有する
  - 前段・本走査・短いオプション・回復走査・値の記録・解決・必須検査は、`src/parser/legacy/` の初版の処理と表をそのまま使う。置き換えるのは、宣言の参照（モデルの一覧へ）と、結果とエラーの書き込み（結果ハンドルの記録関数へ）である。照合は 6.1 の関数を使う。環境変数の取得関数の引数が NULL なら、既定の取得を使う
  - 初版の dispatch スイートの全ケースを、7.1 のフィクスチャとアクセサで移植する
  - 完了条件:
    - `ctest -L parser -R dispatch` が両形式で通り、件数が初版の dispatch スイート以上である
    - 宣言エラーを記録した program に `--help` を渡しても DECL_ERROR になる
    - 同じ program を 2 回解析すると、独立した結果が返る
    - 解析の後で argv が変わっていない
  - _Depends: 6.1, 6.3_
  - _Requirements: 3.1, 4.1, 4.3, 4.4, 4.5, 4.6, 4.7, 4.9, 4.10, 4.11, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 10.2, 10.3, 10.4, 10.11_

- [x] 7.3 (P) 長いオプション・短いオプションのスイートを移植する
  - 初版の long / short スイートの全ケースを、新しいフィクスチャとアクセサで移植する。対象は、`--name value` / `=value` / 空の値、値の消費、前方一致なし、大文字小文字、`--no-x`、まとめ書き、`-o=v`、不明な 1 文字、末尾の値なし、`-vh` である
  - 状態を確かめてから値を参照する（初版の Implementation Notes 3.4 の注意）
  - 完了条件: `ctest -L parser -R "long|short"` が両形式で通り、件数が初版の各スイート以上である
  - 7.2 の完了後に、7.4 / 7.5 と互いに並行に進められる（編集するのは自分のスイートのファイルだけ）
  - _Boundary: ParserTests(long, short)_
  - _Depends: 7.2_
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10, 6.11, 7.2, 7.3, 7.4, 7.5, 10.1_

- [ ] 7.4 (P) 値と位置引数のスイートを移植する
  - 初版の values / positional スイートのうち、解析関数を経由する全ケースを移植する（内部 API で記録規則を確かめるケースは、6.2 で result スイートに移したものとする）。対象は、LIST の連結・区切り・空の項目、VALUE の後勝ち、COUNT のクランプと既定の上限、`--`、`-` 単体、可変長、必須の位置引数の不足、余分な引数、およびサブコマンドなし構成の節（初版の `nosub`）である。`nosub` もビルダーで組み立てる
  - 完了条件: `ctest -L parser -R "values|positional"` が両形式で通り、解析関数経由の件数が初版の各スイート以上である
  - 7.2 の完了後に、7.3 / 7.5 と互いに並行に進められる
  - _Boundary: ParserTests(values, positional)_
  - _Depends: 7.2_
  - _Requirements: 6.12, 6.13, 6.14, 6.15, 7.1, 7.6, 7.7, 7.8, 7.9, 7.10, 7.11, 9.3, 9.4, 9.5_

- [ ] 7.5 (P) エラー・解決・結果のスイートの解析関数経由のケースを移植し、改訂で増えた観点を足す
  - 初版の errors スイートの全ケースを移植する。対象は、トークン・argv 添字・command・`name_len`・`short_name`、回復走査、HELP / VERSION の優先度である
  - 初版の resolve / result スイートの、解析関数を経由するケースを、6.2 / 6.3 の内部 API のテストの後ろに追加する。対象は、環境変数、既定値、必須、NO_COMMAND、handler の実行、出所である
  - 追加の観点:
    - 環境変数の取得関数を差し替えた解析で、模擬関数のバッファを書き換えても値が変わらない
    - 状態ごとのエラーと値の既定値
  - 完了条件: `ctest -L parser -R "errors|resolve|result"` が両形式で通る。errors の件数が初版以上で、resolve と result の解析関数経由の件数も初版以上である
  - 7.2 の完了後に、7.3 / 7.4 と互いに並行に進められる
  - _Boundary: ParserTests(errors, resolve, result)_
  - _Depends: 7.2_
  - _Requirements: 8.6, 9.6, 9.7, 10.5, 10.6, 10.7, 10.8, 10.9, 10.10, 10.12, 11.3, 11.4, 12.5, 15.5_

- [ ] 8. 検証: メモリ不足の網羅、利用側からの通し確認、最終確認
- [ ] 8.1 メモリ不足の網羅試験を実装する
  - oom スイートに、確保プローブの失敗位置を 1 から順に動かし、失敗が起きなくなるまで回す試験を置く。場面は 3 つある:
    - フィクスチャ `tool` の組み立て
    - `tool build -k a,b -o x src` の解析（結果の確保を含む）
    - 環境変数と LIST の既定値の解決
  - 各回で次の 3 つを確かめる:
    - 状態が OUT_OF_MEMORY か、失敗しなかったときと同じ結果になる
    - 結果と program を解放した後に、生存ブロックが 0 になる
    - ビルダーで失敗した場合は、解析が番兵を返す
  - 完了条件: `ctest -L parser -R oom` が両形式で通る。各場面で、少なくとも 1 回は OUT_OF_MEMORY に、最後の 1 回は成功になる
  - _Requirements: 4.7, 4.8, 13.6, 13.7, 15.7_

- [ ] 8.2 統合テストの利用側コードで、宣言から解放までを公開関数だけで通す
  - consumer fixture の main に、次の流れを足す:
    - program を作り、VALUE の `--name` を宣言する
    - 固定の argv（`consumer`、`--name`、`ok`）を解析する
    - 状態が OK で値が `ok` であることを確かめる
    - 結果と program を解放する
  - 期待と違えば、終了コード 1 にする。C++ 側のファイルは変えない
  - 完了条件: `ctest -L integration` が静的版・共有版の両方で通る（サブディレクトリ経路と、install → find_package 経路）。既存の internal_hidden も引き続き通る
  - _Requirements: 14.2, 14.6_

- [ ] 8.3 最終確認: 改訂後の受入条件を通しで確かめ、初版の残骸を片づける
  - `src/parser/legacy/` と `tests/parser/legacy/` を削除する。parser の全テストの件数が、初版の件数以上であることを確かめる
  - 次の 4 つの構成で、ビルドと `ctest` の全件を通す: GCC / Clang × 静的版 / 共有版。いずれも `CLOPTS_WERROR=ON` とする
  - 公開ヘッダについて、次の 2 つを grep で確かめる:
    - 構造体の定義がない
    - すべての関数宣言に `CLOPTS_API` が付いている
  - 共有版のライブラリの公開シンボルを nm / objdump で調べ、内部関数（確保プローブ、`clopts_parser_` で始まるもの）が出ていないことを確かめる
  - 状態 7 つ、使い方エラー 7 種別、宣言エラー 15 種別のそれぞれが、テストから到達していることを確かめる
  - roadmap の Existing Spec Updates の core-parser に、完了の印を付ける
  - 完了条件: 上記がすべて満たされ、作業ツリーに初版の API を参照するコードが残っていない
  - _Requirements: 14.1, 14.2, 14.5, 15.1, 15.3, 15.4, 15.8_

## Implementation Notes
- 1.2: フィクスチャは `tests/parser/test_fixtures.{h,c}`。`strict` のサブコマンド名は `run`。`build` にもハンドラ `fixture_handler`（userdata `&fixture_build_handler_record`）を付けてある。オプション・コマンドの添字は enum 定数で公開。モック環境変数は `fixture_env_t` / `fixture_env_init` / `fixture_env_getenv` / `fixture_env_was_queried`、`fail_next` は 1 回だけ FAILED を返す
- 1.2: `clopts_result_free` は 1.2 で本実装済み（`free_slots` ヘルパ）。`add_count` は出所引数を持たず EXPLICIT を記録する（設計のシグネチャどおり）
- Clang ビルドは `CC=clang cmake -S . -B build-clang -DCLOPTS_WERROR=ON -DFETCHCONTENT_SOURCE_DIR_NAOS=$(pwd)/build/_deps/naos-src` で Naos の再取得を避けられる
- 2.1: `clopts_parser_find_negation` は「完全一致なし」を自分では確かめない。呼び出し側（3.4 の Scanner）が `find_long` を先に試すこと。DUPLICATE / NEGATION_CONFLICT は後に宣言された側を指す。「宣言検査と `clopts_parse` の結果一致」のテストは 3.1 で追加する
- 2.2: `store_begin` は前回の `impl` を `clopts_result_free` で解放してから確保する。未初期化の `result` は前回の結果と区別できず解放すると未定義動作になるため、設計 PublicHeaders の事前条件「未初期化でもよい」は満たせない。3.1 で `parse.h` の契約を「ゼロ初期化または前回の結果」に改める（設計からの逸脱として報告する）
- 2.2: `clopts_result_run` は `store->command`（なし構成は program）のハンドラを呼ぶ。`store_select` は `result->command` を設定しないので、Scanner が `result->command` を同じ値にそろえること。`append_list` は途中で失敗すると追加済みの項目を残して -1 を返す（解放は `clopts_result_free` が行う）。Resolver は `own` のあと `set_value(ENV)` の順で呼べばよい
- 2.3: `check_required` は `result->status` を設定しない（Scanner が USAGE_ERROR にする）。`error.command` は `include_command` が真なら選択コマンド、偽なら NULL。`resolve` は `getenv_fn == NULL` なら既定の取得関数を使う。MSVC の C4996 は `CloptsCompilerOptions.cmake` の `_CRT_SECURE_NO_WARNINGS`（PRIVATE）で抑止済み
- 3.1: `parse.h` の `clopts_parse*` の事前条件を「ゼロ初期化または前回の結果」に改めた（設計の「未初期化でもよい」からの逸脱。design.md は最終検証後に反映済み）。走査状態は `scan_state_t`（pos / command / has_pending / pending_error / dashdash / request / out_of_memory）。`scan_prelude` / `scan_main` は暫定で `scan_stop()`（残りを読み飛ばす）を呼ぶ。3.2〜3.5 で表の各行に置き換え、最終的に `scan_stop` の暫定呼び出しを残さないこと。回復走査（3.6）は `scan_outcome` の保留エラー分岐の直前に入れる。`finish` だけが `result->status` を書く
- 3.2: 前段の各行は `prelude_token` から小関数（`scan_request_help` / `scan_request_version` / `prelude_option` / `prelude_help_command` / `prelude_command`）へ振り分ける。`scan_fail` は最初のエラーだけを記録し、`pos` は不正トークンの次に残す。`prelude_option` は暫定（UNKNOWN_OPTION）で 3.4 / 3.5 が置き換える。`help <cmd>` は `store_select` で対象を選ぶので `help copy x` の `error.command` は `copy`。3.6 の要確認: `tool help --help` / `tool help copy --help` では `--help` 自体がエラーのトークンになり、文字どおりの回復走査では USAGE_ERROR になる（9.1「どこにあっても」との関係を設計で確かめて決め、テストする）
- 3.3: 本走査は `main_token` が dashdash → `--` → `-x...`（暫定の `main_option`）→ それ以外（`-` 単体を含む、`main_positional`）の順に振り分ける。`scan_stop` は削除済み。`main_option` と `prelude_option` は同じ本体の暫定実装で、3.4 / 3.5 で共通のオプション処理にまとめて両方を置き換え、重複を残さないこと。選択後の `--help` / `-h` / `--version` も現状は UNKNOWN_OPTION になるため 3.4 / 3.5 で直す
- 3.4: オプションの入口は前段・本走査とも `scan_option` 1 つ（`--…` → `scan_long_option`、`-x…` → 暫定の `scan_short_option`）。3.5 は `scan_short_option` だけを置き換えればよい。値の消費は `scan_option_value`（`=` の後ろ、または次のトークンをそのまま）を共用できる。テストでは値の参照の前に `NAOS_REQUIRE_INT_EQ` で状態を確かめる（状態違いのまま参照するとライブラリの `assert` でプロセスが落ちるため）
- 3.5: design.md Testing Strategy（short）の「`-vj4` で `j` に `4`」はフィクスチャ（`-j` は上限 8 の COUNT）と矛盾するため、`-vjk4` → `k=["4"]` に読み替え、`-vj4` は `'4'` の UNKNOWN_OPTION として固定した（design.md は最終検証後に反映済み）。`h`・不明な文字・VALUE/LIST の後はそのトークンの残りを見ない。`-xh` の `h` は見られない（保留エラーになる）。9.1 との関係は 3.6 の回復走査で扱う
- 3.6: 回復走査（`scan_recover`）は設計の「エラーのトークンの次から」ではなく保留エラーのトークン自身から探す（コントローラ判断。`tool help --help` / `help copy -h` / `help --version` を 9.1・9.4 どおり HELP / VERSION にするため。`--` 以降は対象外。design.md は最終検証後に反映済み）。help コマンドの経路以外ではエラーのトークンが要求トークンと完全一致しないため他の挙動は変わらない。エラー後のトークンは解釈しないので `--bad -o -h` は HELP、`tool --bad build --help` は program 全体の HELP
- 最終検証（/kiro-validate-impl）の差し戻し 1 回目: 13.4 のサブコマンドなし構成の不足（`--`、`-` 単体、`--no-x`、カウント上限、リスト、必須検査、VERSION の優先度）を、values スイート内の架空の宣言 `nosub`（`static const`）で補った（tests/parser/test_values.c の「13.4: サブコマンドなし構成」節）
- 5.1: 窓口の `clopts_parser_strndup(probe, s, len)` は `s` から `len` バイトをそのまま複製して NUL を付ける（POSIX と違い途中の NUL で止まらない。`s != NULL` を assert）。オーバーフローした呼び出しも確保点として数える。`strdup` は `strndup` 経由で確保点 1 つ。プローブ型と窓口の宣言は `parser_internal.h` 先頭の節にあり、5.2 の書き直しでも残すこと
- 5.2: design 本文の `commands.count` は `clopts_core_vec_t` の `len` を指す。Result 契約に「使い方エラーの記録」「宣言エラーの写し」の内部関数はない。走査（`parser_scan.c`）と検査関数の入口は、内部ヘッダにある `struct clopts_result` のフィールド（status / command / error）に直接書く（コントローラ判断。内部ヘッダは変えない）。スタブのファイル冒頭に、どのタスクが本実装するかを書いてある
- 5.3: 入口は `parser_scan.c` の `scan_enter(program, &done)`（NULL → 凍結 → OOM なら番兵 → 結果確保 → 宣言エラーの写し）。7.2 の解析関数はこれを共有する。`clopts_parser_result_new` はスロットを確保しない（6.2 で追加し、`clopts_result_free` も広げる）。NDEBUG 専用テストは非 NDEBUG で `NAOS_SKIP`（Debug の ctest に Skipped が 1 件出る）。追加で名前の複製が失敗すると、オブジェクトは名前 NULL のまま繋がる
- 6.1: `set_version` の RESERVED_NAME は、照合範囲の順（root、次に command の追加順）で最初の `version` を報告する（モデルに通し番号がないため。design.md の表も改めた）。`set_negatable(o, true)` は一時的な "no-x" を窓口経由で確保して `find_named` を呼ぶので、確保点を 1 つ持つ（失敗なら OOM を記録）。root への `no-x` の追加は command ごとに照会する。規則が成立しても確保に失敗すれば OOM を記録する。`valid_short_name('\0')` は偽
- 6.2: `clopts_parser_result_select` が `result->command` と command 側のスロットを設定し、1 回だけ呼べる（`assert`）。走査器は select の前に `result->command` を書かないこと。help の対象（`help <cmd>`）は select で選ぶ。command のスロットは状態 OK のときだけ読まれる（NO_COMMAND は root だけ）。`positional_count` は OK / NO_COMMAND 以外で 0。`clopts_parser_result_new` は root に option があると確保点 2 つ
- 6.3: `clopts_parser_resolve` / `clopts_parser_check_required` は、`result` が番兵でないことと `result->program == program` を assert する。どちらも `result->status` は書かない（走査器が書く）。不足時の `error.command` は、`include_command` が真なら選択コマンド、偽なら NULL。位置引数の検査は、選択コマンドの位置引数、サブコマンドなし構成なら root の位置引数に対して行う
- 7.1: フィクスチャは `fixture_tool_t` / `fixture_filt_t` / `fixture_strict_t`（ハンドルの構造体）と `fixture_<x>_init(f, probe)` / `fixture_<x>_free(f)`。初版の添字の enum 定数はなく、ハンドルで参照する。模擬環境変数 `fixture_env_t` は値を自分のバッファから貸す。`clobber` は次の呼び出しの冒頭で全バッファを書き換え、`fixture_env_clobber()` で任意の時点に書き換えられる（8.8 の検証は、解析の後に `fixture_env_clobber()` を呼んで値が変わらないことを見る）。照会した名前は 31 文字までの複製
- 7.2: 解析関数は `parse()` が `scan_enter` を共有し、番兵でない結果に `parsed = true` を立てる。走査は `scan_run` で、初版の表と論理をそのまま移した。保留エラーは局所の `scan_error_t` に持って `result->error` に写す。初版の dispatch のうち `previous_result_is_reused` と `invalid_declaration_releases_previous_result` は、結果を再利用しない新 API に合わせて「独立した 2 回の解析」「DECL_ERROR を 2 回解析して全解放」に読み替えた。サブコマンド前の command 固有オプション → UNKNOWN_OPTION（5.3）の専用ケースは 7.3〜7.5 で足すこと
- 7.3: long 45 件（初版 44 + 番兵）、short 28 件（初版 27 + 番兵）。argv は初版と同一で、検証は同等以上。5.3 は `long.command_option_before_the_command_is_unknown`（`tool --output y build`）が担う
