/*
 * UIAPrubyEeVmAd.ino
 * UIAPruby TinyVM Runner — 動的生成
 * コンポーネント: BASE + Ad
 * FQBN: UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 * 要ボードパッケージ: UIAPduino HID v1.2.12 以降（Wiremin / NeoPixelmin）
 * 保存先: I2C EEPROM（24FC256 32KB / CAT24M01WI 128KB）
 */

#include <Arduino.h>
#include <WebHID.h>
#include <Wiremin.h>

#define LED_PIN  2

// ── EEPROM ──────────────────────────────────────────────────
#define EE_DEV_BASE   0x50    // A0/A1/A2 = GND
#define EE_PAGE       32      // 24FC256(64B) / CAT24M01(256B) の両方を割り切る
#define EE_CACHE_SIZE 128     // オペコードキャッシュ（Ec では文字変数バッファと共用）
#define EE_VAR_BASE   2560UL   // 変数領域の先頭
#define EE_SLOT_SIZE  5032UL  // 1 変数あたりのスロット
#define EE_SLOT_NAME  20      // スロット先頭の変数名領域（4 の倍数）

#define CMD_OPEN_W    0x01
#define CMD_WRITE     0x02
#define CMD_CLOSE     0x03
#define CMD_OPEN_R    0x04
#define CMD_READ      0x05
#define CMD_ERASE     0x0B
#define CMD_RUN       0x10
#define CMD_STOP      0x11

#define RSP_MARKER 0x52
#define RSP_OK     0
#define RSP_ERR    1
#define RSP_DATA   2
#define RSP_END    3

static void rsp(uint8_t status, const uint8_t *d, uint8_t len) {
  uint8_t buf[8] = { RSP_MARKER, status, len, 0, 0, 0, 0, 0 };
  if (d && len) { uint8_t n = len > 5 ? 5 : len; for (uint8_t i = 0; i < n; i++) buf[3+i] = d[i]; }
  WebHID.send(buf, 8); delay(12);
}
#define rsp_ok()  rsp(RSP_OK,  0, 0)
#define rsp_err() rsp(RSP_ERR, 0, 0)
#define rsp_end() rsp(RSP_END, 0, 0)

static void stream_bytes(const uint8_t *data, uint8_t len) {
  uint8_t offset = 0;
  while (offset < len) {
    uint8_t chunk = len - offset; if (chunk > 5) chunk = 5;
    rsp(RSP_DATA, data + offset, chunk); offset += chunk;
  }
}

static void get_name(const uint8_t *src, char *dst) {
  uint8_t i = 0;
  for (; i < 26 && src[i]; i++) dst[i] = (char)src[i];
  dst[i] = '\0';
}

#define LOG_EE_OK      0x01
#define LOG_EE_FAIL    0x02
#define LOG_UAP_START  0x10
#define LOG_UAP_MAGIC  0x11
#define LOG_MAGIC_FAIL 0x12
#define LOG_UAP_DONE   0x18
#define LOG_UAP_STOP   0x19
#define LOG_UAP_HALT   0x1A
#define LOG_BAD_OPCODE 0x1F
#define LOG_WARN_VAL   0x20  // warn: レジスタ生値を DEVICE LOG へ

// noinline: LTO による呼び出し箇所ごとの展開を防ぎ共通関数化（Flash 節約）
static void __attribute__((noinline)) hidLog(uint8_t type, uint8_t d0=0, uint8_t d1=0, uint8_t d2=0,
                   uint8_t d3=0, uint8_t d4=0, uint8_t d5=0) {
  uint8_t p[8] = { 'D', type, d0, d1, d2, d3, d4, d5 };
  WebHID.send(p, 8);
}

#define HID_CONSOLE_MARKER 0x50
#define CONSOLE_MORE  0x80
#define CONSOLE_CLEAR 0x04

static void consoleWriteChunk(const char *s, uint8_t len, bool more) {
  uint8_t buf[8] = { HID_CONSOLE_MARKER, (uint8_t)(more ? CONSOLE_MORE : 0), 0, 0, 0, 0, 0, 0 };
  uint8_t n = len > 6 ? 6 : len;
  for (uint8_t i = 0; i < n; i++) buf[2+i] = (uint8_t)s[i];
  // 前レポートのホスト回収を最大~50ms待つ（uiapwebhid_send 内蔵の~12msタイムアウトでは
  // ホストのポーリングジッタで前チャンクが上書き消失することがある）
  // millis() は uint64_t ソフト演算を引き込み Flash +2KB のためカウンタ方式
  for (uint32_t t = 0; WebHID.busy() && t < 800000UL; t++) {}
  WebHID.send(buf, 8);  // 送信後の delay は不要（次回呼び出し時に busy 待ちするため）
}

