# CLOpts の文書

この文書群は、CLOpts を使う開発者のための説明である。CLOpts の概要、最小のサンプル、取り込み方のあらましは [README](../README.md) にある。まず README を読み、そこから必要な章へ進む。

## 章の一覧

| 章 | 内容 |
|----|------|
| [API リファレンス](api.md) | 公開 API の型と関数、ハンドルの寿命、宣言・解析・結果とエラーのアクセサ、エラーの種類、文面の出力、メモリの所有 |
| [解析規則](parsing.md) | argv をどの規則で解釈するか。オプションの書き方、`--` と `-` 単体、サブコマンドとの並べ方、名前の照合、繰り返し指定、値の優先順位と出所、ヘルプと版の要求を、実行例とともに示す |
| [取り込み手順](integration.md) | `add_subdirectory`、`FetchContent`、install してからの `find_package` の 3 通りの手順、CMake のオプション、静的版と共有版の選び方、MSVC の CRT |
| [Windows での注意](windows.md) | UTF-8 の argv と環境変数の得方、`wmain`、`activeCodePage` との関係、共有版の DLL 名と PATH、コンソールのコードページ |
| [ABI の互換性](abi.md) | 共有ライブラリとしての ABI の保証の範囲、版と SOVERSION、互換を保つ変更と壊す変更 |

## 読む順序

1. [取り込み手順](integration.md) で、自分のプロジェクトに合う取り込み方と形式（静的版か共有版か）を決める。
2. [API リファレンス](api.md) の「全体の流れ」で API を呼ぶ順序をつかみ、必要な関数の説明を読む。
3. [解析規則](parsing.md) で、利用者が渡す引数がどう解釈されるかを確かめる。
4. Windows でも動かすなら、[Windows での注意](windows.md) を読む。
5. 共有版を使うなら、[ABI の互換性](abi.md) で、どの版に差し替えてよいかを確かめる。

サンプルプログラムは `examples/` にある（`examples/taskbook/taskbook.c` はサブコマンドあり、`examples/textcount/textcount.c` はサブコマンドなし）。
