/**
 * Hid.h  —  Hid クラス宣言
 *
 * mouse.html（MousePractice.ino）では hidPrint() などをスタンドアロン関数で実装していた。
 * mouse2.html（MousePractice2.ino）では同機能を C++ クラスのメソッドとして実装し、
 * 新たに hid.GetPos() によるカーソル座標取得機能も追加する。
 *
 * ─── Print プロトコル （マーカー 0x50）──────────────────────────────────────
 *  UIAPduino → ブラウザ  (EP3 Input Report, 8 bytes)
 *    byte[0] = 0x50  マーカー
 *    byte[1] = flags   0x80=続きあり  0x02=改行  0x04=画面クリア
 *    byte[2..7] = テキスト本体（最大 6 文字、残りは 0 埋め）
 *
 * ─── GetPos プロトコル（マーカー 0x51）──────────────────────────────────────
 *  UIAPduino → ブラウザ  (EP3 Input Report, 8 bytes)
 *    [0x51, 0x01, 0, 0, 0, 0, 0, 0]      ← GetPos リクエスト
 *
 *  ブラウザ → UIAPduino  (EP0 Feature Report, 16 bytes)
 *    [0x51, 0x01, xLow, xHigh, yLow, yHigh, 0, …]  ← 座標レスポンス
 *    x, y: ワークエリア左上を原点とした int16_t (little-endian)
 * ─────────────────────────────────────────────────────────────────────────
 *
 * 使い方:
 *   hid.Clear();
 *   hid.Println("=== Step 1 ===");
 *   hid.Print("x="); hid.Print(x); hid.Print("  y="); hid.Println(y);
 *
 *   int16_t cx, cy;
 *   if (hid.GetPos(cx, cy)) {
 *     // cx, cy に現在のカーソル座標が入っている
 *   }
 */

#ifndef HID_H
#define HID_H

#include <stdint.h>

// ── GetPos プロトコル定数 ──────────────────────────────────────────────────
#define HID_QUERY_MARKER  0x51   // GetPos パケット種別マーカー
#define HID_QUERY_POS     0x01   // カーソル位置クエリ

// ── Hid クラス ──────────────────────────────────────────────────────────────
class Hid {
public:
  // ── HID コンソールへ出力 ─────────────────────────────────────────────────
  // mouse.html の hidPrint() / hidPrintln() / hidClear() をクラス化したもの

  // 文字列を出力（改行なし、6 文字超は自動分割）
  void Print(const char* s);

  // 整数を出力（改行なし）
  void Print(int v);

  // 文字列を出力して改行
  void Println(const char* s = "");

  // 整数を出力して改行
  void Println(int v);

  // コンソールをクリア
  void Clear();

  // ── カーソル座標取得 ─────────────────────────────────────────────────────
  // x, y : 取得した座標を書き込む変数（参照渡し）
  // 戻り値: true = 成功、false = タイムアウト（500 ms）
  bool GetPos(int16_t &x, int16_t &y);

  // ── ブラウザからのデータ受信（WebHID.recv のラッパー）─────────────────
  // buf    : 受信データを書き込むバッファ
  // maxLen : バッファの最大バイト数
  // 戻り値 : 受信したバイト数（0 = データなし）
  uint8_t Recv(uint8_t* buf, uint8_t maxLen);

private:
  // Print 系の内部送信（EP3 → ブラウザ）
  void _hpSend(uint8_t flags, const char* s, uint8_t n);

  // GetPos クエリパケットを送信（EP3 → ブラウザ）
  void _request(uint8_t queryType);
};

// グローバルインスタンスの宣言（実体は Hid.cpp で定義）
extern Hid hid;

#endif
