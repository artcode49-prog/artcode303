# ACID-303

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](LICENSE)
[![build](https://github.com/artcode49-prog/artcode303/actions/workflows/build.yml/badge.svg)](https://github.com/artcode49-prog/artcode303/actions/workflows/build.yml)

Roland TB-303 のアナログ挙動をモデリングした VST3 シンセサイザーです。
4 極ダイオードラダー VCF / 内蔵 16 ステップシーケンサー / アクセント・スライド /
オリジナル準拠の GUI / ファクトリープリセットを備え、さらに **DAW が無くても
音を検証できる独立系レンダラー** を同梱しています。

- プラグイン形式: **VST3** (Instrument / Synth)
- ライセンス: **AGPL-3.0-or-later**（無償・オープンソース。全文は [`LICENSE`](LICENSE)）
- 対応: macOS (Universal Binary: arm64 + x86_64), 44.1 / 48 / 96 kHz など
- 内部処理: 4 倍オーバーサンプリング (VCF の非線形帰還と VCA のエイリアスを抑圧)
- DSP コアは **JUCE 非依存の純 C++** (`Source/dsp/`) なので、プラグインを
  ビルドしなくても単体でテストできます。

> **ビルド済み VST3 が欲しい場合** — ビルド不要です。macOS 版は Developer ID で
> 署名し Apple の公証も通しています。配布用の zip は GitHub Release にあります。
> → [Release](https://github.com/artcode49-prog/artcode303/releases/latest) /
> [ダウンロード方法](#ダウンロードビルド済み-vst3)

---

## 目次
1. [ダウンロード（ビルド済み VST3）](#ダウンロードビルド済み-vst3)
2. [配布セットを作る](#配布セットを作る)
3. [音作りの構成（サウンドエンジン）](#サウンドエンジン)
4. [パラメータ一覧](#パラメータ一覧)
5. [内蔵シーケンサーの使い方](#シーケンサーの使い方)
6. [GUI の説明](#gui-の説明)
7. [ビルド方法](#ビルド方法)
8. [検証用ツール（レンダラー）](#検証用ツール)
9. [検証結果](#検証結果)
10. [ファイル構成](#ファイル構成)
11. [ライセンス](#ライセンス)
12. [既知の注意点](#既知の注意点)

---

## サウンドエンジン

信号経路はハードウェアと同じ **VCO → VCF → VCA** の一発モノフォニック構成です。
すべて内部オーバーサンプリングレート (`出力 SR × 4`) で動作し、最終段の
6 次 Butterworth ローパス (`0.42 × 出力 SR`) でダウンサンプリングします。

### VCO — `Source/dsp/Tb303Osc.h`
- ノコギリ波 / 矩形波を **PolyBLEP** でバンドリミット。
- 矩形波の極性をノコギリ波の立ち上がりに合わせてあるため、両者を混ぜても
  打ち消し合わず滑らかにモーフします。
- `waveform` は 0..1 の連続値 (0 = 矩形, 1 = ノコギリ)。ハードウェアの
  2 ポジションスイッチはその両端に相当します。

### VCF — `Source/dsp/Tb303Filter.h`
- 4 極ダイオードラダーを **ZDF (Topology Preserving Transform)** で実装。
- 帰還路は飽和 (`satScale = 1.3`) させ、1 サンプルあたり 2 回の不動点反復で
  非線形ゼロ遅延帰還ループを解きます。
- 小信号の応答はそのままに、自励発振の振幅を約 0.70 に抑えます。
- レゾナンスは `k = knob * 4.7 * knob^0.2`。ノブ約 87% から自励発振します
  (オリジナルの「ある地点から急に歌い出す」挙動を再現)。

### エンベロープ — `Source/dsp/Tb303Env.h`
- アタックは瞬時、減衰は指数。純指数よりやや凸な形 (`curveCoef`) にして
  「pluck」感と長いテールを出しています。
- 1 本の Decay エンベロープが VCF (Env Mod 経由) と VCA の両方をドライブ。
- Accent は高速な追加ヒットで VCF のカットオフ・レゾナンス・VCA を同時に押し上げます。

### ボイス / 出力段 — `Source/dsp/Tb303Voice.h`
- カットオフ = ベース値 × `2^(envDepth · env)` でスイープ。
- 入力はハードウェア同様に熱めにドライブ (`1.15 × osc`、Drive で可変)。
- VCA は飽和 (`fastTanh(y) * 0.95`)、その後 `softLimit(y, 0.80)` で
  自励発振時も破綻しないよう整えます。
- スライドは約 75 ms、MIDI ノートオフは 4 ms リリース。

### エンジン / シーケンサー — `Source/dsp/Tb303Engine.h`
- 16 分音符グリッドでステップを発火。スウィングは表拍を長く・裏拍を短くして
  ペアの合計は保つので、ノリは変わってもテンポはずれません。
- 方向: Forward / Reverse / Ping-Pong / Random。
- クロックは **Internal**（ハードウェア同様、GUI の PLAY ボタン）と
  **Host**（DAW のトランスポート / BPM に追従）を切替可能。
- 出音は `softLimit(total, 0.82)`、音量は `pow(volume,1.5) × 1.35`。

---

## パラメータ一覧

オートメーション可能なパラメータは次の 15 個です。

| ID | 表示名 | 範囲 | 既定 | 説明 |
|----|--------|------|------|------|
| `waveform`  | Waveform  | 0..1 | 1.00 | 0 = 矩形, 1 = ノコギリ (GUI は SQR/SAW スイッチ) |
| `tuning`    | Tuning    | 0..1 | 0.50 | ±100 セント (0.5 = 基準) |
| `cutoff`    | Cut Off   | 0..1 | 0.30 | VCF カットオフ (30 Hz .. 5.5 kHz) |
| `resonance` | Resonance | 0..1 | 0.45 | VCF レゾナンス (約 87% から自励発振) |
| `envmod`    | Env Mod   | 0..1 | 0.60 | エンベロープ → カットオフ深さ |
| `decay`     | Decay     | 0..1 | 0.32 | エンベロープ減衰 (35 ms .. 3 秒強) |
| `accent`    | Accent    | 0..1 | 0.70 | アクセント強度 |
| `drive`     | Drive     | 0..1 | 0.20 | VCF 入力の追加オーバードライブ |
| `volume`    | Volume    | 0..1 | 0.80 | 出力レベル |
| `tempo`     | Tempo     | 0..1 | 0.50 | 内部クロック BPM (40 .. 300, 対数) |
| `swing`     | Swing     | 0..1 | 0.00 | シャッフル量 (0 = ストレート) |
| `transpose` | Transpose | 0..1 | 0.50 | -12 .. +12 半音 (0.5 = 0) |
| `length`    | Length    | 1..16 | 16 | パターン長 |
| `direction` | Direction | 4 択 | Forward | Forward / Reverse / Ping-Pong / Random |
| `clock`     | Clock Source | 2 択 | **Host** | Internal / Host（既定は Host = DAW 追従） |

パターン (ノート / オクターブ / アクセント / スライド / レスト) はプラグイン状態
(`ValueTree` の `PATTERN` 子ノード) として保存されるため、DAW のプロジェクトに
そのまま保存・復元されます。

---

## シーケンサーの使い方

1. **クロック源は既定で HOST。** DAW の再生 / 停止と BPM にそのまま追従し、
   DAW を止めれば音も止まります（最後のノートは DECAY に従って自然に減衰）。
   DAW を使わず単体で鳴らしたいときは **CLOCK を INT** に切り替え、
   **PLAY** ボタンで内部クロック再生します。

| モード | 動作 |
|--------|------|
| **HOST**（既定） | DAW のトランスポート / BPM に追従。**PLAY ボタンで手動停止 / 再開もできます** |
| **INT** | 内部クロックで再生。GUI の PLAY ボタンで開始 / 停止 |

> **PLAY ボタンは常に停止できます。** Host モードで DAW が再生中でも、PLAY を
> 押せばその場でシーケンサーが止まり、もう一度押すと再開します。DAW を停止 →
> 再生し直すと自動的に再開します（シンクした 303 の挙動）。なお Host モードで
> DAW が止まっている間はクロックが無いため PLAY を押しても発音しません。

2. GUI 中央の 16 ステップグリッドで各ステップを編集します。

| 行 | 操作 |
|----|------|
| **NOTE** | 縦ドラッグで音程 (パターンルート + 0..12 半音)。ドラッグで複数ステップをまとめて描けます |
| **OCT**  | クリックで -1 / 0 / +1 (オクターブダウン / ノーマル / アップ) |
| **ACC**  | クリックでアクセント ON/OFF |
| **SLD**  | クリックでスライド ON/OFF |
| **REST** | クリックで休符 ON/OFF |
| ダブルクリック | そのステップを初期化 |

補助ボタン:

- **RAND** … ランダムパターン生成
- **CLR** … パターンを初期化
- **COPY / PASTE** … パターンのコピー / 貼り付け
- **DIR / CLOCK / LEN** … 方向・クロック源・パターン長の切替

---

## GUI の説明

`Source/gui/` のアセットを埋め込んで描画します（配布時に外部ファイル不要）。

| アセット | 用途 |
|----------|------|
| `logo.png` | ヘッダ左上のロゴ（従来の "ACID-303" テキストの位置） |
| `knob.png` | 128 フレームのノブフィルムストリップ。回転角に応じたフレームを描画（本体・ポインタ・オレンジの値アークを内包） |
| `sidewood.png` | 左右の木製サイドパネル（パネル全高に合わせて配置） |

- 上部バー: ロゴ、プリセット選択 (`<` / `>` / プリセット名 / **SAVE** / **LOAD**)、
  BPM 表示、ステータス表示
- 左ブロック: `WAVE` スイッチ (SQR/SAW)
- 中央: 16 ステップのパターングリッド
- ノブ: Cut Off / Resonance / Env Mod / Decay / Accent / Drive / Volume /
  Tuning / Tempo / Swing / Transpose
- 下部: MIDI キーボード (クリックで発音。アクセントはベロシティ 0.8 以上)

---

## ダウンロード（ビルド済み VST3）

**ビルド不要です。配布用の zip は GitHub Release に置いてあります。**

<https://github.com/artcode49-prog/artcode303/releases/latest>

| ファイル | 内容 |
|---|---|
| `Acid303-1.0.0-macOS.zip` | macOS 版 (Universal: arm64 + x86_64) — **Developer ID 署名 + Apple 公証済み** |
| `Acid303-1.0.0-Windows.zip` | Windows 版 (x64) — 未署名 |
| `Acid303-1.0.0-macOS-Windows.zip` | 両方 |

### ダウンロード時の警告について

Chrome / Edge が「一般的にダウンロードされていません」「安全ではない可能性が
あります」と表示することがあります。これは **Apple の署名・公証とは無関係**です。
Google Safe Browsing (Chrome) / Microsoft SmartScreen (Edge) が「このファイルは
まだ世の中で知られていない」と判定しているだけで、コード署名では消えません
（Windows は Apple の公証を一切見ません）。中身は検査済みで、実行ファイルは
公証済みの `Acid303` だけです。

- `github.com` からのダウンロードは信頼済み扱いになりやすいため、Release の
  ページから落とすのが最も警告の出にくい経路です
- 出てしまった場合は「保持する / Keep」を選べばそのまま使えます
- ダウンロード数が増えると、この種の警告は自然に出なくなります

### 自分で作る場合

`./make_dist.sh` で同じ zip を手元で作れます（→ [配布セットを作る](#配布セットを作る)）。
できた zip はメール・USB・クラウドなど、どの経路で渡しても構いません（macOS 版は
チケットをステープル済みなので、オフラインでも Gatekeeper を通過します）。

macOS でのインストール先は `~/Library/Audio/Plug-Ins/VST3/`、Windows では
`C:\Program Files\Common Files\VST3\` です。

### GitHub Actions の Artifacts について（配布用ではありません）

> **警告**: Actions の **Artifacts は CI がビルドしただけの未署名バイナリ**です。
> macOS では Gatekeeper に「開発元を確認できません」「壊れているため開けません」
> とブロックされます。署名証明書は各自のキーチェーンにあり CI には無いためです。
> **動作確認用、または Windows ビルドを取り出す用途**としてのみ使ってください。

```bash
# Windows ビルドを CI から取り出す場合 (make_dist.sh も内部でこれを使います)
gh run download -R artcode49-prog/artcode303 -n Acid303-Windows-VST3
```

どうしても Artifacts の macOS 版を使う場合は、展開後に隔離属性を外します:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Acid303.vst3
```

> **注意**: このバイナリは AGPLv3 で配布されます。ソースは本リポジトリです。

---

## 配布セットを作る

他の人に渡せる「一式」を `./make_dist.sh` で作れます。

```bash
./make_dist.sh
```

- macOS 版は `Build/` のビルド済み VST3 を使います（先にビルドが必要です）
- Windows 版は `gh` で最新の CI 成果物を自動取得します
  （手元に Windows ビルドがある場合は `./make_dist.sh path/to/Acid303.vst3`）

出力:

```
Dist/
  Acid303-1.0.0/
    INSTALL-ja.txt                       簡易インストール手順 (日本語 / これを渡せば OK)
    INSTALL-en.txt                       簡易インストール手順 (英語)
    README.md                            このドキュメント
    LICENSE                              AGPLv3 全文
    SOURCE.txt                           ソースコードの入手先
    macOS/Acid303.vst3                   Universal Binary (arm64 + x86_64)
    Windows/Acid303.vst3                 x64
    demo/*.wav                           演奏サンプル (16bit / 48kHz)
    source/Acid303-1.0.0-source.tar.gz   対応するソース (AGPLv3 用)
  Acid303-1.0.0-macOS.zip                macOS 版だけをまとめた zip (単体で完結)
  Acid303-1.0.0-Windows.zip              Windows 版だけをまとめた zip (単体で完結)
  Acid303-1.0.0-macOS-Windows.zip        両 OS をまとめた zip
```

各 zip は `LICENSE` / `SOURCE.txt` / ソース / デモ WAV を含むので、単体で配布
できます。**「このファイルは安全ではありません」と警告が出る OS を切り分けたい
ときは、プラットフォーム別の zip を個別に試してください**（macOS 版は署名・
公証済み、Windows 版はコード署名証明書が無いため未署名です）。

`Dist/` は git 管理外なので、リポジトリを汚しません。

### 署名と公証（Gatekeeper 対策）

macOS では、**署名のないプラグインはダウンロード後に Gatekeeper にブロック**
されます（「開発元を確認できません」「壊れているため開けません」）。
`make_dist.sh` は `Developer ID Application` 証明書がキーチェーンにあれば
**自動で署名**します（現在: `Ryuya Yanagisawa (BDKDD2PV79)`、**この Mac では
署名・公証とも設定済み**）。

公証 (notarization) まで行うと、他の Mac でも警告なしで使えるようになります。
初回だけ次を実行して認証情報をキーチェーンに保存してください（パスワードは
[appleid.apple.com](https://appleid.apple.com) で発行する**アプリ用パスワード**）:

```bash
xcrun notarytool store-credentials ACID303 \
  --apple-id <あなたの Apple ID> --team-id BDKDD2PV79
```

以降は次のように実行すると、署名 → 公証 → ステープルまで自動で行います:

```bash
ACID303_NOTARY_PROFILE=ACID303 ./make_dist.sh
```

署名だけの場合は `./make_dist.sh` のみで構いません（その場合、受け取った人が
隔離属性を外す必要があります → `INSTALL-ja.txt` / `INSTALL-en.txt` 参照）。

> Windows 側はコード署名証明書が無いため未署名です。受け取った人には
> SmartScreen の「詳細情報 → 実行」を案内してください。

> AGPLv3 ではバイナリと一緒に **対応するソース** を提供する必要があるため、
> セットには `source/` にソースのスナップショットを同梱しています。

---

## ビルド方法

### 必要環境
- CMake 3.22 以上
- C++17 コンパイラ (macOS は Xcode Command Line Tools)
- JUCE 8.0.4

### 手順

JUCE はローカルのチェックアウトを `JUCE_PATH` で指定するか、`../JUCE` などの
候補ディレクトリ、無ければ CMake が自動でダウンロードします。

```bash
# 一括ビルド (VST3 + オフラインレンダラー + 検証ツール)
./build.sh

# プラグインを ~/Library/Audio/Plug-Ins/VST3 へインストール
./build.sh --install

# ビルド後に DSP セルフテストと VST3 ホストテストを実行
./build.sh --tests
```

手動でビルドする場合:

```bash
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release \
      -DJUCE_PATH=/path/to/JUCE
cmake --build Build -j"$(sysctl -n hw.ncpu)"
```

> **重要 (macOS / Intel な `cmake` を使う場合)**
> `/usr/local/bin/cmake` など Rosetta 上の Intel バイナリを使うと、既定で
> Intel 専用バイナリが生成され、ネイティブ arm64 の DAW でロードできません。
> このプロジェクトの `CMakeLists.txt` は macOS で自動的に
> `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` を設定し、Universal Binary を
> 作るようにしてあります。

### Windows

Windows では `build.bat` を使います（Visual Studio 2022 を想定）。
JUCE を自動ダウンロードするので `git` が必要です。

```bat
REM 一括ビルド (Release)
build.bat

REM 共通 VST3 フォルダ (%CommonProgramFiles%\VST3) へインストール
build.bat --install

REM ビルド後にセルフテスト / VST3 ホストテストを実行
build.bat --tests
```

手動・CMake プリセットを使う場合:

```bat
cmake --preset windows
cmake --build --preset windows
REM ローカルに JUCE がある場合
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:/path/to/JUCE
cmake --build Build --config Release --parallel
```

> Windows では JUCE の VST3 は **MSVC** でのビルドを推奨します。

### CMake プリセット

`CMakePresets.json` に構成済みプリセットがあります。

| プリセット | 内容 |
|------------|------|
| `macos-universal` | Ninja / Release / arm64 + x86_64 |
| `windows` | Visual Studio 2022 / x64 / Release |

```bash
cmake --preset macos-universal && cmake --build --preset macos-universal
```

### CI (GitHub Actions)

`.github/workflows/build.yml` で **macOS (Universal) と Windows (x64)** の
VST3 を自動ビルドし、成果物 (artifact) としてアップロードします。
`push` / `pull_request` / 手動 (`workflow_dispatch`) で実行されます。

```bash
# このフォルダは git リポジトリ済みで origin 設定済み
git remote -v      # origin  https://github.com/artcode49-prog/artcode303.git
git push origin main
```

push すると GitHub の **Actions** タブでビルドが走り、完了後に
`Acid303-macOS-VST3` と `Acid303-Windows-VST3`（いずれも zip）を
ダウンロードできます。zip には `Acid303.vst3` フォルダごと入っています。

- macOS: `Acid303.vst3` を `~/Library/Audio/Plug-Ins/VST3/` へ
- Windows: `Acid303.vst3` を `C:\Program Files\Common Files\VST3\` へ

> このマシン (macOS) には MSVC / MinGW が無いため Windows バイナリのローカル
> 生成はできません。Windows 版は上記 CI、または Windows 実機での `build.bat`
> 実行で生成してください。

ビルド成果物:

```
# macOS
Build/Acid303_artefacts/Release/VST3/Acid303.vst3   ← プラグイン (要インストール)
Build/tb303_render                                   ← オフラインレンダラー / セルフテスト
Build/tb303_hosttest_artefacts/Release/tb303_hosttest
Build/tb303_screenshot_artefacts/Release/tb303_screenshot

# Windows
Build\Acid303_artefacts\Release\VST3\Acid303.vst3
Build\Release\tb303_render.exe
```

---

## 検証用ツール

### 1. `tb303_render` — DSP コアのレンダラー / 数値検証

JUCE 不要。単体でコンパイルして使えます。

```bash
# 数値セルフテスト (発振器 / フィルタ / シーケンサー)
./Build/tb303_render selftest

# WAV 書き出し (16 bit / 48 kHz)
./Build/tb303_render --out out.wav --pattern acid --bpm 130 --seconds 8 \
                     --cutoff 0.30 --reso 0.55 --envmod 0.70 --decay 0.32 \
                     --accent 0.75 --wave 1.0 --drive 0.2 --volume 0.8 --analyze
```

主なオプション: `--out` `--pattern` (`acid` / `rave` / `funk` / `hi` / `empty`)
`--bpm` `--seconds` `--cutoff` `--reso` `--envmod` `--decay` `--accent`
`--wave` `--drive` `--volume` `--analyze`。

### 2. `tb303_hosttest` — パッケージ済み VST3 の読み込み検証

ビルドした `.vst3` を JUCE のプラグインホスト経由で実際にロードし、音を
レンダリングして WAV に書き出します (DAW 不要)。状態保存 / 復元ラウンドトリップも
確認します。

```bash
./Build/tb303_hosttest_artefacts/Release/tb303_hosttest \
    Build/Acid303_artefacts/Release/VST3/Acid303.vst3 /tmp/check.wav 6 130
```

### 3. `tb303_screenshot` — GUI スモークテスト

プラグインのエディタを生成して PNG に書き出します (DAW 不要)。

```bash
./Build/tb303_screenshot_artefacts/Release/tb303_screenshot panel.png 900 560
```

---

## 検証結果

`tb303_render selftest` はすべて PASS します。

| 検証項目 | 結果 |
|----------|------|
| カットオフの追従 | ポール周波数の `0.435 ×` で -3 dB（理論値 `sqrt(2^(1/4)-1)` と一致） |
| レゾナンスのピーク | res 0.35 / 0.6 / 0.8 で DC に対し +4.0 / +11.5 / +24.3 dB |
| 自励発振 | 振幅が約 0.6983 で有限に収まる |
| シーケンサーのタイミング | 16 分グリッド、最大誤差 1 サンプル (0.021 ms) |
| スウィング | 2:1 (8000 / 4000 サンプル) でもグリッド内に収まり、テンポはずれない |
| VCO のバンドリミット | 非ハーモニック成分 -32.3 dB (矩形) / -31.1 dB (ノコギリ) |
| 出力レベル | ピーク 0.644 / RMS 0.1614（クリップせず健全） |
| PLAY / ホスト同期のトランスポート | 6 ケース（INT の開始 / 停止、HOST の追従、PLAY での停止、ホスト再開での自動再開）| 

パッケージ済み VST3 は、同一設定のネイティブレンダラーと一致する出力を返し
(ピーク 0.643 / RMS 0.1360)、状態の保存 / 復元も完全に往復します (2371 バイト)。
ホストトランスポートの停止 → 再開も end-to-end で検証しています
(停止後 2 秒時点の残響ピーク 0.00017、再開後ピーク 0.633)。

さらに **pluginval (strictness level 8)** を通過します (`SUCCESS`)。
バスのレイアウト、オートメーションパラメータ、エディタ、パラメータのスレッド
安全性、バックグラウンドスレッド状態、パラメータファズを含む全テストに合格して
います。

macOS の配布ビルドは **Developer ID で署名 + Apple の公証 (notarization) 済み**
です。ブラウザからダウンロードしたときと同じ「隔離属性」を付けた状態でも、
Gatekeeper を通過します。

```
$ spctl -a -vvv -t install Acid303.vst3
Acid303.vst3: accepted
source=Notarized Developer ID
origin=Developer ID Application: Ryuya Yanagisawa (BDKDD2PV79)
```

---

## ファイル構成

```
LICENSE                    GNU AGPL v3 ライセンス全文
CMakeLists.txt             ビルド定義 (Universal Binary 強制を含む)
CMakePresets.json          macOS / Windows の CMake プリセット
build.sh                   macOS: ビルド / インストール / テストのヘルパー
build.bat                  Windows: ビルド / インストール / テストのヘルパー
make_dist.sh               配布セット Dist/ を組み立てる (両 OS の VST3 +
                           ドキュメント + デモ WAV + ソーススナップショットを zip 化)
INSTALL-ja.txt / INSTALL-en.txt  配布セットに同梱する簡易インストール手順（日 / 英）
.github/workflows/build.yml  CI: macOS (Universal) と Windows の VST3 を自動ビルド

Dist/                      配布セットの出力先 (git 管理外 / ./make_dist.sh で生成)

Source/
  PluginProcessor.h/.cpp   VST3 ラッパー: 15 パラメータ, パターン状態, MIDI,
                           ホスト同期, 10 ファクトリープリセット, ユーザープリセット
  PluginEditor.h/.cpp      GUI: AcidLookAndFeel, SwitchButton, StepGrid(16 ステップ),
                           MIDI キーボード, トランスポート / プリセットバー
  gui/
    logo.png               ヘッダのロゴ
    knob.png               128 フレームのノブフィルムストリップ
    sidewood.png           左右の木製サイドパネル
  dsp/
    Tb303Common.h          共通ユーティリティ (softLimit, fastTanh, polyBlep, 各種マッピング)
    Tb303Osc.h             VCO (バンドリミット矩形 / ノコギリ, モーフ)
    Tb303Filter.h          ZDF 4 極ダイオードラダー VCF
    Tb303Env.h             Decay / Accent エンベロープ, Smoother
    Tb303Voice.h           VCO→VCF→VCA ボイス
    Tb303Params.h          パラメータブロックとノブ→物理量マッピング
    Tb303Pattern.h         16 ステップパターンとファクトリーパターン
    Tb303Transport.h       PLAY ボタン / ホスト同期のトランスポート判定
    Tb303Engine.h          内蔵シーケンサー + オーバーサンプリング + 出力段

Tools/
  tb303_render.cpp         オフラインレンダラー / 数値セルフテスト
  host_test.cpp            VST3 読み込み + レンダリング + 状態往復テスト
  screenshot.cpp           エディタを PNG に書き出す GUI スモークテスト

Presets/
  demo_*.wav               検証用デモ (16 bit / 48 kHz)
```

---

## ライセンス

本プロジェクトは **GNU Affero General Public License v3.0 またはそれ以降
(AGPL-3.0-or-later)** で配布します。全文は [`LICENSE`](LICENSE) を参照してください。

- 本プロジェクトは **JUCE 8 の AGPLv3 オプション**を利用しています（商用
  ライセンスは不要）。
- `Source/` 以下のコードはすべてオリジナル実装で、Roland の回路図・
  ファームウェア・音色データ等は一切含みません。
- 各ソースファイルの先頭に SPDX ヘッダを付与しています。

### AGPLv3 で配布するということ

- 誰かにバイナリ（VST3 など）を配るときは、**対応するソースコード
  (Corresponding Source) も提供**する必要があります。もっとも簡単な方法は
  **リポジトリを public にして、バイナリと一緒にその URL を案内する**ことです。
- 改変版を第三者が使う場合も同様に AGPLv3 が適用されます（コピーレフト）。
- ソフトウェアは**無保証**で提供されます（AGPLv3 §15, §16）。

### 配布時のチェックリスト

- [x] リポジトリを **public** にする（ソース提供のため）
      → 公開済み: <https://github.com/artcode49-prog/artcode303>
- [x] `LICENSE`（AGPLv3 全文）をバイナリ配布物に同梱する
      → CI が成果物 zip に `LICENSE` と `SOURCE.txt` を同梱
- [x] 著作権表示と「無保証」表示を保持する
      → 各ソースの SPDX ヘッダ + `LICENSE`
- [x] 商標 ("Roland" / "TB-303") を使っていないか確認する
      → 製品名は "ACID-303"（プラグイン名は `Acid303`）
- [ ] macOS: `codesign` + `notarytool` で署名・公証する（警告回避）
- [ ] Windows: コード署名証明書で署名する（推奨）

> 現状のバイナリは**未署名**です。他人に配る場合は macOS の署名・公証を
> 行うと Gatekeeper の警告が出なくなります（個人利用ならそのままで可）。

---

## 既知の注意点

- **クロック源は既定で Host です。** DAW の再生 / 停止に追従します。DAW の
  再生に追従しない場合はクロック源が HOST になっているか確認してください。
- PLAY ボタンは **常に停止ボタンとして機能します。** Host モードで DAW が再生中でも
  PLAY を押せばシーケンサーが止まり、もう一度押すと再開します。DAW の再生を
  停止→再開すると自動で再開します。Host モードで DAW が停止中の間はクロックが
  無いため、PLAY を押しても発音しません。
- DECAY ノブはエンベロープ長（VCF / VCA 共通）を直接制御します。長い DECAY に
  すると停止後もしばらく余韻が残ります（ハードウェア同様）。
- 一部のホストは JUCE ホスト側の `MIDI CC ...` パラメータも列挙しますが、
  これはプラグイン自身の 15 パラメータではなく、実 DAW では公開されません。
- **Presets/*.wav と検証結果は 16 bit / 48 kHz です。** ネイティブレンダラーの
  出力は、同一設定の VST3 出力とビット比較可能なレベルで一致します。

### 修正履歴

- **GitHub Release を公開**：配布用の zip を
  <https://github.com/artcode49-prog/artcode303/releases/latest> に置きました。
  ブラウザの「不審なダウンロード」警告はコード署名ではなく Google Safe Browsing /
  SmartScreen の**評判**によるものなので、信頼済みホスト (github.com) からの
  ダウンロードを入口にしています。
- **配布経路を `Dist/` の署名済み zip に一本化**：CI の Artifacts は未署名で
  Gatekeeper にブロックされるため、配布用の案内から外し「開発用」として
  区別しました。渡すのは `Dist/Acid303-<version>-*.zip` だけです。
- **プラットフォーム別 zip を追加**：`Acid303-1.0.0-macOS.zip` /
  `Acid303-1.0.0-Windows.zip` を追加しました。どちらのビルドが「安全では
  ありません」と警告されるのか切り分けられます。
- **macOS ビルドに Developer ID 署名 + 公証を追加**：ad-hoc 署名のままだと、
  ダウンロードした人が Gatekeeper に「開発元を確認できません」とブロックされて
  しまいました。`make_dist.sh` が `Developer ID Application` 証明書で署名
  （hardened runtime + タイムスタンプ）し、`notarytool` の認証情報
  (`ACID303_NOTARY_PROFILE`) があれば **Apple の公証 + チケットのステープル**
  まで行います。公証済みのため、隔離属性が付いたダウンロードでも
  `accepted / source=Notarized Developer ID` になります。`INSTALL-ja/en.txt` に
  Gatekeeper / SmartScreen の対処も追記しました。
- **配布セット (`./make_dist.sh`)**：両 OS の VST3 + ドキュメント + デモ WAV +
  ソーススナップショットを `Dist/` にまとめて zip 化。`INSTALL-ja.txt` /
  `INSTALL-en.txt`（日 / 英のインストール手順）を同梱し、
  受け取った人がそのまま使える形にしました。
- **PLAY ボタンで停止できるように修正**：Host モードでは PLAY ボタンの状態が
  DAW の再生状態に強制同期され、`sequencerRunning` が再生判定に使われていな
  かったため、DAW が再生中だと PLAY を押しても止められませんでした。トランス
  ポート判定を `Source/dsp/Tb303Transport.h` の `TransportGate` に切り出し、
  **Host モードでも PLAY で停止 / 再開**できるようにしました（DAW を停止→
  再生し直すと自動で再開）。ロジックは JUCE 非依存なのでセルフテストで検証して
  います（6 ケース）。
- **AGPLv3 で配布開始**：`LICENSE`（AGPLv3 全文）と各ソースの SPDX ヘッダを
  追加し、リポジトリを **public** 化。CI の成果物 zip に `LICENSE` と
  `SOURCE.txt` を同梱するようにしました。
- **Windows (x64) 版 VST3 に対応**：GitHub Actions で macOS (Universal) と
  Windows (x64) を自動ビルドし、`Acid303.vst3` フォルダごと zip 化して
  ダウンロードできるようにしました。
- **「停止しても鳴り続ける」問題を修正**：`DECAY` ノブが実際にはエンベロープへ
  反映されていませんでした（ハードコードの 0.4 秒固定）。`decayKnobToSeconds()`
  を配線し、停止後の余韻が DECAY 相応（既定で約 0.14 秒）になりました。
- **クロック源の既定を Internal → Host に変更**：DAW の停止で音が止まります。
- エディタにロゴ / ノブ画像 / 木製サイドパネルを追加。
