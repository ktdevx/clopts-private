# Requirements Document

## Project Description (Input)
**誰が困っているか**: CLOpts を共有ライブラリとして配る（または OS のパッケージとして入る）と、利用側の実行ファイルは、ビルドしたときとは違う版の `libclopts.so` / `.dylib` / `.dll` と組み合わされて動く。ABI が黙って壊れると、再ビルドしていない利用側が実行時に異常終了したり、静かに誤った結果を返したりする。保守者は「この変更は ABI を壊すか」をレビューの目だけで判断していて、公開シンボルの出し漏れや内部シンボルの漏れ、構造体の配置の変化を見落とす。

**現状**: 2026-11-08 の改訂で、共有ライブラリと ABI 互換を範囲に入れた。project-foundation の更新で、静的／共有の選択、`CLOPTS_API`、visibility を hidden にする設定、VERSION / SOVERSION が入る。core-parser の更新で、公開 API がビルダー + 不透明ハンドルになる。ただし、ABI の方針を書いた文書も、公開シンボルの一覧も、自動で検査する仕組みもない。リリースタグもまだない（現在は 0.1.0）。

**何を変えるか**: ABI の方針を文書にする（互換の保証は 1.0.0 から。SOVERSION は 0.x の間は `0.<minor>`、1.0.0 から `1`。互換を保つ変更と壊す変更の一覧、壊すときに SOVERSION を上げる手順、macOS の `compatibility_version` / `current_version` の方針）。公開シンボルの一覧（例: `abi/clopts.symbols`）をリポジトリに置き、共有ビルドの CTest で、実際に export されているシンボルと過不足なく一致することを全 OS で検査する（Linux は `nm -D --defined-only`、macOS は `nm -gU`、MSVC は `dumpbin /exports`、MinGW は `objdump -p` / `llvm-nm` の出力を `cmake -P` スクリプトで正規化して比べる）。公開ヘッダに構造体・共用体の定義がないこと、全公開関数に `CLOPTS_API` が付いていること、公開ヘッダが C99 と C++ の両方からコンパイルできることを検査する。Linux の CI ジョブで、直近のリリースタグを同じコンパイラと `-g` でその場でビルドして基準にし、`abidiff --headers-dir1/--headers-dir2` で比べ、互換のない変更（ビット 8）だけで失敗させる（関数の追加は一覧の照合で管理する。リリースタグがないか 1.0.0 より前ならジョブは飛ばす）。検査は公開ヘッダとビルドされた共有ライブラリだけを対象にし、外部依存は増やさない。

**範囲外**: ELF のシンボルバージョニング（version script）、ABI の前方互換、0.x の間の互換、パッケージマネージャ向けの設定、静的ライブラリの ABI、`CLOPTS_API`・visibility・VERSION / SOVERSION の設定そのもの（project-foundation）、公開 API の設計（core-parser ほか）、利用者向けの取り込み手順とリリース作業そのもの（docs-and-release）。

詳細は同ディレクトリの `brief.md` を参照。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
