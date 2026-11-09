# Brief: docs-and-release

## Problem
CLOpts を使おうとする開発者は、取り込み方（`FetchContent` / `add_subdirectory` / `find_package`、静的と共有の選び方）、ビルダー API での宣言の書き方、結果の読み方、ヘルプとエラーの表示の仕方、Windows での注意を、ソースを読まずに知りたい。また、保守者はリリースの手順（版番号の付け方、SOVERSION、ABI 検査の基準、タグ、変更履歴）が決まっていないと、安定した版を利用者に示せない。Monde では `docs/` と `docs/dev/release.md` に相当する文書があったが、あれはアプリケーション向けで、ライブラリの文書（API リファレンス、取り込み手順、サンプル）はこれから書く。

## Current State
`README.md` は `# CLOpts` の 1 行だけ。project-foundation が install/export（静的・共有の両方）を用意し、core-parser / help-and-messages / platform-argv が API（ビルダー + 不透明ハンドル）を提供し、abi-compat が ABI の方針と検査を用意した時点で、文書とサンプルがない状態になる。Monde の `.kiro/specs/docs-and-release` と `docs/dev/release.md`、`.github/workflows/release.yml` が参考になる（ただし Monde はバイナリ配布、CLOpts はソース配布）。

## Desired Outcome
- `README.md` に、概要（C++ の同名プロジェクト Sirraide/clopts との区別を含む）、特徴、最小のサンプル、取り込み方 3 通り、静的と共有の選び方（`CLOPTS_BUILD_SHARED`、install 経路では prefix ごとに 1 つの形式）、ライセンス（Monde 由来の表示を含む）が書かれている。
- `docs/` に API リファレンス（ハンドルの種類と寿命、ビルダー関数、最初のエラーを記録する方式、結果とエラーのアクセサ、エラー種別、メモリの所有、解析規則の一覧: `--`、`-` 単体、並べ替え、`--no-x`、カウント、環境変数の優先順位）と、Windows での注意（UTF-8 argv の取得、`wmain`、`activeCodePage`、DLL 名に版が入らないこと、PATH 上の DLL の衝突）がある。ABI の方針（abi-compat が所有する文書）への導線がある。
- `examples/` にサブコマンドあり・なしの 2 つのサンプルプログラムがあり、CI でビルドされ、ヘルプ出力が文書の記載と一致することがテストで検査される。
- 利用側プロジェクトを模した統合テストが、`FetchContent`（または `add_subdirectory`）と `install` → `find_package(CLOpts CONFIG)` の両経路で、静的版と共有版のそれぞれについて CI で通る。
- リリース手順（`docs/dev/release.md`）とタグ付けのワークフロー（タグから GitHub Release を作り、ソースアーカイブと変更履歴を添える）が決まっている。リリース前の ABI 検査（公開シンボル一覧、abidiff）の確認と、SOVERSION を上げるときの判断手順が書かれている。Naos の版更新手順も書かれている。

## Approach
Monde の文書構成（`docs/README.md`、`docs/dev/release.md`、`tests/docs/docs_check.cmake` による文書とコードの整合検査）を参考にする。文書中のサンプル出力は、サンプルプログラムの実行結果と CTest で突き合わせる（Monde の `monde-check` マーカー方式）。リリースはソース配布のみとし、事前ビルド済みバイナリは配らない。共有ライブラリはそれぞれの利用環境でビルドしてもらう。

## Scope
- **In**: README、`docs/`（API リファレンス、解析規則、取り込み手順、Windows の注意、Naos 更新手順、リリース手順）、`examples/`（2 サンプル）、文書とコードの整合検査（CTest）、取り込み 2 経路 × 静的／共有の統合テスト、リリースワークフロー、CHANGELOG の形式
- **Out**: ライブラリの機能追加・変更、パッケージマネージャ（vcpkg / Conan）への登録、単一ファイル化、事前ビルド済みバイナリ、Monde 側の置き換え手順

## Boundary Candidates
- 利用者向け文書（README、API リファレンス、解析規則）
- サンプルプログラムと文書との整合検査
- 取り込み経路の統合テスト
- リリース手順とワークフロー

## Out of Boundary
- API の設計と挙動（core-parser、help-and-messages、platform-argv）
- ABI の方針文書と自動検査そのもの（abi-compat。ここではリリース手順から参照する）
- ビルド基盤そのもの（project-foundation。ここでは利用側から見た検証だけを行う）

## Upstream / Downstream
- **Upstream**: core-parser、help-and-messages、platform-argv（文書化と例示の対象）、project-foundation（install/export、静的／共有）、abi-compat（ABI の方針と検査）。参照元は Monde の `docs/`、`docs/dev/release.md`、`tests/docs/`、`.github/workflows/release.yml`、spec `docs-and-release`
- **Downstream**: 将来の機能追加 spec（文書と CHANGELOG の更新規約に従う）、Monde 側の置き換え spec（取り込み手順を参照する）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: project-foundation（install/export の挙動が変わると統合テストに波及する）、abi-compat（リリース手順と ABI 基準の扱い）

## Constraints
- 文書は日本語を主とする（Monde と同じ。英語版 README の要否は要件で決める）。コード中の識別子とヘルプ出力は英語
- 文書中のサンプル出力は実行結果と自動で突き合わせる（手で書いた出力が古くなることを防ぐ）
- リリースはソース配布のみ。版番号は semver。pre-1.0 の間は minor で API / ABI 互換を壊してよいこと、1.0.0 から ABI 後方互換を保証すること（SOVERSION の規則は abi-compat に従う）を明記する
- ライセンス表示: MIT、`The CLOpts Authors` のみ（Monde の著作権表示は併記しない）。Naos はテスト専用なので THIRD_PARTY_NOTICES に載せない
