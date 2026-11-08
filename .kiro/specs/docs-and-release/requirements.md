# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts を使おうとする開発者は、取り込み方（`FetchContent` / `add_subdirectory` / `find_package`、静的と共有の選び方）、ビルダー API での宣言の書き方、結果とエラーの読み方、ヘルプとエラーの表示の仕方、Windows での注意を、ソースを読まずに知りたい。保守者は、リリースの手順（版番号の付け方、SOVERSION、ABI 検査の基準、タグ、変更履歴）が決まっていないと、安定した版を利用者に示せない。

**現状**: `README.md` は `# CLOpts` の 1 行だけ。project-foundation が install/export（静的・共有の両方）を、core-parser / help-and-messages / platform-argv が API（ビルダー + 不透明ハンドル）を、abi-compat が ABI の方針と検査を用意したが、利用者向けの文書、サンプル、取り込み経路の統合テスト、リリース手順はない。Monde には `docs/`、`docs/dev/release.md`、`tests/docs/docs_check.cmake`、`.github/workflows/release.yml` があるが、アプリケーション向け（バイナリ配布）であり、ライブラリ向け（ソース配布）の文書はこれから書く。

**何を変えるか**: `README.md` に概要（C++ の同名プロジェクト Sirraide/clopts との区別を含む）、特徴、最小のサンプル、取り込み方 3 通り、静的と共有の選び方（`CLOPTS_BUILD_SHARED`、`find_package(CLOpts COMPONENTS static|shared)`）、ライセンスを書く。`docs/` に API リファレンス（ハンドルの種類と寿命、ビルダー関数、最初のエラーを記録する方式、結果とエラーのアクセサ、エラー種別、メモリの所有、解析規則の一覧: `--`、`-` 単体、並べ替え、`--no-x`、カウント、環境変数の優先順位）、Windows での注意（UTF-8 argv の取得、`wmain`、`activeCodePage`、DLL 名に版が入らないこと、PATH 上の DLL の衝突）、Naos の版更新手順を置き、abi-compat の ABI 方針文書への導線を張る。`examples/` にサブコマンドあり・なしの 2 サンプルを置き、CI でビルドし、文書中のサンプル出力を実行結果と CTest で突き合わせる（Monde の `monde-check` マーカー方式）。利用側プロジェクトを模した統合テストを、`FetchContent`（または `add_subdirectory`）と `install` → `find_package(CLOpts CONFIG)` の 2 経路 × 静的／共有で CI に通す。リリース手順（`docs/dev/release.md`。semver、pre-1.0 は minor で互換を壊してよく 1.0.0 から ABI 後方互換を保証、リリース前の ABI 検査の確認、SOVERSION を上げる判断手順）と、タグから GitHub Release を作りソースアーカイブと変更履歴を添えるワークフロー、CHANGELOG の形式を決める。文書は日本語が主で、コード中の識別子とヘルプ出力は英語。ライセンス表示は MIT、`The CLOpts Authors` のみ（Monde の著作権表示は併記せず、Naos はテスト専用なので THIRD_PARTY_NOTICES に載せない）。

**範囲外**: ライブラリの機能追加・変更、API の設計と挙動（core-parser、help-and-messages、platform-argv）、ABI の方針文書と自動検査そのもの（abi-compat）、ビルド基盤そのもの（project-foundation）、パッケージマネージャ（vcpkg / Conan）への登録、単一ファイル化、事前ビルド済みバイナリ、Monde 側の置き換え手順。

詳細は同ディレクトリの `brief.md` を参照。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
