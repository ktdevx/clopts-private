# Brief: abi-compat

## Problem
CLOpts を共有ライブラリとして配る（または OS のパッケージとして入る）と、利用側の実行ファイルは、ビルドしたときとは違う版の `libclopts.so` / `.dylib` / `.dll` と組み合わされて動く。ABI が黙って壊れると、再ビルドしていない利用側が実行時に異常終了したり、静かに誤った結果を返したりする。保守者は「この変更は ABI を壊すか」をレビューの目だけで判断していて、公開シンボルの出し漏れや内部シンボルの漏れ、構造体の配置の変化を見落とす。

## Current State
2026-11-08 の改訂で、共有ライブラリと ABI 互換を範囲に入れた。project-foundation の更新で、静的／共有の選択、`CLOPTS_API`、visibility を hidden にする設定、VERSION / SOVERSION が入る。core-parser の更新で、公開 API がビルダー + 不透明ハンドルになる。ただし、ABI の方針を書いた文書も、公開シンボルの一覧も、自動で検査する仕組みもない。リリースタグもまだない（現在は 0.1.0）。

## Desired Outcome
- ABI の方針が文書になっている。互換の保証は 1.0.0 から。SOVERSION は 0.x の間は `0.<minor>`、1.0.0 から `1`。互換を保つ変更（関数の追加、列挙の末尾への追加）と壊す変更（関数の削除、シグネチャの変更、列挙値の変更、公開ヘッダへの構造体定義の追加）の一覧がある。壊すときの手順（SOVERSION を上げる）も書かれている。
- 公開シンボルの一覧（例: `abi/clopts.symbols`）がリポジトリにある。共有ビルドの CTest が、実際に export されているシンボルと過不足なく一致することを全 OS で検査する。Linux は `nm -D --defined-only` で `_init` などを除外し、macOS は `nm -gU` で先頭の `_` を正規化し、MSVC は `dumpbin /exports`、MinGW は `objdump -p` / `llvm-nm` を使う。
- 公開ヘッダに構造体・共用体の定義がないこと（不透明型の前方宣言だけであること）と、全公開関数に `CLOPTS_API` が付いていることを検査する。
- 公開ヘッダが C99 と C++ の両方の利用側からコンパイルできることを検査する。
- Linux の CI ジョブで、直近のリリースタグをその場で同じコンパイラと `-g` でビルドして基準にし、`abidiff --headers-dir1/--headers-dir2` で比べる。終了コードのうち、互換のない変更（ビット 8）だけで失敗させる。関数の追加（ビット 4）は許し、追加は一覧の照合で管理する。リリースタグがまだないか 1.0.0 より前のときは、このジョブは飛ばす。
- macOS の `compatibility_version` と `current_version` の方針（minor ごとに `MACHO_COMPATIBILITY_VERSION` を上げるか）が決まっている。

## Approach
検査は利用側から見える成果物（公開ヘッダとビルドされた共有ライブラリ）だけを対象にし、走査器の実装には依存しない。シンボルの照合は、外部依存を増やさないよう CMake スクリプト（`cmake -P`）で書き、各 OS の標準ツールの出力を正規化して一覧と比べる。abidiff の基準は、XML をリポジトリに置かずに CI で作り直す（libabigail の版が変わると XML が読めなくなるため）。abigail-tools は Ubuntu の universe から入れる。

## Scope
- **In**: ABI 方針の文書（`docs/abi.md` 相当）、公開シンボルの一覧と照合（CTest、共有ビルドのときだけ）、公開ヘッダの構造体定義と `CLOPTS_API` 漏れの検査、C++ からのヘッダのコンパイル検査、abidiff の CI ジョブ、新しい公開関数を足すときの手順（一覧の更新）
- **Out**: ELF のシンボルバージョニング（version script）、ABI の前方互換（新しい利用側を古いライブラリで動かすこと）、0.x の間の互換、パッケージマネージャ向けの設定、静的ライブラリの ABI（静的版は再リンクが前提）

## Boundary Candidates
- ABI 方針と SOVERSION の規則
- 公開シンボル一覧と照合スクリプト
- 公開ヘッダの静的検査（構造体定義なし、`CLOPTS_API` あり、C/C++ 両対応）
- abidiff による版の間の比較（CI ジョブ）

## Out of Boundary
- `CLOPTS_API` の定義、visibility の設定、VERSION / SOVERSION の設定そのもの（project-foundation）
- 公開 API の設計（core-parser、help-and-messages、platform-argv）
- 利用者向けの取り込み手順とリリース作業そのもの（docs-and-release。ここでは基準の扱い方だけを決める）

## Upstream / Downstream
- **Upstream**: project-foundation（更新後: 共有ビルド、`CLOPTS_API`、SOVERSION）、core-parser（更新後: 公開 API の初期の一覧）
- **Downstream**: help-and-messages、platform-argv（公開関数を足すときに一覧を更新する）、docs-and-release（リリース手順で ABI 検査を参照する）

## Existing Spec Touchpoints
- **Extends**: なし（新しい境界）
- **Adjacent**: project-foundation（CI ワークフローとテスト登録の仕組みを共有する）

## Constraints
- 外部依存は増やさない。照合は CMake スクリプトと各 OS の標準ツールで行う。abidiff は Linux の CI ジョブだけで使う
- `dumpbin` は MSVC の開発者環境が要る（CI では `ilammy/msvc-dev-cmd` など）
- シンボルの照合は共有ビルドのときだけ行うので、CI では各 OS に共有版のジョブが必要（project-foundation の更新で用意する）
- 文書は日本語、識別子は英語。コーディングスタイルは Monde と同じ
