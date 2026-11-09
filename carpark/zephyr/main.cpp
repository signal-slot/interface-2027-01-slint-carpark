#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/sensor.h>
#include "slint-zephyr.h"   // Platform (下回り) の初期化
#include "carpark.h"        // carpark.slint から生成される

constexpr int NEAR_MM = 150;       // これより近ければ車あり
constexpr int DETECT_SECONDS = 3;  // この秒数続いたら入庫と判定
using Win = slint::ComponentHandle<AppWindow>;  // AppWindow のハンドルの別名

int detected = 0;    // 近いと数えた連続秒数

// driver から距離を mm で読む. 測れなければ -1.
int tof_read_mm(const device *tof)
{
    sensor_value v{};
    if (sensor_sample_fetch(tof) != 0 ||
        sensor_channel_get(tof, SENSOR_CHAN_DISTANCE, &v) != 0) {
        return -1;
    }
    return v.val1 * 1000 + v.val2 / 1000;
}

// 1 秒ごとに呼ぶ. 空車のときだけ距離を読み, 近い状態が続いたら入庫を
// 知らせる. 一瞬の反射で切り替わらないよう, 連続した秒数で数える.
void tick(const Win &window, const device *tof)
{
    if (window->get_screen() != Screen::Vacant) {
        detected = 0;     // 空車以外では読まない
        return;
    }
    const int mm = tof_read_mm(tof);
    if (mm < 0 || mm >= NEAR_MM) {
        detected = 0;     // 車が居ない
        return;
    }
    if (++detected >= DETECT_SECONDS) {
        detected = 0;
        window->invoke_arrive();  // ボタンと同じ入庫の処理
    }
}

int main(void)
{
    // 画面の出力先. 第 1 部で見た devicetree の chosen から引く.
    const struct device *display =
        DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    slint_zephyr_init(display);         // (1) 下回りを Zephyr 版にする
    auto window = AppWindow::create();  // (2) .slint の AppWindow が C++ の型に
    slint::Timer clock;   // .slint の Timer (リスト 8) の C++ 版. 毎秒 tick を呼ぶ
#if DT_NODE_EXISTS(DT_ALIAS(carpark_tof))   // overlay で ToF を宣言したボードだけ
    const device *tof = DEVICE_DT_GET(DT_ALIAS(carpark_tof));
    if (device_is_ready(tof)) {             // 挿さっていて, 応答した
        window->set_has_tof(true);          // 手動ボタンを消す
        clock.start(slint::TimerMode::Repeated, std::chrono::seconds(1),
                    [&] { tick(window, tof); });
    }
#endif
    window->run();                      // (3) イベントループを回す
    return 0;
}