static void consolePrint(const char *s, uint8_t len, uint8_t flags) {
  bool inspect = (flags & 0x02) != 0;
  bool newline = (flags & 0x01) != 0;
  char tmp[64]; uint8_t tlen = 0;
  if (inspect) { tmp[tlen++] = '"'; }
  for (uint8_t i = 0; i < len && tlen < 62; i++) tmp[tlen++] = s[i];
  if (inspect) { tmp[tlen++] = '"'; }
  if (newline) { tmp[tlen++] = '\n'; }
  uint8_t pos = 0;
  while (pos < tlen) {
    uint8_t chunk = tlen - pos; if (chunk > 6) chunk = 6;
    bool more = (pos + chunk < tlen);
    consoleWriteChunk(tmp + pos, chunk, more); pos += chunk;
  }
}

static bool    autoRun = false;
static uint8_t cmdBuf[32];

static uint8_t recvCmd() {
  if (!WebHID.available()) return 0;
  memset(cmdBuf, 0, sizeof(cmdBuf));
  if (WebHID.recv(cmdBuf, sizeof(cmdBuf)) > 0) return cmdBuf[0];
  return 0;
}

// ── EEPROM アクセス層 ───────────────────────────────────────
// 17bit アドレスの最上位ビットはスレーブアドレス側に入る（CAT24M01 Figure 3）。
// 24FC256 では addr が 0xFFFF を超えないので上位ビットは常に 0。同じコードで動く。
static inline uint8_t ee_dev(uint32_t a) { return (uint8_t)(EE_DEV_BASE | ((a >> 16) & 1)); }

static uint8_t  _ee_cache[EE_CACHE_SIZE + 1];
static uint32_t _ee_cache_base = 0xFFFFFFFFUL;
static uint8_t  _ee_cache_len  = 0;
static uint32_t _ee_pos  = 0;
static uint32_t _ee_wpos = 0;

static void ee_invalidate(void) { _ee_cache_base = 0xFFFFFFFFUL; _ee_cache_len = 0; }

static bool ee_fill(uint32_t base) {
  uint32_t to_boundary = 0x10000UL - (base & 0xFFFFUL);   // 64KB 境界はまたがない
  uint8_t n = (to_boundary < EE_CACHE_SIZE) ? (uint8_t)to_boundary : EE_CACHE_SIZE;
  if (!Wiremin_read_reg16(ee_dev(base), (uint16_t)(base & 0xFFFF), _ee_cache, n)) {
    ee_invalidate(); return false;
  }
  _ee_cache_base = base; _ee_cache_len = n; return true;
}

static inline void ee_seek(uint32_t pos) { _ee_pos = pos; }

static int ee_read_full(uint8_t *dst, uint8_t len) {
  uint8_t got = 0;
  while (got < len) {
    if (_ee_pos < _ee_cache_base || _ee_pos >= _ee_cache_base + _ee_cache_len) {
      if (!ee_fill(_ee_pos)) return got;
    }
    uint16_t off   = (uint16_t)(_ee_pos - _ee_cache_base);
    uint8_t  avail = (uint8_t)(_ee_cache_len - off);
    uint8_t  n     = (uint8_t)(len - got);
    if (n > avail) n = avail;
    for (uint8_t i = 0; i < n; i++) dst[got + i] = _ee_cache[off + i];
    got += n; _ee_pos += n;
  }
  return got;
}

// ページ境界で分割して書く。完了待ちは ACK ポーリング（書き込み中は ACK を返さない）。
static bool ee_write(uint32_t addr, const uint8_t *src, uint8_t len) {
  ee_invalidate();
  while (len) {
    uint16_t room = (uint16_t)(EE_PAGE - (addr % EE_PAGE));
    uint8_t  n    = (room < len) ? (uint8_t)room : len;
    if (!Wiremin_write_reg16(ee_dev(addr), (uint16_t)(addr & 0xFFFF), src, n)) return false;
    for (uint8_t i = 0; i < 20; i++) if (Wiremin_probe(ee_dev(addr))) break;
    addr += n; src += n; len -= n;
  }
  return true;
}

