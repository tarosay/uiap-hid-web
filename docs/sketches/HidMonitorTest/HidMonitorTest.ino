/**
 * HidMonitorTest
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     WebHID Only（Tools → USB）
 *
 * 動作:
 *   - ブラウザ接続待ち（hid.WaitAvailable()）
 *   - 接続後にタイトルヘッダを送信
 *   - 1 秒ごとにカウンタを送信
 *
 * 使い方:
 *   hid-serial-bridge.html で HID 接続後、Protocol モードで受信。
 *   ブリッジの COMモニタ側（TeraTerm 等）でカウンタ出力を確認できます。
 */

#include <WebHID.h>
#include "Hid.h"

#define LED_BUILTIN 2

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  WebHID.begin();
  hid.WaitAvailable();  // ブラウザ接続待ち（Feature Report 到着まで、データは消費しない）
  hid.Ready();        // 準備完了通知（0x53）

  hid.Println("===========================");
  hid.Println("  HidMonitorTest");
  hid.Println("  UIAPduino WebHID Bridge");
  hid.Println("===========================");
  hid.Println("Board : HID ProMicro CH32V003");
  hid.Println("USB   : WebHID Only");
  hid.Println("---------------------------");
  hid.Println("Loop start.");
}

void loop() {
  static int count = 0;
  count++;

  digitalWrite(LED_BUILTIN, count % 2 == 0 ? LOW : HIGH);
  hid.Print("count: ");
  hid.Println(count);
  delay(1000);
}
