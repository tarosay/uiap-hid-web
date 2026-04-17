/**
 * MousePractice2
 *
 * ボード:   HID ProMicro CH32V003 KBD+Mouse
 * バージョン: V1.4 + WebHID (EP3)
 *
 * Mouse Practice の GetPos 版。
 * hid.GetPos(x, y) でカーソル座標を動的に取得し、相対移動量を計算する。
 * これにより、ブラウザウィンドウの位置やサイズに依存せずに動作する。
 *
 * 使い方:
 *   mouse2.html のワークエリアに START マーカーが表示される。
 *   各ステップで START（または DRAG）の上に実マウスを置き、
 *   そこをクリックして UIAPduino を起動する。
 *   UIAPduino は GetPos() でカーソル座標を取得してから動作する。
 */

#include <WebHID.h>
#include <Mouse.h>
#include "Hid.h"

#define LED_BUILTIN   2
#define CMD_RUN_FIXED 0x41  // ブラウザからの実行コマンドマーカー

void releaseAllMouse() {
  Mouse.release(MOUSE_LEFT);
  Mouse.release(MOUSE_RIGHT);
  Mouse.release(MOUSE_MIDDLE);
  delay(20);
}

// ── 三角関数テーブル & ヘルパー ──────────────────────────────────────────────
// 0〜90° の sin 値（×255 スケール）  91 要素: index 0〜90
static const uint8_t sinTable[91] PROGMEM = {
    0,   4,   9,  13,  18,  22,  27,  31,  35,  40,  // 0–9
   44,  49,  53,  57,  62,  66,  70,  75,  79,  83,  // 10–19
   87,  91,  96, 100, 104, 108, 112, 116, 120, 124,  // 20–29
  128, 131, 135, 139, 143, 146, 150, 154, 157, 161,  // 30–39
  164, 167, 171, 174, 177, 181, 184, 187, 190, 193,  // 40–49
  196, 198, 201, 204, 207, 209, 212, 214, 217, 219,  // 50–59
  221, 224, 226, 228, 230, 232, 234, 236, 237, 239,  // 60–69
  241, 242, 243, 245, 246, 247, 248, 249, 250, 251,  // 70–79
  252, 253, 254, 254, 255, 255, 255, 255, 255, 255,  // 80–89
  255                                                 // 90
};

// 返値: −255〜255（sin × 255 スケール）
int16_t isin(int16_t deg) {
  deg = ((deg % 360) + 360) % 360;
  if (deg <=  90) return  (int16_t)pgm_read_byte(&sinTable[deg]);
  if (deg <= 180) return  (int16_t)pgm_read_byte(&sinTable[180 - deg]);
  if (deg <= 270) return -(int16_t)pgm_read_byte(&sinTable[deg - 180]);
  return                 -(int16_t)pgm_read_byte(&sinTable[360 - deg]);
}

int16_t icos(int16_t deg) { return isin(deg + 90); }

