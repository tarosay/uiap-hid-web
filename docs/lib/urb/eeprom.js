// ============================================================
//  UIAPduino との WebHID 接続（EE 版プロトコル）
//
//  もともと uiapruby-ee.html の中にあったものを、そのままここへ移した。
//  分けた理由は URB Block Lab（uiapruby-block.html）からも同じ手順で
//  EEPROM に書けるようにするため。両方に同じコードを置くと、片方だけ古くなる。
//
//  ⚠ このファイルは DOM を触らない。ログの文言も画面の更新も呼び出し側の仕事で、
//     ここは「いつ何が起きたか」をコールバックで知らせるだけ。
//
//  ⚠ sendFeatureReport は EP0 コントロール転送なので、同時に複数呼べない。
//     ここでは必ず 1 コマンド → 1 応答（nextRsp）の順で待つ。
// ============================================================

// ファームの応答フレーム
export const RSP_MARKER = 0x52, RSP_OK = 0, RSP_ERR = 1, RSP_DATA = 2, RSP_END = 3;
// EE 版コマンド（0x06/0x08-0x0A は欠番）
export const CMD_OPEN_W = 0x01, CMD_WRITE = 0x02, CMD_CLOSE = 0x03,
             CMD_OPEN_R = 0x04, CMD_READ  = 0x05;
export const CMD_RUN = 0x10, CMD_STOP = 0x11;
export const CONSOLE_MARKER = 0x50;

export const EE_CHUNK    = 14;  // CMD_WRITE の 1 回あたり上限（ファーム側 dlen <= 14）
export const EE_READ_MAX = 29;  // handleRead() の dlen 上限

// Feature Report は 32 バイト固定（arduino_core_ch32 v1.1.5 以降）。
// 16 バイトで送ると sendFeatureReport が失敗する。
export function mkCmd(bytes) { const b = new Uint8Array(32); bytes.forEach((v, i) => b[i] = v); return b; }

/**
 * UIAPduino 1 台ぶんの接続を作る。
 *
 * handlers:
 *   onConsoleText(text)   puts / print の出力（UTF-8 に組み立て済み）
 *   onStatusReport(bytes) 0x44 で始まる状態レポート（意味付けは呼び出し側）
 *   onDisconnect(device)  USB が抜けた
 */
