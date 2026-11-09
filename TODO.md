# TODO: 静的版と共有版の同一 prefix への並存 install をやめる

## 背景

いまは、静的版と共有版を同じ prefix に並べて install でき、`find_package` で形式を選べる（project-foundation の Requirement 13、6.7、7.6、12.6）。そのために、次のものを持っている。

- MSVC の静的版の出力名を `clopts_static` にする（`cmake/CloptsLibrary.cmake:103-106`）。静的ライブラリと共有版のインポートライブラリが、どちらも `.lib` で衝突するため。
- ターゲット定義を形式ごとのファイル `CLOptsStaticTargets.cmake` / `CLOptsSharedTargets.cmake` に分ける（`cmake/CloptsInstall.cmake`）。
- 静的版だけが置く CRT の記録 `CLOptsStaticRuntime.cmake`（`cmake/CLOptsStaticRuntime.cmake.in`）。
- `cmake/CLOptsConfig.cmake.in`（169 行）の形式を選ぶ処理。全体の約 110 行がこれにあたる。
  - `COMPONENTS static|shared` の解釈と、両方を指定したときの拒否、未知のコンポーネントの拒否
  - 入っている形式の確認
  - すでに読み込まれた `CLOpts::clopts` の形式との突き合わせ
  - 既定の決め方（`CLOPTS_SHARED_LIBS` → `BUILD_SHARED_LIBS` → 入っている方）
  - 失敗の理由ごとの `CLOpts_NOT_FOUND_MESSAGE`
  - `CLOpts_static_FOUND` / `CLOpts_shared_FOUND`
- 検証: `tests/cmake/run_package_selection.cmake`（2 つの形式をビルドして install し、8 ケースを確かめる）と `tests/integration/fixtures/package_select/`、`integration.package_selection` の登録、CI のコメント。
- 文書: `docs/integration.md` の「形式の選択（install 経路）」の節、README、CHANGELOG。

## 問題

- 並存のためだけに、package config がかなり複雑になっている。形式が 1 つなら、ターゲット定義を読み込んで CRT を確かめるだけで済む。
- pkg-config（`.pc`）に対応するときに、さらに複雑になる。
  - 静的版を使う側には `-DCLOPTS_STATIC` が必要になる（`include/clopts/export.h:4`。ないと Windows で `dllimport` になる）。一方、pkg-config の `--static` で切り替わるのは基本的に `Libs.private` だけで、`Cflags` は切り替わらない（`Cflags.private` は pkgconf だけの機能）。
  - `.a` と `.so` が同じディレクトリにあると、`-lclopts` では `.so` が選ばれる。
  - 結局 `clopts.pc` と `clopts-static.pc` の 2 つが必要になり、CMake の `COMPONENTS` とは別の選び方を説明しなければならない。
- 並存を必要とする利用者は、具体的には想定されていない。両方を使いたい人は prefix を分ければよい（vcpkg が静的版と共有版を別の triplet、つまり別の prefix に置くのと同じ考え方）。Linux のディストリは自分でパッケージを組むので、上流が並存まで面倒を見る必要は薄い。

## 目的

- 1 つの prefix に install する形式は 1 つだけ、とする。静的版と共有版の両方を使いたいときは、prefix を分ける。
- package config と install の規則を、形式が 1 つである前提で単純にする。
- 将来 `.pc` に対応するときに、1 つのファイルで済むようにする（静的版なら `Cflags` に `-DCLOPTS_STATIC` を入れるだけ）。

## 方針

### 決めること（推奨案）

1. **ターゲット定義のファイル**: `CLOptsTargets.cmake` の 1 つに戻す（export セット名は `CLOptsTargets`）。
2. **形式と CRT の記録**: `configure_package_config_file` で `CLOptsConfig.cmake` に直接書き込む（`@CLOPTS_LIBRARY_TYPE@`、`@CLOPTS_RECORDED_MSVC_RUNTIME_LIBRARY@` など）。`CLOptsStaticRuntime.cmake.in` は削除する。
   - 理由: 静的版の上から共有版を install したとき、別ファイルの `CLOptsStaticRuntime.cmake` だけが古いまま残ることを防ぐ。全部を両方の形式が必ず書くファイルにまとめれば、同じ prefix に別の形式を上書き install しても「後から入れた方」で一貫した状態になる。
