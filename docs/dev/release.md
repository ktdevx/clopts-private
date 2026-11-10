# リリースの手順（保守者向け）

この文書は、CLOpts の版番号の付け方、SOVERSION を上げるかどうかの判断、リリースの手順、リリースのワークフローの動き、失敗したときの戻し方、文書と変更履歴の更新の規約、Naos の版の更新手順、リリースのワークフローを試す方法を示す。SOVERSION の規則と、互換を保つ変更・壊す変更の区別は、利用者向けの章 [ABI の互換性](../abi.md)（`docs/abi.md`）を正とし、この文書には書き写さない。ABI の検査の読み方は [ABI の検査](abi-checks.md)（`docs/dev/abi-checks.md`）を参照する。

## 関係するファイル

| ファイル | 役割 |
|----------|------|
| `CMakeLists.txt` | `project(clopts VERSION X.Y.Z ...)` が唯一の版番号。SOVERSION とパッケージの互換規則はここから決まる |
| `cmake/CloptsLibrary.cmake` | 版から SOVERSION を計算して共有ライブラリに付ける（規則は `docs/abi.md`） |
| `CHANGELOG.md` | 変更履歴。`## [Unreleased]` と版ごとの節を持つ |
| `tests/docs/check_changelog.cmake` | 変更履歴の形式の検査（MODE=format）と、リリースのときのタグと版の照合とリリースノートの抜き出し（MODE=release） |
| `tests/docs/check_docs.cmake` | 文書の印（出力とソースの照合）、リンク、API の網羅の検査 |
| `abi/clopts.symbols` | 公開シンボル一覧（`docs/dev/abi-checks.md`） |
| `tests/abi/run_abidiff.cmake` | 直近のリリースとの abidiff による比較（CI の `abi-compare` ジョブ） |
| `.github/workflows/ci.yml` | 通常の CI。10 ジョブ（5 環境 × 静的・共有）と `abi-compare` |
| `.github/workflows/release.yml` | リリースのワークフロー。版のタグの push で起動し、GitHub Release を作る |
| `.gitattributes` | ソースアーカイブから除くパス（`ref` と `.gitmodules` の `export-ignore`） |
| `cmake/CloptsNaos.cmake` | テストに使う Naos の版の指定（タグとコミット SHA）。版の指定はここ 1 か所だけ |
| `docs/dev/naos-feedback.md` | Naos の不足や回避策の記録 |
| `README.md`、`docs/integration.md` | FetchContent の例（公開先の URL と `GIT_TAG`） |

## 版番号の規則

