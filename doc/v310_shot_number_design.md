# ThermoMonitor v3.1.0 改修設計書
## ショットナンバー表示機能追加

作成日: 2026-05-13

---

## 1. 改修概要

| 項目 | 内容 |
|---|---|
| 対象バージョン | v3.0.3 → v3.1.0 |
| 対象ファイル | `ThermoMonitor.c` のみ |
| 改修内容 | SHOT START 検出時に shotNumber.txt を取得し、ログにショットナンバーを追加表示 |

---

## 2. 取得元 URL

```
http://csv02.exp.triam.kyushu-u.ac.jp/expinfo/shotNumber.txt
```

- `time.txt` と同一ホスト・同一パスプレフィックス
- HTTP/1.0 GET で取得（既存の `FetchShotTime()` と同じパターン）
- 内容はショットナンバーの整数値（例: `12345`）

---

## 3. ログ出力変更

### 変更前
```
[SHOT START] 14:23:05  CH0=120.3℃  CH1=115.2℃  CH2=118.7℃  CH3=122.1℃
```

### 変更後
```
[SHOT NUMBER] 12345
[SHOT START] 14:23:05  CH0=120.3℃  CH1=115.2℃  CH2=118.7℃  CH3=122.1℃
```

- `[SHOT NUMBER]` を `[SHOT START]` の **直前** に出力
- 取得失敗時は `[SHOT NUMBER] (取得失敗)` と表示し、SHOT START は通常通り継続

---

## 4. コード変更詳細

### 4-1. グローバル変数追加

**場所:** `g_fetchServerClosed` の宣言付近（:148 付近）

```c
static volatile int g_fetchShotNumServerClosed = 0; /* shotNumber TCP切断フラグ */
```

---

### 4-2. TCP コールバック関数追加

**場所:** `ShotTimeTCPCB()` の直後（:800 付近）

```c
static int CVICALLBACK ShotNumberTCPCB(unsigned int handle, int event, int error, void *cbData)
{
    if (event == TCP_DISCONNECT)
        g_fetchShotNumServerClosed = 1;
    return 0;
}
```

- `ShotTimeTCPCB()` と同じパターン
- shotNumber 専用の切断フラグを立てる

---

### 4-3. `FetchShotNumber()` 関数追加

**場所:** `FetchShotTime()` の直後（:847 付近）

```c
/*============================================================================
 * FetchShotNumber - 実験ネットワークから shotNumber.txt を取得
 *
 * 取得成功時: ショットナンバー（整数）を返す
 * 取得失敗時: -1 を返す
 *============================================================================*/
static int FetchShotNumber(void)
{
    unsigned int handle  = 0;
    char         req[256];
    char         buf[512];
    int          shotNum = -1;
    char        *body;

    g_fetchShotNumServerClosed = 0;

    sprintf(req,
            "GET /expinfo/shotNumber.txt HTTP/1.0\r\n"
            "Host: csv02.exp.triam.kyushu-u.ac.jp\r\n"
            "Connection: close\r\n"
            "\r\n");

    if (ConnectToTCPServer(&handle, 80,
                           "csv02.exp.triam.kyushu-u.ac.jp",
                           ShotNumberTCPCB, NULL, 100) < 0)
        return -1;

    if (ClientTCPWrite(handle, req, (int)strlen(req), 100) < 0)
        goto cleanup;

    memset(buf, 0, sizeof(buf));
    ClientTCPRead(handle, buf, (int)sizeof(buf) - 1, 100);

    body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        if (strlen(body) == 0) {
            char bodyBuf[64];
            memset(bodyBuf, 0, sizeof(bodyBuf));
            ClientTCPRead(handle, bodyBuf, sizeof(bodyBuf) - 1, 100);
            body = bodyBuf;
        }
        if (sscanf(body, "%d", &shotNum) != 1)
            shotNum = -1;
    }

cleanup:
    if (!g_fetchShotNumServerClosed)
        DisconnectFromTCPServer(handle);
    return shotNum;
}
```

---

### 4-4. `UpdateShotState()` の変更

**場所:** SHOT START 検出ブロック（:867 付近）

```c
if (g_prevShotTime < 0.0 && tNow >= 0.0) {
    /* SHOT START 検出 */
    int shotNum;

    /* ショットナンバーを取得してログ出力（START より前） */
    shotNum = FetchShotNumber();
    if (shotNum >= 0)
        LogMessageFmt("[SHOT NUMBER] %d", shotNum);
    else
        LogMessageFmt("[SHOT NUMBER] (取得失敗)");

    g_postShotTracking = 1;
    /* ... 以降は既存コードのまま ... */
```

**変更前（既存）:**
```c
    LogMessageFmt("[SHOT START] %s  CH0=%.1f℃  ...", timeStr, ...);
```

**変更後（shotNum 取得後）:**
```c
    /* ショットナンバー取得・表示 */
    shotNum = FetchShotNumber();
    if (shotNum >= 0)
        LogMessageFmt("[SHOT NUMBER] %d", shotNum);
    else
        LogMessageFmt("[SHOT NUMBER] (取得失敗)");

    LogMessageFmt("[SHOT START] %s  CH0=%.1f℃  CH1=%.1f℃  CH2=%.1f℃  CH3=%.1f℃",
                  timeStr,
                  g_lastTemp[0], g_lastTemp[1],
                  g_lastTemp[2], g_lastTemp[3]);
```

---

### 4-5. 内部関数プロトタイプ追加

**場所:** プロトタイプ宣言ブロック（:169 付近）

```c
static int    CVICALLBACK ShotNumberTCPCB(unsigned int handle, int event, int error, void *cbData);
static int    FetchShotNumber(void);
```

---

### 4-6. ヘッダコメント追加

```c
 * 変更点 (v3.0.3 → v3.1.0)  26.05.13:
 *   - FetchShotNumber(): shotNumber.txt を HTTP GET で取得
 *   - UpdateShotState(): SHOT START 検出時に [SHOT NUMBER] をログ出力（START の直前）
 *   - 取得失敗時は "(取得失敗)" と表示し SHOT START は継続
```

---

## 5. 変更ファイル一覧

| ファイル | 変更内容 |
|---|---|
| `ThermoMonitor.c` | グローバル変数1件・関数2件追加・UpdateShotState変更・プロトタイプ追加・ヘッダ更新 |

`.uir` ファイル・`.h` ファイルの変更は **不要**。

---

## 6. 期待動作フロー

```
Timer_Callback (500ms)
  └─ UpdateShotState()
       └─ FetchShotTime() → tNow >= 0.0 (SHOT START検出)
            ├─ FetchShotNumber() → 12345
            ├─ LogMessageFmt("[SHOT NUMBER] 12345")       ← 新規追加
            └─ LogMessageFmt("[SHOT START] 14:23:05 ...")  ← 既存
```

---

## 7. 注意事項

- `FetchShotNumber()` は SHOT START 検出時のみ呼び出す（毎Tick呼ばない）
- タイムアウト 100ms（`FetchShotTime()` と同じ）。通信に失敗しても SHOT START ログは必ず出力する
- `time.txt` と `shotNumber.txt` は別 TCP 接続。`g_fetchShotNumServerClosed` は専用フラグを使用して干渉しない

---

*この設計書に問題なければ実装に進みます。*
