/**
 * HidBridgeTest
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     Keyboard+Mouse+WebHID（Tools → USB）
 *
 * 動作:
 *   - Web から Feature Report で受け取った 16 バイトを
 *     8 バイトずつ 2 回の Input Report でエコーバック
 *   - 自動カウンター送信なし（ブリッジテスト専用）
 *
 * 使い方:
 *   hid-serial-bridge.html で HID 接続後、ブリッジを Start。
 *   通信アプリ（Arduino IDE シリアルモニタ／シリアルプロッタ・TeraTerm 等）から COM9 に送信したデータが
 *   UIAPduino でエコーバックされ、COM9 に返ってくることを確認できる。
 */

#include <WebHID.h>

// EP3 ポーリング間隔は 10ms。15ms 待てば確実に次のポーリングが来る。
#define EP3_WAIT_MS  15

void setup() {
    WebHID.begin();
    delay(2000);  // USB 接続待ち
}

void loop() {
    if (WebHID.available()) {
        uint8_t buf[16];
        uint8_t len = WebHID.recv(buf, sizeof(buf));

        uint8_t sent = 0;
        while (sent < len) {
            uint8_t chunk = (len - sent > 8) ? 8 : (len - sent);
            WebHID.send(buf + sent, chunk);
            sent += chunk;
            if (sent < len) delay(EP3_WAIT_MS);
        }
    }
}
