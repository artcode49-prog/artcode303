# ACID-303

Roland TB-303 のアナログ挙動をモデリングした VST3 シンセサイザーです。
4 極ダイオードラダー VCF / 内蔵 16 ステップシーケンサー / アクセント・スライド /
オリジナル準拠の GUI / ファクトリープリセットを備え、さらに **DAW が無くても
音を検証できる独立系レンダラー** を同梱しています。

- プラグイン形式: **VST3** (Instrument / Synth)
- 対応: macOS (Universal Binary: arm64 + x86_64), 44.1 / 48 / 96 kHz など
- 内部処理: 4 倍オーバーサンプリング (VCF の非線形帰還と VCA のエイリアスを抑圧)
- DSP コアは **JUCE 非依存の純 C++** (`Source/dsp/`) なので、プラグインを
  ビルドしなくても単体でテストできます。

---

## 目次
1. [音作りの構成（サウンドエンジン）](#サウンドエンジン)
2. [パラメータ一覧](#パラメータ一覧)
3. [内蔵シーケンサーの使い方](#シーケンサーの使い方)
4. [GUI の説明](#gui-の説明)
5. [ビルド方法](#ビルド方法)
6. [検証用ツール（レンダラー）](#検証用ツール)
7. [検証結果](#検証結果)
8. [ファイル構成](#ファイル構成)
9. [既知の注意点](#既知の注意点)

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
| **HOST**（既定） | DAW のトランスポート / BPM に追従。PLAY ボタンは効きません |
| **INT** | 内部クロックで再生。GUI の PLAY ボタンで開始 / 停止 |

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
# このフォルダは git リポジトリ済み（初回コミット済み）
git remote add origin https://github.com/<あなたのユーザー>/<リポジトリ>.git
git branch -M main
git push -u origin main
```

push すると GitHub の **Actions** タブでビルドが走り、完了後に
`Acid303-macOS-VST3` と `Acid303-Windows-VST3` をダウンロードできます。

- macOS: `Acid303.vst3` を `~/Library/Audio/Plug-Ins/VST3/` へ
- Windows: 展開して `C:\Program Files\Common Files\VST3\Acid303.vst3` へ

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

パッケージ済み VST3 は、同一設定のネイティブレンダラーと一致する出力を返し
(ピーク 0.643 / RMS 0.1360)、状態の保存 / 復元も完全に往復します (2371 バイト)。

さらに **pluginval (strictness level 8)** を通過します (`SUCCESS`)。
バスのレイアウト、オートメーションパラメータ、エディタ、パラメータのスレッド
安全性、バックグラウンドスレッド状態、パラメータファズを含む全テストに合格して
います。

---

## ファイル構成

```
CMakeLists.txt             ビルド定義 (Universal Binary 強制を含む)
CMakePresets.json          macOS / Windows の CMake プリセット
build.sh                   macOS: ビルド / インストール / テストのヘルパー
build.bat                  Windows: ビルド / インストール / テストのヘルパー
.github/workflows/build.yml  CI: macOS (Universal) と Windows の VST3 を自動ビルド

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
    Tb303Engine.h          内蔵シーケンサー + オーバーサンプリング + 出力段

Tools/
  tb303_render.cpp         オフラインレンダラー / 数値セルフテスト
  host_test.cpp            VST3 読み込み + レンダリング + 状態往復テスト
  screenshot.cpp           エディタを PNG に書き出す GUI スモークテスト

Presets/
  demo_*.wav               検証用デモ (16 bit / 48 kHz)
```

---

## 配布とライセンス

### 配布は可能か

**技術的には可能ですが、ライセンスの確認が必須です。**

- **JUCE** — 本プラグインは JUCE 8 を静的リンクしています。JUCE は
  **AGPLv3** と **商用ライセンス** のデュアルライセンスです。配布する場合、
  次のいずれかが必要です。
  1. **AGPLv3 で配布**：プラグイン全体のソースコードを AGPLv3 で公開する
     （無償。ソース開示が条件）。
  2. **JUCE 商用ライセンス**を購入する（クローズドソース配布が可能）。
  AGPLv3 のままクローズドソースで配布することはできません。
- **本プロジェクトのコード** — `Source/` 以下はすべてオリジナル実装です。
  Roland の回路図・ファームウェア・音色データ等は一切含みません。
- **商標** — "Roland" / "TB-303" は Roland の商標です。製品名にこれらを
  使わないでください（本製品は "ACID-303" であり商標使用に当たりません）。
  あの「スマイリーフェイス」などのトレードドレスも避けてください。
- **macOS の公証** — 他の Mac で警告なく使ってもらうには Apple Developer ID
  での署名と公証 (notarization) が必要です。
- **Windows の署名** — 配布時はコード署名証明書で署名することを推奨します。

### チェックリスト

- [ ] JUCE のライセンス方針を決める（AGPLv3 公開 or 商用ライセンス購入）
- [ ] ライセンス全文 (`LICENSE`) とサードパーティ表記を同梱する
- [ ] macOS: `codesign` + `notarytool` で署名・公証する
- [ ] Windows: コード署名証明書で署名する
- [ ] 商標 ("Roland" / "TB-303") を使っていないか確認する


---

## 既知の注意点

- **クロック源は既定で Host です。** DAW の再生 / 停止に追従します。DAW の
  再生に追従しない場合はクロック源が HOST になっているか確認してください。
- PLAY ボタンは Internal モードでのみ意味を持ちます。Host モードでは DAW の
  トランスポートに従います。
- DECAY ノブはエンベロープ長（VCF / VCA 共通）を直接制御します。長い DECAY に
  すると停止後もしばらく余韻が残ります（ハードウェア同様）。
- 一部のホストは JUCE ホスト側の `MIDI CC ...` パラメータも列挙しますが、
  これはプラグイン自身の 15 パラメータではなく、実 DAW では公開されません。
- **Presets/*.wav と検証結果は 16 bit / 48 kHz です。** ネイティブレンダラーの
  出力は、同一設定の VST3 出力とビット比較可能なレベルで一致します。

### 修正履歴

- **「停止しても鳴り続ける」問題を修正**：`DECAY` ノブが実際にはエンベロープへ
  反映されていませんでした（ハードコードの 0.4 秒固定）。`decayKnobToSeconds()`
  を配線し、停止後の余韻が DECAY 相応（既定で約 0.14 秒）になりました。
- **クロック源の既定を Internal → Host に変更**：DAW の停止で音が止まります。
- エディタにロゴ / ノブ画像 / 木製サイドパネルを追加。
