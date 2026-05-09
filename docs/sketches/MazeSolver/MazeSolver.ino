/**
 * MazeSolver
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     WebHID Only（Tools → USB）
 *
 * Webページが迷路を管理します。
 * このスケッチは迷路の座標を問い合わせながら経路を探索します。
 *
 * 【座標系】
 *   X = 列 (0〜14)、Y = 行 (0〜14)
 *   原点 (0,0) は左上。X が右、Y が下。
 *   ゴールは X=15 または Y=15 の出口セルです。
 *
 * 【ファイル構成】
 *   MazeHID.h     ... WebHID 通信クラス（通信の詳細はここに集約）
 *   MazeSolver.ino... 迷路情報・探索アルゴリズム（このファイル）
 */

#include "MazeHID.h"

MazeHID maze; // WebHID 通信を担うオブジェクト

// ── 迷路情報 ──────────────────────────────────────────────────
uint8_t startX, startY;
uint8_t goalX,  goalY;
uint8_t mazeW,  mazeH;

// ── 簡易乱数 (LCG) ───────────────────────────────────────────
// rand() は -nostdlib 環境では使えないため自前で実装
static uint32_t _rng = 12345;
uint8_t myRand(uint8_t n) {
    _rng = _rng * 1664525 + 1013904223;
    return (uint8_t)((_rng >> 16) % n);
}

// ══════════════════════════════════════════════════════════════
// ここに迷路探索アルゴリズムを実装する！
//
// 利用できる情報:
//   startX, startY  ... スタート座標
//   goalX,  goalY   ... ゴール座標 (X=15 or Y=15 の出口)
//   mazeW,  mazeH   ... 迷路サイズ (15, 15)
//
// 利用できる関数:
//   maze.sense(x, y)   ... そのセルを調べる（移動しない）
//   maze.moveTo(x, y)  ... そのセルへ移動する
//   どちらも MazeHID::RESULT_* を返す
//
// アルゴリズム例:
//   - 右手法（Wall Follower）
//   - 深さ優先探索（DFS）
//   - 幅優先探索（BFS）
// ══════════════════════════════════════════════════════════════
void solveMaze() {
    solveMazeGreedyDFS();
}

// ── 共通ヘルパー ──────────────────────────────────────────────

/** 通過できる結果コードかどうか */
static bool isPassable(uint8_t r) {
    return (r == MazeHID::RESULT_OPEN ||
            r == MazeHID::RESULT_START ||
            r == MazeHID::RESULT_GOAL);
}

/** 移動先として有効な座標かどうか */
static bool isValidTarget(int16_t x, int16_t y) {
    if (x < 0 || y < 0) return false;

    // 通常セル
    if (x < mazeW && y < mazeH) return true;

    // ゴール出口だけは X=15 または Y=15 の外側セルを許可
    if ((uint8_t)x == goalX && (uint8_t)y == goalY) return true;

    return false;
}

// ── 右手法（Wall Follower） ───────────────────────────────────
void solveMazeRHand() {
    uint8_t x = startX, y = startY;

    // dir: 0=右 1=下 2=左 3=上
    const int8_t dx[] = { 1, 0, -1,  0 };
    const int8_t dy[] = { 0, 1,  0, -1 };

    uint8_t dir = 0; // 初期向き（右）

    while (true) {
        const uint8_t right = (dir + 1) & 3;
        const uint8_t front = dir;
        const uint8_t left  = (dir + 3) & 3;
        const uint8_t back  = (dir + 2) & 3;

        // 右 → 前 → 左 → 後ろ の順に試す
        const uint8_t order[] = { right, front, left, back };
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t d  = order[i];
            int8_t  nx = (int8_t)x + dx[d];
            int8_t  ny = (int8_t)y + dy[d];
            if (!isValidTarget(nx, ny)) continue;

            uint8_t r = maze.moveTo((uint8_t)nx, (uint8_t)ny);
            if (r == MazeHID::RESULT_GOAL) return;
            if (r == MazeHID::RESULT_OPEN || r == MazeHID::RESULT_START) {
                x = (uint8_t)nx;
                y = (uint8_t)ny;
                dir = d;
                break;
            }
        }
    }
}

