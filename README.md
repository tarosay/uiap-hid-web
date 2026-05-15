# UIAPduino WebHID Lab

UIAPduino（HID ProMicro CH32V003）と Chrome の **WebHID API** を使った  
双方向通信のデモ・練習ページ集です。

GitHub Pages で公開しており、ブラウザとマイコンがドライバなしで直接やり取りできる  
WebHID の仕組みをさまざまなサンプルで体験できます。

🌐 **サイト URL**: https://tarosay.github.io/uiap-hid-web/

---

## デバッグ / ユーティリティ

| ページ | スケッチ例 | 状態 |
|--------|-----------|------|
| [HID Console](https://tarosay.github.io/uiap-hid-web/hid-console.html) | `HidPrint.ino` | ✅ 公開中 |
| [HID-Serial Bridge](https://tarosay.github.io/uiap-hid-web/hid-serial-bridge.html) | `HidBridgeTest.ino` | ✅ 公開中 |

---

## UIAPduino SD（SD カード連携）

| ページ | スケッチ例 | 状態 |
|--------|-----------|------|
| [SD Filemanager](https://tarosay.github.io/uiap-hid-web/sd-file.html) | `WebHID_SD.ino` | ✅ 公開中 |
| [MIDI → MIDB Converter](https://tarosay.github.io/uiap-hid-web/midi-to-midb.html) | `MidbPlayer.ino` | ✅ 公開中 |

---

## デモページ一覧

| ページ | スケッチ例 | 状態 |
|--------|-----------|------|
| [Echo Test](https://tarosay.github.io/uiap-hid-web/echo.html) | `WebHIDTest.ino` | ✅ 公開中 |
| [Keyboard Practice](https://tarosay.github.io/uiap-hid-web/keyboard.html) | `KeyboardPractice.ino` | ✅ 公開中 |
| [Keyboard Practice 2 — switch 文](https://tarosay.github.io/uiap-hid-web/keyboard2.html) | `KeyboardSwitch.ino` | ✅ 公開中 |
| [Mouse Practice](https://tarosay.github.io/uiap-hid-web/mouse.html) | `MousePractice.ino` | ✅ 公開中 |
| [Mouse Practice 2 — GetPos](https://tarosay.github.io/uiap-hid-web/mouse2.html) | `MousePractice2.ino` | ✅ 公開中 |
| [Maze Solver](https://tarosay.github.io/uiap-hid-web/maze-solver.html) | `MazeSolver.ino` | ✅ 公開中 |
| [Snake Solver](https://tarosay.github.io/uiap-hid-web/snake.html) | `SnakeSolver.ino` | ✅ 公開中 |
| Snake Game | 準備中 | 🔜 Coming Soon |
| Rock Dodge | 準備中 | 🔜 Coming Soon |
| LED Controller | 準備中 | 🔜 Coming Soon |
| Mini Oscilloscope | 準備中 | 🔜 Coming Soon |

---

## 各ページの機能

### HID-Serial Bridge（デバッグ / ユーティリティ）
- UIAPduino の **WebHID 通信**を **Web Serial API** 経由で仮想 COM ポートへ中継するブリッジ
- **com0com**（仮想 COM ペアドライバ）で作った COM ペアを使い、TeraTerm・Python・C# などの外部アプリと UIAPduino をシリアル通信感覚でつなげる
- HID と Serial 両方の接続が完了すると**ブリッジを自動開始**
- Monitor ON/OFF 切替（OFF 時はブリッジ処理に専念、SYSTEM/ERROR ログは常時表示）
- Serial → HID：16 バイトバッファリング + 30 ms タイムアウトフラッシュ（端数パケット対応）
- Feature Report サイズをデバイスの HID ディスクリプタから**自動検出**
- ページ内にセットアップガイド（com0com インストール・設定・使い方）を掲載

### HID Console（デバッグ / ユーティリティ）
- Arduino の `hid.Print()` / `hid.Println()` / `hid.Clear()` 出力をブラウザでリアルタイム表示（緑端末 UI）
- DEC / HEX / ASCII モードで任意データを送信 → Arduino 側 `hid.Recv()` で受け取り
- Feature Report サイズをデバイスの HID ディスクリプタから**自動検出**（16 / 32 バイトどちらのスケッチでも動作）
- バイトプレビューグリッドで送信内容を確認しながら入力
- 送受信ログ（タイムスタンプ付き TX / RX）

### Echo Test
- **Feature Report 送信**（Web → UIAPduino）: 接続時にデバイスから Feature Report サイズを自動検出し、グリッドを動的生成
- **Input Report 受信ログ**（UIAPduino → Web）: エコーバックと 1 秒ごとのカウンターを表示
- **スケッチソースビューア**: `WebHIDTest.ino` をページ内でシンタックスハイライト表示

### Keyboard Practice
- UIAPduino をキーボード HID として動かす 5 ステップのチュートリアル
- 各ステップのコードブロックをコメントアウトから外して書き込む練習形式
- ワークエリア（textarea）に UIAPduino がタイプした内容が流れ込み、正解と一致すると ✓ 表示
- **Step 1**: `Keyboard.print()` で文字列を入力
- **Step 2**: カーソルキー（`KEY_LEFT_ARROW`）で移動して文字を修正
- **Step 3**: `KEY_BACKSPACE` で文字を削除
- **Step 4**: `KEY_RETURN` で改行
- **Step 5**: `KEY_HOME` で行頭に移動して文字を挿入

### Keyboard Practice 2 — switch 文
- Practice 1 と同じ 5 ステップを **switch 文**で書き直す練習
- 1 回書き込めばブラウザからステップを自由に切り替えて何度でも実行できる
- switch 文・`case`・`break` の使い方を実際に動かしながら学べる

### Mouse Practice
- HID マウスの基本操作を **固定座標**で練習するページ
- クリック・四角形頂点クリック・ドラッグ・円ドラッグを段階的に確認できる
- `.ino` に座標を直接埋め込む形式で、マウス HID の仕組みを体験できる
- `hidPrint()` などのスタンドアロン関数で HID コンソールへ出力

### Mouse Practice 2 — GetPos（C++ クラス設計の練習）
- `hid.GetPos(x, y)` でカーソル座標を動的取得し、相対移動量を計算する発展版
- ブラウザウィンドウの位置・サイズに依存せず動作
- Mouse Practice の `hidPrint()` スタンドアロン関数を **Hid クラスのメソッド**として実装し直す C++ OOP の練習
- `hid.Print()` / `hid.Println()` / `hid.GetPos()` / `hid.Recv()` を持つ `Hid` クラスを `Hid.h` / `Hid.cpp` に分離

### SD Filemanager（SD カードファイル管理）
- UIAPduino SD に挿した SD カードの内容をブラウザからリスト表示・ファイルダウンロード
- `SDmin.h`（軽量 SD ライブラリ）を使用し、LFN（長いファイル名）にも対応
- ページ上のソースビューアで `WebHID_SD.ino` を確認・ダウンロード可能

### MIDI → MIDB Converter（SAM2695 用 MIDI プレイヤー）
- MIDI ファイル（.mid）をドラッグするだけで `.midb` バイナリに変換・ダウンロード
- サーバーへのアップロード不要（ブラウザ完結）
- `.midb` 形式: `MIDB` マジック + `[uint16_t 遅延ms][uint8_t データ長][MIDIバイト列]` の繰り返し
- 変換後の `.midb` を SD カードに入れ、`MidbPlayer.ino` を書き込んだ UIAPduino SD に接続した SAM2695 で再生
- MIDI Format 0/1 対応・マルチトラックマージ・テンポチェンジ処理
- SAM2695（GM/GS MIDI 音源 IC）の仕様説明をページ下部に掲載

### Snake Solver（経路探索アルゴリズムの練習）
- UIAPduino が 16×16 の盤面で岩を避けながら指定ステップ数伸び続けるレベル制ゲーム
- Web ページがゲームを進行し、UIAPduino は `snake.sendDir(dx, dy)` で次の方向を返すだけ
- 実装済みアルゴリズム: BFS 最大空間優先（岩があっても最も広い空間へ進む）
- レベルクリアごとに岩が 3 個増加、失敗しても同レベルで岩・スタート位置を変えてリトライ
- `SnakeHID` クラス（`SnakeHID.h`）に通信をカプセル化、C++ クラス設計の練習も兼ねる

### Maze Solver（迷路探索アルゴリズムの練習）
- Web ページが生成した迷路を UIAPduino が **WebHID 経由**でリアルタイムに探索するゲーム
- `maze.sense(x, y)` / `maze.moveTo(x, y)` の 2 関数だけでアルゴリズムを実装できる
- スケッチに実装済みのアルゴリズム:
  - **右手法**（Wall Follower）
  - **DFS**（深さ優先探索）
  - **Greedy DFS**（マンハッタン距離でゴール方向を優先する DFS）
  - **BFS**（幅優先探索・最短経路保証）
- WebHID 通信の詳細を `MazeHID` クラス（`MazeHID.h`）に分離し、C++ クラス設計の練習も兼ねる
- Web 側でも BFS による **回答表示**（ゴールまでの最短経路をキャンバス上に緑表示）が可能

### 全ページ共通
- スケッチソースビューア（シンタックスハイライト・コピー・ダウンロード・GitHub リンク）
- WebHID 接続 / 切断（USB 抜き差しも `navigator.hid` レベルで自動検知）
- Feature Report サイズをデバイスの HID ディスクリプタから自動検出（スケッチ切り替えに対応）
- ログパネル（TX / RX / SYS）

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
- **Chrome** または **Edge**（WebHID / Web Serial API 対応ブラウザ）
- **com0com**（HID-Serial Bridge を使う場合のみ）— 仮想 COM ペアドライバ（Windows 用）

---

## 使い方

1. Arduino IDE を開き、ボードを **「HID ProMicro CH32V003」** に設定
2. Tools → USB → 使用するスケッチに合わせたモードを選択（例: **Keyboard+Mouse+WebHID**）
3. デモページのスケッチソースをコピーまたはダウンロードして Arduino IDE に貼り付け、書き込む
   （または `File → Examples → WebHID → WebHIDTest` から直接開く）
4. Chrome / Edge でサイトを開き、デモページへ移動
5. 「デバイスを選択して接続」ボタンから UIAPduino を選択
   （**usagePage: 0xFF00 でフィルタされるため、WebHID コレクションのみ表示されます**）
6. ページ上の UI でデータ送受信を試す

---

## WebHID 通信の概要

```
[Web ブラウザ]                              [UIAPduino]
 sendFeatureReport()  ─── EP0 ──────────►  usb_handle_user_data()
                         Control Transfer        ↓
                         16 or 32 バイト    WebHID.recv()  /  hid.Recv()

 inputreport イベント  ◄─── EP3 ──────────  WebHID.send()  /  hid.Print()
                         Interrupt IN           ↑
                         8 バイト / 10ms   usb_handle_user_in_request()
```

| 方向 | エンドポイント | プロトコル | サイズ |
|------|--------------|-----------|--------|
| Web → デバイス | EP0 | Control Transfer (Feature Report) | スケッチの `recv` バッファサイズに依存 |
| デバイス → Web | EP3 | Interrupt IN | 8 バイト / パケット |

> **Feature Report サイズについて**  
> スケッチ内の `WebHID.recv(buf, sizeof(buf))` に渡すバッファサイズが、  
> USB ディスクリプタに登録される Feature Report サイズになります。  
> ブラウザは接続時にディスクリプタを読み取り、そのサイズに合わせた `sendFeatureReport()` のみ受け付けます。  
> 各ページはデバイス接続時にサイズを自動検出し、UI に反映します。
>
> | スケッチ | Feature Report サイズ |
> |----------|----------------------|
> | `WebHIDTest.ino` / `HidBridgeTest.ino` | 16 バイト |
> | `WebHID_SD.ino` / `MidbPlayer.ino` | 32 バイト |

> **USB Low Speed の制約**  
> Interrupt エンドポイントの最大パケットサイズは **8 バイト**固定。  
> 16 バイト以上を返す場合は 8 バイトずつ複数回 `WebHID.send()` を呼ぶ。

### Print プロトコル（マーカー 0x50）

`hid.Print()` / `hid.Println()` / `hid.Clear()` は EP3 Input Report で以下の形式を使用：

| byte | 内容 |
|------|------|
| 0 | `0x50`（マーカー） |
| 1 | flags: `0x80`=続きあり / `0x02`=改行 / `0x04`=クリア |
| 2〜7 | テキスト本体（最大 6 文字、残りは 0 埋め） |

---

## ファイル構成

```
docs/                           ← GitHub Pages のルート
  index.html                    ← ポータルページ（デモ一覧・WebHID 解説）
  echo.html                     ← Echo Test デモ
  keyboard.html                 ← Keyboard Practice（コメント外し形式）
  keyboard2.html                ← Keyboard Practice 2（switch 文形式）
  mouse.html                    ← Mouse Practice（固定座標）
  mouse2.html                   ← Mouse Practice 2（GetPos / Hid クラス）
  maze-solver.html              ← Maze Solver（迷路探索アルゴリズムの練習）
  snake.html                    ← Snake Solver（経路探索アルゴリズムの練習）
  hid-console.html              ← HID Console（デバッグ / ユーティリティ）
  hid-serial-bridge.html        ← HID-Serial Bridge（com0com 経由シリアル中継）
  sd-file.html                  ← SD Filemanager（SD カードファイル管理）
  midi-to-midb.html             ← MIDI → MIDB Converter（SAM2695 用）
  images/
    com0com.jpg                 ← com0com Setup GUI スクリーンショット
  sketches/                     ← スケッチ置き場（Arduino IDE 風サブフォルダ）
    WebHIDTest/
      WebHIDTest.ino            ← Echo Test スケッチ（16 バイト Feature Report）
    HidBridgeTest/
      HidBridgeTest.ino         ← HID-Serial Bridge テストスケッチ（エコーバックのみ）
    KeyboardPractice/
      KeyboardPractice.ino      ← Keyboard Practice スケッチ（正解）
    KeyboardSwitch/
      KeyboardSwitch.ino        ← Keyboard Practice 2 スケッチ（正解）
    MousePractice/
      MousePractice.ino         ← Mouse Practice スケッチ
    MousePractice2/
      MousePractice2.ino        ← Mouse Practice 2 スケッチ
      Hid.h                     ← Hid クラス宣言
      Hid.cpp                   ← Hid クラス実装
    HidPrint/
      HidPrint.ino              ← HID Console デモスケッチ
      Hid.h                     ← Hid クラス宣言
      Hid.cpp                   ← Hid クラス実装
    MazeSolver/
      MazeSolver.ino            ← 迷路探索アルゴリズム（メインスケッチ）
      MazeHID.h                 ← MazeHID クラス（WebHID 通信を担当）
    SnakeSolver/
      SnakeSolver.ino           ← スネーク経路アルゴリズム（メインスケッチ）
      SnakeHID.h                ← SnakeHID クラス（WebHID 通信を担当）
    WebHID_SD/
      WebHID_SD.ino             ← SD Filemanager スケッチ（32 バイト Feature Report）
    MidbPlayer/
      MidbPlayer.ino            ← MIDI (.midb) 再生スケッチ（32 バイト Feature Report）
      UIAPSerial.h              ← 軽量 USART1 クラス（宣言）
      UIAPSerial.cpp            ← 軽量 USART1 クラス（実装）
README.md
```

---

## 新しいデモページを追加するには

1. `docs/` に新しい HTML ファイルを作成（例: `docs/newdemo.html`）
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
