/**
 * SnakeVS
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     WebHID Only（Tools → USB）
 *
 * Web ページが対戦スネークを管理します。
 * このスケッチは自分の蛇の次の移動方向を計算して返します。
 *
 * 【ゲームのルール】
 *   ・16×16 の盤面。自分と相手の 2 匹が毎ティック 1 マスずつ成長する（縮まない）
 *   ・壁・岩・どちらかの蛇の体に進入したら死亡。生き残ったほうがラウンド勝ち
 *   ・両者の頭が同じマスに入る／頭を入れ替える動きは両者死亡＝引き分け
 *   ・CPU 5 段階のラダーを 3 本勝負で勝ち上がる
 *
 * 【相手の体の復元 — 重要】
 *   蛇は毎ティック成長して縮まないので、
 *   「相手の体」＝「相手の頭が通った全マス」です。
 *   だから Web は相手の頭の座標だけ送ってくれれば足りて、
 *   こちらは受け取った頭を盤面に記録していくだけで相手の体が完全に分かります。
 *
 * 【このサンプルの実装レベル】
 *   BFS 最大空間優先（Snake Solver のヒント 4 と同じもの）＋
 *   正面衝突の回避。自分が最も広い空間へ進む手を選びますが、
 *   相手の空間を削る意識がありません。
 *   CPU Lv1〜2 は倒せて Lv3 と互角、Lv4（ボロノイ）で止まります。
 *   ここから先はキミのアルゴリズムで！
 *
 * 【座標系】
 *   X = 列 (0〜15)、Y = 行 (0〜15)
 *   原点 (0,0) は左上。X が右、Y が下。
 *
 * 【ファイル構成】
 *   SnakeVSHID.h  ... WebHID 通信クラス（通信の詳細はここに集約）
 *   SnakeVS.ino   ... ゲーム状態・対戦アルゴリズム（このファイル）
 */

#include "SnakeVSHID.h"

// ★ 自分の名前に書き換えよう！（半角英数 12 文字まで）
//    対戦画面の表示とランキングに使われます。
//    2 台で対戦するときは、必ずお互い違う名前にすること！
#define PLAYER_NAME "NO NAME"

SnakeVSHID vs; // WebHID 通信を担うオブジェクト

// ── ゲーム状態 ────────────────────────────────────────────────
// 1 セルの状態を 1 バイトにまとめている。
// bool の配列を 3 枚持つより RAM が 512 バイト少なくて済むので、
// キミが自分のアルゴリズム用に配列を追加する余裕が生まれる。
const uint8_t EMPTY = 0;   // 空きマス
const uint8_t MINE  = 1;   // 自分の体
const uint8_t OPP   = 2;   // 相手の体
const uint8_t ROCK  = 3;   // 岩

uint8_t cell[16][16];  // 上の 4 状態のどれか
uint8_t myX,  myY;     // 自分の頭
uint8_t oppX, oppY;    // 相手の頭
int8_t  curDx, curDy;  // 現在進んでいる向き

// BFS 用グローバルキュー（スタックオーバーフロー防止）
// 座標は (y << 4) | x の 1 バイトに詰めている
static uint8_t bfsQ[256];

// ── 乱数 ──────────────────────────────────────────────────────
// 2 台対戦では両者が同じスケッチを積んでいる。
// 完全に決定論だと 180° 対称の盤面で鏡像同士の動きになり、
// 中央で正面衝突して毎回引き分けになってしまう。
// そこで手の評価順をシャッフルして、鏡像を崩す。
static uint32_t rngState = 1;

static uint32_t rnd() {
    rngState = rngState * 1664525u + 1013904223u;
    return rngState >> 16;
}

/**
 * PLAYER_NAME と開始座標から乱数の種を作る。
 * P1 と P2 では開始座標が入れ替わるので、
 * 万一 2 台の名前が同じでも必ず違う乱数列になる。
 */
static void seedRng(uint8_t mx, uint8_t my, uint8_t ox, uint8_t oy) {
    uint32_t h = 2166136261u;
    for (const char* p = PLAYER_NAME; *p; p++) {
        h ^= (uint8_t)(*p);
        h *= 16777619u;
    }
    rngState = h ^ ((uint32_t)mx * 31u + (uint32_t)my * 17u + (uint32_t)ox * 7u + oy);
    if (rngState == 0) rngState = 1;
}

