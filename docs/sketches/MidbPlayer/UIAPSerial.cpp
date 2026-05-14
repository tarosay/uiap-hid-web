#include <Arduino.h>
#include "UIAPSerial.h"

UIAPSerial uart;

// ── USART1 送受信初期化 ───────────────────────────────────────────────────────
// TX = PD5 (AF push-pull 2MHz), RX = PD6 (入力プルアップ)
void UIAPSerial::begin(uint32_t baud)
{
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1;

  // PD5: AF push-pull 2MHz (CNF=10, MODE=10 → 0xA)
  // PD6: 入力プルアップ    (CNF=10, MODE=00 → 0x8)
  GPIOD->CFGLR = (GPIOD->CFGLR & ~((0xFu << 20) | (0xFu << 24)))
               | (0xAu << 20)   // PD5 TX: AF push-pull 2MHz
               | (0x8u << 24);  // PD6 RX: input pull-up
  GPIOD->OUTDR |= (1u << 6);    // PD6 プルアップ有効

  // ボーレート設定 (クロック 48MHz)
  USART1->BRR   = (uint16_t)(48000000UL / baud);
  USART1->CTLR1 = USART_Mode_Rx | USART_Mode_Tx | USART_CTLR1_UE;

  // 受信バッファクリア
  volatile uint16_t dummy = USART1->DATAR;
  (void)dummy;
}

// ── RX: 受信データあり？ ──────────────────────────────────────────────────────
uint8_t UIAPSerial::available()
{
  return (USART1->STATR & USART_FLAG_RXNE) ? 1 : 0;
}

// ── RX: 1 バイト読み出し ──────────────────────────────────────────────────────
uint8_t UIAPSerial::read()
{
  return (uint8_t)(USART1->DATAR & 0xFFu);
}

// ── TX: 1 バイト送信 ──────────────────────────────────────────────────────────
void UIAPSerial::write(uint8_t b)
{
  while (!(USART1->STATR & USART_FLAG_TXE)) {}
  USART1->DATAR = b;
}

// ── TX: バッファ送信 ──────────────────────────────────────────────────────────
void UIAPSerial::write(const uint8_t* buf, size_t n)
{
  for (size_t i = 0; i < n; i++) write(buf[i]);
}

// ── TX: 文字列送信 ────────────────────────────────────────────────────────────
void UIAPSerial::print(const char* s)
{
  while (*s) write((uint8_t)*s++);
}

// ── TX: 文字列送信 + CRLF ─────────────────────────────────────────────────────
void UIAPSerial::println(const char* s)
{
  print(s);
  write('\r');
  write('\n');
}
