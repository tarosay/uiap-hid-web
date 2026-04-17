/**
 * MousePractice
 *
 * ボード:   HID ProMicro CH32V003 KBD+Mouse
 * バージョン: V1.4 + WebHID (EP3)
 *
 * 使い方:
 *   mouse.html の「UIAPduino で実行」ボタンを押すと
 *   buf[1] にステップ番号（1〜4）が入って届く。
 *
 * 注意:
 *   マウスは相対移動なので、実行前にブラウザ上の START 円の中心へ
 *   実マウスでカーソルを置いてから実行する。
 */

#include <WebHID.h>
#include <Mouse.h>
#include <math.h>

#define LED_BUILTIN 2

static const int STEP_WAIT_MS = 8;

void releaseAllMouse() {
  Mouse.release(MOUSE_LEFT);
  Mouse.release(MOUSE_RIGHT);
  Mouse.release(MOUSE_MIDDLE);
  delay(20);
}
// ── hidPrint / hidPrintln / hidClear ─────────────────────────────────────
// .ino の先頭付近（setup() より前）に貼り付けてください。
// WebHID.send() を使ってブラウザの「HID コンソール」に文字列・数値を表示します。
//
// プロトコル (8 byte パケット):
//   byte[0] = 0x50  マーカー
//   byte[1] = flags  0x80=続きあり  0x02=改行  0x04=画面クリア
//   byte[2..7] = テキスト本体（最大 6 文字、残りは 0 埋め）

#define _HP_MARKER 0x50
#define _HP_MORE 0x80
#define _HP_NL 0x02
#define _HP_CLEAR 0x04

static void _hpSend(uint8_t flags, const char* s, uint8_t n) {
  uint8_t buf[8] = { _HP_MARKER, flags, 0, 0, 0, 0, 0, 0 };
  for (uint8_t i = 0; i < n; i++) buf[i + 2] = (uint8_t)s[i];
  WebHID.send(buf, 8);
  delay(12);  // EP3 は 10ms 間隔なので 12ms 待つ
}

// 文字列を送信（改行なし、6 文字超は自動分割）
void hidPrint(const char* s) {
  int len = strlen(s), off = 0;
  if (!len) return;
  while (off < len) {
    int n = len - off;
    if (n > 6) n = 6;
    _hpSend((off + n < len) ? _HP_MORE : 0, s + off, n);
    off += n;
  }
}

// 整数を送信（改行なし）
void hidPrint(int v) {
  char b[12];
  itoa(v, b, 10);
  hidPrint(b);
}

// 文字列を送信して改行
void hidPrintln(const char* s = "") {
  int len = strlen(s), off = 0;
  if (!len) {
    _hpSend(_HP_NL, "", 0);
    return;
  }
  while (off < len) {
    int n = len - off;
    if (n > 6) n = 6;
    bool last = (off + n >= len);
    _hpSend(last ? _HP_NL : _HP_MORE, s + off, n);
    off += n;
  }
}

// 整数を送信して改行
void hidPrintln(int v) {
  char b[12];
  itoa(v, b, 10);
  hidPrintln(b);
}

// コンソールをクリア
void hidClear() {
  _hpSend(_HP_CLEAR, "", 0);
}

// ── 使用例 ─────────────────────────────────────────────────────────────────
// hidClear();
// hidPrintln("=== Step 2 ===");
// hidPrint("x="); hidPrint(posX); hidPrint("  y="); hidPrintln(posY);
// hidPrint("elapsed: "); hidPrint(elapsed); hidPrintln("ms");

void practice1() {
  const int sx = 93;
  const int sy = 314;
  const int bx = 543;
  const int by = 110;

  Mouse.moveLarge(bx - sx, by - sy, 0, 60);
  Mouse.click(MOUSE_LEFT);
}

void practice2() {
  const int sx = 205;
  const int sy = 142;
  const int bx = 556;
  const int by = 287;
  Mouse.press(MOUSE_LEFT);
  Mouse.moveLarge(bx - sx, by - sy, 0, 60);
  Mouse.release(MOUSE_LEFT);
}

void practice3() {
  const int len = 20;
  Mouse.moveLarge(-len, -len, 0, 2);
  Mouse.click(MOUSE_LEFT);
  delay(50);
  Mouse.moveLarge(2 * len, 0, 0, 4);
  Mouse.click(MOUSE_LEFT);
  delay(50);
  Mouse.moveLarge(0, 2 * len, 0, 4);
  Mouse.click(MOUSE_LEFT);
  delay(50);
  Mouse.moveLarge(-2 * len, 0, 0, 4);
  Mouse.click(MOUSE_LEFT);
  delay(50);
}

void practice4() {
  const int r = 40;    // 半径（px）
  const int STEP = 5;  // 5° 刻み → 72 分割、滑らか

  // START（center）から 12 時方向（上端）へ移動してドラッグ開始
  Mouse.moveLarge(0, -r, 0, 8);
  delay(50);
  Mouse.press(MOUSE_LEFT);
  delay(20);

  // ── 時計回りに 1 周 ──────────────────────────────────────────────────
  // 円上の座標: x = r·sin(θ)/255,  y = -r·cos(θ)/255
  //   θ=0 → 12 時,  θ=90 → 3 時,  θ=180 → 6 時 (時計回り)
  // 1ステップの移動量:
  //   Δx = r · (isin(θ+STEP) − isin(θ))  / 255
  //   Δy = r · (icos(θ)      − icos(θ+STEP)) / 255
  //
  // 整数丸め誤差を×255スケールで累積して補正（円がずれずに閉じる）
  int32_t ex = 0, ey = 0;

  for (int i = 0; i < 360; i += STEP) {
    ex += (int32_t)r * (isin(i + STEP) - isin(i));
    ey += (int32_t)r * (icos(i) - icos(i + STEP));

    int dx = (int)(ex / 255);
    ex -= (int32_t)dx * 255;
    int dy = (int)(ey / 255);
    ey -= (int32_t)dy * 255;

    if (dx || dy) Mouse.moveLarge(dx, dy, 0, 0);
    delay(12);
  }

  Mouse.release(MOUSE_LEFT);
}

/// ── 三角関数テーブル & ヘルパー ──────────────────────────────────────────
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
  255                                                // 90 ← 追加（sin90°=1.0）
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
    digitalWrite(LED_BUILTIN, HIGH);
    releaseAllMouse();
    delay(300);

    switch (buf[1]) {
      case 1:
        hidPrintln("=== Step 1 ===");
        practice1();
        break;

      case 2:
        hidPrintln("=== Step 2 ===");
        practice2();
        break;

      case 3:
        hidPrintln("=== Step 3 ===");
        practice3();
        break;

      case 4:
        hidPrintln("=== Step 4 ===");
        practice4();
        break;
    }

    releaseAllMouse();
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
