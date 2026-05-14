/*
 * MidbPlayer.ino — SAM2695 MIDI Player + SD File Manager (.midb)
 *
 * SD カードの .midb ファイルを順番に再生します。
 * .midb ファイルは midi-to-midb.html で MIDI ファイルから変換・転送します。
 *
 * ボード:   HID ProMicro CH32V003 SD+WebHID  (Board Version: V1.4)
 * FQBN:    UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 *
 * SD 配線:
 *   A2(PC4, pin6)  CS   — DAT3/CS
 *   8              MOSI — CMD/DI
 *   3V3                 — VDD
 *   7              SCK  — CLK
 *   GND                 — VSS
 *   9              MISO — DAT0/DO
 *
 * SAM2695 配線:
 *   uart TX (PD5, pin15) — SAM2695 RX
 *   GND                  — SAM2695 GND
 *
 * WebHID コマンド（Feature Report, 32 bytes, byte[0]）:
 *   ファイル操作は WebHID_SD と同一プロトコル:
 *   0x01 OPEN_W    byte[1..]=ファイル名（null 終端、最大 26 文字）
 *   0x02 WRITE     byte[1]=データ長, byte[2..]=データ（最大 14 バイト）
 *   0x03 CLOSE     ファイルクローズ
 *   0x06 DEL       byte[1..]=ファイル名（null 終端）
 *   0x09 LIST_DIR  byte[1..]=ディレクトリ名（空 = ルート）
 *   0x10 PLAY      次の .midb を再生
 *   0x11 STOP      再生中断
 *   0x20 GM_RESET  GM リセット SysEx 送信
 *   0x30 TEST_NOTE テスト単音再生
 *
 * WebHID レスポンス（Input Report, 8 bytes）:
 *   ファイル操作: [0x52, STATUS, LEN, d0..d4]
 *     STATUS: 0=OK  1=ERR  2=DATA  3=END
 *   再生ログ:   ['D'=0x44, type, ...]
 *     0x01 SD OK      0x02 SD FAIL
 *     0x04 再生開始   0x05 マジック確認 OK   0x06 マジック NG
 *     0x08 再生完了   0x09 停止
 */

#include <Arduino.h>
#include <WebHID.h>
#include <SDmin.h>
#include "UIAPSerial.h"

#define LED_PIN  2
#define PIN_SS   6
#define SAM_BAUD 31250

// ── ファイル操作コマンド（WebHID_SD 統一） ────────────────────
#define CMD_OPEN_W    0x01
#define CMD_WRITE     0x02
#define CMD_CLOSE     0x03
#define CMD_DEL       0x06
#define CMD_LIST_DIR  0x09
// ── 再生コマンド（MidbPlayer 固有） ──────────────────────────
#define CMD_PLAY      0x10
#define CMD_STOP      0x11
#define CMD_GM_RESET  0x20
#define CMD_TEST_NOTE 0x30

// ── ファイル操作レスポンス（WebHID_SD 統一） ─────────────────
#define RSP_MARKER 0x52
#define RSP_OK     0
#define RSP_ERR    1
#define RSP_DATA   2
#define RSP_END    3

static void rsp(uint8_t status, const uint8_t *d, uint8_t len) {
  uint8_t buf[8] = { RSP_MARKER, status, len, 0, 0, 0, 0, 0 };
  if (d && len) {
    uint8_t n = len > 5 ? 5 : len;
    for (uint8_t i = 0; i < n; i++) buf[3 + i] = d[i];
  }
  WebHID.send(buf, 8);
  delay(12);
}
#define rsp_ok()  rsp(RSP_OK,  0, 0)
#define rsp_err() rsp(RSP_ERR, 0, 0)
#define rsp_end() rsp(RSP_END, 0, 0)

static void stream_bytes(const uint8_t *data, uint8_t len) {
  uint8_t offset = 0;
  while (offset < len) {
    uint8_t chunk = len - offset;
    if (chunk > 5) chunk = 5;
    rsp(RSP_DATA, data + offset, chunk);
    offset += chunk;
  }
}

static void get_name(const uint8_t *src, char *dst) {
  uint8_t i = 0;
  for (; i < 26 && src[i]; i++) dst[i] = (char)src[i];
  dst[i] = '\0';
}

// ── 再生ログ（MidbPlayer 固有） ───────────────────────────────
#define LOG_SD_OK      0x01
#define LOG_SD_FAIL    0x02
#define LOG_PLAYING    0x04
#define LOG_MAGIC_OK   0x05
#define LOG_MAGIC_FAIL 0x06
#define LOG_DONE       0x08
#define LOG_STOPPED    0x09

