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
| [HID-Serial Bridge](https://tarosay.github.io/uiap-hid-web/hid-serial-bridge.html) | `HidMonitorTest.ino` / `HidBridgeTest.ino` | ✅ 公開中 |

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
- UIAPduino の **HID通信**を **WebCom通信**（Web Serial API）経由で仮想 COM ポートへ中継するブリッジ
- **com0com**（仮想 COM ペアドライバ）で作った VirtualCOM 通信ペアを使い、Arduino IDE シリアルモニタ／TeraTerm・Python・C# などと UIAPduino をシリアル通信感覚でつなげる
- HID と WebCom 両方の接続が完了すると**ブリッジを自動開始**
- **HID受信モード**: Protocol モード（デフォルト）と Raw Binary モードを切替可能
  - **Protocol モード**: `0x50`/`0x52` パケットをデコードしてテキスト・バイナリを WebCom 通信へ転送（`HidMonitorTest` スケッチ向け）
  - **Raw Binary モード**: 全バイトをそのまま WebCom 通信へ転送（`HidBridgeTest` スケッチ向け）
- モード切替でスケッチビューアが対応スケッチに自動切り替わる
- WebCom 通信 → HID通信：常に Raw。Feature Report サイズ単位でバッファリング + 30 ms タイムアウトフラッシュ
- HID 接続時に接続通知（`0x01` Feature Report）を送信し UIAPduino の `WaitAvailable()` を解除
- Feature Report サイズをデバイスの HID ディスクリプタから**自動検出**
- ページ内にセットアップガイド（com0com インストール・設定・使い方）とプロトコル仕様を掲載

### HID Console（デバッグ / ユーティリティ）
- Arduino の `hid.Print()` / `hid.Println()` / `hid.Clear()` 出力をブラウザでリアルタイム表示（緑端末 UI）
- HID 接続時に接続通知（`0x01` Feature Report）を送信し UIAPduino の `WaitAvailable()` を解除
- `0x53` Ready 通知を受信してログ表示
- DEC / HEX / ASCII モードで任意データを送信 → Arduino 側 `hid.Recv()` で受け取り
- Feature Report サイズをデバイスの HID ディスクリプタから**自動検出**
- バイトプレビューグリッドで送信内容を確認しながら入力
- 送受信ログ（タイムスタンプ付き TX / RX）
- ページ内にプロトコル仕様を掲載

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
- ファイル・**空のフォルダ**の削除に対応（コマンド `CMD_DEL` / `CMD_RMDIR`）
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
- スケッチソースビューア（シンタックスハイライト・コピー・**ZIPダウンロード**・GitHub リンク）
  - 「ZIPダウンロード」ボタンで全ファイルを `スケッチ名/スケッチ名.ino` 構成の ZIP として取得。Arduino IDE でそのまま開ける
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
3. デモページの「ZIPダウンロード」ボタンで ZIP を取得し展開するか、コピーして Arduino IDE に貼り付け、書き込む
   （ZIP を展開すると `スケッチ名/スケッチ名.ino` の構成になり、Arduino IDE でそのまま開ける）
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
                         32 バイト（v1.1.5 以降）  WebHID.recv()  /  hid.Recv()

 inputreport イベント  ◄─── EP3 ──────────  WebHID.send()  /  hid.Print()
                         Interrupt IN           ↑
                         8 バイト / 10ms   usb_handle_user_in_request()
```

| 方向 | エンドポイント | プロトコル | サイズ |
|------|--------------|-----------|--------|
| Web → デバイス | EP0 | Control Transfer (Feature Report) | スケッチの `recv` バッファサイズに依存 |
| デバイス → Web | EP3 | Interrupt IN | 8 バイト / パケット |

> **Feature Report サイズについて**  
> arduino_core_ch32 **v1.1.5** で Feature Report サイズが **16 → 32 バイト** に変更されました。  
> スケッチ内の `WebHID.recv(buf, sizeof(buf))` に渡すバッファサイズが、  
> USB ディスクリプタに登録される Feature Report サイズになります。  
> ブラウザは接続時にディスクリプタを読み取り、そのサイズに合わせた `sendFeatureReport()` のみ受け付けます。  
> 各ページはデバイス接続時にサイズを自動検出し、UI に反映します。
>
> | ボードパッケージ | Feature Report サイズ |
> |-----------------|----------------------|
> | arduino_core_ch32 v1.1.3 以前 | 16 バイト |
> | arduino_core_ch32 v1.1.5 以降 | 32 バイト |

> **USB Low Speed の制約**  
> Interrupt エンドポイントの最大パケットサイズは **8 バイト**固定。  
> 16 バイト以上を返す場合は 8 バイトずつ複数回 `WebHID.send()` を呼ぶ。

### 通信インターフェイス定義

| 用語 | 区間 | 技術 |
|------|------|------|
| **HID通信** | UIAPduino ↔ ブラウザ | USB HID（WebHID API）|
| **WebCom通信** | ブラウザ ↔ 仮想COMポート | Web Serial API |
| **VirtualCOM通信** | 仮想COMペア内部（COM8 ↔ COM9）| com0com |
| **COMモニタ通信** | 仮想COMポート ↔ モニタアプリ | シリアル通信 |

### マーカーバイト一覧

| マーカー | 名称 | 方向 |
|---------|------|------|
| `0x50` | Print | UIAPduino → ブラウザ |
| `0x51` | GetPos | 双方向 |
| `0x52` | Write | UIAPduino → ブラウザ |
| `0x53` | Ready | UIAPduino → ブラウザ |
| `0x01` | 接続通知 | ブラウザ → UIAPduino |

### プロトコル仕様

**Print（`0x50`）** — `hid.Print()` / `hid.Println()` / `hid.Clear()`

| byte | 内容 |
|------|------|
| 0 | `0x50`（マーカー） |
| 1 | flags: `0x80`=続きあり / `0x04`=クリア |
| 2〜7 | テキスト本体（最大 6 文字、`0x00` 埋め） |

※ 改行は `0x0A` をテキストに含めて送信（NL フラグは廃止）

**Write（`0x52`）** — `hid.Write()`

| byte | 内容 |
|------|------|
| 0 | `0x52`（マーカー） |
| 1 | flags: `0x80`=続きあり |
| 2 | ペイロード長（0〜5） |
| 3〜7 | バイナリペイロード |

**Ready（`0x53`）** — `hid.Ready()`（任意使用）

| byte | 内容 |
|------|------|
| 0 | `0x53`（マーカー） |
| 1〜7 | `0x00`（予約） |

**接続通知（`0x01`）** — HID接続時にブラウザが送信

| byte | 内容 |
|------|------|
| 0 | `0x01`（接続通知マーカー） |
| 1〜 | `0x00`（未使用） |

UIAPduino 側の `hid.WaitAvailable()` がこれを検出して処理を開始する。

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
      HidBridgeTest.ino         ← HID-Serial Bridge テストスケッチ（Raw エコーバック）
      Hid.h                     ← Hid クラス宣言
      Hid.cpp                   ← Hid クラス実装
    HidMonitorTest/
      HidMonitorTest.ino        ← HID-Serial Bridge Protocol モード用スケッチ
      Hid.h                     ← Hid クラス宣言
      Hid.cpp                   ← Hid クラス実装
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

## Hid クラス API

`HidBridgeTest` / `HidMonitorTest` / `HidPrint` スケッチで共通して使用する `Hid` クラスのメソッド一覧。

| メソッド | プロトコル | 説明 |
|---------|-----------|------|
| `Print(const char* s)` | `0x50` | テキスト送信（改行なし）|
| `Print(int v)` | `0x50` | 整数送信（改行なし）|
| `Println(const char* s = "")` | `0x50` | テキスト送信 + 改行（`0x0A`）|
| `Println(int v)` | `0x50` | 整数送信 + 改行（`0x0A`）|
| `Clear()` | `0x50` CLEAR | 画面クリア |
| `Write(const uint8_t* buf, uint8_t len)` | `0x52` | バイナリ送信（`0x00` 含むデータも正確に転送）|
| `Send(const uint8_t* buf, uint8_t len)` | Raw | マーカーなし Raw 送信 |
| `Ready()` | `0x53` | 準備完了通知（任意）|
| `Recv(uint8_t* buf, uint8_t maxLen)` | — | Feature Report 受信。戻り値: 受信バイト数 |
| `WaitAvailable(uint32_t timeoutMs = 0)` | — | Feature Report 到着まで待つ。**データを消費しない**。`0` = 無限待ち |
| `GetPos(int16_t& x, int16_t& y)` | `0x51` | カーソル座標取得。500 ms タイムアウト |

> **LTO（Link Time Optimization）について**  
> CH32V003 は Flash 16 KB。LTO が有効なため、スケッチで使用しないメソッドはビルド時に自動削除されます。

---

## 変更履歴

### 2026-05-22

**Feature Report サイズ変更（16 → 32 バイト）対応**  
arduino_core_ch32 v1.1.5 で Feature Report サイズが 16 → 32 バイトに変更されたことに伴う HTML 側の修正。

- `sendFeatureReport()` に渡す `Uint8Array` のサイズを 16 → 32 バイトに修正  
  対象: `keyboard.html` / `keyboard2.html` / `mouse.html` / `mouse2.html` / `snake.html` / `maze-solver.html`
- 各ページのプロトコル仕様・説明テキストに記載されていた "16 バイト" を "32 バイト" に修正  
  対象: 上記 6 ページ + `hid-console.html` / `hid-serial-bridge.html` / `sd-file.html`（計 13 箇所）

**Maze Solver — EP0 並行送信バグ修正**

- `startGame()` を `async` 化し、連続する 3 回の `sendFeatureReport()` を逐次 `await` するよう修正
- `inputreport` イベントハンドラを非同期シリアルキュー（`enqueueCommand` / `_flushCmdQueue`）経由に変更し、EP0 の並行アクセスを防止
- `sendFeature()` に 3 回リトライ（20 ms 間隔）を追加

**Snake Solver — setInterval 並行実行バグ修正**

- `setInterval` + `async tick()` の組み合わせで前の tick 完了前に次の tick が起動するバグを修正
- `tickBusy` フラグを追加して tick の重複実行を防止
- `sendFeature()` に 3 回リトライ（20 ms 間隔）を追加

**SD Filemanager — ディレクトリ削除機能追加**

- ファイル一覧のフォルダ行に削除ボタンを追加
- 新コマンド `CMD_RMDIR (0x0A)` をブラウザ・スケッチ両側に実装（空ディレクトリのみ削除可能）
- `WebHID_SD.ino` に `CMD_RMDIR` ケースを追加
- `SDmin.h` に `sm_rmdir()` を追加（内部共通関数 `_sm_del_entry()` で Flash 増加を約 20 バイトに最小化）

---

### 2026-05-21（スケッチ ZIPダウンロード対応）

**全ページのスケッチダウンロードを ZIP 形式に統一**
- 「ダウンロード」ボタンを「ZIPダウンロード」に変更（全 9 ページ）
- JSZip ライブラリ（CDN）を追加し、クライアントサイドで ZIP を生成
- ZIP は `スケッチ名/` フォルダ内に全ファイルを格納する Arduino IDE 対応構造
  - 例: `HidPrint.zip` → 展開すると `HidPrint/HidPrint.ino`, `HidPrint/Hid.h`, `HidPrint/Hid.cpp`
  - そのまま Arduino IDE で開いてビルド可能

---

### 2026-05-21

**通信インターフェイスの用語定義**  
「Serial」という言葉の混同を避けるため、通信区間ごとに用語を定義した。  
HID通信 / WebCom通信 / VirtualCOM通信 / COMモニタ通信

**Hid クラスの大幅拡充（HidBridgeTest / HidMonitorTest / HidPrint 共通）**
- `WaitConnect()` を廃止し `WaitAvailable(uint32_t timeoutMs = 0)` に改名
  - 実装を `recv()` から `available()` に変更し **データを消費しない** 設計に
  - ブラウザ接続待ちに 0x53 ビーコンを送らない仕様に確定
- `Write()` メソッド（`0x52` プロトコル）追加
- `Ready()` メソッド（`0x53` プロトコル）追加
- Print プロトコルの改行を `0x02` フラグ方式から `0x0A` in text 方式に統一

**HidMonitorTest スケッチ新規追加**  
Protocol モード（`hid.Print`/`hid.Println`）に特化したスケッチ。  
USB: WebHID Only。`WaitAvailable()` + `Ready()` + カウンタ送信。

**hid-serial-bridge.html の改修**
- HID と WebCom 両接続時にブリッジを**自動起動**（どちらが後でも対応）
- HID接続時に接続通知（`0x01` Feature Report）を送信し `WaitAvailable()` を解除
- HID受信モードに **Protocol モード**（デフォルト）と **Raw Binary モード**を追加
- モード切替でスケッチビューアが HidMonitorTest / HidBridgeTest に自動切り替わり
- ページ末尾に折りたたみ式プロトコル仕様セクションを追加

**hid-console.html の改修**
- HID接続時に接続通知（`0x01` Feature Report）を送信し `WaitAvailable()` を解除
- `0x53` Ready 通知を受信してログ表示
- JS プロトコルハンドラを新仕様（`0x0A` in text）に対応しつつ旧仕様（`0x02` フラグ）後方互換を維持
- ページ末尾に折りたたみ式プロトコル仕様セクションを追加

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
