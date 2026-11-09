# Interface 2027年1月号 Slint × Zephyr 駐車場システムのサンプル

Interface 2027年1月号 特集 第2部 第2章「Zephyr製作①…GUI×I/O M5Stack駐車場システム」（鈴木 佑）のサンプル・プログラムです。

M5Stack Core2 で動くコイン・パーキングの精算機を、GUI フレームワークの [Slint](https://slint.dev/) と RTOS の [Zephyr](https://www.zephyrproject.org/) で作ります。画面は PC 上で作って確かめ、同じソースコードのまま M5Stack に書き込んで動かします。

## 2つの版

| ブランチ | 中身 |
|---|---|
| [`main`](../../tree/main) | 記事で作るもの。タッチ操作と、ToF センサによる入庫の自動検知まで |
| `full`（このブランチ） | 記事の写真1の完成形。LED、サーボモータ（フラップ板）、QR コード・リーダも動かす |

このブランチは、記事の最後で読者への宿題にした部品を全部付けた例です。宿題の答え合わせや、配線の参考にしてください。記事を読みながら試すときは `main` を使ってください。

どの部品もなくて構いません。つながっていない部品は使わずに動き、その代わりの操作を画面のボタンで出します。M5Stack Core2 単体でも、PC 上でも、画面だけのアプリとしてそのまま動きます。

## 用意するもの

### ハードウェア

| 品名 | 役割 |
|---|---|
| M5Stack Core2 **v1.3** | 本体。画面、タッチ、電池 |
| Module ExtPort For Core2 | 本体の背面に積んで、Grove の差し込み口を Port B〜E に増やす |
| RGB LED Unit（SK6812 × 3） | 空車・駐車中・精算中を色で知らせる |
| ToF Unit（VL53L0X） | 車が来たことを距離で見つける |
| サーボモータ（FeeTech FS0403） | フラップ板を上げ下げする |
| QR Code Unit | 精算に使う QR コードを読む |
| フラップ板 | 3D プリンタで作る（`hardware/flap-board.stl`） |
| Grove ケーブル | 各ユニットと本体をつなぐ（各ユニットに付属） |
| USB Type-C ケーブル | PC とつなぐ。**データ通信に対応したもの**を使う |

M5Stack Core2 には電源 IC の違う版があります。Zephyr が対応しているのは電源 IC が AXP192 の版で、いま買えるのは v1.3（型番 K010-V13）です。v1.1（AXP2101）では動きません。

サーボモータは FS0403 のように動作電流の小さいものを使ってください。9g 級のサーボの多くは動き出しに大きな電流を引き、Grove の 5V では本体ごと電源が落ちます。

### つなぎ方

| 差し込み口 | つなぐもの | 使うピン |
|---|---|---|
| 本体の Port A | RGB LED Unit | G32 |
| ExtPort の Port B | サーボモータ | G26（PWM） |
| ExtPort の Port C | ToF Unit | G13（SCL）、G14（SDA） |
| ExtPort の Port D | 空き（読者の拡張用） | G34、G35 |
| ExtPort の Port E | QR Code Unit | G19（TX）、G27（RX） |

- ExtPort の DIP スイッチは、Port E の「19」と「27」を両方 ON にします。片方だけだと、命令は送れるのに返事が来ない、という分かりにくい壊れ方をします。
- QR Code Unit の本体のスイッチは UART 側にしておきます。
- `main` では ToF Unit を本体の Port A に挿していましたが、このブランチでは Port A を LED に使うので、ToF Unit は ExtPort の Port C に挿します。

どこに何をつなぐかは `carpark/zephyr/boards/m5stack_core2_esp32_procpu.overlay` に書いてあります。つなぎ方を変えるときは、このファイルを直します。

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
| `carpark/zephyr/main.cpp` | 画面と各部品をつなぐ | リスト9、リスト14 に LED・サーボ・QR を足したもの |
| `carpark/zephyr/CMakeLists.txt` | ビルドの設定 | リスト10 |
| `carpark/zephyr/boards/m5stack_core2_esp32_procpu.conf` | 各部品のドライバを使う設定 | ― |
| `carpark/zephyr/boards/m5stack_core2_esp32_procpu.overlay` | どこに何がつながっているかを書く | リスト13 を広げたもの |
| `carpark/zephyr/dts/bindings/pwm-servo.yaml` | サーボモータをデバイス・ツリーに書くための定義（Zephyr のサンプルから） | ― |
| `hardware/flap-board.stl` | フラップ板の 3D データ | ― |

`CMakeLists.txt` は、Slint 側の `demos/zephyr-common/slint-zephyr-app.cmake` を読み込むだけです。Rust のビルドの指定、ボードごとの設定、Slint を Zephyr の上で動かすためのコード、M5Stack の Grove の 5V を入れる処理などは、そちらが引き受けます。

## うまくいかないとき

- **書き込みが途中で止まる、つながらない**：USB ケーブルを替えてみてください。充電専用のケーブルや、接触の悪いケーブルでは書き込めません。
- **部品を挿したのに使われない**：部品は起動時に見つけます。挿したら M5Stack をリセットしてください。差し込み口が上の「つなぎ方」のとおりかも確かめます。
- **サーボモータが動くと電源が落ちる**：サーボモータの電流が大きすぎます。上の「ハードウェア」の注意を見てください。
- **QR コードを読んでも反応しない**：ExtPort の DIP スイッチ（Port E の 19 と 27）と、QR Code Unit 本体のスイッチを確かめます。
- **`west build` で、Slint の Platform コードの警告が幾つか出る**：動作には影響しません。

## ライセンス

このリポジトリのサンプル・プログラムは、[MIT License](LICENSE-MIT) と [Apache License 2.0](LICENSE-APACHE) のどちらかを選んで使えます。

Slint 本体のライセンスは別です。GPLv3、ロイヤリティ・フリー、商用の3つから選べます。詳しくは [Slint の FAQ](https://github.com/slint-ui/slint/blob/master/FAQ.md) を見てください。Zephyr は Apache License 2.0 です。