// ── 盤面のリセット ────────────────────────────────────────────
void clearAll() {
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            cell[y][x] = EMPTY;
}

// ── ラウンド開始 ──────────────────────────────────────────────
// 岩は CMD_ROCK で事前に設定済み。蛇の状態だけ初期化する。
void initRound(uint8_t mx, uint8_t my, uint8_t ox, uint8_t oy, int8_t dx, int8_t dy) {
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            if (cell[y][x] != ROCK) cell[y][x] = EMPTY;
    myX = mx; myY = my;
    oppX = ox; oppY = oy;
    cell[myY][myX]   = MINE;
    cell[oppY][oppX] = OPP;
    curDx = dx; curDy = dy;
}

// ══════════════════════════════════════════════════════════════
// ここに対戦アルゴリズムを実装する！
//
// 利用できる情報:
//   myX, myY        ... 自分の頭の座標
//   oppX, oppY      ... 相手の頭の座標
//   cell[y][x]      ... EMPTY / MINE / OPP / ROCK のどれか
//   curDx, curDy    ... いま進んでいる向き
//   passable(x, y)  ... そこへ進入できるか（壁の外も判定してくれる）
//
// 利用できる関数:
//   vs.sendDir(dx, dy)  ... 次の方向を送信（必ず呼ぶ）
//
// 方向の表現:
//   dx=+1, dy= 0 ... 右   dx=-1, dy= 0 ... 左
//   dx= 0, dy=+1 ... 下   dx= 0, dy=-1 ... 上
//
// 【勝つための考え方】
//   このサンプルの弱点は「自分の空間しか見ていない」こと。
//   勝つには「相手の空間を削る」視点が必要（ヒント 4 のボロノイ）。
//
//   すでに入っている工夫は 2 つ。どちらも外すと勝負にならない:
//     ・相手が次に入れるマスを避ける（正面衝突＝引き分けの最大の原因）
//     ・手の評価順をシャッフルする（絶対方向の癖と鏡像ロックを防ぐ）
//
//   スタート配置は 180° 対称の 6 種類。絶対方向で優先順位を決めると
//   盤面の左右で癖が出るので、必ず盤面から計算した基準で選ぶこと。
// ══════════════════════════════════════════════════════════════

/** そのセルに進入できるか（壁・岩・両者の体をまとめて判定） */
bool passable(int16_t x, int16_t y) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return false;
    return cell[y][x] == EMPTY;
}

/**
 * 到達可能セル数のカウント（BFS）
 *
 * (sx, sy) から出発して、壁・岩・両者の体を避けながら
 * 到達できるセルの個数を返す。
 * 「その方向が行き止まりかどうか」の判断に使う。
 */
uint16_t countReachable(uint8_t sx, uint8_t sy) {
    bool visited[16][16];
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            visited[y][x] = false;

    const int8_t dx[] = { 1, 0, -1,  0 };
    const int8_t dy[] = { 0, 1,  0, -1 };
    uint16_t head = 0, tail = 0;

    bfsQ[tail++] = (uint8_t)((sy << 4) | sx);
    visited[sy][sx] = true;

    while (head < tail) {
        uint8_t p = bfsQ[head++];
        uint8_t x = p & 15;         // 下位 4 ビットが X
        uint8_t y = p >> 4;         // 上位 4 ビットが Y
        for (uint8_t d = 0; d < 4; d++) {
            int16_t nx = (int16_t)x + dx[d];
            int16_t ny = (int16_t)y + dy[d];
            if (!passable(nx, ny)) continue;
            if (visited[ny][nx]) continue;
            visited[ny][nx] = true;
            bfsQ[tail++] = (uint8_t)((ny << 4) | nx);
        }
    }
    return tail; // 到達できたセルの数
}

