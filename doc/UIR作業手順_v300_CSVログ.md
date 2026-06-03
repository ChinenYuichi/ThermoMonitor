# v3.0.0 UIR作業手順 - ショットCSVログ チェックボックス追加

## 追加するコントロール

| 項目 | 設定値 |
|---|---|
| コントロール種別 | Check Box |
| Variable Name | `CSV_LOG_ENABLE` |
| Label | `ショットCSVログ` |
| Callback Function | `CsvLogEnable_Callback` |
| Default Value | `0`（OFF） |

---

## 手順

### 1. UIRを開く

CVI で `ThermoMonitor.uir` を開く。

### 2. チェックボックスを追加

パネル上の空きスペース（ログエリア付近推奨）を右クリック
→「**Create Control**」→「**Check Box**」を選択して配置。

### 3. プロパティを設定

チェックボックスをダブルクリックしてプロパティ画面を開き、以下を設定：

| プロパティ | 設定値 |
|---|---|
| **Variable Name** | `CSV_LOG_ENABLE` |
| **Label** | `ショットCSVログ` |
| **Callback Function** | `CsvLogEnable_Callback` |
| **Default Value** | `0` |

### 4. UIRを保存

`Ctrl+S` で保存。

### 5. ビルド

`ThermoMonitor.prj` をビルドする。

---

## ビルド確認ポイント

`ThermoMonitor.h`（CVI自動生成）に以下が追加されていること：

```c
#define PANEL_PANEL_CSV_LOG_ENABLE  (任意の数値)
```

---

## 動作確認

1. ThermoMonitor を起動・接続・モニタリング開始
2. 「ショットCSVログ」チェックボックスを ON にする
3. SHOT START が検出されると `C:\Users\quest\ThermoMonitor_shot_YYYYMMDD_HHMMSS.csv` が作成される
4. Timer毎（500ms）に温度データが追記される
5. SHOT STOP 検出後も記録継続
6. 次の SHOT START、またはチェックボックス OFF、またはモニタリング停止でファイルがクローズされる

---

## CSVファイルのイメージ

```csv
時刻,CH0(℃),CH1(℃),CH2(℃),CH3(℃),マーカー
11:11:00,25.2,25.3,25.2,25.0,SHOT START
11:11:00,26.1,25.3,25.2,25.0,
11:11:01,45.3,25.3,25.2,25.0,
...
11:11:52,124.7,25.4,25.3,25.1,SHOT STOP
11:11:52,121.2,25.4,25.3,25.1,
...（温度下降まで継続記録）
```

Excelで開いてグラフ化できます（UTF-8 BOM付きのため文字化けなし）。
