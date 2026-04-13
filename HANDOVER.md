# UIAPduino WebHID Lab — 引継ぎ資料

> 作成: 2026-04-13  
> リポジトリ: https://github.com/tarosay/uiap-hid-web  
> 公開URL: https://tarosay.github.io/uiap-hid-web/

---

## 1. プロジェクト概要

UIAPduino（HID ProMicro CH32V003）と Chrome/Edge の WebHID API を使って  
双方向通信を体験・練習できる **GitHub Pages サイト**。

- `docs/` が GitHub Pages のルート（Settings → Pages → Branch: main / Folder: /docs）
- ポータル (`index.html`) から各デモページへリンク
- 各デモページは WebHID 通信 UI + IDE 風スケッチソースビューアをセットにする

---

## 2. ハードウェア仕様（重要な制約）

| 項目 | 詳細 |
|------|------|
| ボード | HID ProMicro CH32V003 KBD+Mouse / Board Version V1.4 |
| VID/PID | 0x1209 / 0xD004 |
| USB速度 | **Low Speed** → 1パケット最大 **8 バイト** |
| EP1 | マウス（4 byte / 10ms） |
| EP2 | キーボード（8 byte / 10ms） |
| EP3 | WebHID TX: デバイス→Web（8 byte / 10ms Interrupt IN） |
| EP0 | WebHID RX: Web→デバイス（Feature Report、最大 **16 バイト**） |

### EP0 Feature Report の注意点（既修正済み）

16 バイト Feature Report は内部で **8 バイト × 2 パケット** に分割される。  
`uiapusb.c` の `usb_handle_user_data()` に accumulation ロジック追加済み（`webhid_rx_offset` / `webhid_rx_expected_len`）。  
修正ファイル: `arduino_core_ch32/cores/arduino/uiapusb.c`

---

## 3. ファイル構成

```
uiap-hid-web/
├── README.md                           ← プロジェクト概要・使い方
├── HANDOVER.md                         ← この引継ぎ資料
└── docs/                               ← GitHub Pages ルート
    ├── index.html                      ← ポータル（デモカード一覧）
    ├── echo.html                       ← Echo Test デモ
    └── sketches/                       ← スケッチ置き場（Arduino IDE 風）
        └── WebHIDTest/
            └── WebHIDTest.ino
```

新しいデモを追加するたびに:
- `docs/デモ名.html` を追加
- `docs/sketches/スケッチ名/スケッチ名.ino`（同フォルダに `.h`/`.cpp` も可）を追加
- `docs/index.html` の Coming Soon カードを有効化
- `README.md` のテーブルを更新

---

## 4. デザインシステム（CSS 変数・共通スタイル）

全ページ共通のカラーパレット（`echo.html` / `index.html` は同じ値を使用）:

```css
--bg:      #1a1a2e   /* ページ背景 */
--surface: #16213e   /* カード背景 */
--border:  #0f3460   /* 枠線 */
--accent:  #7ec8e3   /* 強調色（水色） */
--text:    #e0e0e0   /* 本文 */
--muted:   #888      /* 薄いテキスト */
--green:   #4caf50   /* 成功・RX */
--orange:  #ff9800   /* 警告・SYS */
```

### 共通 HTML パターン

#### 戻るリンク（全デモページ先頭に必置）
```html
<a class="back-link" href="index.html">&#8592; UIAPduino WebHID Lab</a>
```

#### カード
```html
<div class="card">
  <h2>カードタイトル</h2>
  <!-- 内容 -->
</div>
```

#### IDE 風ソースビューア（SKETCH オブジェクト）
`echo.html` の JS を参照。ファイルを増やすには `SKETCH.files[]` に追記するだけ。

```javascript
const SKETCH = {
  name: 'スケッチ名',
  githubBase: 'https://github.com/tarosay/arduino_core_ch32/blob/main/libraries/WebHID/examples/スケッチ名/',
  files: [
    { name: 'スケッチ名.ino', lang: 'cpp', content: `...` },
    // { name: 'sub.h', lang: 'cpp', content: `...` },  ← タブが増える
  ]
};
```

---

## 5. 既存ページ詳細

### `docs/echo.html` — Echo Test

| 項目 | 値 |
|------|----|
| スケッチ | `WebHIDTest.ino` |
| 動作 | 16 バイト送信 → エコーバック受信 + 1秒カウンター |
| WebHID filter | `{ vendorId: 0x1209, productId: 0xD004, usagePage: 0xFF00, usage: 0x01 }` |
| 送信 | `device.sendFeatureReport(0, uint8Array16)` |
| 受信 | `inputreport` イベント → `e.data`（8 byte） |

---

## 6. デモページ作成テンプレート手順

1. `echo.html` をコピーして `docs/XXX.html` を作成
2. `<title>` / `<h1>` / `.subtitle` を書き換え
3. 通信 UI 部分（カード内）を目的に合わせて書き換え
4. `SKETCH` オブジェクトにスケッチ内容を記述
5. `docs/sketches/XXX/XXX.ino` に実際の .ino を作成（README 兼用）
6. `index.html` のカードを `coming-soon` → `available` に変更
7. `README.md` テーブルを更新
8. コミット＆プッシュ

---

## 7. コミット済み変更履歴

| コミット | 内容 |
|---------|------|
| `5d1d955` | IDE 風タブビューア追加・sketches サブフォルダ構成 |
| `b836089` | ポータル index.html・echo.html・README 大幅改善 |
| `74adc6d` | usagePage フィルター＋デバッグログ追加 |
| `ee5a4f1` | Echo Test ページ初版 |

---

## 8. 次に作るページ：Keyboard Practice（キーボード HID 練習）

### 概要
ブラウザの UI から文字列を送ると UIAPduino が **HID キーボードとして PC に打鍵** するチュートリアルページ。  
WebHID（EP0）で文字列を受け取り、Arduino の `Keyboard.print()` でキー入力を再現する。

### ページ構成案
| カード | 内容 |
|--------|------|
| 接続 | echo.html と同じ接続 UI |
| テキスト送信 | textarea + 送信ボタン。16 バイト単位で Feature Report を複数回送る |
| プリセット | 「Hello, World!」「Test 123」などワンクリック送信ボタン |
| 設定 | 文字間ディレイ（ms）設定 |
| 受信ログ | UIAPduino から返ってくる完了通知・ステータス表示 |
| スケッチソース | IDE 風タブビューア（KeyboardTest.ino） |

### スケッチ動作仕様
- WebHID.recv() で最大 16 バイト受信
- バイト 0: コマンド（0x01 = テキスト送信、0xFF = 設定）
- バイト 1: ペイロード長
- バイト 2〜15: ASCII テキスト（最大 14 文字 / パケット）
- 受信後 `Keyboard.print(text)` で打鍵
- EP3 で完了ステータス（0x01 + 打鍵文字数）を返信

### ファイル
- `docs/keyboard.html`
- `docs/sketches/KeyboardTest/KeyboardTest.ino`

---

## 9. 関連リポジトリ

| リポジトリ | 説明 |
|-----------|------|
| [arduino_core_ch32](https://github.com/tarosay/arduino_core_ch32) | UIAPduino コア・WebHID ファームウェア・スケッチ例 |
| [board_manager_files](https://github.com/tarosay/board_manager_files) | Arduino IDE Board Manager 用 JSON |
| [uiap-hid-web](https://github.com/tarosay/uiap-hid-web) | このリポジトリ（GitHub Pages） |
