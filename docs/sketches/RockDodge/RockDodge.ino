/**
 * RockDodge
 *
 * ボード:   HID ProMicro CH32V003
 * USB:     WebHID Only（Tools → USB）
 *
 * Web ページがゲームを管理します。
 * このスケッチは落ちてくる岩をよけながらアイテムを拾う移動（左/停止/右）を返します。
 *
 * 【ゲームのルール】
 *   ・全レベル 120 ステップ固定。岩に当たったらゲームオーバー。
 *   ・得点アイテム（1〜99点）が落ちてくる。数字は落下中に見える。
 *   ・レベルごとの最低取得アイテム数（ノルマ）に届かないとゲームオーバー。
 *   ・★（面クリアアイテム）を取ると即クリア＋ノルマ免除（得点は 0）。
 *     1 面に最大 3 個、ラインから深く外れた位置に湧く。
 *     ノルマが危ないときの脱出口だが、順調なときはうっかり踏まないこと！
 *   ・壁の奥のお宝を取ると詰む — と思いきや、数ティック後の壁に
 *     そこから届く★が埋まっていることがある（深いお宝の約 4 割）。
 *     「お宝 → ★」の 2 段階を読めれば、他が捨てる高得点を回収できる。
 *   ・岩をよけ続けるだけでは得点は増えない（クリアの +10 点のみ）。
 *
 * 【このサンプルの実装レベル】
 *   生存（時空間先読み）＋「生き残れる移動のうち、すぐ拾えるアイテムを優先」まで。
 *   タダのアイテム（セーフティライン近く）しか拾わないので、序盤のノルマは
 *   クリアできるが得点は最低クラス。高得点を狙う採算判断はキミが実装しよう！
 *
 * 【座標系】
 *   X = 列 (0〜15)、Y = 行 (0〜15)
 *   原点 (0,0) は左上。X が右、Y が下。
 *   プレイヤーは最下段 (Y=15) だけを左右に動く。
 *   岩・アイテムは最上段 (Y=0) に湧き、1ティックごとに 1 マス落下する。
 *
 * 【ファイル構成】
 *   RockDodgeHID.h  ... WebHID 通信クラス（通信の詳細はここに集約）
 *   RockDodge.ino   ... ゲーム状態・回避/取得アルゴリズム（このファイル）
 */

#include "RockDodgeHID.h"

// ★ 自分の名前に書き換えよう！（半角英数 12 文字まで）
//    ランキング登録や、将来の対戦でのデバイス識別に使われます。
#define PLAYER_NAME "NO NAME"

RockDodgeHID dodge; // WebHID 通信を担うオブジェクト

// ── ゲーム状態 ────────────────────────────────────────────────
bool    rocks[16][16]; // true = 岩があるセル（rocks[y][x]）
uint8_t items[16][16]; // 0=なし / 1〜99=得点 / 255=★面クリアアイテム
uint8_t playerX;       // プレイヤーの列（行は常に 15）
uint8_t quota;         // このレベルの最低取得アイテム数
uint8_t targetSteps;   // このレベルのステップ数（120 固定）
uint8_t collected;     // このレベルで取ったアイテム数（★は数えない）

const uint8_t ITEM_CLEAR = 255; // ★（面クリアアイテム）

// ── 盤面のリセット ────────────────────────────────────────────
void clearBoard() {
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            rocks[y][x] = false;
            items[y][x] = 0;
        }
}

// ── 落下シミュレーション ──────────────────────────────────────
// 岩を 1 段下へずらし（最下段は消える）、最上段に湧きマスクを置く
void shiftRocks(uint16_t spawnMask) {
    for (int8_t y = 15; y >= 1; y--)
        for (uint8_t x = 0; x < 16; x++)
            rocks[y][x] = rocks[y - 1][x];
    for (uint8_t x = 0; x < 16; x++)
        rocks[0][x] = (spawnMask >> x) & 1;
}

// アイテムも 1 段下へずらす（最上段は空になる）
void shiftItems() {
    for (int8_t y = 15; y >= 1; y--)
        for (uint8_t x = 0; x < 16; x++)
            items[y][x] = items[y - 1][x];
    for (uint8_t x = 0; x < 16; x++)
        items[0][x] = 0;
}

