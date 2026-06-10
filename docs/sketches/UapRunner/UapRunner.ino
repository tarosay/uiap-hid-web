/*
 * UapRunner.ino  —  URB1 v0 TinyVM Runner + SD File Manager
 *
 * ruby-to-urb.html で生成した URB1 v0 形式の .urb ファイルを
 * TinyVM で逐次実行します。
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
 *   0x10 RUN      — main.urb を TinyVM 実行
 *   0x11 STOP     — 実行中断
 *
 * WebHID レスポンス (Input Report 8 bytes):
 *   ファイル操作: [0x52, STATUS, LEN, d0..d4]
 *   実行ログ:    ['D'=0x44, type, ...]
 *     0x01 SD OK    0x02 SD FAIL
 *     0x10 UAP 開始  0x11 magic OK   0x12 magic NG
 *     0x18 UAP 完了  0x19 UAP 停止
 *     0x1F 不明 opcode
 *
 * URB1 ファイル形式:
 *   offset  size  内容
 *   0       4     magic "URB1"
 *   4       1     version = 1
 *   5       1     flags = 0
 *   6       2     code_size uint16 LE
 *   8       N     bytecode
 *
 * TinyVM 命令セット v0 (リトルエンディアン):
 *   0x00 END
 *   0x01 WAIT_MS   uint16 ms
 *   0x02 GPIO_MODE uint8 pin, uint8 mode (0=IN 1=OUT 2=IN_PULLUP 3=IN_PULLDOWN)
 *   0x03 GPIO_WRITE uint8 pin, uint8 value
 *   0x04 PWM_FREQ  uint8 pin, uint16 frequency (0=stop)
 *   0x05 JUMP      uint16 offset（バイトコード先頭からの絶対オフセット）
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
#define LOG_BAD_OPCODE 0x1F

static void hidLog(uint8_t type, uint8_t d0=0, uint8_t d1=0, uint8_t d2=0,
                   uint8_t d3=0, uint8_t d4=0, uint8_t d5=0) {
  uint8_t p[8] = { 'D', type, d0, d1, d2, d3, d4, d5 };
  WebHID.send(p, 8);
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

// ── TinyVM v0 opcodes ─────────────────────────────────────────────
#define OP_END        0x00
#define OP_WAIT_MS    0x01
#define OP_GPIO_MODE  0x02
#define OP_GPIO_WRITE 0x03
#define OP_PWM_FREQ   0x04
#define OP_JUMP       0x05

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

// ── ファイルを再オープンして絶対 pc バイト目にシーク ──────────────
static bool seekTo(const char *filename, uint16_t target_pc) {
  sm_close_r();
  if (!sm_open_r(filename)) return false;
  uint8_t hdr[8];
  if (sm_read_full(hdr, 8) != 8) return false;  // URB1 ヘッダをスキップ
  uint16_t remain = target_pc;
  while (remain > 0) {
    uint8_t tmp[16];
    uint8_t n = remain > 16 ? 16 : (uint8_t)remain;
    if (sm_read_full(tmp, n) != n) return false;
    remain -= n;
  }
  return true;
}

// ── TinyVM v0 実行 ────────────────────────────────────────────────
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
        else if (b[1] == GPIO_MODE_IN_PULLUP)    pinMode(b[0], INPUT_PULLUP);
        else if (b[1] == GPIO_MODE_IN_PULLDOWN)  pinMode(b[0], INPUT_PULLDOWN);
        else                                     pinMode(b[0], INPUT);
        break;
      }

      case OP_GPIO_WRITE: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        digitalWrite(b[0], b[1] ? HIGH : LOW);
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

      case OP_JUMP: {
        uint8_t b[2];
        if (sm_read_full(b, 2) != 2) goto vm_err;
        pc += 2;
        uint16_t target = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
        if (target > codeSize) goto vm_err;
        if (!seekTo(filename, target)) goto vm_err;
        pc = target;
        break;
      }

      default:
        hidLog(LOG_BAD_OPCODE, opcode, pc & 0xFF, (uint8_t)(pc >> 8));
        sm_close_r(); return false;
    }

    steps++;
  }

  hidLog(LOG_UAP_DONE, steps & 0xFF, steps >> 8);
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
  autoRun = true;  // 起動時に main.urb を自動実行
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
