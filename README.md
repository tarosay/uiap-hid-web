# UIAPduino WebHID Lab

UIAPduino（HID ProMicro CH32V003）と Chrome の **WebHID API** を使った  
双方向通信のデモ・練習ページ集です。

GitHub Pages で公開しており、ブラウザとマイコンがドライバなしで直接やり取りできる  
WebHID の仕組みをさまざまなサンプルで体験できます。

🌐 **サイト URL**: https://tarosay.github.io/uiap-hid-web/

---

## デモページ一覧

| ページ | スケッチ例 | 状態 |
|--------|-----------|------|
| [Echo Test](https://tarosay.github.io/uiap-hid-web/echo.html) | `WebHIDTest.ino` | ✅ 公開中 |
| Maze Solver | 準備中 | 🔜 Coming Soon |
| Snake Game | 準備中 | 🔜 Coming Soon |
| Rock Dodge | 準備中 | 🔜 Coming Soon |
| LED Controller | 準備中 | 🔜 Coming Soon |
| Mini Oscilloscope | 準備中 | 🔜 Coming Soon |

---

## Echo Test ページの機能

- **Feature Report 送信**（Web → UIAPduino）: 16 バイトを自由に入力して送信
- **Input Report 受信ログ**（UIAPduino → Web）: エコーバックと 1 秒ごとのカウンターを表示
- **スケッチソースビューア**: `WebHIDTest.ino` をページ内でシンタックスハイライト表示
  - ワンクリックコピー
  - `.ino` ファイルとして直接ダウンロード
  - GitHub ソースページへのリンク

---

## 必要なもの

### ハードウェア
- **UIAPduino** (HID ProMicro CH32V003) — Board Version V1.4 以降
- USB ケーブル（デバイスと PC を接続）

### ソフトウェア
- **Arduino IDE** 2.x
- **UIAPduino ボードパッケージ** — Board Manager に以下の URL を追加してインストール
  ```
  https://raw.githubusercontent.com/tarosay/board_manager_files/main/package_uiap_hid_index.json
  ```
- **Chrome** または **Edge**（WebHID API 対応ブラウザ）

---

## 使い方

1. Arduino IDE を開き、ボードを **「HID ProMicro CH32V003 KBD+Mouse」** に設定
2. Tools → Board Version → **V1.4** 以降を選択
3. デモページのスケッチソースをコピーまたはダウンロードして Arduino IDE に貼り付け、書き込む
   （または `File → Examples → WebHID → WebHIDTest` から直接開く）
4. Chrome / Edge でサイトを開き、デモページへ移動
5. 「デバイスを選択して接続」ボタンから UIAPduino を選択
6. ページ上の UI でデータ送受信を試す

---

## WebHID 通信の概要

```
[Web ブラウザ]                              [UIAPduino]
 sendFeatureReport()  ─── EP0 ──────────►  usb_handle_user_data()
                         Control Transfer        ↓
                         最大 16 バイト       WebHID.recv()

 inputreport イベント  ◄─── EP3 ──────────  WebHID.send()
                         Interrupt IN           ↑
                         8 バイト / 10ms   usb_handle_user_in_request()
```

| 方向 | エンドポイント | プロトコル | サイズ上限 |
|------|--------------|-----------|-----------|
| Web → デバイス | EP0 | Control Transfer (Feature Report) | 16 バイト |
| デバイス → Web | EP3 | Interrupt IN | 8 バイト / パケット |

> **USB Low Speed の制約**  
> Interrupt エンドポイントの最大パケットサイズは **8 バイト**固定。  
> 16 バイト以上を返す場合は 8 バイトずつ複数回 `WebHID.send()` を呼ぶ。  
> Feature Report の 16 バイトは EP0 で 8 バイト × 2 パケットに自動分割される。

---

## ファイル構成

```
docs/                           ← GitHub Pages のルート
  index.html                    ← ポータルページ（デモ一覧・WebHID 解説）
  echo.html                     ← Echo Test デモ（IDE 風ソースビューア付き）
  sketches/                     ← スケッチ置き場（Arduino IDE 風サブフォルダ）
    WebHIDTest/
      WebHIDTest.ino            ← Echo Test スケッチ
  （今後デモが増えるたびに追加）
README.md
```

---

## 新しいデモページを追加するには

1. `docs/` に新しい HTML ファイルを作成（例: `docs/snake.html`）
2. ページ上部に戻るリンクを追加
   ```html
   <a href="index.html">← UIAPduino WebHID Lab</a>
   ```
3. スケッチを `docs/sketches/スケッチ名/スケッチ名.ino`（以降は同フォルダに `.h` / `.cpp` も追加可）として配置
4. `docs/index.html` の Coming Soon カードをリンク付きカードに更新
5. `README.md` のデモ一覧テーブルを更新
6. コミット＆プッシュ → GitHub Pages に自動反映

---

## 関連リポジトリ

| リポジトリ | 説明 |
|-----------|------|
| [arduino_core_ch32](https://github.com/tarosay/arduino_core_ch32) | UIAPduino Arduino コア（WebHID ファームウェア含む） |
| [board_manager_files](https://github.com/tarosay/board_manager_files) | Board Manager 用インデックス JSON |

---

## ライセンス

MIT License — 詳細は [LICENSE](LICENSE) を参照