export function createLink({ onConsoleText, onStatusReport, onDisconnect } = {}) {
  const rspQueue = [];
  let rspResolve = null;
  let hidDevice  = null;
  let consoleBytes = [];

  function handleConsoleReport(d) {
    // バイトを蓄積し、最終チャンク（more フラグなし）で UTF-8 デコード（マルチバイト文字がチャンク境界をまたぐため）
    for (let i = 2; i < 8; i++) { if (d[i] === 0) break; consoleBytes.push(d[i]); }
    if (!(d[1] & 0x80)) { onConsoleText?.(new TextDecoder().decode(new Uint8Array(consoleBytes))); consoleBytes = []; }
  }

  function handleReport(e) {
    const d = new Uint8Array(e.data.buffer);
    if (d[0] === CONSOLE_MARKER) { handleConsoleReport(d); return; }
    if (d[0] === RSP_MARKER) {
      if (rspResolve) { const f = rspResolve; rspResolve = null; f(d); } else rspQueue.push(d); return;
    }
    if (d[0] !== 0x44) return;
    onStatusReport?.(d);
  }

  function nextRsp(ms = 6000) {
    if (rspQueue.length) return Promise.resolve(rspQueue.shift());
    return new Promise((res, rej) => {
      const t = setTimeout(() => { rspResolve = null; rej(new Error('RSP タイムアウト')); }, ms);
      rspResolve = d => { clearTimeout(t); res(d); };
    });
  }

  const send = bytes => hidDevice.sendFeatureReport(0, mkCmd(bytes));

  // requestDevice() はユーザー操作の直後でないと SecurityError になる。
  // クリックハンドラの中から await を挟まずに呼ぶこと。
  async function connect(filters = [{ usagePage: 0xFF00 }]) {
    const [dev] = await navigator.hid.requestDevice({ filters });
    if (!dev) return null;
    hidDevice = dev;
    if (!hidDevice.opened) await hidDevice.open();
    hidDevice.addEventListener('inputreport', handleReport);
    return hidDevice;
  }

  // 既に許可されている基板へ、選択ダイアログを出さずに繋ぐ。
  // WebHID の許可はオリジンに残るので、一度でも選んでいればユーザー操作なしで開ける。
  // 同じ基板を複数のタブが同時に開けて、入力レポートは開いている全部のタブに届く（実機で確認済み）。
  // exclude: 繋ぎたくない機器（ブートローダなど）の { vendorId, productId }
  async function attach({ exclude = [] } = {}) {
    if (hidDevice) return hidDevice;          // もう繋がっている
    if (!navigator.hid) return null;
    let list;
    try { list = await navigator.hid.getDevices(); } catch (e) { return null; }
    const dev = list.find(d =>
      !exclude.some(x => x.vendorId === d.vendorId && x.productId === d.productId) &&
      d.collections?.some(c => c.usagePage === 0xFF00));
    if (!dev) return null;                    // まだ一度も選んでいない
    try { if (!dev.opened) await dev.open(); } catch (e) { return null; }
    hidDevice = dev;
    hidDevice.addEventListener('inputreport', handleReport);
    return hidDevice;
  }

  async function disconnect() {
    if (!hidDevice) return;
    hidDevice.removeEventListener('inputreport', handleReport);
    await hidDevice.close();
    hidDevice = null;
  }

  navigator.hid?.addEventListener('disconnect', e => {
    if (hidDevice && e.device === hidDevice) {
      hidDevice.removeEventListener('inputreport', handleReport);
      hidDevice = null;
      onDisconnect?.(e.device);
    }
  });

  // ── プログラムの書き込み ──
  // 進み具合は onStage で知らせる。ログの文言を呼び出し側に任せるため、
  // 元のコードでログを出していた位置とまったく同じ順序で呼ぶ。
  //   'stop'     STOP 送信直後（VM 停止待ちに入る）
  //   'open'     OPEN_W 送信直後（応答待ちに入る）  { total }
  //   'opened'   OPEN_W 応答 OK（転送に入る）
  //   'progress' 5% 刻みの進捗                      { pct }
  async function sendProgram(bytes, { onStage } = {}) {
    const total = bytes.length;
    // 実行中の VM を停止してから書き込む
    await send([CMD_STOP]);
    onStage?.('stop');
    await new Promise(r => setTimeout(r, 300));
    await send([CMD_OPEN_W]);   // EE 版は書き込み位置を 0 に戻すだけ（ファイル名なし）
    onStage?.('open', { total });
    const openRsp = await nextRsp(10000);
    if (openRsp[1] !== RSP_OK) return { ok: false, stage: 'open', total };
    onStage?.('opened');
    let lastPct = -1;
    for (let offset = 0; offset < total; offset += EE_CHUNK) {
      const chunk = bytes.slice(offset, Math.min(offset + EE_CHUNK, total));
      await send([CMD_WRITE, chunk.length, ...chunk]);
      const wr = await nextRsp();
      if (wr[1] !== RSP_OK) {
        await send([CMD_CLOSE]); await nextRsp();
        return { ok: false, stage: 'write', offset, total };
      }
      const pct = Math.floor((offset + chunk.length) / total * 100);
      if (pct !== lastPct && (pct % 5 === 0 || offset + EE_CHUNK >= total)) { onStage?.('progress', { pct }); lastPct = pct; }
    }
    await send([CMD_CLOSE]);
    const closeRsp = await nextRsp();
    return closeRsp[1] === RSP_OK ? { ok: true, total } : { ok: false, stage: 'close', total };
  }

  // ── 読み出し（書き込み系は一切呼ばない）──
  // CMD_OPEN_R: [0x04, a0, a1, a2] の 24bit LE アドレスへ読み出し位置を移す
  async function openRead(addr) {
    await send([CMD_OPEN_R, addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF]);
    const r = await nextRsp();
    if (r[1] !== RSP_OK) throw new Error(`OPEN_R FAIL (addr=${addr}）`);
  }

  // CMD_READ: 現在位置から n バイト（n <= EE_READ_MAX）。読み出し位置は n だけ進む
  async function readChunk(n) {
    await send([CMD_READ, n]);
    const out = [];
    for (;;) {
      const r = await nextRsp();
      if (r[1] === RSP_ERR) throw new Error('READ ERR');
      if (r[1] === RSP_END) break;
      if (r[1] === RSP_DATA) for (let i = 0; i < r[2]; i++) out.push(r[3 + i]);
    }
    return out;
  }

  // 任意アドレスから len バイト。OPEN_R で位置決めしてから READ を繰り返す
  async function readAt(addr, len) {
    await openRead(addr);
    const out = [];
    while (out.length < len) {
      const n = Math.min(EE_READ_MAX, len - out.length);
      const b = await readChunk(n);
      if (!b.length) break;
      out.push(...b);
    }
    return new Uint8Array(out);
  }

  return {
    get device() { return hidDevice; },
    get opened() { return !!hidDevice?.opened; },
    connect, attach, disconnect,
    send, nextRsp,
    run:  () => send([CMD_RUN]),
    stop: () => send([CMD_STOP]),
    sendProgram,
    openRead, readChunk, readAt,
    // 検証用フック: 実機の代わりに偽デバイスを差し込み、レポートを流し込む
    setDevice: d => { hidDevice = d; },
    feed: handleReport,
    resetConsole: () => { consoleBytes = []; },
  };
}
