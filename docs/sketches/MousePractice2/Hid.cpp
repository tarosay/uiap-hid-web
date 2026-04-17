#include <Arduino.h>
#include <WebHID.h>
#include "Hid.h"

// ── グローバルインスタンスの実体（Hid.h で extern 宣言）────────────────────
Hid hid;

// ── Print プロトコル内部定数 ──────────────────────────────────────────────────
#define _HP_MARKER 0x50   // byte[0]: パケット種別マーカー
#define _HP_MORE   0x80   // byte[1] flag: 続きパケットあり
#define _HP_NL     0x02   // byte[1] flag: このパケット末尾で改行
#define _HP_CLEAR  0x04   // byte[1] flag: 表示エリアをクリア

// ════════════════════════════════════════════════════════════════════════════
// Print 系 — HID コンソールへ出力（EP3 InputReport, 8 bytes/パケット）
//
//   mouse.html（MousePractice.ino）では hidPrint() などスタンドアロン関数だったが、
//   mouse2.html（MousePractice2.ino）では Hid クラスのメソッドとして実装する。
// ════════════════════════════════════════════════════════════════════════════

// ── 内部: 1 パケット（最大 6 文字）を EP3 で送信 ─────────────────────────────
void Hid::_hpSend(uint8_t flags, const char* s, uint8_t n) {
  uint8_t buf[8] = { _HP_MARKER, flags, 0, 0, 0, 0, 0, 0 };
  for (uint8_t i = 0; i < n; i++) buf[i + 2] = (uint8_t)s[i];
  WebHID.send(buf, 8);
  delay(12);  // EP3 は 10ms 間隔なので 12ms 待つ
}

// ── 文字列を出力（改行なし、6 文字超は自動分割）─────────────────────────────
void Hid::Print(const char* s) {
  int len = strlen(s), off = 0;
  if (!len) return;
  while (off < len) {
    int n = len - off;
    if (n > 6) n = 6;
    _hpSend((off + n < len) ? _HP_MORE : 0, s + off, n);
    off += n;
  }
}

// ── 整数を出力（改行なし）────────────────────────────────────────────────────
void Hid::Print(int v) {
  char b[12];
  itoa(v, b, 10);
  Print(b);
}

// ── 文字列を出力して改行 ─────────────────────────────────────────────────────
void Hid::Println(const char* s) {
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

// ── 整数を出力して改行 ───────────────────────────────────────────────────────
void Hid::Println(int v) {
  char b[12];
  itoa(v, b, 10);
  Println(b);
}

// ── コンソールをクリア ───────────────────────────────────────────────────────
void Hid::Clear() {
  _hpSend(_HP_CLEAR, "", 0);
}

// ════════════════════════════════════════════════════════════════════════════
// GetPos — 現在のマウスカーソル座標を取得
//
//   プロトコル:
//     送信 (EP3 InputReport, 8 bytes):
//       [0x51, 0x01, 0, 0, 0, 0, 0, 0]             ← クエリ
//     受信 (EP0 Feature Report, 16 bytes):
//       [0x51, 0x01, xLow, xHigh, yLow, yHigh, …]  ← ブラウザからの応答
//
//   x, y: ワークエリア左上を原点とした int16_t（little-endian）
//   戻り値: true = 成功, false = タイムアウト（500ms）
// ════════════════════════════════════════════════════════════════════════════

// ── 内部: クエリパケットを EP3 で送信 ────────────────────────────────────────
void Hid::_request(uint8_t queryType) {
  uint8_t buf[8] = { HID_QUERY_MARKER, queryType, 0, 0, 0, 0, 0, 0 };
  WebHID.send(buf, 8);
  delay(12);  // EP3 は 10ms 間隔なので 12ms 待つ
}

// ── WebHID.recv のラッパー ────────────────────────────────────────────────────
//   ブラウザが sendFeatureReport() で送ったデータを受け取る。
//   GetPos() の内部ポーリングとは異なり、スケッチのメインループで呼ぶ用途向け。
uint8_t Hid::Recv(uint8_t* buf, uint8_t maxLen) {
  return WebHID.recv(buf, maxLen);
}

// ── カーソル座標取得 ─────────────────────────────────────────────────────────
bool Hid::GetPos(int16_t &x, int16_t &y) {
  _request(HID_QUERY_POS);

  const unsigned long TIMEOUT_MS = 500;
  unsigned long t0 = millis();
  uint8_t buf[16];

  while (millis() - t0 < TIMEOUT_MS) {
    uint8_t len = WebHID.recv(buf, sizeof(buf));
    if (len >= 6
        && buf[0] == HID_QUERY_MARKER
        && buf[1] == HID_QUERY_POS) {
      x = (int16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
      y = (int16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));
      return true;
    }
    delay(5);
  }
  return false;  // タイムアウト
}
