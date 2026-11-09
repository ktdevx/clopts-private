# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts を共有ライブラリとして配る（または OS のパッケージとして入る）と、利用側の実行ファイルは、ビルドしたときとは違う版の `libclopts.so` / `.dylib` / `.dll` と組み合わされて動く。ABI が黙って壊れると、再ビルドしていない利用側が実行時に異常終了したり、静かに誤った結果を返したりする。保守者は「この変更は ABI を壊すか」をレビューの目だけで判断していて、公開シンボルの出し漏れや内部シンボルの漏れ、構造体の配置の変化を見落とす。

**現状**: 2026-11-08 の改訂で、共有ライブラリと ABI 互換を範囲に入れた。project-foundation の更新で、静的／共有の選択、`CLOPTS_API`、visibility を hidden にする設定、VERSION / SOVERSION が入る。core-parser の更新で、公開 API がビルダー + 不透明ハンドルになる。ただし、ABI の方針を書いた文書も、公開シンボルの一覧も、自動で検査する仕組みもない。リリースタグもまだない（現在は 0.1.0）。

**何を変えるか**: ABI の方針を文書にする（互換の保証は 1.0.0 から。SOVERSION は 0.x の間は `0.<minor>`、1.0.0 から `1`。互換を保つ変更と壊す変更の一覧、壊すときに SOVERSION を上げる手順、macOS の `compatibility_version` / `current_version` の方針）。公開シンボルの一覧（例: `abi/clopts.symbols`）をリポジトリに置き、共有ビルドの CTest で、実際に export されているシンボルと過不足なく一致することを全 OS で検査する（Linux は `nm -D --defined-only`、macOS は `nm -gU`、MSVC は `dumpbin /exports`、MinGW は `objdump -p` / `llvm-nm` の出力を `cmake -P` スクリプトで正規化して比べる）。全公開関数に `CLOPTS_API` が付いていること、公開ヘッダが C99 と C++ の両方からコンパイルできることを検査する（2026-11-09 に、構造体・共用体の定義がないことの検査を外した。公開構造体は定義してよく、定義した後は配置を凍結する）。Linux の CI ジョブで、直近のリリースタグを同じコンパイラと `-g` でその場でビルドして基準にし、`abidiff --headers-dir1/--headers-dir2` で比べ、互換のない変更（ビット 8）だけで失敗させる（関数の追加は一覧の照合で管理する。リリースタグがないか 1.0.0 より前ならジョブは飛ばす）。検査は公開ヘッダとビルドされた共有ライブラリだけを対象にし、外部依存は増やさない。

**範囲外**: ELF のシンボルバージョニング（version script）、ABI の前方互換、0.x の間の互換、パッケージマネージャ向けの設定、静的ライブラリの ABI、`CLOPTS_API`・visibility・VERSION / SOVERSION の設定そのもの（project-foundation）、公開 API の設計（core-parser ほか）、利用者向けの取り込み手順とリリース作業そのもの（docs-and-release）。

詳細は同ディレクトリの `brief.md` を参照。

## Introduction

abi-compat は、CLOpts を共有ライブラリとして配るときに、利用側の実行ファイルを再ビルドしないまま新しい版のライブラリに差し替えても壊れないこと（ABI の後方互換）を、方針と自動の検査で支える。方針は保守者と利用者向けの文書にまとめ、互換の保証が始まる版、SOVERSION の規則、互換を保つ変更と壊す変更の区別、壊すときの手順を定める。検査は、公開シンボルの一覧と実際に export されているシンボルの照合、公開ヘッダの静的な検査（公開関数に export の印があること、C と C++ の両方からコンパイルできること）、直近のリリースとの ABI の比較の 3 種類で、いずれもレビューの目に頼らず、CI で変更を検出する。

検査は利用側から見える成果物（公開ヘッダとビルドされた共有ライブラリ）だけを対象にし、ライブラリ内部の実装には依存しない。