// ── 深さ優先探索（DFS） ───────────────────────────────────────
void solveMazeDFS() {
    const int8_t dx[] = { 1, 0, -1,  0 };
    const int8_t dy[] = { 0, 1,  0, -1 };

    bool    visited[16][16] = {};
    uint8_t stackX[256];
    uint8_t stackY[256];
    uint8_t nextDir[256];

    uint16_t sp = 0;
    stackX[sp]  = startX;
    stackY[sp]  = startY;
    nextDir[sp] = 0;
    visited[startY][startX] = true;

    while (true) {
        uint8_t x = stackX[sp];
        uint8_t y = stackY[sp];
        bool advanced = false;

        while (nextDir[sp] < 4) {
            uint8_t d = nextDir[sp]++;
            int16_t nx = (int16_t)x + dx[d];
            int16_t ny = (int16_t)y + dy[d];

            if (!isValidTarget(nx, ny)) continue;
            if (visited[ny][nx]) continue;
            if (!isPassable(maze.sense((uint8_t)nx, (uint8_t)ny))) continue;

            uint8_t mr = maze.moveTo((uint8_t)nx, (uint8_t)ny);
            if (mr == MazeHID::RESULT_GOAL) return;
            if (mr == MazeHID::RESULT_OPEN || mr == MazeHID::RESULT_START) {
                if (sp + 1 >= 256) return;
                sp++;
                stackX[sp]  = (uint8_t)nx;
                stackY[sp]  = (uint8_t)ny;
                nextDir[sp] = 0;
                visited[ny][nx] = true;
                advanced = true;
                break;
            }
        }

        if (advanced) continue;

        // 行き止まり → バックトラック
        if (sp == 0) return;
        sp--;
        uint8_t br = maze.moveTo(stackX[sp], stackY[sp]);
        if (!(br == MazeHID::RESULT_OPEN || br == MazeHID::RESULT_START)) return;
    }
}

// ── Greedy DFS（ゴール方向を優先する深さ優先探索） ────────────
//
// 通常の DFS との違い:
//   各セルで隣接4方向を試す順序を「ゴールへのマンハッタン距離が
//   小さくなる方向」から優先する。
//   ゴール座標が既知（goalX, goalY）であることを活かしたアルゴリズム。
//
// メモリ:
//   visited[16][16] : 256 B
//   stackX/Y[256]   : 512 B
//   tried[256]      : 256 B  ← 試した方向のビットマスク（4bit/frame）
//   合計 ≒ 1024 B
static uint16_t manhattanToGoal(int16_t x, int16_t y) {
    int16_t ddx = x - (int16_t)goalX;
    int16_t ddy = y - (int16_t)goalY;
    if (ddx < 0) ddx = -ddx;
    if (ddy < 0) ddy = -ddy;
    return (uint16_t)(ddx + ddy);
}

void solveMazeGreedyDFS() {
    const int8_t DDX[] = { 1, 0, -1,  0 };
    const int8_t DDY[] = { 0, 1,  0, -1 };

    bool    visited[16][16] = {};
    uint8_t stackX[256];
    uint8_t stackY[256];
    uint8_t tried[256]; // 各フレームで試した方向のビットマスク

    uint16_t sp   = 0;
    stackX[sp]    = startX;
    stackY[sp]    = startY;
    tried[sp]     = 0;
    visited[startY][startX] = true;

    while (true) {
        uint8_t x = stackX[sp];
        uint8_t y = stackY[sp];

        // 未試行の有効な隣接セルのうち、ゴールへのマンハッタン距離が
        // 最小になる方向を選ぶ
        uint16_t bestDist = 0xFFFF;
        int8_t   bestDir  = -1;

        for (uint8_t d = 0; d < 4; d++) {
            if (tried[sp] & (1 << d)) continue; // 試し済み
            int16_t nx = (int16_t)x + DDX[d];
            int16_t ny = (int16_t)y + DDY[d];
            if (!isValidTarget(nx, ny)) continue;
            if (visited[(uint8_t)ny][(uint8_t)nx]) continue;
            uint16_t dist = manhattanToGoal(nx, ny);
            if (dist < bestDist) { bestDist = dist; bestDir = (int8_t)d; }
        }

        if (bestDir < 0) {
            // 全方向試した → バックトラック
            if (sp == 0) return;
            sp--;
            uint8_t br = maze.moveTo(stackX[sp], stackY[sp]);
            if (br != MazeHID::RESULT_OPEN && br != MazeHID::RESULT_START) return;
            continue;
        }

        tried[sp] |= (1 << bestDir);

        int16_t nx = (int16_t)x + DDX[bestDir];
        int16_t ny = (int16_t)y + DDY[bestDir];

        // まず sense で確認（壁なら次の方向へ）
        if (!isPassable(maze.sense((uint8_t)nx, (uint8_t)ny))) continue;

        uint8_t mr = maze.moveTo((uint8_t)nx, (uint8_t)ny);
        if (mr == MazeHID::RESULT_GOAL) return;

        if (mr == MazeHID::RESULT_OPEN || mr == MazeHID::RESULT_START) {
            if (sp + 1 >= 256) return;
            sp++;
            stackX[sp] = (uint8_t)nx;
            stackY[sp] = (uint8_t)ny;
            tried[sp]  = 0;
            visited[(uint8_t)ny][(uint8_t)nx] = true;
        }
    }
}

