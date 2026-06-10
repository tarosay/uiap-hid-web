/*
 * UIAPrubyRunnerStandard.ino  —  UIAPruby TinyVM Runner 標準版 + I2C (Master + Slave)
 *
 * UIAPrubyRunnerTone（限定版）の全オペコードに加え、Wiremin.h による I2C 機能を追加。
 * スレーブとして動作しながら GPIO/ADC/超音波を共有レジスタ経由でマスターに提供。
 * マスターとして他の I2C スレーブ（UIAPrubyRunnerStandard 等）のレジスタを読み書き可能。
 *
 * ボード:  HID ProMicro CH32V003 SD+WebHID  (Board Version: V1.4)
 * FQBN:   UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 *
 * I2C ピン: SDA=pin3(PC1), SCL=pin4(PC2)  — 4.7kΩ プルアップ要
 * PWM ピン: pin2(PC0)=TIM2_CH3, pin5(PC3)=TIM1_CH3
 *
 * TinyVM オペコード変更:
 *   0x05 PWM_DUTY  uint8 pin, uint8 vm_reg — デューティ比 (0-255) を vm_reg から設定
 *        ※ PWM_FREQ(tone/noTone) を廃止し PWM_DUTY(analogWrite) に置き換え
 *
 * 追加 TinyVM オペコード (I2C):
 *   0x1B I2C_SLAVE_INIT  uint8 addr          — スレーブとして初期化
 *   0x1C I2C_SLAVE_SET   uint8 reg, uint8 vm_reg — 共有レジスタ[reg] = vm_reg の値
 *   0x1D I2C_SLAVE_GET   uint8 reg, uint8 vm_reg — vm_reg = 共有レジスタ[reg]
 *   0x1E I2C_MASTER_INIT (引数なし)           — マスターとして初期化
 *   0x1F I2C_MASTER_GET  uint8 addr, uint8 reg, uint8 vm_reg — スレーブのレジスタを読む
 *   0x20 I2C_MASTER_SET  uint8 addr, uint8 reg, uint8 vm_reg — スレーブのレジスタに書く
 *
 * 追加 TinyVM オペコード (乱数):
 *   0x21 RAND   uint8 min, uint8 max, uint8 dst_reg — min〜max-1 の乱数を Q16.8 整数で dst_reg に格納
 *   0x22 SRAND  uint16_le seed                      — 乱数シード設定 (0 → 1 に補正)
 *
 * 共有レジスタの値は uint8_t (0-255)。vm_reg との対応:
 *   slave_set: vm_reg の下位 8bit をそのまま格納
 *   slave_get / master_get: 読んだ byte を vm_reg に格納
 */

#include <Arduino.h>
#include <WebHID.h>
#include <SDmin.h>
#include <Wiremin.h>

#define LED_PIN  2
#define PIN_SS   6

// ── コマンド ──────────────────────────────────────────────────────
#define CMD_OPEN_W    0x01
#define CMD_WRITE     0x02
#define CMD_CLOSE     0x03
#define CMD_DEL       0x06
#define CMD_LIST_DIR  0x09
#define CMD_RUN       0x10
#define CMD_STOP      0x11

// ── SD レスポンス ─────────────────────────────────────────────────
#define RSP_MARKER 0x52
#define RSP_OK     0
#define RSP_ERR    1
#define RSP_DATA   2
#define RSP_END    3