// ── コマンドハンドラ（ファイル名が無いので位置のリセットだけ）─
static void handleOpenW() { _ee_wpos = 0; rsp_ok(); }
// CMD_OPEN_R: [0x04, a0, a1, a2] の 24bit LE アドレスへ読み出し位置を移す。
// 引数なし（全 0）なら addr=0 となり、従来どおり先頭に戻る動作と同一。
static void handleOpenR() {
  uint32_t addr = (uint32_t)cmdBuf[1] | ((uint32_t)cmdBuf[2] << 8) | ((uint32_t)cmdBuf[3] << 16);
  ee_seek(addr);
  rsp_ok();
}
static void handleClose() { rsp_ok(); }
static void handleWrite() {
  uint8_t dlen = cmdBuf[1];
  if (dlen == 0 || dlen > 14) { rsp_err(); return; }
  if (!ee_write(_ee_wpos, cmdBuf + 2, dlen)) { rsp_err(); return; }
  _ee_wpos += dlen; rsp_ok();
}
static void handleRead() {
  uint8_t dlen = cmdBuf[1];
  if (dlen == 0 || dlen > 29) { rsp_err(); return; }
  uint8_t buf[29];
  if (ee_read_full(buf, dlen) != dlen) { rsp_err(); return; }
  stream_bytes(buf, dlen); rsp_end();
}
// ヘッダの magic を潰すだけでプログラムは無効になる
static void handleErase() {
  uint8_t zero[4] = { 0, 0, 0, 0 };
  if (!ee_write(0, zero, 4)) { rsp_err(); return; }
  rsp_ok();
}

// 長い待ちの最中でも STOP を効かせる（決定 33）
static bool waitMsAbortable(uint16_t ms) {
  while (ms) {
    uint16_t n = ms > 10 ? 10 : ms;
    delay(n); ms -= n;
    if (recvCmd() == CMD_STOP) return false;
  }
  return true;
}


// ── TinyVM opcodes (BASE) ───────────────────────────────────
#define OP_END        0x00
#define OP_WAIT_MS    0x01
#define OP_WAIT_MS_REG 0x12  // レジスタ整数部 × mul ミリ秒待つ
#define OP_GPIO_MODE  0x02
#define OP_GPIO_WRITE 0x03
#define OP_GPIO_READ  0x04
#define OP_JMP        0x06
#define OP_JZ         0x07
#define OP_JNZ        0x08
#define OP_GPIO_TOG   0x09
#define OP_LOAD_BOOL  0x15
#define OP_PRINT_STR  0x16
#define OP_HALT       0x17
#define OP_WARN_REG   0x32  // レジスタ生値を DEVICE LOG へ (warn)
// ── Ad: ADC ─────────────────────────────────────────────────
#define OP_ADC_READ   0x18
// ── I2C マスター（BASE）─────────────────────────────────────
#define OP_I2C_MASTER_INIT 0x1E
#define OP_I2C_MASTER_GET  0x1F
#define OP_I2C_MASTER_SET  0x20

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

static uint8_t _adc_init = 0;
// pin → ADC チャネル変換 + アナログ入力設定 + 1回変換（10bit, 0-1023）
static uint16_t adcRead(uint8_t pin) {
  uint8_t ch;
  switch (pin) {  // 対応ピン: 0(PA1/A1) 1(PA2/A0) 6(PC4/A2) 12(PD2/A3) 15(PD5/A5) 16(PD6/A6)
    case 0:  ch = 1; GPIOA->CFGLR &= ~(0xFU << 4);  break;
    case 1:  ch = 0; GPIOA->CFGLR &= ~(0xFU << 8);  break;
    case 6:  ch = 2; GPIOC->CFGLR &= ~(0xFU << 16); break;  // EE 版で解放（SD 版は SS 専用）
    case 12: ch = 3; GPIOD->CFGLR &= ~(0xFU << 8);  break;
    case 15: ch = 5; GPIOD->CFGLR &= ~(0xFU << 20); break;
    case 16: ch = 6; GPIOD->CFGLR &= ~(0xFU << 24); break;
    default: return 0;
  }
  if (!_adc_init) {
    _adc_init = 1;
    RCC->APB2PCENR |= RCC_APB2Periph_ADC1 | RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN;
    ADC1->SAMPTR2 = 0x3FFFFFFF;  // 全チャネル サンプル時間最大（241クロック）
    ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;  // 電源ON + SWSTART トリガ
    ADC1->CTLR2 |= ADC_RSTCAL; while (ADC1->CTLR2 & ADC_RSTCAL);  // キャリブレーションリセット
    ADC1->CTLR2 |= ADC_CAL;    while (ADC1->CTLR2 & ADC_CAL);     // キャリブレーション
  }
  ADC1->RSQR3 = ch;
  ADC1->CTLR2 |= ADC_SWSTART;
  while (!(ADC1->STATR & ADC_EOC));
  return ADC1->RDATAR;
}

