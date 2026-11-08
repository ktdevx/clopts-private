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

- [ ] 4. 検証: 解析関数経由の結合テストと全体確認
- [ ] 4.1 解析関数を通した値の解決と結果参照のテストを追加し、モジュール全体を検証する
  - スイート resolve に解析関数経由のケースを追加: `--config` の ENV / EXPLICIT / NONE、`tool build ...` で選択外コマンドの環境変数名を照会しない、環境変数で必須が満たされる、`tool build x` の MISSING_OPTION、`filt` の既定値、FAILED で OUT_OF_MEMORY とその後の解放
  - スイート values に解析関数経由の繰り返し規則を追加: `-k a,b -k c` と `--keys=a --keys b,c` の連結、`-k a,,b` と `-k ,` の空項目、`--output a --output b` の後勝ち、`-j` を 9 回で 8、`--color --no-color` と `--no-color --color`、`cat` の `--tags` が既定値・環境変数・明示指定のそれぞれで分割されること
  - スイート result に解析関数経由のケースを追加: 記述子ポインタで全種類の値と出所、選択コマンドと位置引数、`filt` と `build` のハンドラ呼び出しと userdata・戻り値の伝達、OUT_OF_MEMORY 後の 2 回の解放、同じ結果変数で 2 回解析しても漏れない
  - 7 つの状態・使い方エラー 7 種別・宣言エラー 13 種別がテストから到達していることを一覧で確認し、不足があればテストを追加する
  - 完了条件: `cmake --build build && ctest --test-dir build --output-on-failure` が全件通り、GCC / Clang の `-Wall -Wextra -Wpedantic -Werror -Werror=vla` で警告ゼロ。`-DCMAKE_C_FLAGS="-fsanitize=address,undefined"` で構成したビルドの `ctest -L parser` にリーク・不正アクセス・未定義動作の報告がない。`src/parser/` が include する内部ヘッダが `core/core_internal.h` だけである
  - _Boundary: ParserTests_
  - _Requirements: 3.4, 6.3, 6.7, 6.8, 6.9, 6.10, 7.1, 7.2, 7.3, 7.5, 7.8, 7.9, 8.2, 11.1, 11.2, 11.4, 11.5, 11.7, 11.8, 12.1, 12.2, 13.3, 13.4, 13.5, 13.6_