static void hidLog(uint8_t type,
                   uint8_t d0=0, uint8_t d1=0, uint8_t d2=0,
                   uint8_t d3=0, uint8_t d4=0, uint8_t d5=0) {
  uint8_t p[8] = {'D', type, d0, d1, d2, d3, d4, d5};
  WebHID.send(p, 8);
}

// ── 再生状態 ─────────────────────────────────────────────────
static bool    autoPlay   = false;  // 自動連続再生フラグ
static uint8_t pendingCmd = 0;      // 再生中に受信したファイル操作コマンドの退避

// ── コマンドバッファ ──────────────────────────────────────────
static uint8_t cmdBuf[32];

static uint8_t recvCmd() {
  if (pendingCmd) {
    uint8_t c = pendingCmd; pendingCmd = 0; return c;
  }
  if (!WebHID.available()) return 0;
  memset(cmdBuf, 0, sizeof(cmdBuf));
  if (WebHID.recv(cmdBuf, sizeof(cmdBuf)) > 0) return cmdBuf[0];
  return 0;
}

// ── GM リセット ───────────────────────────────────────────────
static const uint8_t GM_RESET[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};

static void sendGmReset() {
  uart.write(GM_RESET, sizeof(GM_RESET));
}

// ── テスト単音 ────────────────────────────────────────────────
static void sendTestNote() {
  sendGmReset();
  delay(100);
  const uint8_t pc[]  = {0xC0, 73};
  const uint8_t on[]  = {0x90, 60, 100};
  const uint8_t off[] = {0x80, 60,   0};
  uart.write(pc,  sizeof(pc));
  uart.write(on,  sizeof(on));
  delay(2000);
  uart.write(off, sizeof(off));
}

// ── ファイルリスト（内部再生用） ──────────────────────────────
#define MAX_FILES 16
static char    fileList[MAX_FILES][27];
static uint8_t fileCount = 0;
static uint8_t fileIdx   = 0;

static void buildFileList() {
  fileCount = 0;
  SmListCtx ctx;
  sm_list_open(&ctx, "");
  char name[27];
  while (fileCount < MAX_FILES) {
    int r = sm_list_next(&ctx, name);
    if (r == 0) break;
    if (r != 1) continue;
    if (!sm_open_r(name)) continue;
    uint8_t m[4];
    bool ok = (sm_read_full(m, 4) == 4
               && m[0]=='M' && m[1]=='I' && m[2]=='D' && m[3]=='B');
    sm_close_r();
    if (!ok) continue;
    strncpy(fileList[fileCount], name, 26);
    fileList[fileCount][26] = '\0';
    fileCount++;
  }
}

// ── ファイル操作ハンドラ ──────────────────────────────────────
static bool sm_writing = false;

static void handleOpenW() {
  if (sm_writing) { sm_close_w(); sm_writing = false; }
  char fname[27];
  get_name(cmdBuf + 1, fname);
  if (!fname[0]) { rsp_err(); return; }
  if (!sm_open_w(fname)) { rsp_err(); return; }
  sm_writing = true;
  rsp_ok();
}

static void handleWrite() {
  if (!sm_writing) { rsp_err(); return; }
  uint8_t dlen = cmdBuf[1];
  if (dlen == 0 || dlen > 14) { rsp_err(); return; }
  if (!sm_write(cmdBuf + 2, dlen)) { rsp_err(); return; }
  rsp_ok();
}

static void handleClose() {
  if (!sm_writing) { rsp_err(); return; }
  sm_close_w();
  sm_writing = false;
  buildFileList();
  rsp_ok();
}

static void handleDel() {
  char fname[27];
  get_name(cmdBuf + 1, fname);
  if (!fname[0]) { rsp_err(); return; }
  if (!sm_del(fname)) { rsp_err(); return; }
  buildFileList();
  rsp_ok();
}

static void handleListDir() {
  char dir[27];
  get_name(cmdBuf + 1, dir);
  SmListCtx ctx;
  if (!sm_list_open(&ctx, dir[0] ? dir : NULL)) { rsp_err(); return; }
  char fname[27];
  uint8_t entry[29];
  int typ;
  while ((typ = sm_list_next(&ctx, fname)) > 0) {
    uint8_t nlen = (uint8_t)strlen(fname);
    entry[0] = (typ == 2) ? 'D' : 'F';
    memcpy(entry + 1, fname, nlen);
    entry[1 + nlen] = 0;
    stream_bytes(entry, 2 + nlen);
  }
  rsp_end();
}

