/**
 * HidPrint — Hid クラスの Print 系メソッドと Recv() のデモスケッチ
 *
 * ボード:   HID ProMicro CH32V003 KBD+Mouse
 * バージョン: V1.4 + WebHID (EP3)
 * 対応ページ: hid-console.html
 *
 * 動作:
 *   1. 起動時に hid コンソールへ "=== HidPrint ===" を表示
 *   2. ブラウザの「送信」ボタンで届いたデータを hid.Recv() で受け取り
 *      受信内容をコンソールに表示する
 *
 * 使い方（Print）:
 *   hid.Clear();
 *   hid.Println("Hello");
 *   hid.Print("n="); hid.Println(42);
 *
 * 使い方（Recv）:
 *   uint8_t buf[16];
 *   uint8_t len = hid.Recv(buf, sizeof(buf));
 *   if (len > 0) { ... }
 */

#include <WebHID.h>
#include "Hid.h"

#define LED_BUILTIN 2

static uint32_t recvCount = 0;

void setup() {
  WebHID.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  delay(500);

  hid.Clear();
  hid.Println("=== HidPrint ===");
  hid.Println("Ready.");
}

void loop() {
  uint8_t buf[16];
  uint8_t len = hid.Recv(buf, sizeof(buf));

  if (len > 0) {
    recvCount++;

    digitalWrite(LED_BUILTIN, HIGH);

    hid.Clear();
    hid.Print("recv #");
    hid.Println((int)recvCount);
    hid.Print("len=");
    hid.Println((int)len);

    // 受信データを最大 8 バイト表示（[i]=value 形式）
    uint8_t show = (len < 8) ? len : 8;
    for (uint8_t i = 0; i < show; i++) {
      hid.Print("[");
      hid.Print((int)i);
      hid.Print("]=");
      hid.Println((int)buf[i]);
    }

    hid.Println("Ready.");
    delay(30);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
