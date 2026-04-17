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

#define LED_BUILTIN 2
#define CMD_RUN_FIXED 0x41  // ブラウザからの実行コマンドマーカー

void releaseAllMouse() {
  Mouse.release(MOUSE_LEFT);
  Mouse.release(MOUSE_RIGHT);
  Mouse.release(MOUSE_MIDDLE);
  delay(20);
}

void moveTo(int bx, int by, int step = 10, int acc = 10) {
  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {  // 座標取得に失敗したら中断
    hid.Println("GetPos timeout!");
    return;
  }

  while (!(bx - cx >= -acc && bx - cx <= acc && by - cy >= -acc && by - cy <= acc)) {
    Mouse.moveLarge((bx - cx) / 2, (by - cy) / 2);
    if (!hid.GetPos(cx, cy)) {  // 座標取得に失敗したら中断
      hid.Println("GetPos timeout!");
      return;
    }
    delay(50);
  }
}

// ── 三角関数テーブル & ヘルパー ──────────────────────────────────────────────
// 0〜90° の sin 値（×255 スケール）  91 要素: index 0〜90
static const uint8_t sinTable[91] PROGMEM = {
  0, 4, 9, 13, 18, 22, 27, 31, 35, 40,               // 0–9
  44, 49, 53, 57, 62, 66, 70, 75, 79, 83,            // 10–19
  87, 91, 96, 100, 104, 108, 112, 116, 120, 124,     // 20–29
  128, 131, 135, 139, 143, 146, 150, 154, 157, 161,  // 30–39
  164, 167, 171, 174, 177, 181, 184, 187, 190, 193,  // 40–49
  196, 198, 201, 204, 207, 209, 212, 214, 217, 219,  // 50–59
  221, 224, 226, 228, 230, 232, 234, 236, 237, 239,  // 60–69
  241, 242, 243, 245, 246, 247, 248, 249, 250, 251,  // 70–79
  252, 253, 254, 254, 255, 255, 255, 255, 255, 255,  // 80–89
  255                                                // 90
};

// 返値: −255〜255（sin × 255 スケール）
int16_t isin(int16_t deg) {
  deg = ((deg % 360) + 360) % 360;
  if (deg <= 90) return (int16_t)pgm_read_byte(&sinTable[deg]);
  if (deg <= 180) return (int16_t)pgm_read_byte(&sinTable[180 - deg]);
  if (deg <= 270) return -(int16_t)pgm_read_byte(&sinTable[deg - 180]);
  return -(int16_t)pgm_read_byte(&sinTable[360 - deg]);
}

int16_t icos(int16_t deg) {
  return isin(deg + 90);
}

// ── Step 1: ボタンをクリック ──────────────────────────────────────────────────
// GetPos() でカーソル現在座標を取得し、BTN まで相対移動してクリック
void practice1() {
  hid.Clear();
  hid.Println("=== Step 1 ===");
  moveTo(543, 110);
  Mouse.click(MOUSE_LEFT);

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {  // 座標取得に失敗したら中断
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("x=");
  hid.Print(cx);
  hid.Print("  y=");
  hid.Println(cy);
}

// ── Step 2: ドラッグで GOAL へ運ぶ ───────────────────────────────────────────
// DRAG の上でカーソル座標を取得し、GOAL まで左ドラッグ
void practice2() {
  hid.Clear();
  hid.Println("=== Step 2 ===");

  Mouse.press(MOUSE_LEFT);
  moveTo(556, 287);
  Mouse.release(MOUSE_LEFT);

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("x=");
  hid.Print(cx);
  hid.Print("  y=");
  hid.Println(cy);
}

// ── Step 3: 正方形の 4 頂点をクリック ──────────────────────────────────────
// START（中心）の座標を GetPos で取得し、±len の 4 頂点を順番にクリック
void practice3() {
  hid.Clear();
  hid.Println("=== Step 3 ===");

  int16_t cx, cy;
  if (!hid.GetPos(cx, cy)) {  // START（中心）の座標を取得
    hid.Println("GetPos timeout!");
    return;
  }
  hid.Print("CENTER x=");
  hid.Print(cx);
  hid.Print("  y=");
  hid.Println(cy);

  const int len = 100;  // 半辺の長さ（px）
  moveTo(cx - len, cy - len, 10, 2);
  Mouse.click(MOUSE_LEFT);
  moveTo(cx + len, cy - len, 10, 2);
  Mouse.click(MOUSE_LEFT);
  moveTo(cx + len, cy + len, 10, 2);
  Mouse.click(MOUSE_LEFT);
  moveTo(cx - len, cy + len, 10, 2);
  Mouse.click(MOUSE_LEFT);
  delay(50);
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
  hid.Print("CENTER x=");
  hid.Print(cx);
  hid.Print("  y=");
  hid.Println(cy);

  const int r = 140;   // 半径（px）
  const int STEP = 10;  // 5° 刻み → 72 分割、滑らか

  int x = cx + (r * isin(0)) / 255;
  int y = cy - (r * icos(0)) / 255;
  moveTo(x, y, 10, 2);

  Mouse.press(MOUSE_LEFT);
  delay(20);

  for (int deg = 0; deg <= 360; deg += STEP) {
    x = cx + (r * isin(deg)) / 255;
    y = cy - (r * icos(deg)) / 255;
    moveTo(x, y, 4, 2);
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
