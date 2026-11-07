# Brief: docs-and-release

## Problem
CLOpts を使おうとする開発者は、取り込み方（`FetchContent` / `add_subdirectory` / `find_package`）、宣言テーブルの書き方、結果の読み方、ヘルプとエラーの表示の仕方、Windows での注意を、ソースを読まずに知りたい。また、保守者はリリースの手順（版番号の付け方、タグ、変更履歴）が決まっていないと、安定した版を利用者に示せない。Monde では `docs/` と `docs/dev/release.md` に相当する文書があったが、あれはアプリケーション向けで、ライブラリの文書（API リファレンス、取り込み手順、サンプル）はこれから書く。

## Current State
`README.md` は `# CLOpts` の 1 行だけ。project-foundation が install/export を用意し、core-parser / help-and-messages / platform-argv が API を提供した時点で、文書とサンプルがない状態になる。Monde の `.kiro/specs/docs-and-release` と `docs/dev/release.md`、`.github/workflows/release.yml` が参考になる（ただし Monde はバイナリ配布、CLOpts はソース配布）。

## Desired Outcome
- `README.md` に、概要（C++ の同名プロジェクト Sirraide/clopts との区別を含む）、特徴、最小のサンプル、取り込み方 3 通り、ライセンス（Monde 由来の表示を含む）が書かれている。
- `docs/` に API リファレンス（宣言型、関数、エラー種別、メモリ所有、解析規則の一覧: `--`、`-` 単体、並べ替え、`--no-x`、カウント、環境変数の優先順位）と、Windows での注意（UTF-8 argv の取得、`wmain`、`activeCodePage`）がある。
- `examples/` にサブコマンドあり・なしの 2 つのサンプルプログラムがあり、CI でビルドされ、ヘルプ出力が文書の記載と一致することがテストで検査される。
- 利用側プロジェクトを模した統合テストが、`FetchContent`（または `add_subdirectory`）と `install` → `find_package(CLOpts CONFIG)` の両経路で CI で通る。
- リリース手順（`docs/dev/release.md`）とタグ付けのワークフロー（タグから GitHub Release を作り、ソースアーカイブと変更履歴を添える）が決まっている。Naos の版更新手順も書かれている。

## Approach
Monde の文書構成（`docs/README.md`、`docs/dev/release.md`、`tests/docs/docs_check.cmake` による文書とコードの整合検査）を参考にする。文書中のサンプル出力は、サンプルプログラムの実行結果と CTest で突き合わせる（Monde の `monde-check` マーカー方式）。リリースはソース配布のみとし、事前ビルド済みバイナリは配らない（MSVC の CRT 不一致の問題を避ける）。

## Scope
- **In**: README、`docs/`（API リファレンス、解析規則、取り込み手順、Windows の注意、Naos 更新手順、リリース手順）、`examples/`（2 サンプル）、文書とコードの整合検査（CTest）、取り込み 2 経路の統合テスト、リリースワークフロー、CHANGELOG の形式
- **Out**: ライブラリの機能追加・変更、パッケージマネージャ（vcpkg / Conan）への登録、単一ファイル化、事前ビルド済みバイナリ、Monde 側の置き換え手順

## Boundary Candidates
- 利用者向け文書（README、API リファレンス、解析規則）
- サンプルプログラムと文書との整合検査
- 取り込み経路の統合テスト
- リリース手順とワークフロー

## Out of Boundary
- API の設計と挙動（core-parser、help-and-messages、platform-argv）
- ビルド基盤そのもの（project-foundation。ここでは利用側から見た検証だけを行う）

## Upstream / Downstream
- **Upstream**: core-parser、help-and-messages、platform-argv（文書化と例示の対象）、project-foundation（install/export）。参照元は Monde の `docs/`、`docs/dev/release.md`、`tests/docs/`、`.github/workflows/release.yml`、spec `docs-and-release`
- **Downstream**: 将来の機能追加 spec（文書と CHANGELOG の更新規約に従う）、Monde 側の置き換え spec（取り込み手順を参照する）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: project-foundation（install/export の挙動が変わると統合テストに波及する）

## Constraints
- 文書は日本語を主とする（Monde と同じ。英語版 README の要否は要件で決める）。コード中の識別子とヘルプ出力は英語
- 文書中のサンプル出力は実行結果と自動で突き合わせる（手で書いた出力が古くなることを防ぐ）
- リリースはソース配布のみ。版番号は semver、pre-1.0 の間は minor で互換性を壊してよいことを明記する
- ライセンス表示: MIT、`The CLOpts Authors` のみ（Monde の著作権表示は併記しない）。Naos はテスト専用なので THIRD_PARTY_NOTICES に載せない
