#!/usr/bin/env node
//
// URB Block Lab 専用ファームを作って base64 の .js に焼き直す。
//
//   docs/uiapruby-ee.html の generateIno
//     → .ino（作業用ディレクトリ）
//     → EE_VAR_BASE を書き換え（下記）
//     → arduino-cli で .bin
//     → docs/lib/flasher/blockBin.js
//
// tools/embed-bins.mjs（URB EE Lab の配布 6 本）と作りは同じ。分けてあるのは、
// こちらだけ EE_VAR_BASE を書き換えるため。ページが出す .ino をそのまま焼く
// 向こうと違い、こちらは「1 行だけ差し替えたもの」を焼く。
//
// ── なぜ EE_VAR_BASE を書き換えるのか ─────────────────────────────────
// ブロックが吐く Ruby は手書きより冗長になりやすいので、EEPROM のプログラム領域を
// 既定の 2,560 B から 10,240 B に広げてある。CAT24M01WI は 128KB あるので、
// 変数スロットは 6% 減るだけで済む。Flash も RAM も増えない（VM は EEPROM から
// 1 バイトずつ読みながら実行していて、プログラムをどこにも載せていない）。
//
// 使い方（リポジトリのルートで）:
//   node tools/build-block-bin.mjs
//
// arduino-cli の場所は環境変数で差し替えられる:
//   ARDUINO_CLI        実行ファイルのパス
//   ARDUINO_CLI_CONFIG arduino-cli.yaml のパス

import fs from 'fs';
import os from 'os';
import path from 'path';
import crypto from 'crypto';
import { execFileSync } from 'child_process';
import { neoMaxLeds } from '../docs/lib/urb/compiler.js';

/** CH32V003 の Flash。これを超える .bin は焼けない。 */
const FLASH_SIZE = 16384;

/** base64 を 1 行に詰める文字数。長い 1 行にすると差分が読めない。 */
const CHUNK = 80;

/** URB Block Lab の構成（spec_urb_block_lab.md）。 */
const COMPS   = { Q1: true, Pw: true, Ad: true, Se: true, Nr: true, Us: true, Rn: true, Ev: true };
// ⚠ 石を替えるのはここ 1 行。焼き込まれる EE_DEV_BASE も、ページが URB EE Lab へ
//    渡す石の指定（blockBin.js の chip を読む）も、すべてここから決まる。
//      'M01'    … CAT24M01WI 0x50/0x51（A1 = L）
//      'M01_52' … CAT24M01WI 0x52/0x53（A1 = H。24FC256 と同じバスに載せるとき）
const CHIP    = 'M01_52';
const CHIP_LABEL = { '256': '24FC256 0x50', 'M01': 'CAT24M01WI 0x50/0x51', 'M01_52': 'CAT24M01WI 0x52/0x53' };
const LEDS    = 64;
const VAR_BASE = 10240;    // プログラム領域の広さ（＝変数領域の先頭）

const COMP_ORDER = ['Q1', 'Tn', 'Pw', 'Ad', 'Se', 'Np', 'Nr', 'Us', 'Rn', 'Tm', 'Ev', 'Ec'];

const root     = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..');
const htmlPath = path.join(root, 'docs', 'uiapruby-ee.html');
const outPath  = path.join(root, 'docs', 'lib', 'flasher', 'blockBin.js');

const CLI = process.env.ARDUINO_CLI ||
  path.join(os.homedir(), 'AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe');
const CLI_CONFIG = process.env.ARDUINO_CLI_CONFIG ||
  path.join(os.homedir(), '.arduinoIDE', 'arduino-cli.yaml');

// ── generateIno を HTML から切り出す ────────────────────────────────────
// ⚠ 行番号で切らないこと。UI を編集すると行がずれて別の場所を掴む。
function loadGenerateIno() {
  const html = fs.readFileSync(htmlPath, 'utf8');
  const start = html.indexOf('const EE_PROG_SIZE');
  if (start < 0) throw new Error('EE_PROG_SIZE が見つかりません');
  const fnAt = html.indexOf('function generateIno(comps) {', start);
  if (fnAt < 0) throw new Error('generateIno が見つかりません');

  let i = html.indexOf('{', fnAt), depth = 0, end = -1;
  for (; i < html.length; i++) {
    if (html[i] === '{') depth++;
    else if (html[i] === '}') { depth--; if (depth === 0) { end = i + 1; break; } }
  }
  if (end < 0) throw new Error('generateIno の終端が見つかりません');
  // neoMaxLeds は lib/urb/compiler.js へ移したので、切り出した本文からは見えない。
  // ページと同じ実装を渡す（ここで別実装を書くと .bin とページがずれる）。
  const body = new Function('comps', 'neoMaxLeds', html.slice(start, end) + '\nreturn generateIno(comps);');
  return comps => body(comps, neoMaxLeds);
}