3. **`CLOpts_LIBRARY_TYPE`**: 残す（`STATIC` / `SHARED`）。config に値を書き込むだけなので安く、利用側が形式を確かめる手段として役に立つ。
4. **`CLOpts_static_FOUND` / `CLOpts_shared_FOUND`**: 削除する。コンポーネントがなくなるので意味がない。
5. **`COMPONENTS`**: 受け付けない。`check_required_components(CLOpts)` だけを呼び、必須のコンポーネントを指定されたら CMake の既定どおり見つからない扱いにする。
   - 代替案: `COMPONENTS static|shared` を「入っている形式の確認」として残す（違えば失敗）。10 行ほどで書けるが、契約と文書とテストが残るので採らない。形式は `CLOpts_LIBRARY_TYPE` で確かめてもらう。
6. **`CLOPTS_SHARED_LIBS`**: 削除する（package config からも、テストからも）。
7. **二重読み込み**: `if(NOT TARGET CLOpts::clopts)` のときだけ `include` する。形式は 1 つしかないので、突き合わせの処理は要らない。
8. **MSVC の出力名**: `OUTPUT_NAME clopts_static` をやめ、静的版も `clopts.lib` にする。
   - 検討した懸念: 名前が同じだと、静的版の `.lib` を `CLOPTS_STATIC` なしで手でリンクしたとき、`__imp_` のリンクエラーになる。ただ、`CLOpts::clopts` を通せば `CLOPTS_STATIC` は自動で伝わるので、そこまで面倒を見る必要はないと判断する。
9. **同じ prefix に別の形式を入れたとき**: 検出しない。「後から入れた方」が有効になる（2 により config・ターゲット定義・ライブラリが一貫する）。前の形式のファイル（`libclopts.a`、`clopts.dll` など）は残るが、ターゲット定義からは参照されない。文書で「形式を変えるときは別の prefix を使うか、prefix を空にしてから install する」と書く。
   - 代替案: `install(CODE)` で、別の形式のファイルがあれば警告する。仕組みが増えるので採らない。
10. **`.pc` の対応**: この TODO の範囲外とする。対応するときは、上の前提で `clopts.pc` 1 つを生成する。

### リリースへの影響

タグはまだなく、CHANGELOG も `## [Unreleased]` だけなので、公開した契約を壊すことにはならない。CHANGELOG は「互換性のない変更」に書くのではなく、`[Unreleased]` の該当する項目を書き直す。

## 作業

仕様 → 実装 → 検証の順で進める。kiro の承認の手順（各フェーズの承認の取り直し、`spec.json` の承認状態の変更）は省き、仕様の文書を直接書き直す。

### 1. 仕様: project-foundation の requirements

`.kiro/specs/project-foundation/requirements.md`。既存の番号をほかの箇所（コードのコメントや design の対応表）が参照しているので、**番号は振り直さず**、消す項目は「（削除）」と理由を残す。

- [ ] Requirement 13 を「1 つの prefix に 1 つの形式」に書き直す。
  - Objective: 1 つの prefix からは、そこに install した形式が提供されること。
  - 13.1: install した形式のターゲット定義と config を、形式によらず同じファイル名で install する（別の形式を同じ prefix に入れたら、後の方で置き換わる）。
  - 13.2〜13.5: 削除（`COMPONENTS` による選択、既定、両方の拒否、入っていない形式の失敗）。
  - 13.6: 残す（`CLOpts_LIBRARY_TYPE` で形式を知らせる）。
- [ ] 6.7（並存 install と形式選択の検証）を削除する。
- [ ] 7.6（CI で並存 install の検証を少なくとも 1 環境で）を削除する。
- [ ] 12.6（MSVC の静的版のファイル名を変える）を削除する。
- [ ] 5.6 は残す。記録の置き場所は design で変える。
- [ ] `brief.md` の 53 行目（MSVC の出力名）、55 行目（`CLOptsStaticTargets` / `CLOptsSharedTargets` と `COMPONENTS`）を直す。
- [ ] `spec.json` は `updated_at` だけを更新する（承認の状態は変えない）。

