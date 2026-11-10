/**
 * HidGraph — HID Graph 用のデモスケッチ（センサーなしで試せます）
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     Keyboard+Mouse+WebHID（Tools → USB）
 * 対応ページ: hid-graph.html
 *
 * 動作:
 *   ボールを 1 m の高さから落として、床ではずませる計算をします。
 *   高さを 50 ms ごとに「h[mm]:値」の形で送ります。
 *   床に当たった時は "bounce"、落とし直す時は "drop" を送ります。
 *
 * HID Graph での見え方:
 *   ・「h [mm]」の段に、高さのグラフが出ます
 *   ・"bounce" と "drop" は数値ではないので、グラフの上の「印」になります
 *   ・h の段の「＋変化」を押すと、速度 [mm/s] の段が出ます
 *     その段でもう一度「＋変化」を押すと、加速度 [mm/s²] の段が出ます
 *     （落ちている間の加速度は、重力の -9800 mm/s² あたりになります）
 *
 * 数値の書き方（HID Graph が自動で見分けます）:
 *   hid.Println(123);                          // 1 行に数値 1 つ
 *   hid.Println("12,34,56");                   // 1 行に数値を複数
 *   hid.Print("temp:"); hid.Println(23);       // 名前つき
 *   hid.Print("temp[℃]:"); hid.Println(23);   // 名前と単位
 */

#include <WebHID.h>
#include "Hid.h"

#define PERIOD_MS  50      // 高さを送る間隔 [ms]
#define DROP_UM    1000000 // 落とす高さ [µm]（= 1 m）
#define GRAVITY    9800    // 重力加速度 [mm/s²]

static int32_t  h;         // 高さ [µm]
static int32_t  v;         // 速度 [mm/s]（上向きが +）
static uint32_t lastMs;

void setup() {
  WebHID.begin();

  hid.WaitAvailable();  // ブラウザ接続待ち
  hid.Clear();          // HID Graph の記録を消して、0 から始める

  h = DROP_UM;
  v = 0;
  lastMs = millis();
  hid.Println("drop");
}

void loop() {
  uint32_t now = millis();
  int32_t dt = (int32_t)(now - lastMs);   // 前回からの時間 [ms]
  if (dt < PERIOD_MS) return;
  lastMs = now;

  // 速度と高さを、dt だけ進める
  v -= GRAVITY * dt / 1000;   // [mm/s]
  h += v * dt;                // [mm/s] × [ms] = [µm]

  if (h <= 0) {
    // 床に当たった。はね返りで速さが 8 割になる
    h = -h;
    v = -v * 8 / 10;
    if (v < 800) {
      // ほとんどはずまなくなったら、落とし直す
      h = DROP_UM;
      v = 0;
      hid.Println("drop");
    } else {
      hid.Println("bounce");
    }
  }

  hid.Print("h[mm]:");
  hid.Println((int)(h / 1000));
}