const generateIno = loadGenerateIno();
const name  = 'UIAPrubyEeVm' + COMP_ORDER.filter(k => COMPS[k]).join('');
const comps = { ...COMPS, chip: CHIP, leds: LEDS };

let ino = generateIno(comps);
if (typeof ino !== 'string' || ino.length === 0) throw new Error('generateIno が文字列を返しません');

const patched = ino.replace(/#define EE_VAR_BASE\s+\d+UL/, `#define EE_VAR_BASE   ${VAR_BASE}UL`);
if (patched === ino) throw new Error('EE_VAR_BASE を差し替えられませんでした');
ino = patched;

const work = fs.mkdtempSync(path.join(os.tmpdir(), 'urb-block-bin-'));
const dir  = path.join(work, name);
fs.mkdirSync(dir, { recursive: true });
fs.writeFileSync(path.join(dir, `${name}.ino`), ino, 'utf8');

const outDir = path.join(work, 'out');
const fqbn = 'UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,opt=oslto';   // Tn 無しなので pwm=default は不要

process.stdout.write(`${name}（EE_VAR_BASE=${VAR_BASE}）をビルド中...`);
try {
  execFileSync(CLI, ['compile', '--config-file', CLI_CONFIG, '--fqbn', fqbn,
                     '--output-dir', outDir, dir], { stdio: 'pipe' });
} catch (e) {
  console.log('');
  console.error('ビルドに失敗しました');
  console.error(String(e.stdout ?? '') + String(e.stderr ?? ''));
  process.exit(1);
}

const binPath = path.join(outDir, `${name}.ino.bin`);
if (!fs.existsSync(binPath)) throw new Error(`.bin が出ていません: ${binPath}`);
const bin = fs.readFileSync(binPath);

// 焼けないものを埋め込んでも、実機で初めて分かることになる。ここで止める。
if (bin.length === 0)        throw new Error('.bin が空です');
if (bin.length > FLASH_SIZE) throw new Error(`.bin が大きすぎます: ${bin.length} > ${FLASH_SIZE}`);

console.log(` ${bin.length} B（残り ${FLASH_SIZE - bin.length} B）`);

const b64   = bin.toString('base64');
const lines = [];
for (let i = 0; i < b64.length; i += CHUNK) lines.push(b64.slice(i, i + CHUNK));

const js = `// このファイルは tools/build-block-bin.mjs が作ります。手で編集しないでください。
//
// URB Block Lab（docs/uiapruby-block.html）が焼くファーム。
// 構成は spec に合わせた 1 本だけ。EE_VAR_BASE を 2,560 → ${VAR_BASE.toLocaleString()} に広げてあるので、
// URB EE Lab の配布 6 本（sketchBins.js）とは中身が違います。

export const FLASH_SIZE = ${FLASH_SIZE};

export const BLOCK_BIN = {
  name:  ${JSON.stringify(name)},
  label: ${JSON.stringify('URB Block Lab 専用 / ' + (CHIP_LABEL[CHIP] || CHIP))},
  chip:  ${JSON.stringify(CHIP)},
  leds:  ${LEDS},
  comps: ${JSON.stringify(COMP_ORDER.filter(k => COMPS[k]))},
  eeVarBase: ${VAR_BASE},
  size:  ${bin.length},
  binSha256: ${JSON.stringify(crypto.createHash('sha256').update(bin).digest('hex'))},
  inoSha256: ${JSON.stringify(crypto.createHash('sha256').update(ino, 'utf8').digest('hex'))},
  base64: [
${lines.map(l => `    '${l}',`).join('\n')}
  ].join(''),
};
`;

fs.writeFileSync(outPath, js, 'utf8');
console.log(`${path.relative(root, outPath)} を書き出しました`);