// ジャンプ: コード領域内を直接移動
static inline bool seekTo(uint16_t target_pc) {
  ee_seek(8UL + target_pc);
  return true;
}

static bool runUap(void) {
  hidLog(LOG_UAP_START);
  ee_seek(0);
  uint8_t header[8];
  if (ee_read_full(header, 8) != 8) { return false; }
  if (header[0] != 'U' || header[1] != 'R' || header[2] != 'B' || header[3] != '1') {
    hidLog(LOG_MAGIC_FAIL, header[0], header[1], header[2], header[3]);
    return false;
  }
  hidLog(LOG_UAP_MAGIC);
  uint16_t codeSize = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
  uint16_t pc = 0, steps = 0;
  int32_t  regs[4] = { 0, 0, 0, 0 };
  while (pc < codeSize) {
    uint8_t cmd = recvCmd();
    if (cmd == CMD_STOP) { hidLog(LOG_UAP_STOP, steps & 0xFF, steps >> 8); return false; }
    uint8_t opcode;
    if (ee_read_full(&opcode, 1) != 1) break;
    pc++;
    switch (opcode) {

      case OP_END:
        hidLog(LOG_UAP_DONE, steps & 0xFF, steps >> 8);
        return true;

      case OP_WAIT_MS: {
        uint8_t b[2]; if (ee_read_full(b, 2) != 2) goto vm_err; pc += 2;
        delay((uint16_t)b[0] | ((uint16_t)b[1] << 8)); break;
      }

      case OP_WAIT_MS_REG: {  // reg, uint16 mul — delay(mul) をレジスタ整数部の回数だけ繰り返す
        // 乗算を使わないのは RV32EC にハード乗算が無く __mulsi3 (約150B) を引き込むため
        uint8_t b[3]; if (ee_read_full(b, 3) != 3) goto vm_err; pc += 3;
        int32_t v = regs[b[0] & 3]; if (v < 0) v = 0;
        uint32_t cnt = (uint32_t)(v >> 8); if (cnt > 65535UL) cnt = 65535UL;
        uint16_t mul = (uint16_t)b[1] | ((uint16_t)b[2] << 8);
        while (cnt--) delay(mul);
        break;
      }

      case OP_WARN_REG: {  // レジスタ生値(Q16.8 下位24bit)を DEVICE LOG へ送る（文字列化はブラウザ側）
        uint8_t b[1]; if (ee_read_full(b, 1) != 1) goto vm_err; pc += 1;
        int32_t v = regs[b[0] & 3];
        hidLog(LOG_WARN_VAL, b[0] & 3, (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16));
        break;
      }

      case OP_GPIO_MODE: {
        uint8_t b[2]; if (ee_read_full(b, 2) != 2) goto vm_err; pc += 2;
        if      (b[1] == GPIO_MODE_OUT)         pinMode(b[0], OUTPUT);
        else if (b[1] == GPIO_MODE_IN_PULLUP)   pinMode(b[0], INPUT_PULLUP);
        else if (b[1] == GPIO_MODE_IN_PULLDOWN) pinMode(b[0], INPUT_PULLDOWN);
        else                                    pinMode(b[0], INPUT);
        break;
      }

      case OP_GPIO_WRITE: {
        uint8_t b[2]; if (ee_read_full(b, 2) != 2) goto vm_err; pc += 2;
        digitalWrite(b[0], b[1] ? HIGH : LOW); break;
      }

      case OP_GPIO_READ: {
        uint8_t b[2]; if (ee_read_full(b, 2) != 2) goto vm_err; pc += 2;
        regs[b[1] & 0x03] = digitalRead(b[0]) ? 1 : 0; break;
      }

      case OP_JMP: {
        uint8_t b[2]; if (ee_read_full(b, 2) != 2) goto vm_err; pc += 2;
        int16_t offset = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
        int32_t new_pc = (int32_t)pc + (int32_t)offset;
        if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
        if (!seekTo((uint16_t)new_pc)) goto vm_err;
        pc = (uint16_t)new_pc; break;
      }

      case OP_JZ: {
        uint8_t b[3]; if (ee_read_full(b, 3) != 3) goto vm_err; pc += 3;
        if (regs[b[0] & 0x03] == 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo((uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        } break;
      }

      case OP_JNZ: {
        uint8_t b[3]; if (ee_read_full(b, 3) != 3) goto vm_err; pc += 3;
        if (regs[b[0] & 0x03] != 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo((uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        } break;
      }

      case OP_GPIO_TOG: {
        uint8_t b[1]; if (ee_read_full(b, 1) != 1) goto vm_err; pc++;
        digitalWrite(b[0], !digitalRead(b[0])); break;
      }

      case OP_LOAD_BOOL: {
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] = b[1] ? 1 : 0; break;
      }

      case OP_PRINT_STR: {
        uint8_t flags, len;
        if (ee_read_full(&flags, 1) != 1) goto vm_err;
        if (ee_read_full(&len,   1) != 1) goto vm_err;
        pc += 2;
        char strbuf[64]; uint8_t toRead = len > 63 ? 63 : len;
        if (ee_read_full((uint8_t*)strbuf, toRead) != toRead) goto vm_err;
        if (len > 63) {
          uint8_t excess = len - 63, skip[16];
          while (excess > 0) { uint8_t n = excess > 16 ? 16 : excess;
            if (ee_read_full(skip, n) != n) goto vm_err; excess -= n; }
        }
        pc += len; consolePrint(strbuf, toRead, flags); break;
      }

      case OP_HALT: {
        uint8_t code; if (ee_read_full(&code,1)!=1) goto vm_err; pc++;
        hidLog(LOG_UAP_HALT, code, steps&0xFF, steps>>8);
        return false;
      }

      case OP_ADC_READ: {
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[1]&0x03] = ((int32_t)(adcRead(b[0]) >> 2)) << 8; break;  // 整数 0-255（Q16.8 の整数部に格納）
      }

      case OP_I2C_MASTER_INIT: { break; }   // バスは setup() で初期化済み
      case OP_I2C_MASTER_GET: {
        uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t val = 0; Wiremin_read_reg(b[0], b[1], &val, 1);
        regs[b[2]&3] = (int32_t)val; break;
      }
      case OP_I2C_MASTER_SET: {
        uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t val = (uint8_t)(regs[b[2]&3] & 0xFF);
        Wiremin_write_reg(b[0], b[1], &val, 1); break;
      }

      default:
        hidLog(LOG_BAD_OPCODE, opcode, pc&0xFF, (uint8_t)(pc>>8));
        return false;
    }
    steps++;
  }
  hidLog(LOG_UAP_DONE, steps&0xFF, steps>>8);
  return true;
vm_err:
  return false;
}

static void ledBlink(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  WebHID.begin();
  delay(5000);
  Wiremin_begin_fast();          // 5V 動作なので 400kHz（決定 20）
  if (!Wiremin_probe(EE_DEV_BASE)) {
    hidLog(LOG_EE_FAIL);
    while (1) ledBlink(1, 100);
  }
  hidLog(LOG_EE_OK);
  ledBlink(3, 150);
  autoRun = true;
}

void loop() {
  if (autoRun) {
    autoRun = false;
    digitalWrite(LED_PIN, HIGH);
    runUap();
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  uint8_t cmd = recvCmd();
  switch (cmd) {
    case CMD_RUN:      autoRun = true; break;  // 次周回の autoRun ブロックで実行（コード共有）
    case CMD_STOP:     break;
    case CMD_OPEN_W:   handleOpenW();   break;
    case CMD_WRITE:    handleWrite();   break;
    case CMD_CLOSE:    handleClose();   break;
    case CMD_OPEN_R:   handleOpenR();   break;
    case CMD_READ:     handleRead();    break;
    case CMD_ERASE:    handleErase();   break;
    default: delay(10); break;
  }
}