本文書の主語は次のとおり。
- the ABI policy document: 本 spec が用意する ABI 方針の文書。
- the symbol check: 公開シンボル一覧と、共有ライブラリが実際に export しているシンボルを照合するテスト。
- the header check: 公開ヘッダの静的検査とコンパイル検査を行うテスト。
- the ABI comparison job: 直近のリリースと現在のソースの ABI を比べる CI ジョブ。

## Boundary Context

- **In scope**: ABI 方針の文書（互換の保証の開始版、SOVERSION の規則、macOS の互換版・現在版の方針、互換を保つ変更と壊す変更の一覧、壊すときの手順、公開関数を足すときの手順）、リポジトリに置く公開シンボル一覧、共有ビルドでの全 OS（Linux、macOS、Windows の MSVC と MinGW）のシンボル照合、公開ヘッダの静的検査（`CLOPTS_API` の付け漏れ）、公開ヘッダの C99 と C++ でのコンパイル検査、Linux の CI での直近のリリースとの ABI 比較（abidiff を使う）。
- **Out of scope**: ELF のシンボルバージョニング、ABI の前方互換（新しい利用側を古いライブラリで動かすこと）、0.x の間の互換、パッケージマネージャ向けの設定、静的ライブラリの ABI（静的版は再リンクが前提）、比較の基準となるデータ（ABI の記述ファイル）のリポジトリへの保存。
- **Adjacent expectations**:
  - project-foundation が `CLOPTS_API` の定義、visibility を hidden にする設定、VERSION / SOVERSION の設定、各 OS の共有版の CI ジョブを提供する。本 spec はそれらを検査するが、設定そのものは変えない。本 spec の文書に書く SOVERSION の規則は、project-foundation が実装している規則（0.x の間は `0.<minor>`、1.0.0 から `<major>`）と一致させる。
  - core-parser、help-and-messages、platform-argv が公開 API を設計する。これらの spec が公開関数を足すときは、本 spec の手順に従って公開シンボル一覧を更新する。
  - docs-and-release が利用者向けの取り込み手順とリリース作業を担う。リリース手順は本 spec の検査と基準の扱いを参照する。
  - 検査は外部依存を増やさない。シンボルの照合とヘッダの検査は、CMake と各 OS の標準のツールだけで動く。ABI 比較のツールは Linux の CI ジョブでだけ使う。

## Requirements

### Requirement 1: ABI 方針の文書
**Objective:** As a CLOpts の保守者と利用者, I want ABI の互換をいつから何について保証し、どの変更が互換を壊すのかを文書で確認したい, so that 変更のレビューと、共有ライブラリを差し替えてよいかの判断を、同じ基準で行える

#### Acceptance Criteria
1. The ABI policy document shall 共有ライブラリの ABI の後方互換を 1.0.0 から保証し、0.x の間は minor ごとに ABI を壊してよいことを示す。
2. The ABI policy document shall 保証の対象が共有ライブラリの後方互換（古い版に対してビルドした利用側が、同じ SOVERSION の新しい版で再ビルドせずに動くこと）であり、前方互換と静的ライブラリは対象外であることを示す。
3. The ABI policy document shall SOVERSION の規則として、0.x の間は `0.<minor>`、1.0.0 以降は `<major>` であることを示す。
4. The ABI policy document shall Windows の DLL のファイル名には版が入らないことと、そのため Windows では版の取り違えをファイル名で防げないことを示す。
5. The ABI policy document shall macOS の共有ライブラリの互換版（compatibility version）と現在版（current version）をどの版番号から決めるかの方針を示す。
6. The ABI policy document shall 互換を保つ変更の一覧として、少なくとも公開関数の追加、列挙の末尾への値の追加、新しい公開構造体の追加（追加した後は配置を凍結する）を挙げる。
7. The ABI policy document shall 互換を壊す変更の一覧として、少なくとも公開関数の削除、公開関数のシグネチャの変更、既存の列挙値の変更や並べ替え、既存の公開構造体・共用体の配置の変更（メンバーの追加・削除・並べ替え・型の変更）を挙げる。
8. The ABI policy document shall 互換を壊す変更を入れるときの手順として、major（0.x の間は minor）を上げて SOVERSION を変えることを示す。
9. The ABI policy document shall 公開関数を追加・削除するときの手順として、同じ変更の中で公開シンボル一覧を更新することを示す。
10. The ABI policy document shall 本 spec の各検査が何を検出し、どの構成（共有ビルドだけか、どの OS か、どの版から有効か）で動くかを示す。
11. The ABI policy document shall 日本語で書き、識別子とコマンドは英語のまま示す。

