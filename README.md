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

## URB Lab（UIAPruby）— 数百円から始めるRuby電子工作

| ページ | スケッチ例 | 状態 |
|--------|-----------|------|
| [URB Lab](https://tarosay.github.io/uiap-hid-web/uiapruby.html) | ブラウザ内で生成（コンポーネント選択式） | ✅ 公開中 |

[**Wakayama.rb**](https://wakayamarb.org) コミュニティが開発した、Ruby 構文で UIAPduino を制御できる組み込み Ruby 環境です。
mruby のビルドでは rake が必要な mrbgems の取捨選択と統合を、URB Lab は**すべてブラウザ内**で行います。
PC に Ruby・クロスコンパイラ等のビルド環境を整える必要はありません。

- ブラウザ内の **@ruby/prism WASM** で Ruby コードを AST 解析
- 独自の軽量バイトコード **URB1 形式**（`.urb`）にコンパイル
- **WebHID** で SD カードへ直接転送
- UIAPduino 上の **TinyVM** がバイトコードを逐次実行
- `main.urb` という名前で SD に保存すると、**電源投入時に自動実行**
- 使う機能（コンポーネント ≒ mrbgems）を選ぶと、対応する **TinyVM ファームの .ino をブラウザが生成**。Arduino IDE で書き込むだけ
- インストール不要・クラウド不要

### コンポーネント一覧

BASE（GPIO / wait_ms / if / unless / until / case-when / loop / for / while / && / || / puts "文字列" / `warn` デバッグ出力 / def（引数付き最大2個） / require）= **12,964 B**。
そこに必要な機能だけを足して、16 KB の Flash に収めます。

| ID | 内容 | Flash 増分 |
|----|------|-----------|
| Q1 | Q16.8 固定小数点演算（四則演算・比較・case/when・ラムダ式） | +592 B |
| Tn | Tone — 周波数制御（ブザー・メロディ） | +1,008 B |
| Pw | PWM デューティ比（モータ・サーボ・LED調光） | +548 B |
| Ad | ADC アナログ入力（整数 0〜255） | +328 B |
| Us | 超音波センサ HC-SR04（距離 cm） | +156 B |
| I2 | I2C / Wire（SDA=pin3 / SCL=pin4・100kHz） | +1,448 B |
| Rn | 乱数 rand / srand | +196 B |
| Ve | 変数の拡張 — 数値SD変数（`$`永続変数・3つ目以降の数値変数・数値配列。確認は `warn`） | +1,516 B |
| Sv | SD変数・数値出力（PRINT_REG・文字変数・to_s。**Ve の機能を含む**） | +2,832 B |

> `warn 変数` は数値を **DEVICE LOG** へ送るデバッグ出力（Ruby の `Kernel#warn` = stderr に相当）。
> 数値の文字列化をブラウザ側で行うため **Sv 不要**で使えます（Q1+Sv 構成のみ Flash 16KB 超過のため非搭載 — `puts`/`p` で代替）。

### UIAPruby サンプルコード

```ruby
# LED チカ
led = GPIO.new(2, GPIO::OUT)
loop do
  led.toggle
  sleep(0.2)
end
```

```ruby
# ボタンでLED制御
led    = GPIO.new(2, GPIO::OUT)
button = GPIO.new(11, GPIO::IN | GPIO::PULL_UP)
loop do
  if button.low?
    led.on
  else
    led.off
  end
end
```

```ruby
# アナログ値で LED の点滅間隔が変わる（Ad 単独）
led = GPIO.new(2, GPIO::OUT)
sensor = ADC.new(0)   # 対応ピン: 0, 1, 12, 15, 16
loop do
  v = sensor.read     # 0〜255 の整数
  led.toggle
  wait_ms v * 2
  wait_ms 20
end
```

```ruby
# HC-SR04 超音波距離センサ（配線: TRIG→pin3(PC1)  ECHO→pin4(PC2)  VCC→5V  GND→GND）
sonar = Ultrasonic.new(3, 4)
loop do
  puts sonar.read   # 距離 (cm) を HID コンソールへ出力（0 = 範囲外）
  sleep(0.5)
end
```

```ruby
# $ で始まる変数は SD カードに保存され、電源を切っても残る（Sv）
$count = $count + 1
s = $count.to_s
msg = "count="
msg << s
puts msg
```

```ruby
# def 引数付き（コンパイル時に定数として展開、Q1 不要）
def blink(pin, ms)
  led = GPIO.new(pin, GPIO::OUT)
  led.write(1)
  sleep(ms)
  led.write(0)
  sleep(ms)
end

blink(2,  0.5)
blink(10, 0.25)
```

```ruby
# 多重代入: 2 変数を 1 行で初期化（Q1 コンポーネント必要）
led = GPIO.new(2, GPIO::OUT)
on_ms, off_ms = 200, 800

loop do
  led.on
  wait_ms on_ms
  led.off
  wait_ms off_ms
end
```

TinyVM 命令セット・URB1 フォーマット・ピン配置の全リファレンスは [URB Lab ページ内](https://tarosay.github.io/uiap-hid-web/uiapruby.html)に掲載しています。

### UIAPduino ピン配置（UIAPruby 対応）

| Arduino ピン | CH32V003 | PWM | ADC | 用途 |
|---|---|:---:|:---:|---|
| 2 | PC0 | ✅ | — | LED 出力（基板上 LED） |
| 3 | PC1 | — | — | SDA（I2 コンポーネント）/ HC-SR04 TRIG 推奨 |
| 4 | PC2 | — | — | SCL（I2 コンポーネント）/ HC-SR04 ECHO 推奨 |
| 5 | PC3 | ✅ | — | PWM / Tone ブザー |
| 11 | PD1 | — | — | タクトスイッチ |
| 0, 1, 12, 15, 16 | PA1/PA2/PD2/PD5/PD6 | — | ✅ | アナログ入力 |
| **6** | **PC4** | **✅\*** | **✅\*** | ⚠ SD カード SS 専用 — 使用不可 |
| 7 / 8 / 9 | PC5/PC6/PC7 | — | — | ⚠ SD カード SCK/MOSI/MISO 専用 — 使用不可 |

> \* ハードウェアは PWM・ADC 対応だが、SD カード搭載版では SS として占有されるため使用不可。

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
| [Rock Dodge](https://tarosay.github.io/uiap-hid-web/rock-dodge.html) | `RockDodge.ino` | ✅ 公開中 |
| [Snake VS](https://tarosay.github.io/uiap-hid-web/snake-vs.html) | `SnakeVS.ino` | ✅ 公開中 |

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

### Rock Dodge（リスクとリターンの判断を学ぶ）
- 落ちてくる岩をよけながら**得点アイテム**を拾うレベル制ゲーム（全レベル 120 ステップ固定）
- **セーフティライン**（画面の水色）に乗っている限り岩には絶対に当たらない。岩の湧きは毎回
  「ライン上の仮想プレイヤーが生き残れること」を検証してから配置される
- ただし乗っているだけでは**アイテムノルマ**（Lv1 で 6 個 → Lv5 で 10 個）に届かずゲームオーバー。
  安全運転で取れるのは 1 面 7〜9 個なので、**得点はラインを外れた先にしかない**
- アイテムの得点は「ラインを何ティック外れる必要があるか（リスク d）」に連動。
  得点の帯は互いに重なっているので「浅いのに高いアタリ」「深いのに安いハズレ」がある
- **壁の岩の一部が 2〜4 列つながってアイテムに変わる**（隙間とは別の開口部）。奥へ入るほど高得点
- ラインから 2 列以上離れると湧きが敵対化 — 壁の隙間がラインの位置に寄り、
  **盤面の途中（12 行目・猶予 3 ティック）から狙い岩**が降ってくる
- ★（面クリアアイテム）は得点 0 で即クリア＋ノルマ免除。深い位置に湧くので、
  順調なときは踏まないように**避けるのが正解**
- **脱出路の仕掛け**: 壁の奥のお宝には、数ティック後の壁に「そこから届く★」が
  仕込まれることがある（深いお宝の約 4 割）。詰み確定に見えて狙える状況を意図的に作っている
- 配布スケッチは時空間先読みで生存＋±1 列のタダ取りまで（岩には当たらないがノルマ壁で止まる）
- ランキングは localStorage に名前つきで保存（名前は `#define PLAYER_NAME`）

### Snake VS（対戦アルゴリズムの練習）
- 16×16 の盤面で 2 匹が**毎ティック 1 マス成長**し、壁・岩・どちらかの体に触れたら死亡。
  生き残ったほうがラウンド勝ち。頭が同じマスに入る／頭を入れ替える動きは両者死亡＝引き分け
- **vs CPU（1台）**: CPU 5 段ラダー（ランダム → 貪欲 → BFS 最大空間 → **ボロノイ** →
  ボロノイ＋先読み）を 3 本勝負（2 本先取）で勝ち上がる。ランキングは撃破した最高レベル
- **2P 対戦（2台）**: UIAPduino 2 台で直接対戦。**5 本勝負**で引き分けも 1 本として数え、
  勝ち数が多いほうが勝ち（同数なら試合も引き分け）。ランキングは**連勝記録**
  （負けても引き分けても 0 に戻るので、自然に勝ち抜き戦になる）
- **相手の体は「相手の頭の軌跡」**。縮まないので、頭の座標 2 バイトを送るだけで
  デバイス側が相手の体を完全に復元できる（盤面全体を送る必要がない）
- スタート位置は **180° 回転対称の 6 パターン**からラウンドごとにランダム。
  どのパターンでも位置の有利不利が生じない（開始時の陣地が 115 対 115 で一致することを実測）
- **ロックステップ進行** — 相手の `CMD_DIR` を待ってから次のティックへ進むので、
  USB の遅延や計算時間では勝敗が決まらない（2 秒タイムアウト・3 回連続で反則負け）
- 開始時に「**3・2・1・スタート**」のカウントダウン。この時間で準備通信（RESET → 岩 → START）を
  済ませるので、待ち時間がそのまま演出になる。2P では 2 台ぶんの準備をこの窓が吸収する
- **陣地（ボロノイ図）の可視化トグル** — どちらが先に到達できるかを色分け表示。
  観客に優勢が一目で伝わり、挑戦者には CPU Lv4 が何を最大化しているかが見える
- 効果音（WebAudio で合成・音源ファイル不要）とミュートトグル
- `SnakeVSHID` クラス（`SnakeVSHID.h`）に通信をカプセル化

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
- **UIAPduino ボードパッケージ v1.2.7 以降**（URB Lab は SDmin のランダムアクセス API と v1.2.7 の ADC ピン修正を使用するため必須）— Board Manager に以下の URL を追加してインストール
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
  rock-dodge.html               ← Rock Dodge（リスクとリターンの判断）
  snake-vs.html                 ← Snake VS（対戦アルゴリズム / vs CPU・2P 対戦）
  hid-console.html              ← HID Console（デバッグ / ユーティリティ）
  hid-serial-bridge.html        ← HID-Serial Bridge（com0com 経由シリアル中継）
  sd-file.html                  ← SD Filemanager（SD カードファイル管理）
  midi-to-midb.html             ← MIDI → MIDB Converter（SAM2695 用）
  uiapruby.html                 ← URB Lab（コンポーネント選択式 Ruby → URB1 コンパイラ + TinyVM 実行環境）
  favicon.svg                   ← 全ページ共通ファビコン
  images/
    com0com.jpg                 ← com0com Setup GUI スクリーンショット
    WakayamarbLOGO.png          ← Wakayama.rb コミュニティロゴ
    24x24みかん.png             ← sv-dotart サンプルのドット絵原図
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
    RockDodge/
      RockDodge.ino             ← 岩よけ＋アイテム取得アルゴリズム（メインスケッチ）
      RockDodgeHID.h            ← RockDodgeHID クラス（WebHID 通信を担当）
    SnakeVS/
      SnakeVS.ino               ← 対戦スネークアルゴリズム（メインスケッチ）
      SnakeVSHID.h              ← SnakeVSHID クラス（WebHID 通信を担当）
    WebHID_SD/
      WebHID_SD.ino             ← SD Filemanager スケッチ（32 バイト Feature Report）
    MidbPlayer/
      MidbPlayer.ino            ← MIDI (.midb) 再生スケッチ（32 バイト Feature Report）
      UIAPSerial.h              ← 軽量 USART1 クラス（宣言）
      UIAPSerial.cpp            ← 軽量 USART1 クラス（実装）
    UIAPrubyVm*/                ← URB Lab TinyVM ファーム（コンポーネント組み合わせ別の動作確認済みビルド。
                                   配布はページ内の generateIno がブラウザで生成）
    Measure_*/                  ← コンポーネント別 Flash サイズ計測用スケッチ
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

### 2026-07-27

**Snake VS — 2P 対戦を追加 + 開始演出（カウントダウン・効果音・陣地可視化）**

- **2P 対戦**（UIAPduino 2 台）を同一ページにモードタブとして追加。盤面・ルール・演出・
  ロックステップ進行は vs CPU と共通
  - P1/P2 を別々のボタンで接続。同じ機体の二重選択は拒否し、**2 台の `PLAYER_NAME` が
    同じなら警告**（どちらの機体か判別できず記録も混ざるため）
  - 各機体へ「自分の頭 → 相手の頭」の順で送るので、**2 台とも同じスケッチのまま**
    自分視点で考えられる。EP0 は同時に使えないので 1 台ずつ直列に送るが、
    両者とも同じ盤面に対して思考するため情報の有利不利は生じない
  - **5 本勝負。引き分けも 1 本として数える**（仕切り直さない）。5 本終えて勝ち数が
    多いほうが勝ち、同数なら試合も引き分け。逆転不可能になった時点で打ち切る。岩は 8 個固定
  - ランキングは**連勝記録**。勝ちで +1、負けても引き分けても 0 に戻る
- **配布スケッチの鏡像ロックを修正**（2P が成立しなかった問題）
  - 実機で「衝突すると必ず両者が死んで永遠に引き分ける」と判明。原因は 2 台とも同じスケッチを
    積んでいることで、①相手の「現在の頭」しか避けない ②絶対方向の固定順で評価する
    ③同点を決定論で選ぶ、の 3 点により **180° 対称の配置で完全な鏡像**になり中央で正面衝突
    していた。配布スケッチ同士 60 戦で 54 が引き分け、その 100% が正面衝突
  - CPU 側には同じ対策を入れてあったため 1P では表面化せず、避ける側が誰もいない 2P で破綻した
  - 対策は 2 つセットで必要（実測: 相手の頭だけ避ける = 決着 10% → **次に入れるマスも避ける**
    = 83%（ただし片側に偏る）→ **手の評価順もシャッフル** = 92%・偏りなし）
  - 乱数の種は `PLAYER_NAME` のハッシュと開始座標から生成。P1/P2 で開始座標が入れ替わるので
    **2 台の名前が同じでも必ず別の乱数列**になる（同名でも決着 90% を確認）
- **開始演出**: 「3・2・1・スタート」のカウントダウン（1 拍 = ちょうど 1 秒。ラウンド間 2 秒 /
  対戦開始・レベルアップ後 3 秒）。この時間で準備通信を覆い隠すので、待ち時間が演出になる。
  カウントダウン中も岩と開始位置が見えるため、観客が配置を読む時間にもなる
- **効果音**（WebAudio で合成・音源ファイル不要）: カウント / スタート / 勝ち / 負け /
  引き分け / レベルアップ / 全段クリア / 衝突。ミュートトグル付き
- **決着テロップ**（WIN / LOSE / DRAW とスコア）と**陣地（ボロノイ図）の可視化トグル**
  - 副産物として、開始時の陣地が **115 対 115 で完全一致**することを確認。
    180° 回転対称の公平性が勝率統計とは別の角度から裏付けられた
- バグ修正: オーバーレイのアニメを `requestAnimationFrame` だけで駆動していたため、
  **タブを裏に回すと試合が固まっていた**。タイマーによるフォールバックを追加
- バグ修正: 陣地表示が 2P でも「CPU」のままだった。P1/P2 に切り替える
  （名前を出す案は「Tarosa 1」「Tarosa 2」が 6 文字で切られて両方「Tarosa」になり
  区別できなかったため、短いタグに変更。思考時間の名前も切り詰めをやめた）

### 2026-07-26 (2)

**Snake VS 公開 — UIAPduino のアルゴリズムが CPU と陣地を取り合う対戦スネーク**

- Snake Solver の「毎ティック成長・BFS 最大空間優先」を対戦に拡張。勝利条件は**生存のみ**
- **CPU 5 段ラダー**（ランダム / 貪欲 / BFS 最大空間 / ボロノイ / ボロノイ＋深さ 2 の先読み）を
  各段 3 本勝負（2 本先取）で勝ち上がる。岩は 0 / 4 / 8 / 10 / 10
- **相手の体 ＝ 相手の頭の軌跡**（縮まないため）。頭の座標 2 バイトを送るだけで
  デバイス側が相手の体を完全に復元できる
- スタート位置は **180° 回転対称の 6 パターン**からランダム。盤面が偶数幅なので
  `x ≠ 15−x` が常に成立し、2 匹が同じ行・列に並ばない（開幕の正面衝突が構造的に起きない）
- **ロックステップ進行**で USB 遅延では勝敗が決まらない（2 秒タイムアウト・3 連続で反則負け・
  観戦用に 1 ティック下限 200ms）
- 配布スケッチは BFS 最大空間優先（Snake Solver ヒント 4 と同じ既公開の知識）。
  CPU Lv3 と互角、Lv4（ボロノイ）で止まる床実装
- **RAM 最適化**: bool 配列 3 枚（768B）では RAM 70% となり挑戦者のボロノイ用距離配列（512B）が
  入らないため、`uint8_t cell[16][16]` 1 枚と座標を `(y<<4)|x` に詰めた `bfsQ[256]` に変更。
  **668B（32%）まで下げて挑戦者用に 1,380B を確保**
- CPU の同点処理を決定論にすると、対称配置で両者が鏡像的に動いて引き分け 80〜90%、
  かつ配置ごとに勝率が偏ることが判明。候補手のシャッフル＋同点ランダム選択で解決
- 引き分けの原因は 100% が正面衝突だったため、CPU に「相手が次に入れるマス」の回避を追加
  （引き分け 80% → 17〜37%）
- 実測（各 60 ラウンド）: 配布スケッチ vs Lv1 = 48勝2敗 / Lv3 = 27勝23敗（互角）/
  Lv4 = 3勝40敗（壁）。CPU 同士も完全単調。自己対戦の P1 勝率は 6 配置平均 47%
- **LED Controller** は作る予定がないため Coming Soon カードを削除

### 2026-07-26

**Rock Dodge 公開 — 岩をよけながら得点アイテムを拾うプラクティス**

- 落ちてくる岩をよけながらアイテムを拾い、スコアを競うレベル制ゲーム（全レベル 120 ステップ固定）。
  プレイヤーは最下段を左右に動くだけ（1D）
- **セーフティライン**（仮想プレイヤー）を導入。岩の湧きはライン基準で生存を保証するため、
  **ラインに乗っている限り岩には当たらない**。ただし乗っているだけでは
  アイテムノルマ（Lv1 で 6 個 → Lv5 で 10 個）に届かずゲームオーバー
  — **岩をよけ続けるだけでは得点は増えない**設計
- アイテムの得点は**リスク d（ラインを外れる必要のあるティック数）に連動**。
  得点の帯を互いに重ねてあるので「浅いのに高いアタリ」があり、表示された数字を読む意味がある
- **壁の岩の一部が 2〜4 列つながってアイテムに変わる**（隙間とは別の開口部）。奥ほど高得点
- ラインから 2 列以上離れると湧きが敵対化 — 壁の隙間がラインの位置に寄り、
  **12 行目（猶予 3 ティック）から狙い岩**が降る
- ★（面クリアアイテム）は得点 0 で即クリア＋ノルマ免除。1 面に最大 3 個、深い位置（d 4〜7）に湧く。
  順調なときは踏まないよう避けるのが正解（★を避ける実装は避けない実装より約 20% 高得点）
- **脱出路の仕掛け**: 壁の奥のお宝を置いたら、数ティック後の壁に「そこから届く★」を
  約 4 割の確率で埋め込む。詰み確定に見えて狙える状況を意図的に作る
- プロトコルは `CMD_TICK` を**列ごとのアイテム値配列**にして 1 行に複数アイテムを置けるようにした
  （送信 21 バイトのため**受信バッファは 32 バイト必須**）
- 配布スケッチは時空間先読みで生存＋±1 列のタダ取りまでの床実装。岩には当たらないが
  ノルマ壁で止まる。採算判断はヒントで方針のみ示しコードは載せていない
- ランキングは localStorage（プロジェクタ投影での挑戦会を想定）。名前は `#define PLAYER_NAME`
- 実測（1〜14 面通しプレイ）: 床（配布スケッチ）= 111点 / Lv2.3 / 岩死 0 に対し、
  最適実装 = 2,814点 / Lv12.3。**25 倍のスコア差**がつく技量勾配を確認

### 2026-07-12 (3)

**URB Lab — 新コンポーネント Ve（変数の拡張 — 数値SD変数）を追加**

- Sv（+2,832B）から数値系の機能だけを切り出した軽量コンポーネント（**+1,516B**）。
  **Sv 自体は無変更**（Ve の上位互換 — ファーム出力もバイト単位で不変）
- Ve で使えるもの: `$count` などの**数値永続変数** / **3つ目以降の数値変数**（揮発・SD 自動配置）/
  **数値配列**（`$`付きで永続）。値の確認は `warn`（DEVICE LOG）
- `puts` の数値出力（PRINT_REG）・文字変数・`to_s` は従来どおり Sv（コンパイルエラーで案内）
- 動機は「$変数は便利だが Sv が大きすぎて他コンポーネントと併用できない」の解消。
  **Q1+Us+Ve = 15,132B / Q1+Rn+Ve = 15,180B** — 旧 Sv では 16KB を超えて不可能だった
  組み合わせが使えるようになった
- Sv と Ve を両方選択した場合は Sv に正規化（Flash 見積もりの二重加算なし・ZIP 名も Sv 単独と同一）
- サンプル2本を追加。**Sv は Ve の上位互換のため、Ve のサンプルは Sv 選択時にも表示される**
  - **ve-blink** — 電源を入れるたびに LED の点滅回数が 1 回ずつ増える $永続変数のデモ
    （出力命令を使わないため Q1+Ve / Q1+Sv どちらの構成でも動く）
  - **ve-counter** — 起動ごとに +1 する永続カウンタを warn で表示
    （Q1+Sv 構成では warn がファームから省略されるため、Sv 選択時はボタン非表示）
- 参照スケッチ UIAPrubyVmVe（14,480B）/ UIAPrubyVmQ1Ve（15,020B）を追加

### 2026-07-12 (2)

**URB Lab — サーボ angle() を 50Hz 標準タイミング化 + ファーム MUL_Q16 の乗算バグ修正**

- **サーボが正しく動かないバグを修正**（実機症状: 0→90 指定で45°しか動かない / 135°・150°・180° が
  無視または暴走）。原因は2つの複合:
  1. **旧 500Hz 設計の破綻** — パルス 1〜2ms のマッピングは SG90 系（0.5〜2.4ms = 0〜180°）の
     中央半分しかカバーせず、さらに大角度では周期 2ms 内に Low ギャップがほぼ消える
     （180°で 8µs）ためサーボがパルス境界を認識できない
  2. **ファーム MUL_Q16 の int16 キャストバグ** — ±128 以上の値を含む乗算がすべて壊れる
     （angle 専用ではなく Ruby の `a * b` 全般。変数 angle が 128° 以上で不正 duty になっていた）
- **angle() は 50Hz・パルス 0.5〜2.4ms に変更**（Arduino Servo ライブラリの既定 544〜2400µs と
  ほぼ同等レンジ）。`servo.frequency(50)` が必須（違うとコンパイルエラーで案内）。
  8bit PWM のため分解能は約 7°/ステップ。SG90 実機で 0〜180° 全域動作を確認済み
- **MUL_Q16 を int32 乗算に修正**（積 ±32768 未満まで正確）。Q1 系 4 スケッチ
  （UIAPrubyVmQ1 / Q1Pw / Q1Sv / Q1Tn）を再生成 — **書き込み済みボードは再書き込みが必要**
  （angle だけはコンパイラ側の演算順対策により旧ファームでも正しく動く）
- Flash: Q1 +596 → **+592 B**（▲4B）。Q1+Sv = 16,212 B（98%、16KB 内に収まる）
- `tools/gen-ino.js` の旧ファイル名（urb-lab.html）参照を修復

### 2026-07-12

**スケッチ ZIP に sketch.yaml を同梱 — Arduino IDE 2.1+ でボード設定が自動選択される**

- スケッチダウンロード ZIP を持つ**全 12 ページ**に `sketch.yaml`（`default_fqbn:` 1行）を同梱:
  URB Lab / SD Filemanager / MIDI → MIDB / HID Console / Keyboard 1・2 / Mouse 1・2 /
  Snake Solver / Maze Solver / Echo Test / HID-Serial Bridge
- 展開した .ino を Arduino IDE 2.1 以降で開くと、ボード「**HID ProMicro CH32V003**」と
  カスタムオプション（Board Version V1.4 / USB / Optimize oslto、URB Lab の Tn 選択時は PWM default）が
  **自動選択**され、書き込み前のボード設定作業が不要になる
- **USB 設定は各ページの解説どおりに出し分け**: Keyboard 1 = Keyboard+Mouse（kbd）/
  Snake Solver・HID-Serial Bridge の HidMonitorTest = WebHID Only（webhid）/
  HID-Serial Bridge の HidBridgeTest とその他ページ = Keyboard+Mouse+WebHID（kbdweb）/
  URB Lab・SD Filemanager・MIDI → MIDB = WebHID Only（webhid）
- ボードパッケージ（v1.2.7）のインストール自体は従来どおり必要（sketch.yaml は選択のみ）。
  古い IDE では無視されるだけで無害
- arduino-cli 検証済み: `default_fqbn` のみの yaml は `--fqbn` 指定あり/なしのどちらのビルドも正常
  （過去にビルドを壊したのは profiles 系の記述を含む yaml で、今回の形式は問題なし）

### 2026-07-11 (4)

**URB Lab — ラムダ式対応（コンパイル時インライン展開）+ Q1 チェック漏れ修正**

- **ラムダ式に対応**（コンパイラのみ・VM 変更なし）。`f = ->(x) { x < 15 }` で定義し、
  以下の場所で使用可能。呼び出し位置へコンパイル時にインライン展開されるため
  実行時オーバーヘッド・追加 Flash なし
  - `case d / when f` / `when ->(x) { x < 15 }` — case 対象が引数に渡り条件として展開
  - `if between.call(d, 5, 10)` — if / while / until / unless / 三項演算子の条件
    （複数引数・`&&` / `||` も可）
  - `y = double.call(n)` — 数値式（戻り値 = 本体の式の値）
  - `wait_ms half.call(500)` — 引数がすべて定数ならコンパイル時に計算され、
    リテラルを書ける場所ならどこでも使える
  - 制約: 本体は式1つのみ / 引数は数値リテラル（定数式）または変数のみ / 再帰不可
    （いずれもコンパイルエラーで案内）
- **バグ修正: Q1 チェック漏れ** — `d = 1.0`（数値代入）と `if d < 15`（比較演算）が
  Q1 未選択でもコンパイルできてしまい、BASE ファームで実行時「不明なオペコード」に
  なっていた（LOAD_Q16 / CMP_*_Q16 は Q1 命令）。コンパイル時に Q1 を案内するエラーを
  表示するよう修正。`d = sonar.read` / `rand` / `wait_ms d * 10` など Q1 不要の経路は従来どおり
- ページの API リファレンスに比較演算・case/when（値/レンジ）・ラムダ式の項を追加
- 全 36 サンプル回帰コンパイル OK（Us / Ad / Rn / I2 単独などの最小構成でも確認）

### 2026-07-11 (3)

**URB Lab — `case/when` のレンジ対応 + else 節が消えるバグを修正（コンパイラのみ・VM 変更なし）**

- `when 0...15`（終端含まず）/ `when 1..3`（終端含む）/ 始端なし `when ..0` /
  終端なし `when 20.. then` のレンジ比較に対応。多値との混在（`when 0...3, 10`）も可
- 既存オペコード **CMP_LT_Q16 / CMP_GT_Q16 ×2 + 条件ジャンプ**に展開するだけなので
  ファームウェア・スケッチは無変更（Q1 コンポーネント必須）。
  レンジは比較 2 回で済み、多値列挙（`when 0, 1, 2, ...`）よりバイトコードが大幅に小さい
- **バグ修正**: case 式の **else 節がバイトコードから丸ごと欠落**していた問題を修正
  （コンパイラが Prism CaseNode の `elseClause` ではなく `consequent` を参照していたため、
  case-when 導入以降 else の中身がエラーも出ずに消えていた）
- 全 36 サンプル回帰コンパイル OK

### 2026-07-11 (2)

**URB Lab — `warn` デバッグ出力を追加（Sv 不要で数値を DEVICE LOG へ）**

- 新オペコード **WARN_REG (0x32)** を BASE に追加。`warn d` / `warn sonar.read` で
  レジスタ生値（Q16.8 下位 24bit）を既存の hidLog パケット（type 0x20）で送信し、
  **数値→文字列変換はブラウザ側**で実施 → Sv（+2,832B）なしでデバッグ出力が可能に
- DEVICE LOG に `warn: 14` / `warn: 14.25` 形式で表示（整数はそのまま・小数部があれば小数表示）
- **Q1+Sv 構成のみ非搭載**（Flash 16KB を 72B 超過するため）。コンパイラが
  「puts / p で代替」を案内するエラーを表示。Q1+Sv は従来と同一の 16,216B を維持
- `hidLog()` に noinline を付与（LTO のインライン展開を抑制、▲20B）
- 全 14 スケッチ再生成・再ビルド確認済み。BASE 12,888 → **12,964 B**（+76B）。
  各コンポーネント差分・Flash 見積もりを実測値に更新（Q1+Sv は warn 省略分 −176B 補正）

**SD Filemanager / MIDI → MIDB Converter — スケッチダウンロードを ZIP 形式に統一**

- 両ページのスケッチダウンロードが単一ファイル直ダウンロードのまま残っていたのを、
  他ページと同じ **ZIP 形式**（JSZip・`スケッチ名/スケッチ名.ino` 構造）に変更
  - SD Filemanager: `WebHID_SD.zip`（WebHID_SD.ino）
  - MIDI → MIDB: `MidbPlayer.zip`（MidbPlayer.ino + UIAPSerial.h + UIAPSerial.cpp）
- ボタン表記も「DL」→「ZIPダウンロード」に統一。展開するとフォルダ名と .ino 名が一致し、
  Arduino IDE でそのまま開ける
- 変換結果の `.midb` や SD カード内ファイルのダウンロードは従来どおり単体ファイルのまま

**URB Lab — ボードパッケージ v1.2.7 対応・解説文の修正**

- **必要ボードパッケージを v1.2.7 以降に引き上げ**
  - v1.2.7 の ADC ピン割り当て修正（ADC1_IN6/IN7 → PD6/PD4）により、Ad コンポーネントの
    pin16 (A6) が正しい物理ピンで動作する（v1.2.6 以前は誤ったピンを読んでいた）
  - ページ内の必要ソフトウェア表記・生成 .ino ヘッダー・配布用スケッチ 14 本のヘッダーを更新
- **コンポーネント Flash 増分を v1.2.7 で全再計測**（BASE = 12,888 B 変化なし）
  - Ad: +324 → **+328 B**（ADC ピン修正の影響）
  - Sv: +2,744 → **+2,828 B**（`print(0)` 修正 = PRINT_REG が使う Print クラスの影響）
  - Q1 / Tn / Pw / Us / I2 / Rn は変化なし。全組み合わせが 16 KB 以内に収まることを実測確認
    （最大は Q1+Sv の 16,216 B）
- **解説文の修正**:
  - スケッチ書き込み手順に「UIAPduino のボタンを押しながら USB ケーブルを挿す」説明を追加
  - `main.urb` 自動実行の開始が「電源オンから 5 秒後」である旨を追記（2 箇所）
  - エディタの初期サンプルを led-blink サンプルボタンの内容（コメント付き）と一致させた

---

### 2026-06-29

**URB Lab — ソースエディタ ファイル操作・整形・スクロール改善**

- **ソースエディタ ファイル操作**:
  - 「開く」ボタン: `.rb` ファイルを読み込み（Chrome/Edge では `showOpenFilePicker()` でファイルハンドルを取得し上書き保存に対応）
  - ドラッグ＆ドロップ: エディタ枠に `.rb` ファイルをドロップして読み込み
  - 「保存」ボタン: ハンドルがあれば同じファイルに直接上書き保存、なければ同名でダウンロード
  - 「名前を付けて保存」ボタン: `showSaveFilePicker()` でファイル名・場所を指定して保存
  - 「クリア」ボタン: エディタをクリアしファイルハンドルもリセット
- **「整形」ボタン（Prism AST ベース）**:
  - @ruby/prism WASM の AST を走査してブロック構造（`do...end` / `if` / `until` / `while` / `def` / `class` 等）に応じたインデントを算出
  - 日本語コメント等を含む場合も UTF-8 バイトオフセットで正確に行番号を算出
  - コメント行は前後の行の深度の max で補完
  - 代入演算子 `=` 前後のスペースを自動正規化（`==` / `=>` / `+=` 等は対象外）
  - 構文エラーがある場合は整形を中止してエラーを通知
- **自動スクロール**:
  - 「コンパイル」ボタン押下後、成功・エラー問わずコンパイル結果セクションへスクロール
  - 「SD カードに送信」完了後、ファイル一覧更新完了後に DEVICE LOG の RUN ボタンへスクロール

---

### 2026-06-26

**URB Lab — Tn の PWMmin 化（▲580B 削減）・ピンバリデーション追加**

- **Tn コンポーネント**: `tone()`/`noTone()` を廃止し `PWMmin.h` で実装
  - Flash: +1,592 B → **+1,012 B**（▲580 B 削減）
  - FQBN に `pwm=default` が必要。ブラウザ生成コードに自動付加
- **Pw/Tn コンパイラ**: `PWM.new` / `Tone.new` で有効ピン（0/2/5/12）をチェック
  - pin 6 (PC4) は SD カード SS 専用（`PIN_SS=6`）のため使用不可 → コンパイルエラー
- **Tn 説明文修正**: 「サーボ制御」を削除（サーボは Pw の機能）

---

### 2026-06-13

**URB Lab — 構文拡張・def 引数付き・多重代入・コンポーネントチェック完備**

- **新対応構文**（コンパイラのみ、TinyVM は変更なし）:
  - `until cond` — while の条件を反転した構文
  - `case; when cond` — 条件式による case 分岐（条件なし case）
  - `case x; when val` — 数値等値比較（Q1 必須）
  - `if a && b` / `if a || b` — 複合条件（短絡評価）
- **`def` 引数付き対応**（限定的）: 引数最大 2 個・数値リテラルのみ。コンパイル時に定数展開するため Q1 不要
  - `def blink(pin, ms) ... end` のように定義し `blink(2, 0.5)` で呼び出す
- **多重代入 `a, b = 1, 2`**（Q1 必須）: 2 変数を 1 行で初期化
- **コンポーネントチェック完備**: Tn / Pw / Ad / Us / Rn のコンストラクタ・`rand` / `srand` がコンポーネント未選択時にエラーを出すよう修正
- `index.html`: ボードマネージャ URL 欄に「コピー」ボタンを追加

---

### 2026-06-12（サイト整備）

**Coming Soon の整理・ブラウザ警告・戻るリンク統一**

- 公開予定を整理: **Mini Oscilloscope を削除**（HID-Serial Bridge + Arduino IDE シリアル
  プロッタで役割を代替済み）。**Rock Dodge** はセンサ不要・C 言語のアルゴリズムのみで
  何秒耐えるかを競う仕様に説明を修正。**Snake Game — VS 対戦**（コンピュータのレベル制・
  UIAPduino 2台での人対人・ネット対戦を計画）カードを新規追加
- `uiapruby.html` に WebHID 非対応ブラウザ（Firefox / Safari）向けの警告バナーを追加
  （コンパイルと .ino / .urb ダウンロードは利用可能な旨を併記）
- `uiapruby.html` / `midi-to-midb.html` の「← UIAPduino WebHID Lab」戻るリンクを
  ヘッダー最上部（左上）へ移動し、全 12 ページで位置を統一

---

### 2026-06-12

**URB Lab 全面刷新 — コンポーネント選択式へ**

- `uiapruby.html` を **URB Lab** として全面リニューアル（旧 `urb-lab.html` を改名。旧ページ
  `_uiapruby.html` / `ruby-to-urb.html` と専用スケッチ `UapRunner` / `UIAPrubyRunner` /
  `UIAPrubyRunnerStandard` は削除 — git 履歴から参照可）
- **コンポーネント選択式**（≒ mrbgems）: BASE 12,888 B + Q1/Tn/Pw/Ad/Us/I2/Rn/Sv の8コンポーネント
  から必要な機能だけを選び、**TinyVM ファームの .ino をブラウザ内で生成**（mruby における rake の
  役割をブラウザへ移植）。Flash 見積もりをリアルタイム表示
- **Sv コンポーネント**: `$` 永続変数（SD カード保存・電源断でも保持）・文字列変数・配列・
  `to_s` / `<<` 連結・PRINT_REG。24×24 ドット絵デモ（sv-dotart）
- **新コンポーネント**: Tn（Tone）/ Pw（PWM デューティ・サーボ）/ I2（I2C, Wiremin.h）/
  Rn（rand / srand）
- **ADC 仕様変更**: `sensor.read` が**整数 0〜255** を返すように（旧 0.00〜0.99 を廃止）。
  ADC を直接レジスタ制御に書き換え Flash +708 → +324 B
- バイトコード形式を **URB1**（`.urb`）に改称、自動実行ファイル名は `main.urb`
- **SDmin v1.2.5**（ボードパッケージ）のランダムアクセス API（`sm_seek` / `sm_write_at`）を
  利用。ボードパッケージ **v1.2.5 以降が必須**
- `index.html` のトップ導線を「UIAPduino SD をお使いの方へ」パネルへのページ内リンクに変更

---

### 2026-06-05

**UIAPruby — HC-SR04 超音波距離センサ対応**

- `UIAPrubyRunner.ino` に **ULTRASONIC_READ (0x1A)** オペコードを追加
  - `Ultrasonic.new(trig, echo)` / `puts sonar.read` で距離 (cm) を HID コンソールへ出力
  - 推奨配線: TRIG → pin3 (PC1)、ECHO → pin4 (PC2)（ピンは任意の GPIO に変更可）
  - `pulseIn()` / `micros()` は `uint64_t` soft-lib を引き込むため不使用。`SysTick->CNT`（32bit, 48MHz）直読みで実装
  - Flash 使用量: 16,216 / 16,384 バイト（残り 168 バイト）
- `uiapruby.html` のコンパイラ・UI を対応
  - `Ultrasonic.new(trig, echo)` 認識・バイトコード生成
  - ピン表・オペコード表・リファレンス表・サンプルコード・逆アセンブル表示を更新

---

### 2026-06-02

**UIAPruby Lab 公開・ADC 対応**

- `uiapruby.html` を新規追加 — Ruby 構文 → UAP1 バイトコード変換・実行環境
  - @ruby/prism WASM によるブラウザ内 Ruby パーサー
  - UIAPduino TinyVM v1 向けコンパイラ（GPIO / PWM / if / loop / for / def / Q16.8 演算 / require）
  - WebHID で SD カードへ .uap 直接転送
  - HID コンソール（`puts` / `print` / `p` / `putc` 出力）
- `UIAPrubyRunner.ino` に **ADC_READ (0x18)** と **PRINT_REG (0x19)** オペコードを追加
  - `ADC.new(pin)` / `puts sensor.read` で ADC 値を HID コンソールへ出力
  - Flash 使用量: 16,068 / 16,384 バイト (98%)（ADC ドライバ込み）
- Wakayama.rb コミュニティロゴ（`images/WakayamarbLOGO.png`）をヘッダーに追加
- ボードパッケージ必要バージョンを v1.2.3 以降に更新
- TinyVM 命令表・ピン配置リファレンス・Q16.8 表記を整備

---

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
4. `docs/index.html` にリンク付きカードを追加（Coming Soon カードがあればそれを置き換える）
5. `README.md` を更新 — **忘れやすいので 4 箇所すべて**:
   - 「デモページ一覧」テーブルに行を追加
   - 「各ページの機能」に説明を追加
   - 「ファイル構成」に HTML とスケッチフォルダを追加
   - 「変更履歴」に日付つきで追記
6. コミット＆プッシュ → GitHub Pages に自動反映

---

## 関連リポジトリ

| リポジトリ | 説明 |
|-----------|------|
| [arduino_core_ch32](https://github.com/tarosay/arduino_core_ch32) | UIAPduino Arduino コア（WebHID ファームウェア含む） |
| [board_manager_files](https://github.com/tarosay/board_manager_files) | Board Manager 用インデックス JSON |
| [Wakayama.rb](https://wakayamarb.org) | UIAPruby 開発元 — 和歌山発 Ruby コミュニティ |

---

## ライセンス

MIT License — 詳細は [LICENSE](LICENSE) を参照