// ── MIDB 再生 ──────────────────────────────────────────────────
static bool playMidb(const char* filename) {
  uint8_t fn[6] = {0};
  for (uint8_t i = 0; i < 6 && filename[i]; i++) fn[i] = (uint8_t)filename[i];
  hidLog(LOG_PLAYING, fn[0], fn[1], fn[2], fn[3], fn[4], fn[5]);

  if (!sm_open_r(filename)) return false;

  uint8_t magic[4];
  if (sm_read_full(magic, 4) != 4
      || magic[0] != 'M' || magic[1] != 'I'
      || magic[2] != 'D' || magic[3] != 'B') {
    hidLog(LOG_MAGIC_FAIL, magic[0], magic[1], magic[2], magic[3]);
    sm_close_r();
    return false;
  }
  hidLog(LOG_MAGIC_OK);

  uint8_t hdr[3];
  uint8_t buf[32];
  uint16_t evCount   = 0;
  uint16_t uartBytes = 0;

  while (sm_read_full(hdr, 3) == 3) {
    uint16_t wait_ms = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
    uint8_t  len     = hdr[2];

    if (wait_ms > 0) delay(wait_ms);

    if (len > 0) {
      uint8_t toRead = len;
      while (toRead > 0) {
        uint8_t chunk = toRead > (uint8_t)sizeof(buf) ? (uint8_t)sizeof(buf) : toRead;
        int n = sm_read_full(buf, chunk);
        if (n <= 0) break;
        uart.write(buf, (size_t)n);
        uartBytes += (uint16_t)n;
        toRead -= (uint8_t)n;
      }
    }

    evCount++;

    uint8_t cmd = recvCmd();
    bool isFileOp = (cmd == CMD_OPEN_W || cmd == CMD_WRITE || cmd == CMD_CLOSE
                     || cmd == CMD_DEL || cmd == CMD_LIST_DIR);
    if (cmd == CMD_STOP || isFileOp) {
      hidLog(LOG_STOPPED,
             evCount   & 0xFF, evCount   >> 8,
             uartBytes & 0xFF, uartBytes >> 8);
      sm_close_r();
      sendGmReset();  // 鳴りっぱなし防止
      if (isFileOp) pendingCmd = cmd;  // ファイル操作は loop() で処理
      return false;
    }
  }

  hidLog(LOG_DONE,
         evCount   & 0xFF, evCount   >> 8,
         uartBytes & 0xFF, uartBytes >> 8);
  sm_close_r();
  sendGmReset();
  return true;  // 自然終了 → 次の曲へ
}

// ── LED ───────────────────────────────────────────────────────
static void ledBlink(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}

// ── setup / loop ──────────────────────────────────────────────
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WebHID.begin();
  uart.begin(SAM_BAUD);
  delay(10000);

  if (!sm_init(PIN_SS) || !sm_mount()) {
    hidLog(LOG_SD_FAIL);
    while (1) ledBlink(1, 100);
  }

  buildFileList();
  hidLog(LOG_SD_OK);

  if (fileCount > 0) {
    ledBlink(3, 150);
    autoPlay = true;  // 電源オン時に自動再生開始
  }
}

void loop() {
  uint8_t cmd = recvCmd();

  // 自動連続再生: コマンドなしでも autoPlay 中は次の曲を再生
  if (cmd == 0 && autoPlay && fileCount > 0) cmd = CMD_PLAY;

  switch (cmd) {
    case CMD_PLAY:
      if (fileCount == 0) break;
      autoPlay = true;
      {
        digitalWrite(LED_PIN, HIGH);
        bool done = playMidb(fileList[fileIdx]);
        digitalWrite(LED_PIN, LOW);
        fileIdx = (fileIdx + 1) % fileCount;
        if (!done) autoPlay = false;  // 停止/割り込みなら連続再生中断
      }
      delay(200);
      break;

    case CMD_STOP:
      autoPlay = false;
      sendGmReset();  // 鳴りっぱなし防止
      break;

    case CMD_GM_RESET:
      sendGmReset();
      break;

    case CMD_TEST_NOTE:
      sendTestNote();
      break;

    case CMD_OPEN_W:
      handleOpenW();
      break;

    case CMD_WRITE:
      handleWrite();
      break;

    case CMD_CLOSE:
      handleClose();
      break;

    case CMD_DEL:
      handleDel();
      break;

    case CMD_LIST_DIR:
      handleListDir();
      break;

    default:
      if (fileCount == 0) ledBlink(1, 800);
      else delay(10);
      break;
  }
}