## Implementation Notes
- 1.2: フィクスチャは `tests/parser/test_fixtures.{h,c}`。`strict` のサブコマンド名は `run`。`build` にもハンドラ `fixture_handler`（userdata `&fixture_build_handler_record`）を付けてある。オプション・コマンドの添字は enum 定数で公開。モック環境変数は `fixture_env_t` / `fixture_env_init` / `fixture_env_getenv` / `fixture_env_was_queried`、`fail_next` は 1 回だけ FAILED を返す
- 1.2: `clopts_result_free` は 1.2 で本実装済み（`free_slots` ヘルパ）。`add_count` は出所引数を持たず EXPLICIT を記録する（設計のシグネチャどおり）
- Clang ビルドは `CC=clang cmake -S . -B build-clang -DCLOPTS_WERROR=ON -DFETCHCONTENT_SOURCE_DIR_NAOS=$(pwd)/build/_deps/naos-src` で Naos の再取得を避けられる
- 2.1: `clopts_parser_find_negation` は「完全一致なし」を自分では確かめない。呼び出し側（3.4 の Scanner）が `find_long` を先に試すこと。DUPLICATE / NEGATION_CONFLICT は後に宣言された側を指す。「宣言検査と `clopts_parse` の結果一致」のテストは 3.1 で追加する
- 2.2: `store_begin` は前回の `impl` を `clopts_result_free` で解放してから確保する。未初期化の `result` は前回の結果と区別できず解放すると未定義動作になるため、設計 PublicHeaders の事前条件「未初期化でもよい」は満たせない。3.1 で `parse.h` の契約を「ゼロ初期化または前回の結果」に改める（設計からの逸脱として報告する）
- 2.2: `clopts_result_run` は `store->command`（なし構成は program）のハンドラを呼ぶ。`store_select` は `result->command` を設定しないので、Scanner が `result->command` を同じ値にそろえること。`append_list` は途中で失敗すると追加済みの項目を残して -1 を返す（解放は `clopts_result_free` が行う）。Resolver は `own` のあと `set_value(ENV)` の順で呼べばよい
- 2.3: `check_required` は `result->status` を設定しない（Scanner が USAGE_ERROR にする）。`error.command` は `include_command` が真なら選択コマンド、偽なら NULL。`resolve` は `getenv_fn == NULL` なら既定の取得関数を使う。MSVC の C4996 は `CloptsCompilerOptions.cmake` の `_CRT_SECURE_NO_WARNINGS`（PRIVATE）で抑止済み
- 3.1: `parse.h` の `clopts_parse*` の事前条件を「ゼロ初期化または前回の結果」に改めた（設計の「未初期化でもよい」からの逸脱。design.md は未更新）。走査状態は `scan_state_t`（pos / command / has_pending / pending_error / dashdash / request / out_of_memory）。`scan_prelude` / `scan_main` は暫定で `scan_stop()`（残りを読み飛ばす）を呼ぶ。3.2〜3.5 で表の各行に置き換え、最終的に `scan_stop` の暫定呼び出しを残さないこと。回復走査（3.6）は `scan_outcome` の保留エラー分岐の直前に入れる。`finish` だけが `result->status` を書く
- 3.2: 前段の各行は `prelude_token` から小関数（`scan_request_help` / `scan_request_version` / `prelude_option` / `prelude_help_command` / `prelude_command`）へ振り分ける。`scan_fail` は最初のエラーだけを記録し、`pos` は不正トークンの次に残す。`prelude_option` は暫定（UNKNOWN_OPTION）で 3.4 / 3.5 が置き換える。`help <cmd>` は `store_select` で対象を選ぶので `help copy x` の `error.command` は `copy`。3.6 の要確認: `tool help --help` / `tool help copy --help` では `--help` 自体がエラーのトークンになり、文字どおりの回復走査では USAGE_ERROR になる（9.1「どこにあっても」との関係を設計で確かめて決め、テストする）
- 3.3: 本走査は `main_token` が dashdash → `--` → `-x...`（暫定の `main_option`）→ それ以外（`-` 単体を含む、`main_positional`）の順に振り分ける。`scan_stop` は削除済み。`main_option` と `prelude_option` は同じ本体の暫定実装で、3.4 / 3.5 で共通のオプション処理にまとめて両方を置き換え、重複を残さないこと。選択後の `--help` / `-h` / `--version` も現状は UNKNOWN_OPTION になるため 3.4 / 3.5 で直す
- 3.4: オプションの入口は前段・本走査とも `scan_option` 1 つ（`--…` → `scan_long_option`、`-x…` → 暫定の `scan_short_option`）。3.5 は `scan_short_option` だけを置き換えればよい。値の消費は `scan_option_value`（`=` の後ろ、または次のトークンをそのまま）を共用できる。テストでは値の参照の前に `NAOS_REQUIRE_INT_EQ` で状態を確かめる（状態違いのまま参照するとライブラリの `assert` でプロセスが落ちるため）
- 3.5: design.md Testing Strategy（short）の「`-vj4` で `j` に `4`」はフィクスチャ（`-j` は上限 8 の COUNT）と矛盾するため、`-vjk4` → `k=["4"]` に読み替え、`-vj4` は `'4'` の UNKNOWN_OPTION として固定した（design.md は未修正）。`h`・不明な文字・VALUE/LIST の後はそのトークンの残りを見ない。`-xh` の `h` は見られない（保留エラーになる）。9.1 との関係は 3.6 の回復走査で扱う
- 3.6: 回復走査（`scan_recover`）は設計の「エラーのトークンの次から」ではなく保留エラーのトークン自身から探す（コントローラ判断。`tool help --help` / `help copy -h` / `help --version` を 9.1・9.4 どおり HELP / VERSION にするため。`--` 以降は対象外。design.md は未修正）。help コマンドの経路以外ではエラーのトークンが要求トークンと完全一致しないため他の挙動は変わらない。エラー後のトークンは解釈しないので `--bad -o -h` は HELP、`tool --bad build --help` は program 全体の HELP
