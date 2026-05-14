/*
 * WebHID_SD.ino — SD ファイル操作 via WebHID
 *
 * Commands (Feature Report 0, 16 bytes, Browser → Device):
 *   [0x01, name[15]] OPEN_W    — open for write (create / truncate)
 *   [0x02, len, data[14]] WRITE — write len bytes (1–14)
 *   [0x03, ...]      CLOSE     — flush + close
 *   [0x04, name[15]] OPEN_R    — open for read
 *   [0x05, ...]      READ      — stream all file content
 *   [0x06, name[15]] DEL       — delete file
 *   [0x07, ...]      LIST      — stream root dir (files only, backward compat)
 *   [0x08, name[15]] MKDIR     — create directory
 *   [0x09, name[15]] LIST_DIR  — stream directory entries with type prefix
 *
 * Paths support subdirectories: "DIR/FILE.TXT"
 *
 * LIST_DIR stream format (packed in DATA payloads):
 *   ['D'][name][0x00]  — directory entry
 *   ['F'][name][0x00]  — file entry
 *   (repeated, then RSP_END)
 *
 * Responses (Input Report, 8 bytes, Device → Browser):
 *   [0x52, STATUS, LEN, d0..d4]
 *   STATUS: 0=OK  1=ERR  2=DATA  3=END  4=LOG
 *   LOG: テキストを5バイトずつ送信、LEN=0 で行末
 *
 * Board: HID ProMicro CH32V003 (V1.4)
 * FQBN:  UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto
 *
 * SD wiring:
 *   A2(PC4,pin6) CS   — DAT3/CS
 *   8            MOSI — CMD/DI
 *   3V3               — VDD
 *   7            SCK  — CLK
 *   GND               — VSS
 *   9            MISO — DAT0/DO
 */

#include <Arduino.h>
#include <WebHID.h>
#include <SDmin.h>

#define LED_PIN 2
#define PIN_SS  6

#define CMD_OPEN_W   0x01
#define CMD_WRITE    0x02
#define CMD_CLOSE    0x03
#define CMD_OPEN_R   0x04
#define CMD_READ     0x05
#define CMD_DEL      0x06
#define CMD_LIST     0x07
#define CMD_MKDIR    0x08
#define CMD_LIST_DIR 0x09

#define RSP_MARKER 0x52
#define RSP_OK     0
#define RSP_ERR    1
#define RSP_DATA   2
#define RSP_END    3
#define RSP_LOG    4

static void led_blink(void) {
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
}

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

static void rsp_log(const char *msg) {
  uint8_t len = (uint8_t)strlen(msg);
  uint8_t offset = 0;
  while (offset < len) {
    uint8_t chunk = len - offset;
    if (chunk > 5) chunk = 5;
    rsp(RSP_LOG, (const uint8_t *)msg + offset, chunk);
    offset += chunk;
  }
  rsp(RSP_LOG, 0, 0); // 行末
}

static void get_name(const uint8_t *src, char *dst) {
  uint8_t i = 0;
  for (; i < 26 && src[i]; i++) dst[i] = (char)src[i];
  dst[i] = '\0';
}

static void stream_bytes(const uint8_t *data, uint8_t len) {
  uint8_t offset = 0;
  while (offset < len) {
    uint8_t chunk = len - offset;
    if (chunk > 5) chunk = 5;
    rsp(RSP_DATA, data + offset, chunk);
    offset += chunk;
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  WebHID.begin();
  delay(10000);

  rsp_log("sm_init...");
  if (!sm_init(PIN_SS)) {
    rsp_log("sm_init FAIL");
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);  delay(100);
    }
  }
  rsp_log("sm_init OK");

  rsp_log("sm_mount...");
  if (!sm_mount()) {
    rsp_log("sm_mount FAIL");
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);  delay(100);
    }
  }
  rsp_log("sm_mount OK");
  rsp_log("ready");
}

void loop() {
  uint8_t buf[32];
  if (WebHID.recv(buf, sizeof(buf)) == 0) return;

  char name[27];

  switch (buf[0]) {
    case CMD_OPEN_W:
      get_name(&buf[1], name);
      if (!name[0]) { rsp_err(); break; }
      led_blink();
      if (sm_open_w(name)) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_WRITE:
      if (buf[1] == 0 || buf[1] > 14) { rsp_err(); break; }
      led_blink();
      if (sm_write(&buf[2], buf[1])) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_CLOSE:
      led_blink();
      if (sm_close_w()) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_OPEN_R:
      get_name(&buf[1], name);
      if (!name[0]) { rsp_err(); break; }
      led_blink();
      if (sm_open_r(name)) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_READ:
      {
        uint8_t rbuf[5];
        int n;
        digitalWrite(LED_PIN, HIGH);
        while ((n = sm_read(rbuf, 5)) > 0) {
          rsp(RSP_DATA, rbuf, (uint8_t)n);
        }
        sm_close_r();
        digitalWrite(LED_PIN, LOW);
        rsp_end();
      }
      break;

    case CMD_DEL:
      get_name(&buf[1], name);
      if (!name[0]) { rsp_err(); break; }
      led_blink();
      if (sm_del(name)) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_LIST:
      {
        SmListCtx ctx;
        sm_list_open(&ctx, NULL);
        char fname[27];
        digitalWrite(LED_PIN, HIGH);
        while (sm_list_next(&ctx, fname)) {
          uint8_t flen = (uint8_t)(strlen(fname) + 1);
          stream_bytes((const uint8_t *)fname, flen);
        }
        digitalWrite(LED_PIN, LOW);
        rsp_end();
      }
      break;

    case CMD_MKDIR:
      get_name(&buf[1], name);
      if (!name[0]) { rsp_err(); break; }
      led_blink();
      if (sm_mkdir(name)) { rsp_ok(); } else { rsp_err(); }
      break;

    case CMD_LIST_DIR:
      {
        get_name(&buf[1], name);
        SmListCtx ctx;
        if (!sm_list_open(&ctx, name[0] ? name : NULL)) { rsp_err(); break; }
        char fname[27];
        uint8_t entry[29];
        int typ;
        digitalWrite(LED_PIN, HIGH);
        while ((typ = sm_list_next(&ctx, fname)) > 0) {
          uint8_t nlen = (uint8_t)strlen(fname);
          entry[0] = (typ == 2) ? 'D' : 'F';
          memcpy(entry + 1, fname, nlen);
          entry[1 + nlen] = 0;
          stream_bytes(entry, 2 + nlen);
        }
        digitalWrite(LED_PIN, LOW);
        rsp_end();
      }
      break;
  }
}