// ══════════════════════════════════════════════════════════════
// ここに岩をよけながらアイテムを拾うアルゴリズムを実装する！
//
// 利用できる情報:
//   playerX         ... 現在のプレイヤーの列（行は 15 固定）
//   rocks[y][x]     ... 岩があるか（true=あり）
//   items[y][x]     ... アイテム（0=なし / 1〜99=得点 / 255=★）
//   quota           ... このレベルの最低取得アイテム数
//
// 利用できる関数:
//   dodge.sendDir(dx)  ... 次の移動を送信（必ず呼ぶ）
//
// 【時間のルール】
//   1 ティックの順序は ①プレイヤー移動 → ②岩・アイテムが 1 段落下。
//   衝突・取得判定は①の直後と②の直後の 2 回行われる。
//   落下は完全に決定論なので、t ティック後、いま rocks[15-t][x] にある岩が
//   最下段 (y=15) に到達する。未来は全部計算できる！
//
// 【セーフティライン】
//   Web は「停止 → 中央寄り → 端寄り」の優先順で動く仮想プレイヤー
//   （セーフティライン）を走らせていて、岩の湧きはラインの生存だけを保証する。
//   ラインから外れている間は、自分の位置を詰ませる湧きが来ることがある。
//   高得点アイテムはラインの外に湧く。何ティック外れるかの見積もりが勝負！
// ══════════════════════════════════════════════════════════════

// 時空間探索のメモ: memo[t][x] = -1 未探索 / 0 死ぬ / 1 生き残れる
static int8_t memo[17][16];

/**
 * 「t ティック後に列 x にいて安全か」を判定する
 *
 * 判定①（移動直後・落下前）: そのとき最下段に来るのは今の rocks[16-t] 行
 * 判定②（落下後）:           そのとき最下段に来るのは今の rocks[15-t] 行
 * t が 17 以上なら、いま盤面にある岩は全部落ちきっているので安全。
 */
bool safeAt(int16_t x, uint8_t t) {
    if (x < 0 || x > 15) return false;   // 壁の外へは行けない
    if (t >= 17) return true;
    if (rocks[16 - t][x]) return false;              // 判定①
    if (t <= 15 && rocks[15 - t][x]) return false;   // 判定②
    return true;
}

/**
 * 時空間 DFS: 「t ティック後に列 x にいる」状態から
 * 盤面の岩が全部落ちきるまで生き残れるかを調べる。
 * 状態数は最大 16列 × 17ティック = 272 なので一瞬で終わる。
 */
bool canSurvive(uint8_t x, uint8_t t) {
    if (t >= 16) return true; // ここまで生きていれば既知の岩は残っていない
    if (memo[t][x] >= 0) return memo[t][x];
    memo[t][x] = 0;
    const int8_t DD[] = { 0, -1, 1 };
    for (uint8_t i = 0; i < 3; i++) {
        int16_t nx = (int16_t)x + DD[i];
        if (nx < 0 || nx > 15) continue;
        if (!safeAt(nx, t + 1)) continue;
        if (canSurvive((uint8_t)nx, t + 1)) {
            memo[t][x] = 1;
            return true;
        }
    }
    return false;
}

/**
 * 隣（±1 列）に着地する一番近いアイテムの方向を返す（0 = なし or 真下）
 * ★は追わない。
 *
 * わざと ±1 列しか見ていない。セーフティラインはプレイヤーを追ってくるので、
 * 1 列の寄り道ならすぐ合流できて狩られにくいからだ。
 *
 * この安全運転だけで取れるのは 1 面あたり 7〜9 個。
 * ノルマは Lv1 で 6 個、Lv5 で 10 個まで増えるので、
 * 途中のレベルから必ず足りなくなる。
 * 壁の中に並んだ高得点アイテムを狙いに行くのがキミの仕事！
 */
int8_t nearItemDir() {
    int16_t best = 127;
    int8_t  dir  = 0;
    for (int8_t y = 15; y >= 0; y--) {
        for (int16_t c = (int16_t)playerX - 1; c <= (int16_t)playerX + 1; c++) {
            if (c < 0 || c > 15) continue;
            if (!items[y][c] || items[y][c] == ITEM_CLEAR) continue;
            int16_t dist  = (c > playerX) ? c - playerX : playerX - c;
            int16_t tLand = 15 - y;               // 着地までのティック数
            if (dist > tLand + 1) continue;       // 移動が間に合わない
            if (dist < best) {
                best = dist;
                dir  = (c > playerX) ? 1 : (c < playerX) ? -1 : 0;
            }
        }
    }
    return dir;
}

/**
 * 生存＋タダ取りアルゴリズム
 *
 * 1. 近く（半径 2）に取れるアイテムがあればその方向を優先
 * 2. なければ「停止 → 中央寄り → 端寄り」（セーフティラインと同じ方策）
 * 3. どの候補も生存経路（canSurvive）があることが絶対条件
 * 4. 同点なら「すぐ拾える」方向を選ぶ
 *
 * これはあくまで床（最低ライン）。序盤のノルマはこれでクリアできるが、
 * 得点は最低クラス。高得点アイテムの採算判断はキミの仕事だ！
 */
