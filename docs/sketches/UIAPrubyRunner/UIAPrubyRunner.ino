/*
 * UIAPrubyRunner.ino  —  UIAPruby v1 TinyVM Runner + SD File Manager
 *
 * UIAPruby v1 の .uap ファイルを TinyVM で逐次実行します。
 * .uap は uiapruby.html で Ruby 構文から変換・転送します。
 *
 * ボード:  HID ProMicro CH32V003 SD+WebHID  (Board Version: V1.4)
 * FQBN:   UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 *
 * SD 配線:
 *   A2(PC4, pin6) CS / 8 MOSI / 3V3 VDD / 7 SCK / GND VSS / 9 MISO
 *
 * WebHID コマンド (Feature Report 32 bytes):
 *   0x01 OPEN_W   / 0x02 WRITE  / 0x03 CLOSE
 *   0x06 DEL      / 0x09 LIST_DIR
 *   0x10 RUN      — SCRIPT.UAP を TinyVM 実行
 *   0x11 STOP     — 実行中断
 *
 * WebHID レスポンス (Input Report 8 bytes):
 *   ファイル操作: [0x52, STATUS, LEN, d0..d4]
 *   実行ログ:    ['D'=0x44, type, ...]
 *     0x01 SD OK    0x02 SD FAIL
 *     0x10 UAP 開始  0x11 magic OK   0x12 magic NG
 *     0x18 UAP 完了  0x19 UAP 停止   0x1A UAP HALT
 *     0x1F 不明 opcode
 *   HID Console: [0x50, flags, byte0..5]
 *     flags: 0x80=MORE 0x04=CLEAR
 *
 * UAP1 ファイル形式:
 *   offset  size  内容
 *   0       4     magic "UAP1"
 *   4       1     version = 1
 *   5       1     flags = 0
 *   6       2     code_size uint16 LE
 *   8       N     bytecode
 *
 * TinyVM 命令セット v1 (リトルエンディアン):
 *   0x00 END
 *   0x01 WAIT_MS      uint16 ms
 *   0x02 GPIO_MODE    uint8 pin, uint8 mode (0=IN 1=OUT 2=IN_PULLUP)
 *   0x03 GPIO_WRITE   uint8 pin, uint8 value
 *   0x04 GPIO_READ    uint8 pin, uint8 reg
 *   0x05 PWM_FREQ     uint8 pin, uint16 frequency
 *   0x06 JMP          int16 relative_offset
 *   0x07 JZ           uint8 reg, int16 relative_offset
 *   0x08 JNZ          uint8 reg, int16 relative_offset
 *   0x09 GPIO_TOGGLE  uint8 pin
 *   0x0A LOAD_Q16     uint8 reg, int32 value
 *   0x0B ADD_Q16      uint8 dst, uint8 src
 *   0x0C SUB_Q16      uint8 dst, uint8 src
 *   0x0D MUL_Q16      uint8 dst, uint8 src
 *   0x0E DIV_Q16      uint8 dst, uint8 src
 *   0x0F CMP_LT_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x10 CMP_GT_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x11 CMP_EQ_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x12 CMP_LE_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x13 CMP_GE_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x14 CMP_NE_Q16   uint8 lhs, uint8 rhs, uint8 out
 *   0x15 LOAD_BOOL    uint8 reg, uint8 value
 *   0x16 PRINT_STR    uint8 flags, uint8 len, byte[len] str
 *   0x17 HALT         uint8 error_code
 *   0x18 ADC_READ     uint8 pin, uint8 reg       → Q16.8 (0.0〜1.0)
 *   0x19 PRINT_REG   uint8 flags, uint8 reg    → Q16.8 を "n.nn" 形式で出力
 *
 * レジスタ: R0–R3 (int32_t)
 * Q16.8: 1.0 = 256  0.5 = 128  0.1 ≒ 26
 * JMP/JZ/JNZ offset: 命令直後位置からの相対バイトオフセット (int16)
 */