// ── 幅優先探索（BFS） ─────────────────────────────────────────
void solveMazeBFS() {
    const int8_t dx[] = { 1, 0, -1,  0 };
    const int8_t dy[] = { 0, 1,  0, -1 };

    bool    visited[16][16]   = {};
    uint8_t parentDir[16][16];
    uint8_t qx[256], qy[256];

    for (uint8_t yy = 0; yy < 16; yy++)
        for (uint8_t xx = 0; xx < 16; xx++)
            parentDir[yy][xx] = 255;

    uint16_t head = 0, tail = 0;
    qx[tail] = startX;
    qy[tail] = startY;
    tail++;
    visited[startY][startX] = true;

    bool found = false;

    while (head < tail) {
        uint8_t x = qx[head];
        uint8_t y = qy[head];
        head++;

        for (uint8_t d = 0; d < 4; d++) {
            int16_t nx = (int16_t)x + dx[d];
            int16_t ny = (int16_t)y + dy[d];

            if (!isValidTarget(nx, ny)) continue;
            if (visited[ny][nx]) continue;

            uint8_t sr = maze.sense((uint8_t)nx, (uint8_t)ny);
            if (!isPassable(sr)) continue;

            visited[ny][nx]   = true;
            parentDir[ny][nx] = d;

            if (sr == MazeHID::RESULT_GOAL ||
                ((uint8_t)nx == goalX && (uint8_t)ny == goalY)) {
                found = true;
                break;
            }

            if (tail >= 256) return;
            qx[tail] = (uint8_t)nx;
            qy[tail] = (uint8_t)ny;
            tail++;
        }

        if (found) break;
    }

    if (!found) return;

    // ゴールからスタートへ親をたどって経路復元
    uint8_t pathX[256], pathY[256];
    uint16_t pathLen = 0;
    uint8_t cx = goalX, cy = goalY;

    while (!(cx == startX && cy == startY)) {
        if (pathLen >= 256) return;
        pathX[pathLen] = cx;
        pathY[pathLen] = cy;
        pathLen++;

        uint8_t d = parentDir[cy][cx];
        if (d == 255) return;
        cx = (uint8_t)((int16_t)cx - dx[d]);
        cy = (uint8_t)((int16_t)cy - dy[d]);
    }

    // スタートからゴールへ移動
    while (pathLen > 0) {
        pathLen--;
        uint8_t r = maze.moveTo(pathX[pathLen], pathY[pathLen]);
        if (r == MazeHID::RESULT_GOAL) return;
        if (!(r == MazeHID::RESULT_OPEN || r == MazeHID::RESULT_START)) return;
    }
}

// ── setup / loop ──────────────────────────────────────────────
void setup() {
    maze.begin(); // USB HID 初期化 + CMD_READY 送信
}

void loop() {
    if (!maze.available()) return;

    uint8_t buf[16];
    maze.recv(buf, sizeof(buf));

    switch (buf[0]) {
        case MazeHID::CMD_MAZE_SIZE:
            mazeW = buf[1]; mazeH = buf[2];
            break;
        case MazeHID::CMD_GOAL:
            goalX = buf[1]; goalY = buf[2];
            break;
        case MazeHID::CMD_START:
            startX = buf[1]; startY = buf[2];
            solveMaze();
            maze.sendSolved();
            break;
        case MazeHID::CMD_RESET:
            // 必要ならリセット処理をここに記述
            break;
    }
}
