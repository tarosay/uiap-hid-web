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
bool    rocks[16][16]; // true = 岩があるセル
uint8_t headX, headY;

// BFS 用グローバルキュー（スタックオーバーフロー防止）
static uint8_t bfsQx[256];
static uint8_t bfsQy[256];

// ── ボードのリセット ──────────────────────────────────────────
void clearAll() {
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            board[y][x] = false;
            rocks[y][x] = false;
        }
}

// ── ステージ開始 ──────────────────────────────────────────────
// 岩は CMD_ROCK で事前に設定済み。スネークのボードだけリセットする。
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
//   board[y][x]     ... スネークが占有しているか（true=NG）
//   rocks[y][x]     ... 岩があるか（true=NG）
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
 * 到達可能セル数のカウント（BFS）
 *
 * (sx, sy) から出発して壁・スネーク・岩を避けながら
 * 到達できるセルの個数を返す。
 * 「行き先が行き詰まりかどうか」の判断に使う。
 *
 * メモリ:
 *   bfsQx/Y[256] : グローバル (512 B)
 *   visited[16][16] : スタック (256 B)
 */
uint16_t countReachable(uint8_t sx, uint8_t sy) {
    bool visited[16][16];
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            visited[y][x] = false;

    const int8_t dx[] = { 1, 0, -1,  0 };
    const int8_t dy[] = { 0, 1,  0, -1 };
    uint16_t head = 0, tail = 0;

    bfsQx[tail] = sx;
    bfsQy[tail] = sy;
    tail++;
    visited[sy][sx] = true;

    while (head < tail) {
        uint8_t x = bfsQx[head];
        uint8_t y = bfsQy[head];
        head++;
        for (uint8_t d = 0; d < 4; d++) {
            int16_t nx = (int16_t)x + dx[d];
            int16_t ny = (int16_t)y + dy[d];
            if (nx < 0 || nx >= 16 || ny < 0 || ny >= 16) continue;
            if (visited[ny][nx] || board[ny][nx] || rocks[ny][nx]) continue;
            visited[ny][nx] = true;
            bfsQx[tail] = (uint8_t)nx;
            bfsQy[tail] = (uint8_t)ny;
            tail++;
        }
    }
    return tail; // 到達できたセルの数
}

/**
 * 最大空間優先（Maximize Space）アルゴリズム
 *
 * 4 方向それぞれについて到達可能セル数を BFS で数え、
 * 最も広い空間につながる方向を選ぶ。
 * 岩がある場合でも壁・岩・自分の体を正しく避けられる。
 */
void computeNextDir(int8_t& outDx, int8_t& outDy) {
    const int8_t DDX[] = { 1, 0, -1,  0 };
    const int8_t DDY[] = { 0, 1,  0, -1 };

    uint16_t bestCount = 0;
    bool     found     = false;

    for (uint8_t d = 0; d < 4; d++) {
        int16_t nx = (int16_t)headX + DDX[d];
        int16_t ny = (int16_t)headY + DDY[d];
        if (nx < 0 || nx >= 16 || ny < 0 || ny >= 16) continue;
        if (board[ny][nx] || rocks[ny][nx]) continue;

        uint16_t cnt = countReachable((uint8_t)nx, (uint8_t)ny);
        if (!found || cnt > bestCount) {
            bestCount = cnt;
            outDx     = DDX[d];
            outDy     = DDY[d];
            found     = true;
        }
    }

    if (!found) {
        // 逃げ場なし（ゲームオーバーになる）
        outDx = 1;
        outDy = 0;
    }
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
        case SnakeHID::CMD_RESET:
            // ステージリセット: ボードと岩を全消去
            clearAll();
            break;

        case SnakeHID::CMD_ROCK:
            // 岩の位置を登録（CMD_START の前に送られる）
            rocks[buf[2]][buf[1]] = true;
            break;

        case SnakeHID::CMD_START:
            // ゲーム開始: スネークのボードを初期化し、最初の方向を送信
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
    }
}
