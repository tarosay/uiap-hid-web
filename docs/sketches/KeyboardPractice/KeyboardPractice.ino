/**
 * KeyboardPractice
 *
 * ボード:   HID ProMicro CH32V003 KBD+Mouse
 * バージョン: V1.4 + WebHID (EP3)
 *
 * 動作:
 *   Keyboard Practice ページの「実行」ボタンを押すと
 *   WebHID 経由でデータが届く。データを受信したら、
 *   コメントを外したキーボード操作を実行する。
 *
 * 使い方:
 *   練習したいステップのコードブロックだけコメント(//)を外して
 *   Arduino IDE から書き込む。
 */

#include <WebHID.h>
#include <Keyboard.h>

void setup() {
  WebHID.begin();
  Keyboard.begin();
  delay(2000);    // USB 接続が安定するまで待つ
}

void loop() {
  uint8_t buf[16];
  uint8_t len = WebHID.recv(buf, sizeof(buf));

  if (len > 0) {
    delay(500);   // ブラウザがワークエリアにフォーカスを移すまで待つ

    // ─── Step 1: "Hello UIAPduino World." を入力する ─────────────────
    Keyboard.print("Hello UIAPduino World.");

    // ─── Step 2: カーソルを 6 個左に動かして W を w に修正する ──────
    // Keyboard.print("Hello UIAPduino World.");
    // delay(100);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 1
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 2
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 3
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 4
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 5
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_LEFT_ARROW);   // ← 6（"W" の前に来た）
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.press(KEY_DELETE);       // "W" を削除
    // Keyboard.releaseAll();
    // Keyboard.print("w");              // 小文字 "w" を入力

    // ─── Step 3: Backspace で "!" を削除する ─────────────────────────
    // Keyboard.print("UIAPduino!");
    // delay(100);
    // Keyboard.press(KEY_BACKSPACE);    // "!" を削除
    // Keyboard.releaseAll();

    // ─── Step 4: Enter キーで改行する ────────────────────────────────
    // Keyboard.print("Hello,");
    // Keyboard.press(KEY_RETURN);       // 改行
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.print("UIAPduino!");

    // ─── Step 5: Home キーで行頭に戻り文字を挿入する ─────────────────
    // Keyboard.print("UIAPduino <<");
    // delay(100);
    // Keyboard.press(KEY_HOME);         // 行頭へ移動
    // Keyboard.releaseAll();
    // delay(50);
    // Keyboard.print(">> ");            // 行頭に挿入
  }
}
