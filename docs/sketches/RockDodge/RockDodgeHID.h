#pragma once
#include <WebHID.h>

/**
 * RockDodgeHID — WebHID を通じた Rock Dodge ゲームサーバーとの通信を担うクラス
 *
 * Web ページ（ゲームサーバー）とのコマンド送受信をカプセル化します。
 * よけ方の決定（アルゴリズム）とは切り離されています。
 */
class RockDodgeHID {
public:
    // ── コマンド (Web → UIAPduino, Feature Report 32byte) ──────
    static const uint8_t CMD_START    = 0x01;  // [1]=startX [2]=ノルマ [3]=ステップ数(120)
    static const uint8_t CMD_TICK     = 0x02;  // [1]=playerX [2]=maskL [3]=maskH
                                               // [4]=狙い岩列(255=なし, 12行目に出現)
                                               // [5..20]=列ごとのアイテム値
                                               //   0=なし / 1〜99=得点 / 255=★面クリア
    static const uint8_t CMD_RESET    = 0x05;  // 岩・アイテム・状態をリセット
    static const uint8_t CMD_GET_NAME = 0x40;  // プレイヤー名を要求

    // ── コマンド (UIAPduino → Web, Input Report 8byte) ─────────
    static const uint8_t CMD_DIR      = 0x10;  // [1]=dx（-1=左, 0=停止, +1=右）
    static const uint8_t CMD_READY    = 0x14;  // 起動完了
    static const uint8_t CMD_NAME     = 0x41;  // [1]=chunk(0/1) [2..7]=名前6文字

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

    /** データを受信する */
    void recv(uint8_t* buf, uint8_t len) { WebHID.recv(buf, len); }

    /**
     * 次の移動方向を Web ページへ送信する
     * @param dx  X 方向（-1=左, 0=停止, +1=右）
     */
    void sendDir(int8_t dx) {
        WebHID.send(CMD_DIR, (uint8_t)dx, 0, 0, 0, 0, 0, 0);
    }

    /**
     * プレイヤー名を Web ページへ送信する（6文字×2分割）
     * @param name  半角英数 12 文字までの名前
     */
    void sendName(const char* name) {
        uint8_t b[12];
        for (uint8_t i = 0; i < 12; i++) b[i] = 0;
        for (uint8_t i = 0; i < 12 && name[i]; i++) b[i] = (uint8_t)name[i];
        WebHID.send(CMD_NAME, 0, b[0], b[1], b[2],  b[3],  b[4],  b[5]);
        delay(10); // 連続送信の取りこぼし防止
        WebHID.send(CMD_NAME, 1, b[6], b[7], b[8],  b[9],  b[10], b[11]);
    }
};
