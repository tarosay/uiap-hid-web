/*
 * UIAPrubyVmRn.ino
 * UIAPruby TinyVM Runner — 動的生成
 * コンポーネント: BASE + Rn
 * FQBN: UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 * 要ボードパッケージ: UIAPduino HID v1.2.5 以降（SDmin の sm_seek / sm_write_at を使用）
 */

#include <Arduino.h>
#include <WebHID.h>
#include <SDmin.h>

#define LED_PIN  2
#define PIN_SS   6

#define CMD_OPEN_W    0x01
#define CMD_WRITE     0x02
#define CMD_CLOSE     0x03
#define CMD_DEL       0x06
#define CMD_LIST_DIR  0x09
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
// ── Rn: RAND ────────────────────────────────────────────────
#define OP_RAND  0x21
#define OP_SRAND 0x22

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

static uint32_t _rng;  // xorshift32 seed (BSS: 0→seed補正)

// ジャンプ: 開いているコードファイル内を直接移動（ファイル開き直し不要）
static bool seekTo(uint16_t target_pc) {
  return sm_seek(8UL + target_pc);
}

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
  uint16_t pc = 0, steps = 0;
  int32_t  regs[4] = { 0, 0, 0, 0 };
  while (pc < codeSize) {
    uint8_t cmd = recvCmd();
    if (cmd == CMD_STOP) { hidLog(LOG_UAP_STOP, steps & 0xFF, steps >> 8); sm_close_r(); return false; }
    uint8_t opcode;
    if (sm_read_full(&opcode, 1) != 1) break;
    pc++;
    switch (opcode) {

      case OP_END:
        hidLog(LOG_UAP_DONE, steps & 0xFF, steps >> 8);
        sm_close_r(); return true;

      case OP_WAIT_MS: {
        uint8_t b[2]; if (sm_read_full(b, 2) != 2) goto vm_err; pc += 2;
        delay((uint16_t)b[0] | ((uint16_t)b[1] << 8)); break;
      }

      case OP_WAIT_MS_REG: {  // reg, uint16 mul — delay(mul) をレジスタ整数部の回数だけ繰り返す
        // 乗算を使わないのは RV32EC にハード乗算が無く __mulsi3 (約150B) を引き込むため
        uint8_t b[3]; if (sm_read_full(b, 3) != 3) goto vm_err; pc += 3;
        int32_t v = regs[b[0] & 3]; if (v < 0) v = 0;
        uint32_t cnt = (uint32_t)(v >> 8); if (cnt > 65535UL) cnt = 65535UL;
        uint16_t mul = (uint16_t)b[1] | ((uint16_t)b[2] << 8);
        while (cnt--) delay(mul);
        break;
      }

      case OP_GPIO_MODE: {
        uint8_t b[2]; if (sm_read_full(b, 2) != 2) goto vm_err; pc += 2;
        if      (b[1] == GPIO_MODE_OUT)         pinMode(b[0], OUTPUT);
        else if (b[1] == GPIO_MODE_IN_PULLUP)   pinMode(b[0], INPUT_PULLUP);
        else if (b[1] == GPIO_MODE_IN_PULLDOWN) pinMode(b[0], INPUT_PULLDOWN);
        else                                    pinMode(b[0], INPUT);
        break;
      }

      case OP_GPIO_WRITE: {
        uint8_t b[2]; if (sm_read_full(b, 2) != 2) goto vm_err; pc += 2;
        digitalWrite(b[0], b[1] ? HIGH : LOW); break;
      }

      case OP_GPIO_READ: {
        uint8_t b[2]; if (sm_read_full(b, 2) != 2) goto vm_err; pc += 2;
        regs[b[1] & 0x03] = digitalRead(b[0]) ? 1 : 0; break;
      }

      case OP_JMP: {
        uint8_t b[2]; if (sm_read_full(b, 2) != 2) goto vm_err; pc += 2;
        int16_t offset = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
        int32_t new_pc = (int32_t)pc + (int32_t)offset;
        if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
        if (!seekTo((uint16_t)new_pc)) goto vm_err;
        pc = (uint16_t)new_pc; break;
      }

      case OP_JZ: {
        uint8_t b[3]; if (sm_read_full(b, 3) != 3) goto vm_err; pc += 3;
        if (regs[b[0] & 0x03] == 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo((uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        } break;
      }

      case OP_JNZ: {
        uint8_t b[3]; if (sm_read_full(b, 3) != 3) goto vm_err; pc += 3;
        if (regs[b[0] & 0x03] != 0) {
          int16_t offset = (int16_t)((uint16_t)b[1] | ((uint16_t)b[2] << 8));
          int32_t new_pc = (int32_t)pc + (int32_t)offset;
          if (new_pc < 0 || new_pc > (int32_t)codeSize) goto vm_err;
          if (!seekTo((uint16_t)new_pc)) goto vm_err;
          pc = (uint16_t)new_pc;
        } break;
      }

      case OP_GPIO_TOG: {
        uint8_t b[1]; if (sm_read_full(b, 1) != 1) goto vm_err; pc++;
        digitalWrite(b[0], !digitalRead(b[0])); break;
      }

      case OP_LOAD_BOOL: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        regs[b[0]&3] = b[1] ? 1 : 0; break;
      }

      case OP_PRINT_STR: {
        uint8_t flags, len;
        if (sm_read_full(&flags, 1) != 1) goto vm_err;
        if (sm_read_full(&len,   1) != 1) goto vm_err;
        pc += 2;
        char strbuf[64]; uint8_t toRead = len > 63 ? 63 : len;
        if (sm_read_full((uint8_t*)strbuf, toRead) != toRead) goto vm_err;
        if (len > 63) {
          uint8_t excess = len - 63, skip[16];
          while (excess > 0) { uint8_t n = excess > 16 ? 16 : excess;
            if (sm_read_full(skip, n) != n) goto vm_err; excess -= n; }
        }
        pc += len; consolePrint(strbuf, toRead, flags); break;
      }

      case OP_HALT: {
        uint8_t code; if (sm_read_full(&code,1)!=1) goto vm_err; pc++;
        hidLog(LOG_UAP_HALT, code, steps&0xFF, steps>>8);
        sm_close_r(); return false;
      }

      case OP_RAND: {
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        if (!_rng) _rng = 1;
        _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
        uint8_t range = (b[1] > b[0]) ? (uint8_t)(b[1]-b[0]) : 1;
        uint8_t r = (uint8_t)_rng; while (r >= range) r -= range;
        regs[b[2]&3] = (int32_t)((r + b[0]) << 8); break;
      }
      case OP_SRAND: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        _rng = (uint32_t)b[0] | ((uint32_t)b[1]<<8); if (!_rng) _rng = 1; break;
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
    case CMD_RUN:      autoRun = true; break;  // 次周回の autoRun ブロックで実行（コード共有）
    case CMD_STOP:     break;
    case CMD_OPEN_W:   handleOpenW();   break;
    case CMD_WRITE:    handleWrite();   break;
    case CMD_CLOSE:    handleClose();   break;
    case CMD_DEL:      handleDel();     break;
    case CMD_LIST_DIR: handleListDir(); break;
    default: delay(10); break;
  }
}