### Requirement 2: 公開シンボル一覧
**Objective:** As a CLOpts の保守者, I want 公開するシンボルの一覧をリポジトリで管理したい, so that 公開 API の増減が差分としてレビューに現れる

#### Acceptance Criteria
1. The symbol check shall リポジトリに置かれた 1 つの公開シンボル一覧を、公開するシンボルの唯一の基準として使う。
2. The symbol check shall 公開シンボル一覧に、OS によらない同じシンボル名（接頭辞 `clopts_` を持つ名前）を 1 行に 1 つずつ並べた形式を受け付ける。
3. If 公開シンボル一覧に重複した名前、または接頭辞 `clopts_` を持たない名前が含まれるとき, the symbol check shall 失敗し、該当する名前を報告する。
4. The symbol check shall 公開シンボル一覧の空行と注釈の行を無視する。

### Requirement 3: export されたシンボルとの照合
**Objective:** As a CLOpts の保守者, I want 共有ライブラリが実際に export しているシンボルが公開シンボル一覧と過不足なく一致することを、全 OS で自動で確かめたい, so that 公開関数の出し漏れと内部シンボルの漏れを、レビューで見落としても CI で検出できる

#### Acceptance Criteria
1. Where 共有ライブラリとしてビルドされているとき, the symbol check shall CTest のテスト（ラベル `abi`）として登録され、ビルドされた共有ライブラリが export しているシンボルと公開シンボル一覧を照合する。
2. Where 静的ライブラリとしてビルドされているとき, the symbol check shall 照合のテストを登録しない。
3. The symbol check shall Linux（GCC、Clang）、macOS（Apple Clang）、Windows（MSVC、MinGW-w64）のそれぞれの共有ビルドで動く。
4. The symbol check shall OS ごとの表記の違い（macOS の先頭の `_` など）と、ツールチェーンが自動で加えるシンボル（初期化・終了処理など、CLOpts のソースに由来しないもの）を取り除いたうえで比べる。
5. If 公開シンボル一覧にあるシンボルが export されていないとき, the symbol check shall 失敗し、不足しているシンボル名をすべて報告する。
6. If 公開シンボル一覧にないシンボルが export されているとき, the symbol check shall 失敗し、余分なシンボル名をすべて報告する。
7. When 照合が失敗したとき, the symbol check shall 公開シンボル一覧の場所と、更新するか export をやめるかを選ぶよう促す案内を報告に含める。
8. If export されたシンボルを読み出すためのツールが見つからないか、実行に失敗したとき, the symbol check shall 成功扱いにせず失敗し、見つからなかったツールまたは失敗した理由を報告する。
9. When export されたシンボルと公開シンボル一覧が過不足なく一致するとき, the symbol check shall 成功する。

### Requirement 4: 公開ヘッダの静的検査
**Objective:** As a CLOpts の保守者, I want 公開ヘッダに export の印のない関数が紛れ込んだら自動で気づきたい, so that 関数の出し漏れを防げる

