# Interface 2027年1月号 Slint × Zephyr 駐車場システムのサンプル

Interface 2027年1月号 特集 第2部 第2章「Zephyr製作①…GUI×I/O M5Stack駐車場システム」（鈴木 佑）のサンプル・プログラムです。

M5Stack Core2 で動くコイン・パーキングの精算機を、GUI フレームワークの [Slint](https://slint.dev/) と RTOS の [Zephyr](https://www.zephyrproject.org/) で作ります。画面は PC 上で作って確かめ、同じソースコードのまま M5Stack に書き込んで動かします。

## 2つの版

| ブランチ | 中身 |
|---|---|
| `main`（このブランチ） | 記事で作るもの。タッチ操作と、ToF センサによる入庫の自動検知まで |
| [`full`](../../tree/full) | 記事の写真1の完成形。LED、サーボモータ（フラップ板）、QR コード・リーダも動かす |

記事を読みながら試すときは `main` を使ってください。`full` は、記事の最後で読者への宿題にした部品を全部付けた例です。宿題の答え合わせや、配線の参考にしてください。

## 用意するもの

### ハードウェア

| 品名 | 役割 |
|---|---|
| M5Stack Core2 **v1.3** | 本体。画面、タッチ、電池 |
| ToF Unit（VL53L0X） | 車が来たことを距離で見つける |
| Grove ケーブル | ToF Unit と本体の Port A をつなぐ（ToF Unit に付属） |
| USB Type-C ケーブル | PC とつなぐ。**データ通信に対応したもの**を使う |

M5Stack Core2 には電源 IC の違う版があります。Zephyr が対応しているのは電源 IC が AXP192 の版で、いま買えるのは v1.3（型番 K010-V13）です。v1.1（AXP2101）では動きません。

ToF Unit がなくても、画面のボタンで入庫を操作して一通り動かせます。

### PC

Linux（Ubuntu など）で確かめています。次のものを入れておきます。

- Zephyr の開発環境（west と Zephyr SDK）。入れ方は [Zephyr の Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) を見てください。記事では第1部で扱っています
- [rustup](https://rustup.rs/)。Slint のライブラリは Rust で書かれているので、ビルドに Rust を使います
- SDL2 の開発用パッケージ。PC 上で画面の窓を開くのに使います（Ubuntu なら `sudo apt install libsdl2-dev`）

## 試し方

### 1. 取ってくる

このリポジトリを clone したディレクトリが、そのまま west の作業場所（workspace）になります。

```
$ git clone https://github.com/signal-slot/interface-2027-01-slint-carpark.git
$ cd interface-2027-01-slint-carpark
$ west init -l carpark      # carpark の west.yml を土台にする
$ west update               # Zephyr と Slint を隣に取ってくる
$ west zephyr-export
```

`west update` が、`carpark/west.yml` に書いてある Zephyr（v4.4.1）と Slint（M5Stack Core2 対応を入れた [signal-slot/slint](https://github.com/signal-slot/slint/tree/zephyr-carpark) の `zephyr-carpark` ブランチ）を取ってきます。Slint を手で clone する必要はありません。精算機に要る部品だけを取るようにしてあるので、Zephyr を全部取るより小さく済みます。

### 2. PC で動かす

PC 向けのビルドには、Rust の nightly 版が要ります。作業場所の中だけで nightly を使うように設定します。

```
$ rustup toolchain install nightly
$ rustup override set nightly
$ west build -b native_sim/native/64 carpark/zephyr
$ ./build/zephyr/zephyr.exe
```

窓が開き、精算機の画面が出ます。タッチの代わりにマウスで操作できます。最初の1回は Slint のライブラリをビルドするので数分かかります。2回目からは、直したファイルの分だけビルドし直します。

### 3. M5Stack で動かす

M5Stack の CPU（ESP32 の Xtensa）向けの Rust を、espup で入れます。ここまでは一度だけです。

```
$ cargo install espup
$ espup install
$ source ~/export-esp.sh    # このシェルで esp 版の Rust を使えるようにする
$ west blobs fetch hal_espressif
$ west packages pip --install
```

M5Stack を USB でつないで、ビルドして書き込みます。

```
$ west build -p -b m5stack_core2/esp32/procpu carpark/zephyr
$ west flash
```

`-p` は、ビルドをまっさらからやり直す指定です。同じ build ディレクトリでボードを替えるときに付けます。付け忘れると、west がボードの食い違いをエラーで知らせます。以後の M5Stack 向けのビルドでは要りません。PC 向けに戻すときは、また `-p` を付けます。

新しいシェルを開いたときは、ビルドの前に `source ~/export-esp.sh` をもう一度実行します。

## ファイルの構成

記事のリスト番号は、誌面の番号です。

| ファイル | 中身 | 記事 |
|---|---|---|
| `carpark/west.yml` | Zephyr と Slint をどこから取ってくるか | リスト11 |
| `carpark/ui/carpark.slint` | 画面一式 | リスト1〜8、ToF センサの節の変更 |
| `carpark/zephyr/main.cpp` | 画面と ToF センサをつなぐ | リスト9、リスト14 |
| `carpark/zephyr/CMakeLists.txt` | ビルドの設定 | リスト10 |
| `carpark/zephyr/prj.conf` | センサを使う設定（`CONFIG_SENSOR=y`） | ToF センサの節 |
| `carpark/zephyr/boards/m5stack_core2_esp32_procpu.overlay` | Port A に ToF センサがつながっていることを書く | リスト13 |

`CMakeLists.txt` は、Slint 側の `demos/zephyr-common/slint-zephyr-app.cmake` を読み込むだけです。Rust のビルドの指定、ボードごとの設定、Slint を Zephyr の上で動かすためのコード、M5Stack の Grove の 5V を入れる処理などは、そちらが引き受けます。

## うまくいかないとき

- **書き込みが途中で止まる、つながらない**：USB ケーブルを替えてみてください。充電専用のケーブルや、接触の悪いケーブルでは書き込めません。
- **ToF センサを挿したのにボタンが消えない**：ToF センサは起動時に見つけます。挿したら M5Stack をリセットしてください。ToF Unit が Port A（本体側面の赤い差し込み口）に挿さっているかも確かめます。
- **`west build` で、Slint の Platform コードの警告が幾つか出る**：動作には影響しません。

## ライセンス

このリポジトリのサンプル・プログラムは、[MIT License](LICENSE-MIT) と [Apache License 2.0](LICENSE-APACHE) のどちらかを選んで使えます。

Slint 本体のライセンスは別です。GPLv3、ロイヤリティ・フリー、商用の3つから選べます。詳しくは [Slint の FAQ](https://github.com/slint-ui/slint/blob/master/FAQ.md) を見てください。Zephyr は Apache License 2.0 です。