#include <Arduino.h>
#include <WebHID.h>
#include <SDmin.h>

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
  // flags: 0x01=newline  0x02=inspect(quotes)  0x03=inspect+newline
  bool inspect = (flags & 0x02) != 0;
  bool newline = (flags & 0x01) != 0;

  // Build effective string in a small buffer
  // For long strings, stream directly
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
// スケール 256 (2^8)。MUL/DIV は int32_t のみで実装（int64_t は Flash 超過）
#define Q16_SHIFT 8

// ── TinyVM opcodes ────────────────────────────────────────────────
#define OP_END        0x00
#define OP_WAIT_MS    0x01
#define OP_GPIO_MODE  0x02
#define OP_GPIO_WRITE 0x03
#define OP_GPIO_READ  0x04
#define OP_PWM_FREQ   0x05
#define OP_JMP        0x06
#define OP_JZ         0x07
#define OP_JNZ        0x08
#define OP_GPIO_TOG   0x09
#define OP_LOAD_Q16   0x0A
#define OP_ADD_Q16    0x0B
#define OP_SUB_Q16    0x0C
#define OP_MUL_Q16    0x0D
#define OP_DIV_Q16    0x0E
#define OP_CMP_LT     0x0F
#define OP_CMP_GT     0x10
#define OP_CMP_EQ     0x11
#define OP_CMP_LE     0x12
#define OP_CMP_GE     0x13
#define OP_CMP_NE     0x14
#define OP_LOAD_BOOL  0x15
#define OP_PRINT_STR  0x16
#define OP_HALT       0x17
#define OP_ADC_READ   0x18
#define OP_PRINT_REG  0x19   // uint8 flags, uint8 reg  → Q16.8 を小数文字列で出力

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

// ── ファイルを再オープンして pc バイト目まで読み飛ばし ─────────────
static bool seekTo(const char *filename, uint16_t target_pc) {
  sm_close_r();
  if (!sm_open_r(filename)) return false;
  uint8_t hdr[8];
  if (sm_read_full(hdr, 8) != 8) return false;  // skip UAP1 header
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

  if (header[0] != 'U' || header[1] != 'A' || header[2] != 'P' || header[3] != '1') {
    hidLog(LOG_MAGIC_FAIL, header[0], header[1], header[2], header[3]);
    sm_close_r(); return false;
  }
  hidLog(LOG_UAP_MAGIC);

  uint16_t codeSize = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
  uint16_t pc    = 0;
  uint16_t steps = 0;
  int32_t    regs[4] = { 0, 0, 0, 0 };  // R0–R3

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

      case OP_PWM_FREQ: {
        uint8_t b[3];
        if (sm_read_full(b, 3) != 3) goto vm_err;
        pc += 3;
        uint16_t freq = (uint16_t)b[1] | ((uint16_t)b[2] << 8);
        if (freq == 0) noTone(b[0]);
        else           tone(b[0], freq);
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

      // Phase 7: Q16.8 演算 (int32_t のみ)
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
      // Q16.8 MUL/DIV: int32_t のみ（int64_t は Flash 超過）
      // MUL: int16_t キャストで ±127.996 の範囲に制限して int32_t 乗算
      // DIV: a*256/b  Q16.8 有効値なら a*256 は int32_t に収まる
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
        // skip any excess bytes beyond buffer
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

      // ADC_READ: analogRead(pin) を Q16.8 (0〜255) でレジスタに格納
      // 10bit ADC (0-1023) >> 2 → 0-255 ≈ 0.0-1.0 in Q16.8
      case OP_ADC_READ: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        uint8_t reg = b[1] & 0x03;
        regs[reg] = (int32_t)(analogRead(b[0]) >> 2);
        break;
      }

      // PRINT_REG: Q16.8 レジスタ値を "nnn.nn\n" 形式でコンソール出力
      // flags: 0x01=newline  reg: R0-R3
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
  autoRun = true;  // 起動時に SCRIPT.UAP を自動実行
}

void loop() {
  if (autoRun) {
    autoRun = false;
    digitalWrite(LED_PIN, HIGH);
    runUap("SCRIPT.UAP");
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  uint8_t cmd = recvCmd();
  switch (cmd) {
    case CMD_RUN:
      digitalWrite(LED_PIN, HIGH);
      runUap("SCRIPT.UAP");
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
