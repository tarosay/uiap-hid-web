/*
 * UIAPrubyEeVmEc.ino
 * UIAPruby TinyVM Runner — 動的生成
 * コンポーネント: BASE + Ec
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

static void var_print_buf(const char *s, uint16_t len, uint8_t flags);
static void consolePrint(const char *s, uint8_t len, uint8_t flags) { var_print_buf(s, len, flags); }

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
// ── I2C マスター（BASE）─────────────────────────────────────
#define OP_I2C_MASTER_INIT 0x1E
#define OP_I2C_MASTER_GET  0x1F
#define OP_I2C_MASTER_SET  0x20
// ── Ev: 数値 EEPROM 変数（$永続・揮発・配列）────────────────
#define OP_VAR_LOAD      0x25
#define OP_VAR_STORE     0x26
#define OP_VAR_LOAD_IDX  0x27
#define OP_VAR_STORE_IDX 0x28
// ── Ec: 文字変数 / PRINT_REG / to_s ─────────────────────────
#define OP_PRINT_REG     0x19  // Q16.8→HIDコンソール出力
#define OP_TO_S          0x29  // Q16.8→文字列変数（EEPROM）
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

#define EE_MAX_VARS 6  // RAM節約のため6個まで（SD 版と同じ）
struct VarMeta { uint8_t type; uint16_t count; char name[17]; };
static VarMeta  _var[EE_MAX_VARS];
static uint8_t  _var_count = 0;
static uint16_t _var_code_start = 8;
static uint8_t  _var_lit[33];   // リテラルのステージング（最大32B）

// スロット: [0..19] 変数名 / [20..] データ（4B 境界 = CAT24M01 の ECC 1 グループ）
static inline uint32_t var_slot(uint8_t idx) {
  return EE_VAR_BASE + (uint32_t)idx * EE_SLOT_SIZE;
}
static inline uint32_t var_addr(uint8_t idx, uint16_t elem) {
  return var_slot(idx) + EE_SLOT_NAME + (uint32_t)elem * 4UL;
}
// SD 版は {プログラム名}_{変数名}.urv で永続値を分離していた（プログラム名は常に
// "main" なので実質のキーは変数名）。EE 版はスロット先頭の変数名で同じ意味論を作る。
static bool var_name_matches(uint8_t idx) {
  uint8_t buf[17];
  ee_seek(var_slot(idx));
  if (ee_read_full(buf, 17) != 17) return false;
  for (uint8_t i = 0; i < 17; i++) {
    uint8_t c = (i < 16) ? (uint8_t)_var[idx].name[i] : 0;
    if (buf[i] != c) return false;
    if (c == 0) break;
  }
  return true;
}
static void var_name_write(uint8_t idx) {
  uint8_t buf[EE_SLOT_NAME];
  memset(buf, 0, EE_SLOT_NAME);
  for (uint8_t i = 0; i < 16 && _var[idx].name[i]; i++) buf[i] = (uint8_t)_var[idx].name[i];
  ee_write(var_slot(idx), buf, EE_SLOT_NAME);
}

// 文字変数はスロットのデータ部から 128B 固定。
// バッファはオペコードキャッシュと共用する（決定 15 — Flash 差ゼロ / RAM 128B 得）。
static uint16_t var_read_str(uint8_t idx) {
  if (idx >= EE_MAX_VARS) idx = 0;
  uint32_t a = var_addr(idx, 0);
  memset(_ee_cache, 0, EE_CACHE_SIZE + 1);
  ee_invalidate();
  Wiremin_read_reg16(ee_dev(a), (uint16_t)(a & 0xFFFF), _ee_cache, EE_CACHE_SIZE);
  uint16_t n = 0;
  while (n < EE_CACHE_SIZE && _ee_cache[n]) n++;
  return n;
}
static void var_write_str(uint8_t idx) {
  if (idx >= EE_MAX_VARS) idx = 0;
  ee_write(var_addr(idx, 0), _ee_cache, EE_CACHE_SIZE);
}
static uint8_t var_read_lit(uint8_t len) {
  uint8_t t = len > 32 ? 32 : len;
  if (ee_read_full(_var_lit, t) != t) return 0xFF;
  return t;
}
// 数値変数: SD 版のファイル名組み立て→open→seek→read→close が、アドレス計算 1 回になる
static int32_t var_load_num(uint8_t idx, uint16_t elem) {
  if (idx >= EE_MAX_VARS) idx = 0;
  uint32_t save = _ee_pos;
  uint8_t buf[4];
  int32_t val = 0;
  ee_seek(var_addr(idx, elem));
  if (ee_read_full(buf, 4) == 4)
    val = (int32_t)((uint32_t)buf[0]|(uint32_t)buf[1]<<8|(uint32_t)buf[2]<<16|(uint32_t)buf[3]<<24);
  ee_seek(save);
  return val;
}
static void var_store_num(uint8_t idx, uint16_t elem, int32_t v) {
  if (idx >= EE_MAX_VARS) idx = 0;
  uint8_t buf[4] = { (uint8_t)v, (uint8_t)(v>>8), (uint8_t)(v>>16), (uint8_t)(v>>24) };
  ee_write(var_addr(idx, elem), buf, 4);   // 4B = ECC の 1 グループにぴったり乗る
}
// 文字列をチャンク分割して HID コンソールへ（flags: 0x01=改行 0x02=inspect）
static void var_print_buf(const char *s, uint16_t len, uint8_t flags) {
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

// ジャンプ: コード領域内を直接移動
static inline bool seekTo(uint16_t target_pc) {
  ee_seek((uint32_t)_var_code_start + target_pc);
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
  _var_code_start = 8; _var_count = 0;
  if (header[4] >= 2) {
    uint8_t vc; if (ee_read_full(&vc, 1) != 1) { return false; }
    _var_count = vc < EE_MAX_VARS ? vc : EE_MAX_VARS;
    _var_code_start = 9;
    for (uint8_t i = 0; i < _var_count; i++) {
      uint8_t meta[3]; if (ee_read_full(meta, 3) != 3) { return false; }
      _var[i].type = meta[0];
      _var[i].count = (uint16_t)meta[1] | ((uint16_t)meta[2] << 8);
      if (_var[i].count == 0) _var[i].count = 1;
      _var_code_start += 3;
      uint8_t ni = 0;
      while (ni < 16) {
        uint8_t c; if (ee_read_full(&c, 1) != 1) { return false; }
        _var_code_start++;
        _var[i].name[ni++] = c;
        if (c == 0) break;
      }
      _var[i].name[16] = '\0';
    }
    
    // 名前一致 かつ $永続 のときだけ値を残す。それ以外は名前を書いて 0 クリア。
    // SD 版の .urv 生成・ディレクトリ検索に相当する処理は不要。
    for (uint8_t i = 0; i < _var_count; i++) {
      uint8_t persist = _var[i].type & 0x80;  // type bit7 = 永続変数（$var）
      _var[i].type &= 0x7F;
      if (persist && var_name_matches(i)) continue;
      var_name_write(i);
      if (_var[i].type == 1 || _var[i].type == 3) {   // 文字 / 文字配列
        memset(_ee_cache, 0, EE_CACHE_SIZE + 1);
        var_write_str(i);
        continue;
      }
      for (uint16_t e = 0; e < _var[i].count; e++) var_store_num(i, e, 0);
    }
    ee_seek(_var_code_start);
  }
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

      case OP_PRINT_REG: {
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        char buf[12]; uint8_t len = q16_to_str(regs[b[1]&3], buf);
        consolePrint(buf, len, b[0]); break;
      }

      // LOAD は LOAD_IDX の elem=0 版、STORE は STORE_IDX の elem=0 版（case 統合で Flash 節約）
      case OP_VAR_LOAD:
      case OP_VAR_LOAD_IDX: {
        uint8_t vi, dreg; uint16_t elem;
        if (opcode == OP_VAR_LOAD) {
          uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
          vi = b[0]; elem = 0; dreg = b[1];
        } else {
          uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
          vi = b[0]; elem = (uint16_t)(regs[b[1]&3] >> 8); dreg = b[2];
        }
        
        regs[dreg&3] = var_load_num(vi, elem);
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_VAR_STORE:
      case OP_VAR_STORE_IDX: {
        uint8_t vi, sreg; uint16_t elem;
        if (opcode == OP_VAR_STORE) {
          uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
          vi = b[0]; elem = 0; sreg = b[1];
        } else {
          uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
          vi = b[0]; elem = (uint16_t)(regs[b[1]&3] >> 8); sreg = b[2];
        }
        var_store_num(vi, elem, regs[sreg&3]);   // 4B = ECC の 1 グループ
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_TO_S: {
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        
        memset(_ee_cache, 0, EE_CACHE_SIZE + 1);
        q16_to_str(regs[b[0]&3], (char*)_ee_cache);  // 残りは0なので null 終端済み
        var_write_str(b[1]);
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      // SET は CAT_LIT の d=0 版、COPY は CAT の d=0 版（case 統合で Flash 節約）
      case OP_VAR_STR_SET:
      case OP_VAR_STR_CAT_LIT: {  // var_idx, len, byte[len]
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        uint8_t toRead = var_read_lit(b[1]); if (toRead == 0xFF) goto vm_err;
        pc += b[1];
        
        uint16_t d;
        if (opcode == OP_VAR_STR_CAT_LIT) { d = var_read_str(b[0]); }
        else { memset(_ee_cache, 0, EE_CACHE_SIZE + 1); d = 0; }
        for (uint8_t i = 0; i < toRead && d < 127; i++) _ee_cache[d++] = _var_lit[i];
        var_write_str(b[0]);
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_COPY:
      case OP_VAR_STR_CAT: {  // dst, src（COPY は d=0 から、CAT は末尾から。127B で切り捨て）
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        
        uint16_t d = 0;
        uint8_t dst[EE_CACHE_SIZE];
        if (opcode == OP_VAR_STR_CAT) { d = var_read_str(b[0]); memcpy(dst, _ee_cache, EE_CACHE_SIZE); }
        else memset(dst, 0, EE_CACHE_SIZE);
        uint16_t s = var_read_str(b[1]);   // src を共用バッファへ
        for (uint16_t i = 0; i < s && d < EE_CACHE_SIZE - 1; i++) dst[d++] = _ee_cache[i];
        dst[d] = 0;
        memcpy(_ee_cache, dst, EE_CACHE_SIZE);
        var_write_str(b[0]);
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_VAR_PRINT: {  // flags, var_idx
        uint8_t b[2]; if (ee_read_full(b,2)!=2) goto vm_err; pc+=2;
        
        uint16_t n = var_read_str(b[1]);
        var_print_buf((const char*)_ee_cache, n, b[0]);
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_CMP: {  // var_idx, out_reg, len, byte[len]
        uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
        uint8_t len = b[2];
        uint8_t toRead = var_read_lit(len); if (toRead == 0xFF) goto vm_err;
        pc += len;
        
        uint16_t n = var_read_str(b[0]);
        uint8_t eq = (n == len) ? 1 : 0;  // len>32 はコンパイラが弾く
        if (eq) for (uint8_t i = 0; i < toRead; i++) if (_ee_cache[i] != _var_lit[i]) { eq = 0; break; }
        regs[b[1]&3] = eq;
        if (!seekTo(pc)) goto vm_err;
        break;
      }

      case OP_VAR_STR_CMP_V: {  // a_idx, b_idx, out_reg
        uint8_t b[3]; if (ee_read_full(b,3)!=3) goto vm_err; pc+=3;
        
        uint16_t na = var_read_str(b[0]);
        uint8_t tmp[EE_CACHE_SIZE];
        memcpy(tmp, _ee_cache, EE_CACHE_SIZE);
        uint16_t nb = var_read_str(b[1]);
        uint8_t eq = (na == nb) ? 1 : 0;
        if (eq) for (uint16_t i = 0; i < na; i++) if (tmp[i] != _ee_cache[i]) { eq = 0; break; }
        regs[b[2]&3] = eq;
        if (!seekTo(pc)) goto vm_err;
        break;
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