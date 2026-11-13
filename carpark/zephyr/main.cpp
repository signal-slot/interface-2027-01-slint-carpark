/*
 * Copyright (c) 2026 Tasuku Suzuki
 * SPDX-License-Identifier: MIT OR Apache-2.0
 *
 * 精算機の main (記事のリストを束ねた版). 画面は必ず動く. 周辺ユニットは
 * 「宣言 (overlay の alias) がコンパイル時にあるか」と「実物が実行時に応答するか
 * (device_is_ready)」の二段で確かめ, そろっているものだけを駆動する. 無い
 * ユニットの役割は画面のボタンが務め, UI は has-* を見てそのボタンを出す.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include "slint-zephyr.h"
#include "carpark.h"

using Win = slint::ComponentHandle<AppWindow>;

/* ---- LED (Port A) : 画面が決めた色を運ぶ ---- */
static led_rgb dim(const slint::Color &c)
{
    return { .r = uint8_t(c.red() / 6),
             .g = uint8_t(c.green() / 6),
             .b = uint8_t(c.blue() / 6) };
}

/* ---- ToF (Port C) : 距離で入庫を判定 ---- */
static constexpr int NEAR_MM = 150;
static constexpr int DETECT_SECONDS = 3;  // 近い状態がこの秒数続いたら入庫

static int tof_read_mm(const device *tof)
{
    sensor_value v{};
    if (sensor_sample_fetch(tof) != 0 ||
        sensor_channel_get(tof, SENSOR_CHAN_DISTANCE, &v) != 0) {
        return -1;
    }
    return v.val1 * 1000 + v.val2 / 1000;
}

/* ---- QR (Port E) : UART の生の機器. 命令を送って操る ----
 * M5Stack Unit QRCode. 115200 8N1.
 *   23 61 41     ping        24 61 41 が返れば通信路が生きている
 *   21 61 41 00  手動モード   命令したときだけ読む
 *   32 75 01/02  読み取り 開始 (照明が点く) / 停止
 * 結果は生テキスト. 制御応答 33 75 02 と 22 61 41 は結果ではない.
 */
#if DT_NODE_EXISTS(DT_ALIAS(carpark_scanner))
static const device *qr = DEVICE_DT_GET(DT_ALIAS(carpark_scanner));

static void qr_send(const uint8_t *cmd, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(qr, cmd[i]);
    }
}

static void qr_set_decode(bool on)
{
    const uint8_t cmd[] = { 0x32, 0x75, uint8_t(on ? 0x01 : 0x02) };
    qr_send(cmd, sizeof(cmd));
    printk("qr: 読み取り%s\n", on ? "開始" : "停止");
}

static bool qr_read(char *text, size_t cap)
{
    static uint8_t buf[128];
    static size_t len;
    unsigned char c;
    while (uart_poll_in(qr, &c) == 0) {
        if (len < sizeof(buf)) { buf[len++] = c; }
    }
    if (len == 0) { return false; }
    const bool ack = len >= 3 &&
        ((buf[0] == 0x33 && buf[1] == 0x75 && buf[2] == 0x02) ||
         (buf[0] == 0x22 && buf[1] == 0x61 && buf[2] == 0x41));
    const size_t n = MIN(len, cap - 1);
    for (size_t i = 0; i < n; i++) {
        text[i] = (buf[i] >= 0x20 && buf[i] < 0x7f) ? char(buf[i]) : '.';
    }
    text[n] = '\0';
    len = 0;
    return !ack && n > 0;
}
#endif

int main(void)
{
    const device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    slint_zephyr_init(display);
    auto window = AppWindow::create();

    /* run() が返るまで生きている必要があるので, main のローカルに置く. */
    slint::Timer tof_timer, scanner;

#if DT_NODE_EXISTS(DT_ALIAS(carpark_indicator))
    const device *led = DEVICE_DT_GET(DT_ALIAS(carpark_indicator));
    if (device_is_ready(led)) {
        auto led_write = [&] {
            const led_rgb c = dim(window->get_led_color());
            led_rgb px[3] = { c, c, c };
            led_strip_update_rgb(led, px, 3);
        };
        window->on_led_color_changed(led_write);  // 色が変わったときだけ書く
        led_write();                               // 起動時の初期色 (changed は変化時のみ)
    }
#endif

#if DT_NODE_EXISTS(DT_ALIAS(carpark_flap))
    static const pwm_dt_spec flap = PWM_DT_SPEC_GET(DT_ALIAS(carpark_flap));
    if (device_is_ready(flap.dev)) {
        auto flap_write = [&] {
            pwm_set_pulse_dt(&flap, PWM_USEC(window->get_flap_angle()));
        };
        window->on_flap_angle_changed(flap_write);  // 角度が動くたびに出す
        flap_write();                                // 起動時の初期位置
    }
#endif

#if DT_NODE_EXISTS(DT_ALIAS(carpark_tof))
    const device *tof = DEVICE_DT_GET(DT_ALIAS(carpark_tof));
    if (device_is_ready(tof)) {
        window->set_has_tof(true);
        tof_timer.start(slint::TimerMode::Repeated, std::chrono::seconds(1),
            [&] {
                static int detected;
                static bool armed = true; /* 精算後, 車が一度離れるまで再検知しない */
                const int mm = tof_read_mm(tof);
                const bool near = mm >= 0 && mm < NEAR_MM;
                if (window->get_screen() != Screen::Vacant) {
                    return;
                }
                if (!near) {
                    armed = true;
                    detected = 0;
                    return;
                }
                if (armed && ++detected >= DETECT_SECONDS) {
                    detected = 0;
                    armed = false;
                    window->invoke_arrive();
                }
            });
    }
#endif

#if DT_NODE_EXISTS(DT_ALIAS(carpark_scanner))
    if (device_is_ready(qr)) {
        window->set_has_scanner(true);
        /* 手動モードにして, 命令したときだけ読ませる. */
        const uint8_t manual[] = { 0x21, 0x61, 0x41, 0x00 };
        qr_send(manual, sizeof(manual));
        scanner.start(slint::TimerMode::Repeated, std::chrono::milliseconds(50),
            [&] {
                static bool decoding = false;
                const bool paying = window->get_screen() == Screen::Paying;
                if (paying != decoding) {
                    qr_set_decode(paying); /* 入ったら点灯・開始, 出たら停止 */
                    decoding = paying;
                }
                if (paying) {
                    char text[64];
                    if (qr_read(text, sizeof(text))) {
                        window->invoke_paid();
                    }
                }
            });
    }
#endif

    window->run();
    return 0;
}