- 版番号は [Semantic Versioning](https://semver.org/lang/ja/) に従い、`major.minor.patch` の形をとる。互換を保つ機能の追加は minor、互換を保つ修正だけなら patch を上げる。
- 版のタグは `vX.Y.Z` の形にする（`v0.1.0`、`v1.4.2`）。先頭が 0 の数（`v01.2.3`）や、`-rc1` などの接尾辞は使わない。リリースのワークフローは `v[0-9]+.[0-9]+.[0-9]+` の形のタグでだけ起動する。
- 版番号を書く場所は、ルートの `CMakeLists.txt` の `project(clopts VERSION X.Y.Z ...)` の 1 か所だけである。ヘッダやほかの CMake ファイルに版番号を書き写さない（`clopts-config-version.cmake` も SOVERSION もここから作られる）。変更履歴の節の見出しと、文書の FetchContent の例の `GIT_TAG` は、リリースのときにこの版に合わせる。
- 1.0.0 より前は、minor を上げるときに API と ABI の互換を壊してよい。patch では壊さない。
- 1.0.0 からは、共有ライブラリの ABI の後方互換を保証する。互換を壊す変更は major を上げるときにだけ入れる。
- SOVERSION は版から決まり、その規則は `docs/abi.md` の「版と SOVERSION」に従う。SOVERSION を直接書き換えることはしない。

## SOVERSION を上げるかどうかの判断

リリースの前に、前回のリリースからの変更に互換を壊す変更が含まれるかを確かめ、版の上げ方を決める。SOVERSION は版から決まるので、判断の結果は「どの桁を上げるか」として表れる。

1. 前回のタグからの、公開ヘッダと公開シンボル一覧の差分を見る（`v0.1.0` は前回のタグに置き換える）。

   ```sh
   git fetch --tags
   git diff v0.1.0.. -- include/ abi/clopts.symbols
   ```

2. 変更履歴の `## [Unreleased]` の節に `### 互換性のない変更` があるかを見る。
3. 差分の中に、`docs/abi.md` の「互換を壊す変更」に当たるものがあるかを判断する。差分から判断がつかないとき（型の配置の変化など）は、1.0.0 以降なら CI の `abi-compare` の結果（`docs/dev/abi-checks.md` の「`abi-compare`: abidiff の報告」）で確かめる。
4. 互換を壊す変更が含まれるなら、`docs/abi.md` の「互換を壊すときの手順」に従って major（0.x の間は minor）を上げる。含まれないなら、機能の追加があれば minor、修正だけなら patch を上げる。1.0.0 以降で、互換を壊す変更を patch や minor の版に入れてはいけない。
5. 互換を壊す変更を含むリリースでは、変更履歴の `### 互換性のない変更`（その版の節の最初の分類）に、何が変わったかと、利用側に必要な対応（呼び出しの書き換え、利用側の再ビルド、Windows での DLL の置き換えなど）を書く。

## リリースの手順

以下では、出す版を `X.Y.Z` と書く。コマンドはリポジトリのルートで実行する。

### 1. 版番号と変更履歴を更新する

1. ブランチを作り、`CMakeLists.txt` の `project(clopts VERSION X.Y.Z LANGUAGES C)` を新しい版にする。
2. `CHANGELOG.md` の `## [Unreleased]` の見出しを `## [X.Y.Z] - YYYY-MM-DD`（リリース日）に変え、その上に新しい空の `## [Unreleased]` の節を作る。版の節の中身が空行だけだと、リリースの検査が失敗する。
3. `README.md` と `docs/integration.md` の FetchContent の例が、公開先の URL（`https://github.com/ktdevx/clopts-private.git`）と新しいタグ（`GIT_TAG vX.Y.Z`）を指していることを確かめ、違っていれば直す。
4. 互換を壊す変更を含むときは、「SOVERSION を上げるかどうかの判断」の 5 に従って変更履歴に書く。

### 2. 手元で検査する

変更履歴とタグと版の照合を、リリースのワークフローと同じスクリプトで行う。成功すると標準出力に `version X.Y.Z` の 1 行だけが出て、`OUTPUT` に Release の本文になるノートが書かれる。

```sh
cmake -DMODE=release -DTAG=vX.Y.Z -DSOURCE_DIR=. -DOUTPUT=build/release-notes.md \
      -P tests/docs/check_changelog.cmake
```

```text
version X.Y.Z
```

`build/release-notes.md` を開き、Release の本文として読める内容になっていることを確かめる。失敗したときの報告は次のとおり。

| 報告 | 意味 |
|------|------|
| `tag vX.Y.Z does not match project VERSION A.B.C` | `CMakeLists.txt` の版を更新していないか、タグを打ち間違えた |
| `no section for version X.Y.Z in CHANGELOG.md` | 変更履歴にその版の節がない |
| `section for version X.Y.Z is empty` | その版の節が空行だけである |
| `CHANGELOG.md:<行>: <理由>` | 変更履歴の書式の誤り（見出し、日付、版の順序、重複、未知の分類、`互換性のない変更` の位置） |

続けて、静的版と共有版をそれぞれビルドし、全テストと、文書と ABI の検査を実行する。共有版のビルドでだけ、export されたシンボルと公開シンボル一覧の照合（`abi.exports`）が登録される。

```sh
cmake -S . -B build -DCLOPTS_WERROR=ON -DCLOPTS_BUILD_SHARED=OFF
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -L docs --output-on-failure
ctest --test-dir build -L abi --output-on-failure

cmake -S . -B build-shared -DCLOPTS_WERROR=ON -DCLOPTS_BUILD_SHARED=ON
cmake --build build-shared
ctest --test-dir build-shared --output-on-failure
ctest --test-dir build-shared -L abi --output-on-failure
```

リリース前の ABI の確認として、次の 2 つを確かめる。読み方は `docs/dev/abi-checks.md` の「失敗したときの読み方」を参照する。

- 共有版のビルドで `ctest -L abi` が成功している（公開シンボル一覧の形式と、export されたシンボルとの照合を含む）。
- 1.0.0 以降は、直近のリリースとの ABI 比較が成功している。CI の `abi-compare` ジョブの結果で確かめる。手元で確かめるときは `docs/dev/abi-checks.md` の「手元での abidiff の実行」に従う。1.0.0 より前は比較せず `abi-compare: skipped: version X.Y.Z is before 1.0.0` で成功する。

### 3. PR をマージし、main の CI を確かめる

1. 1 と 2 の変更で PR を作り、レビューを受けてマージする。
2. マージ後の main の CI がすべて成功していることを確かめる。10 ジョブ（`linux-gcc`、`linux-clang`、`macos-appleclang`、`windows-msvc`、`windows-mingw` のそれぞれ `static` と `shared`）と `abi-compare` である。1.0.0 以降は、`abi-compare` の出力が `abi-compare: skipped: ...` ではなく `abi-compare: compatible: ...` であることを確かめる（同じ major のリリースタグがあるのに skipped なら、タグが取得できていない）。

   ```sh
   gh run list --branch main --workflow ci.yml --limit 1
   ```

### 4. タグを作って push する

CI が成功した main のコミットにタグを付けて push する。タグの push がリリースのワークフローを起動する。

```sh
git switch main
git pull --ff-only
git tag vX.Y.Z
git push origin vX.Y.Z
```

タグは注釈なしの軽量タグでよい。タグはワークフローが Release を作り終えるまで動かさない。

### 5. ワークフローが作った Release を確かめる

```sh
gh run list --workflow release.yml --limit 1
gh release view vX.Y.Z
```

- リリースのワークフロー（`release.yml`）の実行が成功している。
- Release の名前が `CLOpts X.Y.Z` で、本文が変更履歴の `## [X.Y.Z]` の節（手元の `build/release-notes.md` と同じ）である。
- 添付ファイルが `clopts-X.Y.Z.tar.gz`、`clopts-X.Y.Z.zip`、`SHA256SUMS` の 3 つで、事前ビルド済みのバイナリがない。
- 必要なら添付ファイルを取得して、`sha256sum -c SHA256SUMS` が通り、展開したものに `ref/` と `.gitmodules` がないことを確かめる。

   ```sh
   dir=$(mktemp -d)
   gh release download vX.Y.Z --dir "$dir"
   (cd "$dir" && sha256sum -c SHA256SUMS)
   rm -rf "$dir"
   ```

## リリースのワークフローの動き

`.github/workflows/release.yml` は、`vX.Y.Z` の形のタグの push で起動する。通常の CI（`ci.yml`）とは独立した別のワークフローで、`ci.yml` の結果を待たない（main の CI の成功は、手順 3 で人が確かめる）。同じタグのワークフローは並行に走らない。ジョブ `release`（`ubuntu-latest`）だけが `contents: write` の権限を持ち、タグは環境変数 `TAG` で渡す。

| 手順 | 内容 |
|------|------|
| 1 | タグのコミットを取得する（全履歴。submodule は取得しない） |
| 2 | タグの形式（`^v[0-9]+\.[0-9]+\.[0-9]+$`）を確かめる |
| 3 | `cmake -DMODE=release -DTAG="$TAG" -DSOURCE_DIR="$PWD" -DOUTPUT="$RUNNER_TEMP/notes.md" -P tests/docs/check_changelog.cmake` で、変更履歴の形式、タグと `project(VERSION)` の一致、その版の節があって空でないことを確かめ、ノートを抜き出す。標準出力の `version X.Y.Z` から版を得る |
| 4 | `git archive` で `clopts-X.Y.Z.tar.gz` と `clopts-X.Y.Z.zip` を作る（中のパスの先頭は `clopts-X.Y.Z/`）。`.gitattributes` の `export-ignore` で `ref` と `.gitmodules` が除かれる |
| 5 | tar.gz を展開し、`ref` と `.gitmodules` がないことを確かめ、`-DBUILD_TESTING=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON` で、ネットワークを使わずに構成してライブラリをビルドする |
| 6 | `sha256sum` で `SHA256SUMS` を作る |
| 7 | `gh release create "$TAG" --verify-tag --title "CLOpts X.Y.Z" --notes-file "$RUNNER_TEMP/notes.md"` で Release を作り、tar.gz、zip、`SHA256SUMS` を添える |

手順 1〜6 のどれかが失敗すると、手順 7 に進まず、Release は作られない。失敗したステップの名前とスクリプトの報告がログに残る。同じタグの Release がすでにあると手順 7 が失敗する（上書きしない）。

### ソースだけを配る理由

Release に添えるのはソースアーカイブだけで、事前ビルド済みのバイナリ（静的ライブラリ、共有ライブラリ、DLL）は配らない。

- MSVC では、ライブラリと利用側で CRT の種別（`/MD` と `/MT`、Debug と Release）が一致しないと、リンクの失敗や実行時の不具合になる。利用側と同じビルドの中でビルドすれば、利用側の設定がそのまま使われる（`docs/integration.md`）。
- 共有ライブラリは、利用環境のコンパイラとツールチェーンでビルドするものとする。ABI の互換は SOVERSION で表し（`docs/abi.md`）、どの環境向けのバイナリを用意するかの組み合わせを持たない。
- 取り込み方（`add_subdirectory`、`FetchContent`、install してからの `find_package`）は、いずれもソースからのビルドを前提にしている。

GitHub が Release に自動で付ける「Source code (zip / tar.gz)」は、タグの木をそのまま固めたもので、`export-ignore` は効くが、展開したものがビルドできるかは確かめられていない。ワークフローの添付ファイルは、ディレクトリ名が `clopts-X.Y.Z/` にそろい、オフラインの構成でビルドできることを確かめたうえで、`SHA256SUMS` で内容を照合できる。利用者にはワークフローの添付ファイルを案内する。

## 失敗したときの戻し方

### Release が作られる前に失敗したとき

手順 1〜6 のどれかが失敗したときは、Release は作られていない。タグを消し、原因を直してから同じ版で打ち直す。

```sh
git push origin :refs/tags/vX.Y.Z
git tag -d vX.Y.Z
```

1. ワークフローのログで、失敗したステップと報告を確かめる（版の不一致、変更履歴の節がない、アーカイブの中身、オフラインのビルドの失敗など）。
2. 上のコマンドでリモートと手元のタグを消す。
3. 原因を PR で直してマージし、main の CI の成功を確かめる（リリースの手順の 3）。
4. 手元で `check_changelog.cmake` の release を実行して成功を確かめてから、直したコミットにタグを付け直して push する（リリースの手順の 4）。

### Release が作られたあとに不備が見つかったとき

- 本文（変更履歴の書き間違い）だけの不備なら、`CHANGELOG.md` を PR で直し、`gh release edit vX.Y.Z --notes-file <直したノート>` で本文を差し替える。タグは動かさない。
- 中身（コードやアーカイブ）の不備で、Release を作った直後で、まだ誰も取得していないと言えるとき（告知していない、ダウンロード数が 0）に限り、Release とタグを消して、直してから同じ版で打ち直してよい。

  ```sh
  gh release delete vX.Y.Z --cleanup-tag --yes
  git tag -d vX.Y.Z
  ```

- それ以外（告知した、取得された可能性がある、FetchContent でタグを指定した利用者がいる）は、公開したタグを消したり別のコミットに付け直したりしない。同じタグの中身が変わると、利用者のビルドが黙って変わるからである。不備を直した patch 版（`X.Y.(Z+1)`）を出し、変更履歴の `### 修正` に書く。必要なら、不備のある版の Release の本文に、使わないよう注記を足す。

## 文書と変更履歴の更新の規約

### 変更履歴

- 機能を追加・変更・削除する変更や、利用者に見える修正は、同じ変更（同じ PR）の中で `CHANGELOG.md` の `## [Unreleased]` の節に項目を足す。リリースのときにまとめて書かない。
- 分類は `### 互換性のない変更`、`### 追加`、`### 変更`、`### 非推奨`、`### 削除`、`### 修正`、`### セキュリティ` だけを使う。互換を壊す変更は `### 互換性のない変更` に書き、その節の最初の分類として置き、利用側に必要な対応も書く。
- 書式の詳細は `CHANGELOG.md` の冒頭の説明にある。書式は `check_changelog.cmake` の format で検査される（テスト `docs.changelog`）。コードブロックの中でも `## ` で始まる行は見出しとして扱われるので、変更履歴にそういう行を書かない。

### 文書の検査の一覧

利用者向けの文書（`README.md` と `docs/*.md`。`docs/dev/` は含まない）は、ラベル `docs` のテストで検査する。`ctest -L docs --output-on-failure` で実行できる。

| テスト | 検査する内容 |
|--------|--------------|
| `docs.outputs` | `clopts-check` の印の直後のブロックが、サンプルプログラムの実際の出力と終了コードに一致すること。taskbook と textcount のそれぞれに、終了コード 0 と 0 以外の印があること（`CLOPTS_BUILD_EXAMPLES` が ON のときだけ登録） |
| `docs.sources` | `clopts-source` の印の直後のブロックが、指定したファイルと 1 バイトも違わないこと（README の最小サンプルと `tests/docs/readme_minimal.c`） |
| `docs.links` | 相対リンクの先が存在し、`docs/dev/` を指さないこと |
| `docs.api_coverage` | `abi/clopts.symbols` のすべての名前が `docs/api.md` にバッククォート付きで現れること |
| `docs.changelog` | `CHANGELOG.md` の書式 |
| `docs_selftest.*` | 上の検査スクリプト自身の自己テスト |

### 印の書式

出力の照合の印は、照合するコードブロックの直前の行に、行頭から 1 行で書く。属性の既定は `exit=0`、`stream=stdout` である。`--` の後ろが、登録されたプログラム名と引数である。

````text
<!-- clopts-check: [exit=<int>] [stream=stdout|stderr] [env=<NAME>=<VALUE>]... -- <program> [<arg>...] -->
```text
<期待する出力の各行>
```
````

ソースの照合の印は、`<!-- clopts-source: <リポジトリのルートからの相対パス> -->` の直後にコードブロックを置く。印はコメントなので表示されない。読者に実行するコマンドを見せたいときは、印の前の本文か別のコードブロックに書く。

### ヘルプや出力が変わったときの印の直し方

ライブラリの挙動やサンプルを変えて出力が変わると、`docs.outputs` が `<文書>:<行>: output of '<program> <args>' differs (first difference at <文書>:<行>)` と、期待と実際の最初の違い、書き出したファイルのパスを報告する。

1. 変化が意図したものかを確かめる。意図しない変化なら、文書ではなくコードを直す。
2. 意図した変化なら、報告された印のプログラムを同じ引数で実行し（実行ファイルは `build/examples/taskbook` など）、出力をそのまま印の直後のブロックに貼り直す。報告の `files:` に出る実際の出力のファイルをそのまま使ってもよい。
3. 終了コードが変わったときは、印の `exit=` を直す。
4. 本文の説明も、変わった出力に合わせて直す。
5. `ctest -L docs --output-on-failure` が通ることを確かめる。

### 公開関数を足したとき

- `docs/dev/abi-checks.md` の手順で `abi/clopts.symbols` を更新するのと同じ変更の中で、`docs/api.md` に関数の説明を足す。名前は `` `name` `` か `` `name()` `` の形で書く（引数付きで書くときは `` `name`（`args`）`` の形）。足さないと `docs.api_coverage` が `not documented in docs/api.md: <name>` で失敗する。
- 変更履歴の `### 追加` に項目を足す。

## Naos の版の更新手順

テストに使う Naos の版の指定は、`cmake/CloptsNaos.cmake` の 1 か所に集めてある。

```cmake
set(CLOPTS_NAOS_TAG "v0.4.0")
set(CLOPTS_NAOS_COMMIT "09fde7172c4b63d416c32e98499cb0c0306d3dea")
```

1. 新しい版のタグと、そのタグが指すコミットの SHA を確かめる。

   ```sh
   git ls-remote https://github.com/vabelorg/naos.git 'refs/tags/v0.5.0*'
   ```

   注釈付きタグでは、`refs/tags/v0.5.0^{}` の行の SHA がコミットを指す。
2. `CLOPTS_NAOS_TAG` と `CLOPTS_NAOS_COMMIT` の両方を同じ変更で更新する。片方だけを変えると、構成時の版の照合（Naos の `include/naos/version.h` とタグの比較）が `Naos version mismatch` で止まる。
3. 新しいビルドツリーで構成し、全テストと、各ラベルのテストを実行する。

   ```sh
   cmake -S . -B build-naos -DCLOPTS_WERROR=ON
   cmake --build build-naos
   ctest --test-dir build-naos --output-on-failure
   ctest --test-dir build-naos -L core --output-on-failure
   ctest --test-dir build-naos -L decl --output-on-failure
   ctest --test-dir build-naos -L parser --output-on-failure
   ctest --test-dir build-naos -L help --output-on-failure
   ctest --test-dir build-naos -L platform --output-on-failure
   ctest --test-dir build-naos -L toolchain --output-on-failure
   ctest --test-dir build-naos -L integration --output-on-failure
   ctest --test-dir build-naos -L abi --output-on-failure
   ctest --test-dir build-naos -L docs --output-on-failure
   ```

4. PR を作り、CI のすべての環境（10 ジョブ）でテストが通ることを確かめてからマージする。
5. 変更履歴には書かない（Naos はテスト専用で、配布物にも利用者のビルドにも入らない）。

Naos の不足や、CLOpts の側で入れた回避策は、[Naos への要望](naos-feedback.md)（`docs/dev/naos-feedback.md`）に、その文書の書式で追記する。Naos のリポジトリに Issue やコメントを作らない。版を更新して回避策が要らなくなったときは、記録を消さずに状態を更新する。

## リリースのワークフローを試す方法

`.github/workflows/release.yml` を変えたときは、本物のリポジトリに版のタグを打つ前に試す。

### 手元で手順 3〜6 を再現する

ワークフローの手順 3〜6 は、手元の Linux で同じコマンドで再現できる。本物の `CHANGELOG.md` を変えずに試すときは、`## [X.Y.Z] - YYYY-MM-DD` の節を持つ仮の変更履歴を作って `CHANGELOG=` で渡す（`X.Y.Z` は `CMakeLists.txt` の版）。

```sh
mkdir -p /tmp/clopts-release-test
sed 's/^## \[Unreleased\]$/## [Unreleased]\n\n## [0.1.0] - 2026-11-09/' CHANGELOG.md \
    > /tmp/clopts-release-test/CHANGELOG.md
cmake -DMODE=release -DTAG=v0.1.0 -DSOURCE_DIR=. \
      -DCHANGELOG=/tmp/clopts-release-test/CHANGELOG.md \
      -DOUTPUT=/tmp/clopts-release-test/notes.md \
      -P tests/docs/check_changelog.cmake
```

```text
version 0.1.0
```

アーカイブは、コミットしていない `.gitattributes` の変更も効くよう `--worktree-attributes` を付けて作る（ワークフローでは、タグの木にコミットされた `.gitattributes` が使われる）。

```sh
git archive --worktree-attributes --format=tar.gz --prefix=clopts-0.1.0/ \
    -o /tmp/clopts-release-test/clopts-0.1.0.tar.gz HEAD
tar -xzf /tmp/clopts-release-test/clopts-0.1.0.tar.gz -C /tmp/clopts-release-test
test ! -e /tmp/clopts-release-test/clopts-0.1.0/ref
test ! -e /tmp/clopts-release-test/clopts-0.1.0/.gitmodules
cmake -S /tmp/clopts-release-test/clopts-0.1.0 -B /tmp/clopts-release-test/build \
      -DBUILD_TESTING=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/clopts-release-test/build
```

終わったら `/tmp/clopts-release-test` を消す。

### フォークか仮のタグで全体を試す

`gh release create` を含む全体は、GitHub の上でしか試せない。できるだけ自分のフォークで試す。

1. フォークに変更を push し、フォークで仮の版のタグ（`CMakeLists.txt` の版と、仮の変更履歴の節に合わせる）を push する。
2. フォークの Actions でリリースのワークフロー（`release.yml`）が成功し、Release の名前、本文、添付ファイルが期待どおりであることを確かめる。失敗させる場合（版の不一致、節がない）も試し、Release が作られないことを確かめる。
3. 試したあとは、Release とタグを必ず消す。

   ```sh
   gh release delete vX.Y.Z --repo <fork> --cleanup-tag --yes
   git tag -d vX.Y.Z
   ```

本物のリポジトリで仮のタグを使うのは、フォークで試せない理由があるときだけにし、試したあとは同じようにタグと Release を消す。本物のリポジトリに残った仮のタグは、CI の `abi-compare` が比較の基準に選んでしまう。
