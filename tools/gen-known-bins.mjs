#!/usr/bin/env node
//
// 配ったことのあるファームの一覧を作る。「⚙ 準備」の「基板のファームを調べる」が、
// 基板から読み出した Flash をこの一覧と照合して、どの版が入っているかを出す。
//
//   git の履歴にある docs/lib/flasher/blockBin.js と sketchBins.js の全版
//   ＋ 作業ツリーの版（まだコミットしていないビルド）
//     → docs/lib/flasher/knownBins.js
//
// ── なぜ履歴から作るのか ────────────────────────────────────────────────
// 基板に入っているのは、焼いた時点のページが持っていた bin。ページを更新しても
// 基板の中身は変わらないので、今の bin だけと比べると古い版を見分けられない。
// 過去に配った bin のハッシュは git に全部残っているので、そこから拾う。
//
// ── なぜ binSha256 でまとめるのか ───────────────────────────────────────
// .ino の文面だけが変わって .bin が同じになることがある（コメントの差など）。
// 基板から見て区別できるのは .bin だけなので、同じ .bin は 1 件にまとめ、
// 出てきたコミットを並べる。
//
// build-block-bin.mjs と embed-bins.mjs が最後にこれを呼ぶ。手で走らせてもよい:
//   node tools/gen-known-bins.mjs

import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';

const root    = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..');
const outPath = path.join(root, 'docs', 'lib', 'flasher', 'knownBins.js');

const SOURCES = [
  { file: 'docs/lib/flasher/blockBin.js',   page: 'URB Block Lab' },
  { file: 'docs/lib/flasher/sketchBins.js', page: 'URB EE Lab' },
];

const git = args => execFileSync('git', args, { cwd: root, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });

// bin 1 件ぶんの { name, label, size, binSha256 } を拾う。
// base64 の行は読まない（ハッシュとサイズがあれば照合できる）。
function parseBins(text) {
  const out = [];
  const re = /name:\s*['"]([^'"]+)['"][\s\S]*?label:\s*['"]([^'"]+)['"][\s\S]*?size:\s*(\d+)[\s\S]*?binSha256:\s*['"]([0-9a-f]{64})['"]/g;
  let m;
  while ((m = re.exec(text))) out.push({ name: m[1], label: m[2], size: +m[3], binSha256: m[4] });
  return out;
}

const byHash = new Map();
function add(bin, page, commit) {
  let e = byHash.get(bin.binSha256);
  if (!e) {
    e = { binSha256: bin.binSha256, size: bin.size, page, name: bin.name, label: bin.label, commits: [] };
    byHash.set(bin.binSha256, e);
  }
  // ラベルは新しい版の言い方に揃える（「CAT24M01WI」→「CAT24M01WI 0x50」のように後から詳しくなった）
  e.label = bin.label;
  if (commit && !e.commits.some(c => c.hash === commit.hash)) e.commits.push(commit);
}

for (const src of SOURCES) {
  // 古い順に積む。date は最初に配った日として使う
  const log = git(['log', '--reverse', '--format=%h%x09%ad%x09%s', '--date=short', '--', src.file]).trim();
  for (const line of log ? log.split('\n') : []) {
    const [hash, date, subject] = line.split('\t');
    const text = git(['show', `${hash}:${src.file}`]);
    for (const bin of parseBins(text)) add(bin, src.page, { hash, date, subject });
  }
  // まだコミットしていないビルド。コミットしてから作り直せば、コミットの情報に置き換わる
  const wt = fs.readFileSync(path.join(root, src.file), 'utf8');
  for (const bin of parseBins(wt)) {
    if (!byHash.has(bin.binSha256)) add(bin, src.page, null);
  }
}

const entries = [...byHash.values()];
const js = `// このファイルは tools/gen-known-bins.mjs が作ります。手で編集しないでください。
//
// これまでに配ったファームの一覧（git の履歴にある blockBin.js / sketchBins.js の全版）。
// 「⚙ 準備」の「基板のファームを調べる」が、基板から読み出した Flash の先頭 size バイトの
// SHA-256 をここの binSha256 と比べて、どの版が入っているかを出す。
//
// commits が空のものは、まだコミットしていないビルド。

export const KNOWN_BINS = ${JSON.stringify(entries, null, 2)};
`;

fs.writeFileSync(outPath, js, 'utf8');
console.log(`${path.relative(root, outPath)} を書き出しました（${entries.length} 件）`);