void computeNextDir(int8_t& outDx) {
    for (uint8_t t = 0; t < 17; t++)
        for (uint8_t x = 0; x < 16; x++)
            memo[t][x] = -1;

    int8_t toItem   = nearItemDir();
    int8_t toCenter = (playerX < 8) ? 1 : -1;
    int8_t order[3];
    if (toItem != 0) {
        order[0] = toItem; order[1] = 0; order[2] = (int8_t)-toItem;
    } else {
        order[0] = 0; order[1] = toCenter; order[2] = (int8_t)-toCenter;
    }

    bool    found     = false;
    int8_t  bestDx    = 0;
    uint8_t bestBonus = 0;

    for (uint8_t i = 0; i < 3; i++) {
        int16_t nx = (int16_t)playerX + order[i];
        if (nx < 0 || nx > 15) continue;
        if (!safeAt(nx, 1)) continue;
        if (!canSurvive((uint8_t)nx, 1)) continue;

        // 生存できる候補の中で「すぐ拾える」度を数える
        uint8_t bonus = 0;
        if (items[15][nx]) bonus++; // 移動した瞬間に拾える（判定①）
        if (items[14][nx]) bonus++; // 落下してきて拾える（判定②）

        if (!found || bonus > bestBonus) {
            found     = true;
            bestBonus = bonus;
            bestDx    = order[i];
        }
    }
    if (found) { outDx = bestDx; return; }

    // 生存経路が見つからない: せめて即死しない方向を選ぶ
    for (uint8_t i = 0; i < 3; i++) {
        int16_t nx = (int16_t)playerX + order[i];
        if (nx >= 0 && nx <= 15 && safeAt(nx, 1)) {
            outDx = order[i];
            return;
        }
    }
    outDx = 0; // 逃げ場なし（ゲームオーバーになる）
}

void solveTick() {
    int8_t dx;
    computeNextDir(dx);
    dodge.sendDir(dx);
}

// ── setup / loop ──────────────────────────────────────────────
void setup() {
    dodge.begin(); // USB HID 初期化 + CMD_READY 送信
}

void loop() {
    if (!dodge.available()) return;

    // Feature Report は 32 バイト。CMD_TICK は 21 バイト目まで使う
    uint8_t buf[32];
    dodge.recv(buf, sizeof(buf));

    switch (buf[0]) {
        case RockDodgeHID::CMD_RESET:
            // ゲームリセット: 岩・アイテムを全消去
            clearBoard();
            break;

        case RockDodgeHID::CMD_START:
            // ゲーム開始: 初期位置とレベル条件を受け取り、最初の方向を送信
            playerX     = buf[1];
            quota       = buf[2];
            targetSteps = buf[3];
            collected   = 0;
            solveTick();
            break;

        case RockDodgeHID::CMD_TICK: {
            // 1ティック進んだ: Web と同じ順序で状態を更新する
            playerX = buf[1];
            uint16_t mask = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);

            // 判定①で拾われた分を消す（★以外なら取得数をカウント）
            if (items[15][playerX] && items[15][playerX] != ITEM_CLEAR) collected++;
            items[15][playerX] = 0;
            shiftRocks(mask);         // 岩を 1 段落とし row0=湧きマスク
            shiftItems();             // アイテムも 1 段落とす
            // 判定②で拾われた分を消す（★以外なら取得数をカウント）
            if (items[15][playerX] && items[15][playerX] != ITEM_CLEAR) collected++;
            items[15][playerX] = 0;

            // 新しいアイテムを登録（buf[5..20] = 列ごとの値）
            // 0=なし / 1〜99=得点 / 255=★面クリアアイテム
            // 1 行に何個でも湧く。壁の岩がアイテムに置き換わっていることもある！
            for (uint8_t x = 0; x < 16; x++)
                items[0][x] = buf[5 + x];

            // 狙い岩: セーフティラインを外れていると、盤面の途中
            // （12 行目 = 猶予 3 ティック）から岩が降ってくる！
            if (buf[4] != 255)
                rocks[12][buf[4]] = true;

            solveTick();
            break;
        }

        case RockDodgeHID::CMD_GET_NAME:
            // プレイヤー名を返す（ランキング・デバイス識別用）
            dodge.sendName(PLAYER_NAME);
            break;
    }
}
