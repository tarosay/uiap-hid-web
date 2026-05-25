#pragma once
#include <WebHID.h>

/**
 * MazeHID — WebHID を通じた迷路サーバーとの通信を担うクラス
 *
 * Web ページ（迷路サーバー）とのコマンド送受信・
 * sense / moveTo などのプロトコル操作をカプセル化します。
 * 迷路の解き方（アルゴリズム）とは切り離されています。
 */
class MazeHID {
public:
    // ── コマンド (Web → UIAPduino, Feature Report 32byte) ──────
    static const uint8_t CMD_START     = 0x01;  // [1]=sx [2]=sy
    static const uint8_t CMD_RESULT    = 0x02;  // [1]=result_code
    static const uint8_t CMD_MAZE_SIZE = 0x03;  // [1]=width [2]=height
    static const uint8_t CMD_GOAL      = 0x04;  // [1]=gx [2]=gy
    static const uint8_t CMD_RESET     = 0x05;

    // ── コマンド (UIAPduino → Web, Input Report 8byte) ─────────
    static const uint8_t CMD_MOVE      = 0x10;  // [1]=x [2]=y  移動
    static const uint8_t CMD_SENSE     = 0x11;  // [1]=x [2]=y  確認のみ
    static const uint8_t CMD_SOLVED    = 0x12;  // 完了通知
    static const uint8_t CMD_READY     = 0x14;  // 起動完了

    // ── 結果コード ──────────────────────────────────────────────
    static const uint8_t RESULT_OPEN   = 0x00;  // 通路（移動できる）
    static const uint8_t RESULT_WALL   = 0x01;  // 壁（移動できない）
    static const uint8_t RESULT_GOAL   = 0x02;  // ゴール！
    static const uint8_t RESULT_START  = 0x03;  // スタート地点
    static const uint8_t RESULT_OUT    = 0x04;  // 範囲外

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
    bool available() {
        return WebHID.available();
    }

    /** データを受信する（16バイト固定） */
    void recv(uint8_t* buf, uint8_t len) {
        WebHID.recv(buf, len);
    }

    /** 迷路クリアを Web ページへ通知する */
    void sendSolved() {
        WebHID.send(CMD_SOLVED, 0, 0, 0, 0, 0, 0, 0);
    }

    /**
     * (x,y) のセルを確認する（移動しない）
     * @return 結果コード (RESULT_*)
     */
    uint8_t sense(uint8_t x, uint8_t y) {
        WebHID.send(CMD_SENSE, x, y, 0, 0, 0, 0, 0);
        delay(_waitMs);
        while (!WebHID.available()) {}
        uint8_t buf[16];
        WebHID.recv(buf, sizeof(buf));
        return buf[1]; // result_code
    }

    /**
     * (x,y) のセルへ移動する
     * @return 結果コード (RESULT_*)
     */
    uint8_t moveTo(uint8_t x, uint8_t y) {
        WebHID.send(CMD_MOVE, x, y, 0, 0, 0, 0, 0);
        delay(_waitMs);
        while (!WebHID.available()) {}
        uint8_t buf[16];
        WebHID.recv(buf, sizeof(buf));
        return buf[1]; // result_code
    }

private:
    static const uint8_t _waitMs = 15; // EP3 ポーリング待ち時間 (ms)
};