static void rsp(uint8_t status, const uint8_t *d, uint8_t len) {
  uint8_t buf[8] = { RSP_MARKER, status, len, 0, 0, 0, 0, 0 };
  if (d && len) { uint8_t n = len > 5 ? 5 : len; for (uint8_t i = 0; i < n; i++) buf[3+i] = d[i]; }
  WebHID.send(buf, 8);
  delay(12);
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

// ── 実行ログ ──────────────────────────────────────────────────────
#define LOG_SD_OK      0x01
#define LOG_SD_FAIL    0x02
#define LOG_UAP_START  0x10
#define LOG_UAP_MAGIC  0x11
#define LOG_MAGIC_FAIL 0x12
#define LOG_UAP_DONE   0x18
#define LOG_UAP_STOP   0x19
#define LOG_UAP_HALT   0x1A
#define LOG_BAD_OPCODE 0x1F

static void hidLog(uint8_t type, uint8_t d0=0, uint8_t d1=0, uint8_t d2=0,
                   uint8_t d3=0, uint8_t d4=0, uint8_t d5=0) {
  uint8_t p[8] = { 'D', type, d0, d1, d2, d3, d4, d5 };
  WebHID.send(p, 8);
}

// ── HID Console 出力 ──────────────────────────────────────────────
#define HID_CONSOLE_MARKER 0x50
#define CONSOLE_MORE  0x80
#define CONSOLE_CLEAR 0x04

static void consoleWriteChunk(const char *s, uint8_t len, bool more) {
  uint8_t buf[8] = { HID_CONSOLE_MARKER, (uint8_t)(more ? CONSOLE_MORE : 0), 0, 0, 0, 0, 0, 0 };
  uint8_t n = len > 6 ? 6 : len;
  for (uint8_t i = 0; i < n; i++) buf[2+i] = (uint8_t)s[i];
  WebHID.send(buf, 8);
  delay(12);
}

static void consolePrint(const char *s, uint8_t len, uint8_t flags) {
  bool inspect = (flags & 0x02) != 0;
  bool newline = (flags & 0x01) != 0;
  char tmp[64];
  uint8_t tlen = 0;
  if (inspect) { tmp[tlen++] = '"'; }
  for (uint8_t i = 0; i < len && tlen < 62; i++) tmp[tlen++] = s[i];
  if (inspect) { tmp[tlen++] = '"'; }
  if (newline) { tmp[tlen++] = '\n'; }
  uint8_t pos = 0;
  while (pos < tlen) {
    uint8_t chunk = tlen - pos;
    if (chunk > 6) chunk = 6;
    bool more = (pos + chunk < tlen);
    consoleWriteChunk(tmp + pos, chunk, more);
    pos += chunk;
  }
}

// ── コマンドバッファ ──────────────────────────────────────────────
static bool    autoRun = false;
static uint8_t cmdBuf[32];

static uint8_t recvCmd() {
  if (!WebHID.available()) return 0;
  memset(cmdBuf, 0, sizeof(cmdBuf));
  if (WebHID.recv(cmdBuf, sizeof(cmdBuf)) > 0) return cmdBuf[0];
  return 0;
}

// ── ファイル操作ハンドラ ──────────────────────────────────────────
static bool sm_writing = false;

static void handleOpenW() {
  if (sm_writing) { sm_close_w(); sm_writing = false; }
  char fname[27]; get_name(cmdBuf+1, fname);
  if (!fname[0]) { rsp_err(); return; }
  if (!sm_open_w(fname)) { rsp_err(); return; }
  sm_writing = true; rsp_ok();
}
static void handleWrite() {
  if (!sm_writing) { rsp_err(); return; }
  uint8_t dlen = cmdBuf[1];
  if (dlen == 0 || dlen > 14) { rsp_err(); return; }
  if (!sm_write(cmdBuf+2, dlen)) { rsp_err(); return; }
  rsp_ok();
}
static void handleClose() {
  if (!sm_writing) { rsp_err(); return; }
  sm_close_w(); sm_writing = false; rsp_ok();
}
static void handleDel() {
  char fname[27]; get_name(cmdBuf+1, fname);
  if (!fname[0]) { rsp_err(); return; }
  if (!sm_del(fname)) { rsp_err(); return; }
  rsp_ok();
}
static void handleListDir() {
  char dir[27]; get_name(cmdBuf+1, dir);
  SmListCtx ctx;
  if (!sm_list_open(&ctx, dir[0] ? dir : NULL)) { rsp_err(); return; }
  char fname[27]; uint8_t entry[29]; int typ;
  while ((typ = sm_list_next(&ctx, fname)) > 0) {
    uint8_t nlen = (uint8_t)strlen(fname);
    entry[0] = (typ == 2) ? 'D' : 'F';
    memcpy(entry+1, fname, nlen); entry[1+nlen] = 0;
    stream_bytes(entry, 2+nlen);
  }
  rsp_end();
}

// ── Q16.8 演算 ───────────────────────────────────────────────────
#define Q16_SHIFT 8

// ── TinyVM opcodes ────────────────────────────────────────────────
#define OP_END         0x00
#define OP_WAIT_MS     0x01
#define OP_GPIO_MODE   0x02
#define OP_GPIO_WRITE  0x03
#define OP_GPIO_READ   0x04
#define OP_PWM_DUTY    0x05
#define OP_JMP         0x06
#define OP_JZ          0x07
#define OP_JNZ         0x08
#define OP_GPIO_TOG    0x09
#define OP_LOAD_Q16    0x0A
#define OP_ADD_Q16     0x0B
#define OP_SUB_Q16     0x0C
#define OP_MUL_Q16     0x0D
#define OP_DIV_Q16     0x0E
#define OP_CMP_LT      0x0F
#define OP_CMP_GT      0x10
#define OP_CMP_EQ      0x11
#define OP_CMP_LE      0x12
#define OP_CMP_GE      0x13
#define OP_CMP_NE      0x14
#define OP_LOAD_BOOL   0x15
#define OP_PRINT_STR   0x16
#define OP_HALT        0x17
#define OP_ADC_READ    0x18
#define OP_PRINT_REG   0x19
#define OP_ULTRASONIC  0x1A
// I2C opcodes
#define OP_I2C_SLAVE_INIT   0x1B  // uint8 addr7
#define OP_I2C_SLAVE_SET    0x1C  // uint8 reg, uint8 vm_reg → shared[reg] = regs[vm_reg] & 0xFF
#define OP_I2C_SLAVE_GET    0x1D  // uint8 reg, uint8 vm_reg → regs[vm_reg] = shared[reg]
#define OP_I2C_MASTER_INIT  0x1E  // (no args)
#define OP_I2C_MASTER_GET   0x1F  // uint8 addr7, uint8 reg, uint8 vm_reg
#define OP_I2C_MASTER_SET   0x20  // uint8 addr7, uint8 reg, uint8 vm_reg
// 乱数
#define OP_RAND       0x21   // uint8 min, uint8 max, uint8 dst_reg → Q16.8 整数 (min〜max-1)
#define OP_SRAND      0x22   // uint16_le seed

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

// ── ADC 直接レジスタ読み取り ──────────────────────────────────────
// [7:4]=port(0=A,2=C,3=D), [3:0]=pin  — analogpin index = ADC channel for CH32V003
static const uint8_t _adc_pins[8] = { 0x02,0x01,0x24,0x32,0x33,0x35,0x36,0x34 };

static uint16_t adc_read_direct(uint8_t apin) {
  if (apin >= 8) return 0;
  uint8_t enc  = _adc_pins[apin];
  uint8_t port = enc >> 4;
  uint8_t pin  = enc & 0xF;
  GPIO_TypeDef *gpio    = (port==0) ? GPIOA : (port==2) ? GPIOC : GPIOD;
  uint32_t      rcc_gpio = (port==0) ? RCC_IOPAEN : (port==2) ? RCC_IOPCEN : RCC_IOPDEN;
  RCC->APB2PCENR |= RCC_ADC1EN | rcc_gpio;
  gpio->CFGLR &= ~(0xFU << (pin * 4));
  RCC->CFGR0 = (RCC->CFGR0 & ~RCC_ADCPRE) | RCC_ADCPRE_DIV8;
  ADC1->CTLR1 = 0; ADC1->CTLR2 = 0;
  ADC1->SAMPTR2 = 0x3FFFFFFF; ADC1->RSQR1 = 0; ADC1->RSQR3 = apin;
  ADC1->CTLR2 = ADC_ADON;
  ADC1->CTLR2 |= ADC_EXTTRIG | ADC_SWSTART;
  while (!(ADC1->STATR & ADC_EOC));
  uint16_t val = (uint16_t)ADC1->RDATAR;
  ADC1->CTLR2 = 0;
  return val;
}

// ── 乱数 xorshift32 (乗算なし、BSS配置) ─────────────────────────
static uint32_t _rng;  // BSS (0初期化→rng_step内でセード補正)

// ── ファイルを再オープンして pc バイト目まで読み飛ばし ─────────────
static bool seekTo(const char *filename, uint16_t target_pc) {
  sm_close_r();
  if (!sm_open_r(filename)) return false;
  uint8_t hdr[8];
  if (sm_read_full(hdr, 8) != 8) return false;
  uint16_t remain = target_pc;
  while (remain > 0) {
    uint8_t tmp[16];
    uint8_t n = remain > 16 ? 16 : (uint8_t)remain;
    if (sm_read_full(tmp, n) != n) return false;
    remain -= n;
  }
  return true;
}

// ── TinyVM 実行 ───────────────────────────────────────────────────
static bool runUap(const char *filename) {
  hidLog(LOG_UAP_START);

  if (!sm_open_r(filename)) return false;

  uint8_t header[8];
  if (sm_read_full(header, 8) != 8) { sm_close_r(); return false; }

  if (header[0] != 'U' || header[1] != 'R' || header[2] != 'B' || header[3] != '1') {
    hidLog(LOG_MAGIC_FAIL, header[0], header[1], header[2], header[3]);
    sm_close_r(); return false;
  }
  hidLog(LOG_UAP_MAGIC);

  uint16_t codeSize = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
  uint16_t pc    = 0;
  uint16_t steps = 0;
  int32_t    regs[4] = { 0, 0, 0, 0 };

  while (pc < codeSize) {
    uint8_t cmd = recvCmd();
    if (cmd == CMD_STOP) {
      hidLog(LOG_UAP_STOP, steps & 0xFF, steps >> 8);
      sm_close_r(); return false;
    }

    uint8_t opcode;
    if (sm_read_full(&opcode, 1) != 1) break;
    pc++;

    switch (opcode) {

      case OP_END:
        hidLog(LOG_UAP_DONE, steps & 0xFF, steps >> 8);
        sm_close_r(); return true;

      case OP_WAIT_MS: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        delay((uint16_t)b[0] | ((uint16_t)b[1] << 8));
        break;
      }

      case OP_GPIO_MODE: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        if      (b[1] == GPIO_MODE_OUT)          pinMode(b[0], OUTPUT);
        else if (b[1] == GPIO_MODE_IN_PULLUP)   pinMode(b[0], INPUT_PULLUP);
        else if (b[1] == GPIO_MODE_IN_PULLDOWN) pinMode(b[0], INPUT_PULLDOWN);
        else                                    pinMode(b[0], INPUT);
        break;
      }

      case OP_GPIO_WRITE: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        digitalWrite(b[0], b[1] ? HIGH : LOW);
        break;
      }

      case OP_GPIO_READ: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        uint8_t reg = b[1] & 0x03;
        regs[reg] = digitalRead(b[0]) ? 1 : 0;
        break;
      }

      case OP_PWM_DUTY: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        uint8_t duty = (uint8_t)(regs[b[1]&3] & 0xFF);
        if (b[0] == 2) {    // PC0 = TIM2_CH3  PSC=187,ARR=255 → ~1kHz
          RCC->APB1PCENR |= RCC_TIM2EN; RCC->APB2PCENR |= RCC_IOPCEN;
          GPIOC->CFGLR = (GPIOC->CFGLR & ~0xFU) | 0xBU;
          TIM2->PSC=187; TIM2->ATRLR=255; TIM2->CH3CVR=duty;
          TIM2->CHCTLR2 = (TIM2->CHCTLR2 & ~0x0070U) | 0x0060U;
          TIM2->CCER |= TIM_CC3E; TIM2->CTLR1 |= TIM_CEN;
        } else {
          // TIM1: D5(PC3/CH3), D0(PA1/CH2), D12(PD2/CH1)
          GPIO_TypeDef *gpio; uint32_t rcc_gpio, cfglr_mask, cfglr_val;
          volatile uint32_t *cvr; volatile uint16_t *chctlr; uint32_t ctlr_mask, ctlr_val, ccer_bit;
          if (b[0] == 5) {
            gpio=GPIOC; rcc_gpio=RCC_IOPCEN; cfglr_mask=0xFU<<12; cfglr_val=0xBU<<12;
            cvr=&TIM1->CH3CVR; chctlr=&TIM1->CHCTLR2; ctlr_mask=0x0070U; ctlr_val=0x0060U; ccer_bit=TIM_CC3E;
          } else if (b[0] == 0) {
            gpio=GPIOA; rcc_gpio=RCC_IOPAEN; cfglr_mask=0xFU<<4; cfglr_val=0xBU<<4;
            cvr=&TIM1->CH2CVR; chctlr=&TIM1->CHCTLR1; ctlr_mask=0x7000U; ctlr_val=0x6000U; ccer_bit=TIM_CC2E;
          } else {                // D12: PD2 = TIM1_CH1
            gpio=GPIOD; rcc_gpio=RCC_IOPDEN; cfglr_mask=0xFU<<8; cfglr_val=0xBU<<8;
            cvr=&TIM1->CH1CVR; chctlr=&TIM1->CHCTLR1; ctlr_mask=0x0070U; ctlr_val=0x0060U; ccer_bit=TIM_CC1E;
          }
          RCC->APB2PCENR |= RCC_TIM1EN | rcc_gpio;
          gpio->CFGLR = (gpio->CFGLR & ~cfglr_mask) | cfglr_val;
          TIM1->PSC=187; TIM1->ATRLR=255; *cvr=duty;
          *chctlr = (*chctlr & ~ctlr_mask) | ctlr_val;
          TIM1->CCER |= ccer_bit; TIM1->BDTR |= TIM_MOE; TIM1->CTLR1 |= TIM_CEN;
        }
        break;
      }

      case OP_JMP: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        int16_t offset = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
        int32_t new_pc = (int32_t)pc + (int32_t)offset;
        if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
        if (!seekTo(filename, (uint16_t)new_pc)) goto vm_err;
        pc = (uint16_t)new_pc;
        break;
      }

      case OP_JZ: {
        uint8_t b[3];
        if (sm_read_full(b, 3) != 3) goto vm_err;
        pc += 3;
        uint8_t reg = b[0] & 0x03;
        if (regs[reg] == 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo(filename, (uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        }
        break;
      }

      case OP_JNZ: {
        uint8_t b[3];
        if (sm_read_full(b, 3) != 3) goto vm_err;
        pc += 3;
        uint8_t reg = b[0] & 0x03;
        if (regs[reg] != 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo(filename, (uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        }
        break;
      }

      case OP_GPIO_TOG: {
        uint8_t b[1];
        if (sm_read_full(b, 1) != 1) goto vm_err;
        pc++;
        digitalWrite(b[0], !digitalRead(b[0]));
        break;
      }

      case OP_LOAD_Q16: {
        uint8_t b[5]; if (sm_read_full(b,5)!=5) goto vm_err; pc+=5;
        uint8_t reg = b[0]&3;
        regs[reg] = (int32_t)((uint32_t)b[1]|(uint32_t)b[2]<<8|(uint32_t)b[3]<<16|(uint32_t)b[4]<<24);
        break;
      }
      case OP_ADD_Q16: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] += regs[b[1]&3]; break;
      }
      case OP_SUB_Q16: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] -= regs[b[1]&3]; break;
      }
      case OP_MUL_Q16: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] = ((int32_t)(int16_t)regs[b[0]&3] * (int16_t)regs[b[1]&3]) >> 8;
        break;
      }
      case OP_DIV_Q16: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        int32_t den = regs[b[1]&3];
        if (den == 0) goto vm_err;
        regs[b[0]&3] = (regs[b[0]&3] * 256) / den;
        break;
      }
      case OP_CMP_LT: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]< regs[b[1]&3])?1:0; break; }
      case OP_CMP_GT: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]> regs[b[1]&3])?1:0; break; }
      case OP_CMP_EQ: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]==regs[b[1]&3])?1:0; break; }
      case OP_CMP_LE: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]<=regs[b[1]&3])?1:0; break; }
      case OP_CMP_GE: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]>=regs[b[1]&3])?1:0; break; }
      case OP_CMP_NE: { uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3; regs[b[2]&3]=(regs[b[0]&3]!=regs[b[1]&3])?1:0; break; }

      case OP_LOAD_BOOL: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] = b[1] ? 1 : 0; break;
      }

      case OP_PRINT_STR: {
        uint8_t flags, len;
        if (sm_read_full(&flags, 1) != 1) goto vm_err;
        if (sm_read_full(&len,   1) != 1) goto vm_err;
        pc += 2;
        char strbuf[64];
        uint8_t toRead = len > 63 ? 63 : len;
        if (sm_read_full((uint8_t*)strbuf, toRead) != toRead) goto vm_err;
        if (len > 63) {
          uint8_t excess = len - 63;
          uint8_t skip[16];
          while (excess > 0) {
            uint8_t n = excess > 16 ? 16 : excess;
            if (sm_read_full(skip, n) != n) goto vm_err;
            excess -= n;
          }
        }
        pc += len;
        consolePrint(strbuf, toRead, flags);
        break;
      }

      case OP_HALT: {
        uint8_t code; if (sm_read_full(&code,1)!=1) goto vm_err; pc++;
        hidLog(LOG_UAP_HALT, code, steps&0xFF, steps>>8);
        sm_close_r(); return false;
      }

      case OP_ADC_READ: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        uint8_t reg = b[1] & 0x03;
        regs[reg] = (int32_t)(adc_read_direct(b[0]) >> 2);
        break;
      }

      case OP_PRINT_REG: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        int32_t v = regs[b[1]&3];
        char buf[12]; uint8_t len=0;
        if (v < 0) { buf[len++]='-'; v=-v; }
        uint8_t ip = (uint8_t)(v >> 8);
        uint8_t fp = (uint8_t)(((uint16_t)(v & 0xFF) * 100) >> 8);
        if (ip >= 100) buf[len++] = '0' + ip/100;
        if (ip >=  10) buf[len++] = '0' + (ip/10)%10;
        buf[len++] = '0' + ip%10;
        buf[len++] = '.';
        buf[len++] = '0' + fp/10;
        buf[len++] = '0' + fp%10;
        consolePrint(buf, len, b[0]);
        break;
      }

      case OP_ULTRASONIC: {
        uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        digitalWrite(b[0], HIGH); delay(1); digitalWrite(b[0], LOW);
        uint32_t cnt = 60000;
        while (!digitalRead(b[1]) && --cnt);
        uint32_t t0 = SysTick->CNT; cnt = 400000;
        while (digitalRead(b[1]) && --cnt);
        regs[b[2]&3] = (int32_t)(((uint32_t)(SysTick->CNT-t0)<<8)/2784UL);
        break;
      }

      // ── I2C opcodes ────────────────────────────────────────────
      case OP_I2C_SLAVE_INIT: {
        uint8_t b[1]; if (sm_read_full(b,1)!=1) goto vm_err; pc++;
        Wiremin_slave_begin(b[0]);
        break;
      }

      case OP_I2C_SLAVE_SET: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        Wiremin_slave_set(b[0], (uint8_t)(regs[b[1]&3] & 0xFF));
        break;
      }

      case OP_I2C_SLAVE_GET: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[1]&3] = (int32_t)(uint8_t)Wiremin_slave_get(b[0]);
        break;
      }

      case OP_I2C_MASTER_INIT: {
        Wiremin_begin();
        break;
      }

      case OP_I2C_MASTER_GET: {
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t val = 0;
        Wiremin_read_reg(b[0], b[1], &val, 1);
        regs[b[2]&3] = (int32_t)val;
        break;
      }

      case OP_I2C_MASTER_SET: {
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t val = (uint8_t)(regs[b[2]&3] & 0xFF);
        Wiremin_write_reg(b[0], b[1], &val, 1);
        break;
      }

      case OP_RAND: {
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        if (!_rng) _rng = 1;
        _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
        uint8_t range = (b[1] > b[0]) ? (uint8_t)(b[1]-b[0]) : 1;
        uint8_t r = (uint8_t)_rng; while (r >= range) r -= range;
        regs[b[2]&3] = (int32_t)((r + b[0]) << 8);
        break;
      }
      case OP_SRAND: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        _rng = (uint32_t)b[0] | ((uint32_t)b[1]<<8); if (!_rng) _rng = 1;
        break;
      }

      default:
        hidLog(LOG_BAD_OPCODE, opcode, pc&0xFF, (uint8_t)(pc>>8));
        sm_close_r(); return false;
    }

    steps++;
  }

  hidLog(LOG_UAP_DONE, steps&0xFF, steps>>8);
  sm_close_r(); return true;

vm_err:
  sm_close_r(); return false;
}

// ── LED ───────────────────────────────────────────────────────────
static void ledBlink(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}

// ── setup / loop ──────────────────────────────────────────────────
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WebHID.begin();
  delay(5000);

  if (!sm_init(PIN_SS) || !sm_mount()) {
    hidLog(LOG_SD_FAIL);
    while (1) ledBlink(1, 100);
  }

  hidLog(LOG_SD_OK);
  ledBlink(3, 150);
  autoRun = true;
}

void loop() {
  if (autoRun) {
    autoRun = false;
    digitalWrite(LED_PIN, HIGH);
    runUap("main.urb");
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  uint8_t cmd = recvCmd();
  switch (cmd) {
    case CMD_RUN:
      digitalWrite(LED_PIN, HIGH);
      runUap("main.urb");
      digitalWrite(LED_PIN, LOW);
      delay(200);
      break;
    case CMD_STOP: break;
    case CMD_OPEN_W:    handleOpenW();   break;
    case CMD_WRITE:     handleWrite();   break;
    case CMD_CLOSE:     handleClose();   break;
    case CMD_DEL:       handleDel();     break;
    case CMD_LIST_DIR:  handleListDir(); break;
    default: delay(10); break;
  }
}
