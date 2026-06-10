// urb-lab.html から generateIno を抽出して .ino を生成する検証用スクリプト
// 使い方: node gen-ino.js Q1,Tn  → docs/sketches/UIAPrubyVmQ1Tn/UIAPrubyVmQ1Tn.ino
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'urb-lab.html'), 'utf8');

const start = html.indexOf('function generateIno(comps) {');
if (start < 0) { console.error('generateIno not found'); process.exit(1); }
const endMarker = "  return lines.join('\\n');\n}";
const end = html.indexOf(endMarker, start);
if (end < 0) { console.error('generateIno end not found'); process.exit(1); }
const fnText = html.slice(start, end + endMarker.length);

const generateIno = new Function('comps', fnText + '\nreturn generateIno(comps);');

const sel = (process.argv[2] || '').split(',').filter(s => s);
const comps = { Q1: false, Tn: false, Pw: false, Ad: false, Us: false, I2: false, Rn: false, Sv: false };
for (const k of sel) {
  if (!(k in comps)) { console.error(`unknown component: ${k}`); process.exit(1); }
  comps[k] = true;
}

const code = ['Q1','Tn','Pw','Ad','Us','I2','Rn','Sv'].filter(k => comps[k]).join('');
const sketchName = code ? `UIAPrubyVm${code}` : 'UIAPrubyVm';
const dir = path.join(__dirname, '..', 'docs', 'sketches', sketchName);
fs.mkdirSync(dir, { recursive: true });
const file = path.join(dir, `${sketchName}.ino`);
fs.writeFileSync(file, generateIno(comps));
console.log(`generated: ${file}`);
