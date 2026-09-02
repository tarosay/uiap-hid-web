# Blockly（vendor）

URB Block Lab（`docs/uiapruby-block.html`）が使う Blockly の配布ファイル。

- **版**: 13.2.1
- **入手元**: npm の `blockly` パッケージ（`npm pack blockly`）
- **ライセンス**: Apache-2.0 — https://github.com/google/blockly

## 置いてあるもの

| | 中身 |
|---|---|
| `blockly_compressed.js` | コア。UMD なので `<script>` で読める |
| `blocks_compressed.js` | 標準ブロック（ループ・論理・変数・計算・文字） |
| `msg/ja.js` | 日本語。**コアの後に読む**と `Blockly.Msg` へ流し込まれる |
| `media/` | カーソル・スプライト・効果音。`inject` の `media` に渡す |

Ruby ジェネレータは Blockly には無いので、ページ側で書く。

## 更新のしかた

```
npm pack blockly
tar xzf blockly-<版>.tgz
```

展開した `package/` から上の4つを上書きコピーし、この README の版を直す。
npm ビルドはページ側に持ち込まない（`docs/` は単一 HTML ＋ vendor したファイルだけで動く）。
