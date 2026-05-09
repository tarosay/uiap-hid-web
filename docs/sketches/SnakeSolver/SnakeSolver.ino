/**
 * SnakeSolver
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     WebHID Only（Tools → USB）
 *
 * Web ページがスネークゲームを管理します。
 * このスケッチは次の移動方向を計算して返します。
 *
 * 【座標系】
 *   X = 列 (0〜15)、Y = 行 (0〜15)
 *   原点 (0,0) は左上。X が右、Y が下。
 *
 * 【ファイル構成】
 *   SnakeHID.h      ... WebHID 通信クラス（通信の詳細はここに集約）
 *   SnakeSolver.ino ... ゲーム状態・探索アルゴリズム（このファイル）
 */

#include "SnakeHID.h"

SnakeHID snake; // WebHID 通信を担うオブジェクト

// ── ゲーム状態 ────────────────────────────────────────────────
bool    board[16][16]; // true = スネークが占有しているセル
uint8_t headX, headY;

// ── ボードの初期化 ────────────────────────────────────────────
void initBoard(uint8_t sx, uint8_t sy) {
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            board[y][x] = false;
    headX = sx;
    headY = sy;
    board[sy][sx] = true;
}

// ══════════════════════════════════════════════════════════════
// ここにスネークの移動方向を決めるアルゴリズムを実装する！
//
// 利用できる情報:
//   headX, headY    ... 現在の頭の座標
//   board[y][x]     ... そのセルを占有しているか（true=NG）
//
// 利用できる関数:
//   snake.sendDir(dx, dy)  ... 次の方向を送信（必ず呼ぶ）
//
// 方向の表現:
//   dx=+1, dy= 0  ... 右
//   dx=-1, dy= 0  ... 左
//   dx= 0, dy=+1  ... 下
//   dx= 0, dy=-1  ... 上
// ══════════════════════════════════════════════════════════════

/**
 * Hamiltonian Path（蛇行型）アルゴリズム
 *
 * 16×16 グリッドを以下のように蛇行する経路を辿る:
 *   偶数行 → 左から右へ (x: 0→15)
 *   奇数行 → 右から左へ (x: 15→0)
 *
 * この経路は全 256 マスを 1 度ずつ通過するハミルトン路なので、
 * どこからスタートしても壁・自分自身に衝突せず全マス埋められる。
 *
 * 計算方法:
 *   1. 現在位置 (headX, headY) を経路上の番号 pos に変換
 *   2. 次の番号 (pos+1) % 256 を座標 (nx, ny) に逆変換
 *   3. (nx-headX, ny-headY) が次の移動方向
 */
void computeNextDir(int8_t& outDx, int8_t& outDy) {
    // 現在位置を経路番号に変換
    uint16_t pos;
    if (headY % 2 == 0) {
        pos = (uint16_t)headY * 16 + headX;          // 偶数行: 左→右
    } else {
        pos = (uint16_t)headY * 16 + (15 - headX);   // 奇数行: 右→左
    }

    // 次の経路番号を座標に変換
    uint16_t nextPos = (pos + 1) % 256;
    uint8_t  ny = (uint8_t)(nextPos / 16);
    uint8_t  nx = (ny % 2 == 0)
                  ? (uint8_t)(nextPos % 16)           // 偶数行: 左→右
                  : (uint8_t)(15 - nextPos % 16);     // 奇数行: 右→左

    outDx = (int8_t)nx - (int8_t)headX;
    outDy = (int8_t)ny - (int8_t)headY;
}

void solveTick() {
    int8_t dx, dy;
    computeNextDir(dx, dy);
    snake.sendDir(dx, dy);
}

// ── setup / loop ──────────────────────────────────────────────
void setup() {
    snake.begin(); // USB HID 初期化 + CMD_READY 送信
}

void loop() {
    if (!snake.available()) return;

    uint8_t buf[16];
    snake.recv(buf, sizeof(buf));

    switch (buf[0]) {
        case SnakeHID::CMD_START:
            // ゲーム開始: ボードを初期化し、最初の方向を送信
            initBoard(buf[1], buf[2]);
            solveTick();
            break;

        case SnakeHID::CMD_TICK:
            // 1ステップ進んだ: 頭の位置を更新し、次の方向を送信
            headX = buf[1];
            headY = buf[2];
            board[headY][headX] = true;
            solveTick();
            break;

        case SnakeHID::CMD_RESET:
            // リセット（必要なら処理を追加）
            break;
    }
}