#### Acceptance Criteria
1. The header check shall 公開ヘッダのディレクトリにあるすべての公開ヘッダを対象にし、新しく足された公開ヘッダも登録し直さずに対象に含める。
2. （廃止、2026-11-09）公開構造体を凍結する規則（steering の tech.md）に変わり、構造体・共用体の定義は検査せず、既存の公開構造体の配置の変化は 1.0.0 以降の ABI 比較（Requirement 6）で検出する。
3. （廃止、2026-11-09）構造体・共用体の定義を検査しなくなったため、前方宣言を特に許す規則は要らなくなり、前方宣言も定義も同じく検査の対象外になった。
4. If 公開ヘッダにある関数の宣言に `CLOPTS_API` が付いていないとき, the header check shall 失敗し、該当するヘッダと関数名を報告する。
5. The header check shall 公開ヘッダの中で定義される `static inline` の関数と関数ポインタ型の宣言を、`CLOPTS_API` の検査の対象から外す。
6. The header check shall コメントの中の記述を検査の対象から外す。
7. The header check shall 共有ビルドと静的ビルドのどちらでも CTest のテスト（ラベル `abi`）として登録され、同じ結果を返す。
8. When 公開ヘッダに違反がないとき, the header check shall 成功する。

### Requirement 5: C と C++ からのコンパイル検査
**Objective:** As a CLOpts を C または C++ のプログラムから使う開発者, I want 公開ヘッダがどちらの言語の利用側からもそのままコンパイルできてほしい, so that C++ のプロジェクトにも追加の設定なしで取り込める

#### Acceptance Criteria
1. The header check shall 各公開ヘッダを単独で include した翻訳単位を、C99 としてコンパイルできることを確かめる。
2. The header check shall 各公開ヘッダを単独で include した翻訳単位を、C++ としてコンパイルできることを確かめる。
3. The header check shall C++ の翻訳単位で、公開関数が C のリンケージで宣言されていること（C++ の名前修飾を受けないこと）を確かめる。
4. The header check shall CLOpts のビルドと同じ警告の設定（警告をエラーとして扱う設定を含む）でコンパイルする。
5. The header check shall 対応環境（Linux の GCC と Clang、macOS の Apple Clang、Windows の MSVC と MinGW-w64）のそれぞれで動く。
6. If C++ のコンパイラが使えない構成でビルドされたとき, the header check shall C++ のコンパイル検査を、失敗ではなく実行されなかったテストとして報告する。
7. If 公開ヘッダが C99 または C++ としてコンパイルできないとき, the header check shall 失敗し、該当するヘッダと言語を報告する。
8. The header check shall CLOpts を利用する側のプロジェクトに C++ を要求しない（C++ の検査はテストを有効にしたときだけ行う）。

### Requirement 6: 直近のリリースとの ABI 比較
**Objective:** As a CLOpts の保守者, I want 1.0.0 以降の変更が直近のリリースとの ABI 互換を壊していないかを CI で確かめたい, so that 構造体の配置や関数のシグネチャの変化など、シンボル名の照合では分からない非互換を、リリース前に検出できる

#### Acceptance Criteria
1. The ABI comparison job shall Linux の CI で、main への push と pull request のときに動く。
2. The ABI comparison job shall 比較の基準として、現在の版と同じ SOVERSION を持つリリースタグのうち最も新しいものを使う。
3. If 基準にできるリリースタグがないとき、または現在の版が 1.0.0 より前のとき, the ABI comparison job shall 比較を行わず、比較を飛ばした理由をジョブの出力に示して成功する。
4. The ABI comparison job shall 基準のリリースタグを、現在のソースと同じコンパイラと同じビルド設定（デバッグ情報付き）でその場でビルドして基準を作り、基準のデータをリポジトリに置かない。
5. The ABI comparison job shall 比較で、両方の版の公開ヘッダを公開 API の範囲として指定し、公開ヘッダに出てこない内部の型とシンボルの差を判定に含めない。
6. If 比較で互換のない変更が検出されたとき, the ABI comparison job shall 失敗し、検出された変更の内容をジョブの出力に示す。
7. When 比較で検出された変更が関数の追加など互換を保つ変更だけのとき, the ABI comparison job shall 成功する。
8. If 比較のツールそのものが実行に失敗したとき, the ABI comparison job shall 成功扱いにせず失敗し、失敗したことをジョブの出力に示す。
9. The ABI comparison job shall 他の CI ジョブ（各環境のビルドとテスト）の構成を変えずに、独立したジョブとして結果を示す。
