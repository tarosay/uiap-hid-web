#pragma once
#include <stdint.h>

/**
 * UIAPSerial.h  —  USART1 送受信クラス (UIAPLog2 用)
 *
 * CH32V003 USART1: RX = PD6 (ピン16), TX = PD5 (ピン15)
 * レジスタ直接アクセス（HardwareSerial 未使用）
 *
 * 使い方:
 *   uart.begin(9600);           // TX/RX 初期化
 *   if (uart.available()) {     // 受信データあり
 *     uint8_t b = uart.read();  // 1 バイト読み出し
 *   }
 *   uart.write('A');            // 1 バイト送信
 *   uart.write(buf, n);         // n バイト送信
 *   uart.print("hello");        // 文字列送信
 *   uart.println("world");      // 文字列送信 + CRLF
 */
class UIAPSerial {
public:
  void    begin(uint32_t baud);

  // RX
  uint8_t available();
  uint8_t read();

  // TX
  void    write(uint8_t b);
  void    write(const uint8_t* buf, size_t n);
  void    print(const char* s);
  void    println(const char* s = "");
};

extern UIAPSerial uart;
