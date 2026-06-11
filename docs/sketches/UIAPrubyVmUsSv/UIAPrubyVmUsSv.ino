/*
 * UIAPrubyVmUsSv.ino
 * UIAPruby TinyVM Runner — 動的生成
 * コンポーネント: BASE + Us + Sv
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

static void sv_print_buf(const char *s, uint16_t len, uint8_t flags);
static void consolePrint(const char *s, uint8_t len, uint8_t flags) { sv_print_buf(s, len, flags); }

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
// ── Us: Ultrasonic ──────────────────────────────────────────
#define OP_ULTRASONIC 0x1A
// ── Sv: SD変数 / PRINT_REG ──────────────────────────────────
#define OP_PRINT_REG     0x19  // Q16.8→HIDコンソール出力
#define OP_VAR_LOAD      0x25
#define OP_VAR_STORE     0x26
#define OP_VAR_LOAD_IDX  0x27
#define OP_VAR_STORE_IDX 0x28
#define OP_TO_S          0x29  // Q16.8→文字列変数(.urv)
#define OP_VAR_STR_SET     0x2B  // 文字列リテラル → 文字変数
#define OP_VAR_STR_COPY    0x2C  // 文字変数 → 文字変数
#define OP_VAR_STR_CAT     0x2D  // 文字変数を連結
#define OP_VAR_STR_CAT_LIT 0x2E  // リテラルを連結
#define OP_VAR_PRINT       0x2F  // 文字変数 → HIDコンソール
#define OP_VAR_STR_CMP     0x30  // 文字変数 == リテラル → reg
#define OP_VAR_STR_CMP_V   0x31  // 文字変数 == 文字変数 → reg

#define GPIO_MODE_IN          0
#define GPIO_MODE_OUT         1
#define GPIO_MODE_IN_PULLUP   2
#define GPIO_MODE_IN_PULLDOWN 3

#define SV_MAX_VARS 6  // RAM節約のため6個まで（2026-06-11 仕様変更）
struct SvMeta { uint8_t type; uint16_t count; char name[17]; };
static SvMeta   _sv[SV_MAX_VARS];
static uint8_t  _sv_count = 0;
static uint16_t _sv_code_start = 8;
static char     _sv_prog[17];

static void sv_prog_name(const char *fn) {
  uint8_t i = 0;
  while (fn[i] && fn[i] != '.' && i < 16) { _sv_prog[i] = fn[i]; i++; }
  _sv_prog[i] = '\0';
}
static void sv_build_fname(uint8_t idx, char *out) {
  if (idx >= SV_MAX_VARS) idx = 0;  // 不正 idx の一元ガード
  uint8_t p = 0;
  for (uint8_t i = 0; _sv_prog[i]; i++) out[p++] = _sv_prog[i];
  out[p++] = '_';
  for (uint8_t i = 0; _sv[idx].name[i]; i++) out[p++] = _sv[idx].name[i];
  out[p++] = '.'; out[p++] = 'u'; out[p++] = 'r'; out[p++] = 'v';
  out[p] = '\0';
}
// 文字変数ワークバッファ（256B + null）/ リテラルステージング（最大64B）
static uint8_t _sv_sbuf[129];
static uint8_t _sv_lit[33];

// .urv → _sv_sbuf（全体をゼロクリアして読み込み）。戻り値 = strlen
// 128B は sm_read_full の len（uint8_t）に収まるため単発読み
static uint16_t sv_read_str(uint8_t idx) {
  char vf[27]; sv_build_fname(idx, vf);
  memset(_sv_sbuf, 0, 129);
  if (sm_open_r(vf)) { sm_read_full(_sv_sbuf, 128); sm_close_r(); }
  uint16_t n = 0;
  while (n < 128 && _sv_sbuf[n]) n++;
  return n;
}
// _sv_sbuf → .urv（128B 部分上書き — sm_write_at でファイル作り直しなし）
static void sv_write_str(uint8_t idx) {
  char vf[27]; sv_build_fname(idx, vf);
  sm_write_at(vf, 0, _sv_sbuf, 128);  // .urv は初期化時に 128B 確保済み
}
// コードストリームからリテラルを _sv_lit へ（最大32B）。戻り値 = 取得バイト数, 失敗 0xFF
// 32B 超の余剰は読み捨て不要（呼び出し元が reopenCode で絶対位置に復帰するため）
static uint8_t sv_read_lit(uint8_t len) {
  uint8_t t = len > 32 ? 32 : len;
  if (sm_read_full(_sv_lit, t) != t) return 0xFF;
  return t;
}
// 数値SD変数の読み込み（VAR_LOAD/LOAD_IDX 共通 — sm_seek で要素位置へ直行）
static int32_t sv_load_num(uint8_t idx, uint16_t elem) {
  char vf[27]; sv_build_fname(idx, vf);
  int32_t val = 0;
  if (sm_open_r(vf)) {
    uint8_t buf[4];
    if (sm_seek((uint32_t)elem * 4) && sm_read_full(buf,4)==4)
      val = (int32_t)((uint32_t)buf[0]|(uint32_t)buf[1]<<8|(uint32_t)buf[2]<<16|(uint32_t)buf[3]<<24);
    sm_close_r();
  }
  return val;
}
// 文字列をチャンク分割して HID コンソールへ（flags: 0x01=改行 0x02=inspect）
static void sv_print_buf(const char *s, uint16_t len, uint8_t flags) {
  bool inspect = (flags & 0x02) != 0, newline = (flags & 0x01) != 0;
  if (inspect) consoleWriteChunk("\"", 1, true);
  uint16_t pos = 0;
  while (pos < len) {
    uint8_t n = (len - pos) > 6 ? 6 : (uint8_t)(len - pos);
    bool more = (pos + n < len) || inspect || newline;
    consoleWriteChunk(s + pos, n, more);
    pos += n;
  }
  if (inspect) consoleWriteChunk("\"", 1, newline);
  if (newline) consoleWriteChunk("\n", 1, false);
}

// Q16.8 → "n.nn" 文字列（PRINT_REG / TO_S 共有）
static uint8_t q16_to_str(int32_t v, char *buf) {
  uint8_t len = 0;
  if (v < 0) { buf[len++] = '-'; v = -v; }
  uint8_t ip = (uint8_t)(v >> 8);
  uint8_t fp = (uint8_t)(((uint16_t)(v & 0xFF) * 100) >> 8);
  if (ip >= 100) buf[len++] = '0' + ip/100;
  if (ip >=  10) buf[len++] = '0' + (ip/10)%10;
  buf[len++] = '0' + ip%10; buf[len++] = '.';
  buf[len++] = '0' + fp/10; buf[len++] = '0' + fp%10;
  return len;
}

// ジャンプ: 開いているコードファイル内を直接移動（ファイル開き直し不要）
static bool seekTo(uint16_t target_pc) {
  return sm_seek((uint32_t)_sv_code_start + target_pc);
}

// SD変数操作後の復帰: コードファイルを開き直して目的位置へ
static bool reopenCode(const char *filename, uint16_t target_pc) {
  if (!sm_open_r(filename)) return false;
  return seekTo(target_pc);
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
  _sv_code_start = 8; _sv_count = 0;
  sv_prog_name(filename);
  if (header[4] >= 2) {
    uint8_t vc; if (sm_read_full(&vc, 1) != 1) { sm_close_r(); return false; }
    _sv_count = vc < SV_MAX_VARS ? vc : SV_MAX_VARS;
    _sv_code_start = 9;
    for (uint8_t i = 0; i < _sv_count; i++) {
      uint8_t meta[3]; if (sm_read_full(meta, 3) != 3) { sm_close_r(); return false; }
      _sv[i].type = meta[0];
      _sv[i].count = (uint16_t)meta[1] | ((uint16_t)meta[2] << 8);
      if (_sv[i].count == 0) _sv[i].count = 1;
      _sv_code_start += 3;
      uint8_t ni = 0;
      while (ni < 16) {
        uint8_t c; if (sm_read_full(&c, 1) != 1) { sm_close_r(); return false; }
        _sv_code_start++;
        _sv[i].name[ni++] = c;
        if (c == 0) break;
      }
      _sv[i].name[16] = '\0';
    }
    sm_close_r();
    memset(_sv_sbuf, 0, 129);  // 0埋めバッファとして流用
    for (uint8_t i = 0; i < _sv_count; i++) {
      uint8_t persist = _sv[i].type & 0x80;  // type bit7 = 永続変数（$var）
      _sv[i].type &= 0x7F;
      char vf[27]; sv_build_fname(i, vf);
      if (persist) {  // 永続変数: 既存の .urv があれば値を保持（初回のみ0生成）
        if (sm_open_r(vf)) { sm_close_r(); continue; }
      }
      if (!sm_open_w(vf)) continue;
      uint32_t sz;
      if (_sv[i].type == 1) sz = 128;
      else if (_sv[i].type == 3) sz = (uint32_t)_sv[i].count * 128;
      else sz = (uint32_t)_sv[i].count * 4;
      while (sz > 0) { uint8_t n = sz > 128 ? 128 : (uint8_t)sz; sm_write(_sv_sbuf, n); sz -= n; }
      sm_close_w();
    }
    if (!reopenCode(filename, 0)) return false;
  }
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

      case OP_PRINT_REG: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        char buf[12]; uint8_t len = q16_to_str(regs[b[1]&3], buf);
        consolePrint(buf, len, b[0]); break;
      }

      // LOAD は LOAD_IDX の elem=0 版、STORE は STORE_IDX の elem=0 版（case 統合で Flash 節約）
      case OP_VAR_LOAD:
      case OP_VAR_LOAD_IDX: {
        uint8_t vi, dreg; uint16_t elem;
        if (opcode == OP_VAR_LOAD) {
          uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
          vi = b[0]; elem = 0; dreg = b[1];
        } else {
          uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
          vi = b[0]; elem = (uint16_t)(regs[b[1]&3] >> 8); dreg = b[2];
        }
        sm_close_r();
        regs[dreg&3] = sv_load_num(vi, elem);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_VAR_STORE:
      case OP_VAR_STORE_IDX: {
        uint8_t vi, sreg; uint16_t elem;
        if (opcode == OP_VAR_STORE) {
          uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
          vi = b[0]; elem = 0; sreg = b[1];
        } else {
          uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
          vi = b[0]; elem = (uint16_t)(regs[b[1]&3] >> 8); sreg = b[2];
        }
        char vf[27]; sv_build_fname(vi, vf);
        int32_t val = regs[sreg&3];
        uint8_t buf[4];
        buf[0]=val&0xFF; buf[1]=(val>>8)&0xFF; buf[2]=(val>>16)&0xFF; buf[3]=(val>>24)&0xFF;
        sm_close_r();
        // sm_write_at: 4バイトだけ部分上書き（任意要素 OK — 旧16要素制限は撤廃。範囲外 elem は無視される）
        sm_write_at(vf, (uint32_t)elem * 4, buf, 4);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_TO_S: {
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        sm_close_r();
        memset(_sv_sbuf, 0, 129);
        q16_to_str(regs[b[0]&3], (char*)_sv_sbuf);  // 直接書き込み（残りは0のため null 終端済み）
        sv_write_str(b[1]);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      // SET は CAT_LIT の d=0 版、COPY は CAT の d=0 版（case 統合で Flash 節約）
      case OP_VAR_STR_SET:
      case OP_VAR_STR_CAT_LIT: {  // var_idx, len, byte[len]
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        uint8_t toRead = sv_read_lit(b[1]); if (toRead == 0xFF) goto vm_err;
        pc += b[1];
        sm_close_r();
        uint16_t d;
        if (opcode == OP_VAR_STR_CAT_LIT) { d = sv_read_str(b[0]); }
        else { memset(_sv_sbuf, 0, 129); d = 0; }
        for (uint8_t i = 0; i < toRead && d < 127; i++) _sv_sbuf[d++] = _sv_lit[i];
        sv_write_str(b[0]);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_COPY:
      case OP_VAR_STR_CAT: {  // dst, src（COPY は d=0 から、CAT は末尾から。127B で切り捨て）
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        sm_close_r();
        uint16_t d;
        if (opcode == OP_VAR_STR_CAT) { d = sv_read_str(b[0]); }
        else { memset(_sv_sbuf, 0, 129); d = 0; }
        char vf[27]; sv_build_fname(b[1], vf);
        if (d < 127 && sm_open_r(vf)) { sm_read_full(_sv_sbuf + d, (uint16_t)(127 - d)); sm_close_r(); }
        _sv_sbuf[127] = 0;
        sv_write_str(b[0]);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_VAR_PRINT: {  // flags, var_idx
        uint8_t b[2]; if (sm_read_full(b,2)!=2) goto vm_err; pc+=2;
        sm_close_r();
        uint16_t n = sv_read_str(b[1]);
        sv_print_buf((const char*)_sv_sbuf, n, b[0]);
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_CMP: {  // var_idx, out_reg, len, byte[len]
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t len = b[2];
        uint8_t toRead = sv_read_lit(len); if (toRead == 0xFF) goto vm_err;
        pc += len;
        sm_close_r();
        uint16_t n = sv_read_str(b[0]);
        uint8_t eq = (n == len) ? 1 : 0;  // len>32 はコンパイラが弾く
        if (eq) for (uint8_t i = 0; i < toRead; i++) if (_sv_sbuf[i] != _sv_lit[i]) { eq = 0; break; }
        regs[b[1]&3] = eq;
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_CMP_V: {  // a_idx, b_idx, out_reg
        uint8_t b[3]; if (sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        sm_close_r();
        sv_read_str(b[0]);
        char vf[27]; sv_build_fname(b[1], vf);
        uint8_t eq = 1; uint16_t pos = 0;
        if (sm_open_r(vf)) {
          uint8_t tmp[16];
          while (pos < 128) {
            if (sm_read_full(tmp, 16) != 16) { break; }
            for (uint8_t i = 0; i < 16; i++) if (tmp[i] != _sv_sbuf[pos+i]) { eq = 0; break; }
            if (!eq) break;
            pos += 16;
          }
          sm_close_r();
          if (pos < 128 && eq) eq = 0;  // 読み切れなかった（サイズ不正）
        } else {
          eq = (_sv_sbuf[0] == 0) ? 1 : 0;  // 相手ファイルなし = 空文字列と比較
        }
        regs[b[2]&3] = eq;
        if (!reopenCode(filename, pc)) goto vm_err;
        break;
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

      case OP_ULTRASONIC: {
        uint8_t b[3]; if(sm_read_full(b,3)!=3) goto vm_err; pc+=3;
        digitalWrite(b[0], HIGH); delay(1); digitalWrite(b[0], LOW);
        uint32_t cnt = 60000;
        while (!digitalRead(b[1]) && --cnt);
        uint32_t t0 = SysTick->CNT; cnt = 400000;
        while (digitalRead(b[1]) && --cnt);
        regs[b[2]&3] = (int32_t)(((uint32_t)(SysTick->CNT-t0)<<8)/2784UL); break;
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