### 2. 仕様: project-foundation の design / tasks / research

`.kiro/specs/project-foundation/design.md`。

- [ ] 概要（35 行目「静的版と共有版を同じ prefix に install でき…」）、成果物の一覧（53 行目の `CLOptsStaticRuntime.cmake.in`、60 行目の package config の契約）、102 行目の変更してはいけない契約の一覧を直す。
- [ ] ファイル構成（186、216、217、236、251、260、261 行目）から `run_package_selection.cmake`、`package_select/`、`CLOptsStaticRuntime.cmake.in` を除き、`CLOptsConfig.cmake.in` の説明を直す。
- [ ] 293 行目の図から `MSVC は OUTPUT_NAME clopts_static` を除く。
- [ ] 「package config での形式の選択（13.2〜13.6）」の節（317〜341 行目）を、1 つの形式を読み込むだけの流れに置き換える。
- [ ] 要件の対応表（392〜431 行目）から 6.7、7.6、12.6、13.2〜13.5 を除き、13.1、13.6 を直す。
- [ ] LibraryHelper（513 行目の `if(MSVC)` の `OUTPUT_NAME`）を直す。
- [ ] InstallHelper / PackageConfig（580〜625 行目）を書き直す。`<Kind>` をやめる。形式と CRT は config に書き込む。入力・規則・事後条件から `COMPONENTS`、`CLOPTS_SHARED_LIBS`、`CLOpts_<c>_FOUND` を除く。「同じ prefix に並べる 2 つの形式は同じ版から」の制約を削除する。
- [ ] PackageSelectionTest の節（769〜793 行目）を削除する。
- [ ] ToolchainTests（823 行目の MSVC の `OUTPUT_NAME`）、統合テスト（839 行目の `-DCLOPTS_SHARED_LIBS` を渡すこと）、CI（880 行目の 7.6）、エラー処理（906 行目）、テストの一覧（925 行目）を直す。
- [ ] Design Decisions に、並存をやめた理由（この TODO の「問題」）を 1 項目足す。
- [ ] `tasks.md` に改訂の項目（「（改訂 12）」など）として、この変更の作業を足す。済んでいる 7・9・10.x・11 の記述は履歴として残し、改訂の項目で置き換えたことを書く。
- [ ] `research.md` は調査時の記録なので書き換えない。必要なら、並存をやめたことを末尾に 1 行足す。

### 3. 仕様: ほかの spec と steering

- [ ] `.kiro/specs/docs-and-release/requirements.md`: 8 行目、35 行目、1.6（README で `COMPONENTS` を示す）、4.4（並べて install できることと形式の選び方の文書）、4.7（記述例の形に「形式の選び方」を含む）を、「形式は install した方が使われる。両方使うなら prefix を分ける」に書き直す。
- [ ] `.kiro/specs/docs-and-release/design.md`（253、332 行目）、`tasks.md`（95、96 行目）、`brief.md`（10 行目）を直す。
- [ ] `.kiro/steering/roadmap.md` の 74 行目（「MSVC での静的版の別名、install の export セットを静的・共有で分け config で選ばせる（`COMPONENTS static|shared`）」）を直す。

### 4. 実装

- [ ] `cmake/CloptsLibrary.cmake`: `if(MSVC) ... OUTPUT_NAME clopts_static` を削除する。
- [ ] `cmake/CloptsInstall.cmake`
  - `kind` をやめ、`EXPORT CLOptsTargets` / `FILE CLOptsTargets.cmake` にする。
  - 形式（`STATIC` / `SHARED`）と CRT の記録を変数にして、`configure_package_config_file` で config に渡す。`CLOptsStaticRuntime.cmake` の生成と install を削除する。
  - 冒頭のコメント（15〜23 行目）と、91 行目の「同じ prefix で上書きしても同じになるように」というコメントを直す。
- [ ] `cmake/CLOptsStaticRuntime.cmake.in` を削除する。
- [ ] `cmake/CLOptsConfig.cmake.in` を書き直す。
  - `@PACKAGE_INIT@` → `if(NOT TARGET CLOpts::clopts) include(CLOptsTargets.cmake)` → `set(CLOpts_LIBRARY_TYPE "@...@")` → 静的版なら CRT の比較と警告（いまの条件のまま） → `check_required_components(CLOpts)`。
  - 30〜40 行を目安にする。