/**
 * 最大空間優先（Maximize Space）アルゴリズム
 *
 * 4 方向それぞれについて到達可能セル数を BFS で数え、
 * 最も広い空間につながる方向を選ぶ。
 *
 * これは Snake Solver のヒント 4 と同じ考え方。
 * 「自分が詰まないこと」だけを見ていて、相手を詰ませる視点がない。
 * CPU Lv4（ボロノイ）に勝つにはここを超える必要がある。
 */
void computeNextDir(int8_t& outDx, int8_t& outDy) {
    const int8_t DDX[] = { 1, 0, -1,  0 };
    const int8_t DDY[] = { 0, 1,  0, -1 };

    // 評価する順番をシャッフルする。
    // 絶対方向の固定順（右→下→左→上）で調べると、盤面の左右で癖が出て
    // 同じスケッチ同士の対戦で片側が有利になってしまう。
    uint8_t order[4] = { 0, 1, 2, 3 };
    for (uint8_t i = 3; i > 0; i--) {
        uint8_t j = (uint8_t)(rnd() % (i + 1));
        uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }

    int16_t bestScore = -30000;
    bool    found     = false;

    for (uint8_t k = 0; k < 4; k++) {
        uint8_t d = order[k];
        int16_t nx = (int16_t)myX + DDX[d];
        int16_t ny = (int16_t)myY + DDY[d];
        if (!passable(nx, ny)) continue;
        if (nx == (int16_t)oppX && ny == (int16_t)oppY) continue; // 相手の頭そのもの

        int16_t score = (int16_t)countReachable((uint8_t)nx, (uint8_t)ny);

        // 【重要】相手も次の 1 手で入れるマスは危険。
        // 同時着手なので、そこへ進むと相手も同じマスを選んでいた場合に
        // 正面衝突して両者死亡（引き分け）になる。
        // 避けるべきは「相手のいまの頭」ではなく「相手が次に入れるマス」。
        int16_t dxo = nx - (int16_t)oppX; if (dxo < 0) dxo = -dxo;
        int16_t dyo = ny - (int16_t)oppY; if (dyo < 0) dyo = -dyo;
        if (dxo + dyo == 1) score -= 40;

        if (!found || score > bestScore) {
            bestScore = score;
            outDx     = DDX[d];
            outDy     = DDY[d];
            found     = true;
        }
    }

    if (!found) {
        // 逃げ場なし。まっすぐ進んで散る
        outDx = curDx;
        outDy = curDy;
    }
}

void solveTick() {
    int8_t dx = curDx, dy = curDy;
    computeNextDir(dx, dy);
    curDx = dx; curDy = dy;
    vs.sendDir(dx, dy);
}

// ── setup / loop ──────────────────────────────────────────────
void setup() {
    vs.begin(); // USB HID 初期化 + CMD_READY 送信
}

void loop() {
    if (!vs.available()) return;

    uint8_t buf[32];
    vs.recv(buf, sizeof(buf));

    switch (buf[0]) {
        case SnakeVSHID::CMD_RESET:
            // ラウンドリセット: 盤面と岩を全消去
            clearAll();
            break;

        case SnakeVSHID::CMD_ROCK:
            // 岩の位置を登録（CMD_START の前に送られる）
            cell[buf[2]][buf[1]] = ROCK;
            break;

        case SnakeVSHID::CMD_START:
            // ラウンド開始: 両者の初期位置と自分の初期の向きを受け取る
            // buf[5], buf[6] は向きに +1 したもの（0=-1, 1=0, 2=+1）
            initRound(buf[1], buf[2], buf[3], buf[4],
                      (int8_t)buf[5] - 1, (int8_t)buf[6] - 1);
            seedRng(buf[1], buf[2], buf[3], buf[4]);
            solveTick();
            break;

        case SnakeVSHID::CMD_TICK:
            // 1ティック進んだ: 両者の頭を更新する。
            // 頭が通ったマスを記録していくだけで、体が復元できる。
            myX  = buf[1]; myY  = buf[2];
            oppX = buf[3]; oppY = buf[4];
            cell[myY][myX]   = MINE;
            cell[oppY][oppX] = OPP;
            solveTick();
            break;

        case SnakeVSHID::CMD_GET_NAME:
            // プレイヤー名を返す（対戦画面の表示・ランキング用）
            vs.sendName(PLAYER_NAME);
            break;
    }
}
