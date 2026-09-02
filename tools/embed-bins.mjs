#!/usr/bin/env node
//
// 配布用 .bin を作って base64 の .js に焼き直す。ino → bin → base64 を一連で回す。
//
//   docs/uiapruby-ee.html の generateIno
//     → 4 構成の .ino（作業用ディレクトリ）
//     → arduino-cli で .bin
//     → docs/lib/flasher/sketchBins.js
//
// .bin をファイルとして置かないのは決定 43。base64 で JS に内包する。
//
// ── なぜ版を突き合わせるのか ────────────────────────────────────────────
// generateIno を直したのに .bin を作り直し忘れると、ページが出す .ino と
// 焼かれる中身が食い違う。それを実行時に弾けるよう、生成した .ino 本文の
// SHA-256 を一緒に埋めておく。ページ側は同じ構成で .ino を組み立て直して
// ハッシュを比べ、違えば焼かずに止める。
// generateIno の出力は決定的なので、同じ構成なら必ず同じハッシュになる。
//
// ── なぜ npm run build に繋がないのか ───────────────────────────────────
// .bin を作るには arduino-cli が要る。持っていない環境でもページは動くので、
// 生成物はリポジトリに追跡させ、generateIno を直したときだけ手で走らせる。
//
// 使い方（リポジトリのルートで）:
//   node tools/embed-bins.mjs
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
import { pathToFileURL } from 'url';

/** CH32V003 の Flash。これを超える .bin は焼けない。 */
const FLASH_SIZE = 16384;

/** base64 を 1 行に詰める文字数。長い 1 行にすると差分が読めない。 */
const CHUNK = 80;

const root    = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..');
const htmlPath = path.join(root, 'docs', 'uiapruby-ee.html');
const outPath  = path.join(root, 'docs', 'lib', 'flasher', 'sketchBins.js');

const CLI = process.env.ARDUINO_CLI ||
  path.join(os.homedir(), 'AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe');
const CLI_CONFIG = process.env.ARDUINO_CLI_CONFIG ||
  path.join(os.homedir(), '.arduinoIDE', 'arduino-cli.yaml');

// ── 配布する 6 つ（決定 41 / 47）────────────────────────────────────────
// Tn（ブザー）と Pw（サーボ）は同じタイマーの分周器を共有するので同時に選べない（決定 42）。
// 石で変わるのは .ino の EE_SLOT_SIZE と EE_DEV_BASE の 2 行だけ。
// CAT24M01WI は A1 の配線で 0x50 と 0x52 のどちらにもできるので、石として 2 つ数える。
const BASE_COMPS = { Q1: true, Ad: true, Se: true, Np: true, Us: true, Rn: true, Ec: true };
const TARGETS = [
  { key: 'Pw256',    comps: { ...BASE_COMPS, Pw: true }, chip: '256',    label: 'サーボ / 24FC256' },
  { key: 'PwM01',    comps: { ...BASE_COMPS, Pw: true }, chip: 'M01',    label: 'サーボ / CAT24M01WI 0x50' },
  { key: 'PwM01_52', comps: { ...BASE_COMPS, Pw: true }, chip: 'M01_52', label: 'サーボ / CAT24M01WI 0x52' },
  { key: 'Tn256',    comps: { ...BASE_COMPS, Tn: true }, chip: '256',    label: 'ブザー / 24FC256' },
  { key: 'TnM01',    comps: { ...BASE_COMPS, Tn: true }, chip: 'M01',    label: 'ブザー / CAT24M01WI 0x50' },
  { key: 'TnM01_52', comps: { ...BASE_COMPS, Tn: true }, chip: 'M01_52', label: 'ブザー / CAT24M01WI 0x52' },
];

/** LED 数は 6 つとも 64 で焼く（決定 47）。 */
const LEDS = 64;

/** ページと同じ並び。スケッチ名はこの順で作られる。 */
const COMP_ORDER = ['Q1', 'Tn', 'Pw', 'Ad', 'Se', 'Np', 'Nr', 'Us', 'Rn', 'Tm', 'Ev', 'Ec'];

// ── generateIno を HTML から切り出す ────────────────────────────────────
// ⚠ 行番号で切らないこと。UI を編集すると行がずれて別の場所を掴む（実際に一度壊した）。
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

function sketchName(comps) {
  return 'UIAPrubyEeVm' + COMP_ORDER.filter(k => comps[k]).join('');
}