### 5. テスト

- [ ] `tests/cmake/run_package_selection.cmake` を削除する。
- [ ] `tests/integration/fixtures/package_select/` を削除する。
- [ ] `tests/integration/CMakeLists.txt`: `integration.package_selection` の登録（96〜106 行目）と冒頭コメント（23〜31 行目）を削除する。`install_find_package` に渡している `-DCLOPTS_SHARED_LIBS=...`（94 行目）と、そのコメント（14 行目）を削除する。
- [ ] `tests/integration/fixtures/consumer/CMakeLists.txt`: `CLOPTS_SHARED_LIBS` で形式を求めるという説明（13 行目）を直す。`CLOpts_LIBRARY_TYPE` の照合は残す。
- [ ] `tests/toolchain/fixtures/library_type/CMakeLists.txt`: 113〜117 行目の MSVC の `OUTPUT_NAME` の期待を、ほかの環境と同じ「空」にする（12.6 のコメントも削除する）。
- [ ] 形式を上書き install したときの検証を足すか決める（方針 9）。足すなら、`install_find_package` の中で「静的版を入れてから共有版を入れる」程度の小さいものにする。推奨は「足さない」（Windows では 2 回ビルドが要り、`package_selection` を消した分の実行時間がまた増えるため）。

### 6. CI

- [ ] `.github/workflows/ci.yml` の 85〜86 行目のコメント（`integration.package_selection` が走る）を削除する。ジョブの構成は変えない。

### 7. 文書

- [ ] `docs/integration.md`
  - 104〜105 行目の変数の表から `CLOpts_static_FOUND` / `CLOpts_shared_FOUND` を除く。
  - 「形式の選択（install 経路）」の節（150〜213 行目）を短い節に置き換える。内容: install した形式がそのまま使われる。両方使うなら prefix を分け、`CMAKE_PREFIX_PATH` で切り替える。同じ prefix に別の形式を入れると後の方で置き換わり、前の形式のファイルが残るので、形式を変えるときは prefix を空にする。形式は `CLOpts_LIBRARY_TYPE` で確かめられる。
  - 290 行目のテストの一覧から `integration.package_selection` を除く。
  - 「静的と共有の選び方」の節と README からのリンク先（`#静的と共有の選び方`）が切れないことを確かめる。
- [ ] `README.md` の 191〜194 行目を、上と同じ内容に短く書き直す（`COMPONENTS shared` の例を、prefix を分ける例にする）。197 行目の「並べて install する手順、`COMPONENTS` がないときの順序」の案内も直す。
- [ ] `CHANGELOG.md` の 22 行目（「静的版と共有版を並べて置いて `COMPONENTS static` / `COMPONENTS shared` で選べる」）を直す。
- [ ] `docs/abi.md` の 40 行目（MSVC の `clopts.dll` とインポートライブラリ `clopts.lib`）はそのままでよいことを確かめる。

## 完了条件

- [ ] `grep -rnE "COMPONENTS|StaticTargets|SharedTargets|clopts_static|package_selection|package_select|CLOPTS_SHARED_LIBS|_static_FOUND|_shared_FOUND|StaticRuntime"` が、`.kiro/specs/*/research.md`、tasks.md の改訂の履歴、この TODO 以外でヒットしない。
- [ ] `cmake/CLOptsConfig.cmake.in` に形式を選ぶ処理がない。
- [ ] Linux（GCC）の静的版と共有版で `ctest --test-dir build --output-on-failure` が全件通る（`integration.install_find_package` を含む）。
- [ ] CI の 10 ジョブ（5 環境 × 静的／共有）がすべて通る。特に windows-msvc の静的版で、`clopts.lib` が静的ライブラリとして install され、`install_find_package` が通る。
- [ ] `/kiro-validate-impl project-foundation` が通る。docs のテスト（ラベル `docs`）が通る。
- [ ] 作業が済んだら、この TODO.md を削除する。
