/**
 * HidBridgeTest
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     Keyboard+Mouse+WebHID（Tools → USB）
 *
 * 動作:
 *   - 起動時に hid.Ready() で準備完了をブラウザへ通知
 *   - ブラウザから Feature Report で受け取ったデータを
 *     hid.Send() でそのまま Raw エコーバック
 *
 * 使い方:
 *   hid-serial-bridge.html で HID 接続後、Raw Binary モードでブリッジを Start。
 *   COMモニタ（TeraTerm 等）から送ったデータが UIAPduino を経由してそのまま返ってきます。
 */

#include <WebHID.h>
#include "Hid.h"

void setup() {
    WebHID.begin();
    delay(2000);    // USB 接続待ち
    hid.Ready();    // 準備完了通知（0x53）
}

void loop() {
    uint8_t buf[16];
    uint8_t len = hid.Recv(buf, sizeof(buf));
    if (len > 0) hid.Send(buf, len);   // Raw エコーバック
}