// ── Step 1: ボタンをクリック ──────────────────────────────────────────────────
// GetPos() でカーソル現在座標を取得し、BTN まで相対移動してクリック
void practice1() {
  hid.Clear();
  hid.Println("=== Step 1 ===");

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {              // 座標取得に失敗したら中断
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("START x="); hid.Print(cx);
  hid.Print("  y=");      hid.Println(cy);

  const int bx = 543, by = 110;           // BTN の目標座標（ワークエリア基準）
  hid.Print("dx="); hid.Print(bx - cx);
  hid.Print("  dy="); hid.Println(by - cy);

  Mouse.moveLarge(bx - cx, by - cy, 0, 60);
  Mouse.click(MOUSE_LEFT);
}

// ── Step 2: ドラッグで GOAL へ運ぶ ───────────────────────────────────────────
// DRAG の上でカーソル座標を取得し、GOAL まで左ドラッグ
void practice2() {
  hid.Clear();
  hid.Println("=== Step 2 ===");

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("DRAG x="); hid.Print(cx);
  hid.Print("  y=");     hid.Println(cy);

  const int gx = 556, gy = 287;           // GOAL の座標
  hid.Print("dx="); hid.Print(gx - cx);
  hid.Print("  dy="); hid.Println(gy - cy);

  Mouse.press(MOUSE_LEFT);
  Mouse.moveLarge(gx - cx, gy - cy, 0, 60);
  Mouse.release(MOUSE_LEFT);
}

// ── Step 3: 正方形の 4 頂点をクリック ──────────────────────────────────────
// START（中心）の座標を GetPos で取得し、±len の 4 頂点を順番にクリック
void practice3() {
  hid.Clear();
  hid.Println("=== Step 3 ===");

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {             // START（中心）の座標を取得
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("CENTER x="); hid.Print(cx);
  hid.Print("  y=");       hid.Println(cy);

  const int len = 20;                     // 半辺の長さ（px）
  // 頂点: 左上 → 右上 → 右下 → 左下 の順（時計回り）
  int vx[4] = { cx - len, cx + len, cx + len, cx - len };
  int vy[4] = { cy - len, cy - len, cy + len, cy + len };

  int px = cx, py = cy;                   // 現在のカーソル位置を追跡
  for (int i = 0; i < 4; i++) {
    Mouse.moveLarge(vx[i] - px, vy[i] - py, 0, (i == 0) ? 2 : 4);
    px = vx[i];
    py = vy[i];
    Mouse.click(MOUSE_LEFT);
    delay(50);
  }
}

// ── Step 4: START を囲む円をドラッグ ─────────────────────────────────────────
// START（中心）座標を GetPos で取得し、12 時方向へ移動後に時計回り 1 周ドラッグ
void practice4() {
  hid.Clear();
  hid.Println("=== Step 4 ===");

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("CENTER x="); hid.Print(cx);
  hid.Print("  y=");       hid.Println(cy);

  const int r    = 40;   // 半径（px）
  const int STEP = 5;    // 5° 刻み → 72 分割、滑らか

  // 12 時方向（上端）へ移動してドラッグ開始
  Mouse.moveLarge(0, -r, 0, 8);
  delay(50);
  Mouse.press(MOUSE_LEFT);
  delay(20);

  // ── 時計回りに 1 周 ──────────────────────────────────────────────────────
  // 円上の座標: x = r·sin(θ)/255, y = −r·cos(θ)/255  （θ=0 → 12 時）
  // 1 ステップの移動量（×255 スケールで累積して丸め誤差を補正）:
  //   Δx =  r · (isin(θ+STEP) − isin(θ))  / 255
  //   Δy =  r · (icos(θ)      − icos(θ+STEP)) / 255
  int32_t ex = 0, ey = 0;

  for (int i = 0; i < 360; i += STEP) {
    ex += (int32_t)r * (isin(i + STEP) - isin(i));
    ey += (int32_t)r * (icos(i) - icos(i + STEP));

    int dx = (int)(ex / 255); ex -= (int32_t)dx * 255;
    int dy = (int)(ey / 255); ey -= (int32_t)dy * 255;

    if (dx || dy) Mouse.moveLarge(dx, dy, 0, 0);
    delay(12);
  }

  Mouse.release(MOUSE_LEFT);
}

void setup() {
  WebHID.begin();
  Mouse.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  releaseAllMouse();
  delay(1000);

}

void loop() {
  uint8_t buf[16];
  uint8_t len = WebHID.recv(buf, sizeof(buf));

  if (len > 0) {
    // GetPos レスポンス (0x51) は practice() 内の GetPos() が消費する。
    // タイムアウト後に届いた残留パケットはここで破棄する。
    if (buf[0] == HID_QUERY_MARKER) return;

    // 実行コマンド (0x41) 以外は無視
    if (buf[0] != CMD_RUN_FIXED) return;

    digitalWrite(LED_BUILTIN, HIGH);
    releaseAllMouse();
    delay(300);

    switch (buf[1]) {
      case 1: practice1(); break;
      case 2: practice2(); break;
      case 3: practice3(); break;
      case 4: practice4(); break;
    }

    releaseAllMouse();
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