function fqbnFor(comps) {
  // Tn は PWMmin を使うので pwm=default が要る（ページ側の分岐と揃える）
  return `UIAP_HID:ch32v:CH32V003:pnum=V14,usb=webhid,${comps.Tn ? 'pwm=default,' : ''}opt=oslto`;
}

const generateIno = loadGenerateIno();
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'urbee-bins-'));
const built = [];

for (const t of TARGETS) {
  const comps = { ...t.comps, chip: t.chip, leds: LEDS };
  const name  = sketchName(comps);
  const ino   = generateIno(comps);
  if (typeof ino !== 'string' || ino.length === 0) throw new Error(`${t.key}: generateIno が文字列を返しません`);

  const dir = path.join(work, t.key, name);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, `${name}.ino`), ino, 'utf8');

  const outDir = path.join(work, t.key, 'out');
  process.stdout.write(`${t.key}: ${name} をビルド中...`);
  try {
    execFileSync(CLI, ['compile', '--config-file', CLI_CONFIG, '--fqbn', fqbnFor(comps),
                       '--output-dir', outDir, dir], { stdio: 'pipe' });
  } catch (e) {
    console.log('');
    console.error(`${t.key}: ビルドに失敗しました`);
    console.error(String(e.stdout ?? '') + String(e.stderr ?? ''));
    process.exit(1);
  }

  const binPath = path.join(outDir, `${name}.ino.bin`);
  if (!fs.existsSync(binPath)) throw new Error(`${t.key}: .bin が出ていません: ${binPath}`);
  const bin = fs.readFileSync(binPath);

  // 焼けないものを埋め込んでも、実機で初めて分かることになる。ここで止める。
  if (bin.length === 0)          throw new Error(`${t.key}: .bin が空です`);
  if (bin.length > FLASH_SIZE)   throw new Error(`${t.key}: .bin が大きすぎます: ${bin.length} > ${FLASH_SIZE}`);

  built.push({
    ...t,
    name,
    comps: COMP_ORDER.filter(k => comps[k]),
    size: bin.length,
    base64: bin.toString('base64'),
    binSha256: crypto.createHash('sha256').update(bin).digest('hex'),
    inoSha256: crypto.createHash('sha256').update(ino, 'utf8').digest('hex'),
  });
  console.log(` ${bin.length} B (${Math.round(bin.length / FLASH_SIZE * 100)}%)`);
}

fs.rmSync(work, { recursive: true, force: true });

// ── 生成 ────────────────────────────────────────────────────────────────
const entries = built.map(b => {
  const chunks = [];
  for (let i = 0; i < b.base64.length; i += CHUNK) chunks.push(`      '${b.base64.slice(i, i + CHUNK)}'`);
  return `  ${b.key}: {
    name: '${b.name}',
    label: '${b.label}',
    chip: '${b.chip}',
    leds: ${LEDS},
    comps: [${b.comps.map(c => `'${c}'`).join(', ')}],
    size: ${b.size},
    binSha256: '${b.binSha256}',
    inoSha256: '${b.inoSha256}',
    base64: [
${chunks.join(',\n')}
    ].join(''),
  }`;
}).join(',\n');

fs.mkdirSync(path.dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, `// このファイルは tools/embed-bins.mjs が作る。手で直さない。
//
// 中身は docs/uiapruby-ee.html の generateIno が出す .ino をビルドした .bin を
// そのまま base64 にしたもの。「書き込む」ボタンがこれを基板の Flash へ流し込む。
//
// generateIno を直したら、リポジトリのルートで
//   node tools/embed-bins.mjs
// を走らせ、この生成物を一緒にコミットすること。
//
// inoSha256 は、その構成で generateIno が出す .ino 本文の SHA-256。
// ページ側は焼く前に同じ構成で .ino を組み立て直してハッシュを比べ、
// 食い違えば「.bin を作り直し忘れている」として焼かずに止める。

/** @type {number} CH32V003 の Flash。復号後の大きさの確認に使う */
export const FLASH_SIZE = ${FLASH_SIZE};

/** LED 数は 6 つとも同じ値で焼いてある（決定 47） */
export const SKETCH_BIN_LEDS = ${LEDS};

export const SKETCH_BINS = {
${entries},
};
`, 'utf8');

console.log('');
for (const b of built) console.log(`  ${b.key.padEnd(6)} ${b.name}  ${b.size} B  ino ${b.inoSha256.slice(0, 12)}`);
console.log(`\n出力: ${path.relative(root, outPath)}`);
