#pragma once
#include <WebHID.h>

/**
 * SnakeHID — WebHID を通じたスネークゲームサーバーとの通信を担うクラス
 *
 * Web ページ（ゲームサーバー）とのコマンド送受信をカプセル化します。
 * スネークの移動方向の決定（アルゴリズム）とは切り離されています。
 */
class SnakeHID {
public:
    // ── コマンド (Web → UIAPduino, Feature Report 16byte) ──────
    static const uint8_t CMD_START  = 0x01;  // [1]=startX [2]=startY
    static const uint8_t CMD_TICK   = 0x02;  // [1]=headX  [2]=headY（移動後）
    static const uint8_t CMD_RESET  = 0x05;

    // ── コマンド (UIAPduino → Web, Input Report 8byte) ─────────
    static const uint8_t CMD_DIR    = 0x10;  // [1]=dx [2]=dy（次の移動方向）
    static const uint8_t CMD_READY  = 0x14;  // 起動完了

    /**
     * USB HID の初期化と起動通知を行う
     * setup() の先頭で呼ぶ
     */
    void begin() {
        WebHID.begin();
        delay(2000); // USB 接続待ち
        WebHID.send(CMD_READY, 0, 0, 0, 0, 0, 0, 0);
    }

    /** Web ページからデータが届いているか */
    bool available() { return WebHID.available(); }

    /** データを受信する（16バイト固定） */
    void recv(uint8_t* buf, uint8_t len) { WebHID.recv(buf, len); }

    /**
     * 次の移動方向を Web ページへ送信する
     * @param dx  X 方向（-1=左, 0=なし, +1=右）
     * @param dy  Y 方向（-1=上, 0=なし, +1=下）
     */
    void sendDir(int8_t dx, int8_t dy) {
        WebHID.send(CMD_DIR, (uint8_t)dx, (uint8_t)dy, 0, 0, 0, 0, 0);
    }
};
