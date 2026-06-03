/*==============================================================================
 * ThermoMonitor.c
 * CPSN-SSI-4C 熱電対温度モニタ (v3.1.0 - UIR + ストリップチャート)
 *
 * 変更点 (v1.x → v2.4.0):
 *   - プログラム動的UI生成 (NewPanel/NewCtrl) → UIRファイル (LoadPanel) に変更
 *   - 温度トレンドグラフ (CTRL_STRIP_CHART) を追加
 *   - グラフ設定: 表示時間幅・チャネル表示ON/OFF・グラフクリア
 *
 * 変更点 (v2.4.0 → v2.5.0)  26.04.28:
 *   - RDP接続断対応: 自動再接続機能を追加 (5秒間隔・最大10回)
 *   - TriggerAutoReconnect() / AttemptReconnect() 関数追加
 *
 * 変更点 (v2.5.0 → v2.6.0)  26.04.30:
 *   - センサタイプ「Q型」追加 (Y軸レンジ: 0 ~ 150℃)
 *
 * 変更点 (v2.6.0 → v2.7.0)  26.05.01:
 *   - マルチモニター対応: ATTR_SCALE_CONTENTS_ON_RESIZE + ATTR_WINDOW_ZOOM 追加
 *   - 異なる解像度のモニターへドラッグ時に全コントロールが自動スケーリングされる
 *
 * 変更点 (v2.7.0 → v2.8.0)  26.05.01:
 *   - CH毎Y軸スケーリング機能追加: PANEL_CH0_YMAX/YMIN 等8コントロール追加
 *   - ApplyChYAxisScaling() 新規追加・Timer_Callbackから毎周期呼び出し
 *   - センサタイプ変更時は各CHコントロールに初期値をセット（全CH独立設定可能）
 *
 * 変更点 (v2.8.0 → v2.9.0)  26.05.01:
 *   - ショット連動温度ログ機能追加
 *   - FetchShotTime(): 実験ネットワーク time.txt を HTTP GET で取得
 *   - UpdateShotState(): SHOT START/STOP 検出・開始温度・ピーク温度をログ出力
 *
 * 変更点 (v2.9.0 → v2.9.1)  26.05.01:
 *   - LogMessage() にファイル追記ログを追加 (C:\Users\quest\ThermoMonitor_log.txt)
 *
 * 変更点 (v2.9.1 → v2.9.2)  26.05.01:
 *   - ピーク追跡を Timer_Callback に移動 (FetchShotTime の成否に依存しない)
 *   - FetchShotTime(): ボディ空時の2回目 ClientTCPRead 追加 (SHOT検出遅延バグ修正)
 *
 * 変更点 (v2.9.2 → v2.9.3)  26.05.04:
 *   - FetchShotTime(): ShotTimeTCPCB コールバック追加
 *   - サーバー先切断時 (HTTP Connection:close) に DisconnectFromTCPServer をスキップ
 *   - NON-FATAL RUN-TIME ERROR -12 を解消
 *
 * 変更点 (v2.9.3 → v2.9.4)  26.05.05:
 *   - UpdateShotState(): 早期リターン条件を (<= -9000.0) → (== -9999.0) に変更
 *   - 本番サーバーの待機値 (-36000.0) が早期リターンされ SHOT STOP 未検出のバグを修正
 *
 * 変更点 (v2.9.4 → v3.0.0)  26.05.05:
 *   - ショットCSVログ機能追加: チェックボックス連動で SHOT 毎に CSV ファイル自動生成
 *   - OpenShotCsvFile() / CloseShotCsvFile() / WriteCsvRow() 新規追加
 *   - SHOT START でCSV作成開始、SHOT STOP 後も記録継続、次SHOT STARTまたはOFF時にクローズ
 *   - 保存先: プログラムフォルダ\ThermoMonitor_shot_YYYYMMDD_HHMMSS.csv
 *
 * 変更点 (v3.0.0 → v3.0.1)  26.05.05:
 *   - CH毎ピーク温度到達時刻を追跡 (g_shotPeakTimeStr[])
 *   - SHOT STOP ログに [PEAK TIME] 行を追加出力
 *
 * 変更点 (v3.0.1 → v3.0.2)  26.05.05:
 *   - 真のピーク追跡 (g_truePeakTemp/g_truePeakTimeStr): SHOT START〜温度下降確定まで継続
 *   - [SHOT STOP] はSTOP時点の現在温度を表示
 *   - [PEAK TIME] はTimer_Callbackが5秒安定後 (TRUE_PEAK_STABLE_TICKS) に出力
 *   - SHOT STOP後も温度上昇継続するケースに対応 (CSVログの真の最高値を捕捉)
 *
 * 変更点 (v3.0.2 → v3.0.3)  26.05.07:
 *   - ThermoMonitor_log.txt 保存先をカレントディレクトリ（プログラムフォルダ）に変更
 *   - 起動時に GetDir() でパスを確定 → ユーザー名に依存しない
 *
 * 変更点 (v3.0.3 → v3.1.0)  26.05.13:
 *   - FetchShotNumber(): shotNumber.txt を HTTP GET で取得
 *   - UpdateShotState(): SHOT START 検出時に [SHOT NUMBER] をログ出力（START の直前）
 *   - 取得失敗時は "(取得失敗)" と表示し SHOT START は継続
 *============================================================================*/

#include <formatio.h>
#include <ansi_c.h>
#include <cvirte.h>
#include <userint.h>
#include <utility.h>
#include <stdarg.h>
#include "ThermoMonitor_app.h"   /* アプリ定数・プロトタイプ + ThermoMonitor.h(CVI生成) */
#include "modbus_tcp.h"
#include <tcpsupp.h>   /* ConnectToTCPServer / ClientTCPWrite / ClientTCPRead */

#ifndef nullChk
#define nullChk(fCall) if (!(fCall)) { error = -1; goto Error; }
#endif
#ifndef errChk
#define errChk(fCall) if ((error = (fCall)) < 0) goto Error;
#endif
#ifndef tryAttr
#define tryAttr(fCall) do { int _attrErr = (fCall); if (_attrErr < 0 && _attrErr != -46) {} } while (0)
#endif

/*--- センサタイプ名称 -------------------------------------------------------*/
const char *SensorTypeNames[SENSOR_TYPE_COUNT] = {
    "J型 (-100 ~ 1200℃)",
    "K型 (-100 ~ 1372℃)",
    "E型 (-100 ~ 1000℃)",
    "N型 (-100 ~ 1300℃)",
    "R型 (0 ~ 1768℃)",
    "S型 (0 ~ 1768℃)",
    "T型 (-100 ~ 400℃)",
    "Q型 (0 ~ 150℃)"
};

/*--- グラフ トレース色 (CH0=赤, CH1=黄緑, CH2=シアン, CH3=マゼンタ) --------*/
static const unsigned int g_traceColors[SSI4C_NUM_CHANNELS] = {
    0x00FF4040,   /* CH0: 赤     */
    0x0080FF00,   /* CH1: 黄緑   */
    0x0000FFFF,   /* CH2: シアン */
    0x00FF80FF    /* CH3: マゼンタ*/
};

/*--- チャネルUIコントロールID配列 -------------------------------------------*/
static const int ctrlTemp[SSI4C_NUM_CHANNELS] = {
    PANEL_TEMP_CH0, PANEL_TEMP_CH1, PANEL_TEMP_CH2, PANEL_TEMP_CH3
};
static const int ctrlLED[SSI4C_NUM_CHANNELS] = {
    PANEL_LED_CH0, PANEL_LED_CH1, PANEL_LED_CH2, PANEL_LED_CH3
};
static const int ctrlFault[SSI4C_NUM_CHANNELS] = {
    PANEL_FAULT_CH0, PANEL_FAULT_CH1, PANEL_FAULT_CH2, PANEL_FAULT_CH3
};
static const int ctrlChk[SSI4C_NUM_CHANNELS] = {
    PANEL_CHK_CH0, PANEL_CHK_CH1, PANEL_CHK_CH2, PANEL_CHK_CH3
};

/*--- 内部変数 ---------------------------------------------------------------*/
static int    panelHandle    = 0;
static int    g_stripHandle[SSI4C_NUM_CHANNELS] = {0, 0, 0, 0};   /* CH毎ストリップチャート */
static int    g_isMonitoring = 0;
static int    g_readCount    = 0;
static double g_lastTemp[SSI4C_NUM_CHANNELS] = {0.0, 0.0, 0.0, 0.0};

/* データ形式自動判定結果 */
static DataFormat g_currentDataFormat = DATA_FORMAT_RAW_SSI;  /* デフォルト */
static int        g_formatConfidence  = 0;   /* 判定一致チャンネル数 (0-4) */
static int        g_consecutiveErrors  = 0;   /* 連続通信エラーカウンタ */
static int        g_blinkState         = 0;   /* 点滅状態トグル (0/1) */
static int        g_isReconnecting     = 0;   /* 自動再接続中フラグ */
static int        g_reconnectAttempts  = 0;   /* 再接続試行回数 */

/* ショット連動ログ */
static int    g_shotActive            = 0;
static double g_prevShotTime          = -9999.0;

/* 真のピーク追跡（SHOT START〜下降確定まで継続） */
static int    g_postShotTracking      = 0;
static double g_truePeakTemp[SSI4C_NUM_CHANNELS];
static char   g_truePeakTimeStr[SSI4C_NUM_CHANNELS][32];
static int    g_peakStableCount       = 0;
#define TRUE_PEAK_STABLE_TICKS        10   /* 10ティック(5秒)更新なしでピーク確定 */

static volatile int g_fetchServerClosed    = 0; /* Connection:close でサーバーが先切断済み */
static volatile int g_fetchShotNumServerClosed = 0; /* shotNumber TCP切断フラグ */

/* ショットCSVログ */
static FILE  *g_csvFile               = NULL;
static char   g_csvFilePath[MAX_PATHNAME_LEN] = {0};
static char   g_csvRowMarker[32]      = {0}; /* 次のCSV行に付けるマーカー */
static char   g_logFilePath[MAX_PATHNAME_LEN] = {0};

#define MAX_CONSECUTIVE_ERRORS  3

/*--- 内部関数プロトタイプ ---------------------------------------------------*/
static int  LoadMainPanel(void);
static int  GetNumericCtrlAsDouble(int panel, int control, double *outVal);
static int  SetCtrlBooleanCompat(int panel, int control, int value);
static int  GetCtrlBooleanCompat(int panel, int control, int *outVal);
static void ApplyDefaultCtrlValues(void);
static void InitStripChart(void);
static void ApplyTimeRange(void);
static void UpdateStripChart(const ChannelData *chData);
static void   TriggerAutoReconnect(void);
static void   AttemptReconnect(void);
static int    CVICALLBACK ShotTimeTCPCB(unsigned int handle, int event, int error, void *cbData);
static double FetchShotTime(void);
static int    CVICALLBACK ShotNumberTCPCB(unsigned int handle, int event, int error, void *cbData);
static int    FetchShotNumber(void);
static void   UpdateShotState(void);
static void   OpenShotCsvFile(void);
static void   CloseShotCsvFile(void);
static void   WriteCsvRow(const char *marker);

/*============================================================================
 * LoadMainPanel - メインパネルの UIR 読み込み
 *
 * 旧命名(ThermoMonitor.uir) / 新命名(ThermoMonitor_ui.uir) の両方を試行する。
 *============================================================================*/
static int LoadMainPanel(void)
{
    int handle;
    ssize_t fileSize;

    if (GetFileInfo("ThermoMonitor.uir", &fileSize) >= 0) {
        handle = LoadPanel(0, "ThermoMonitor.uir", PANEL);
        if (handle >= 0) return handle;
    }

    return -94;  /* File not found */
}

/*============================================================================
 * GetNumericCtrlAsDouble - 数値コントロール値を double で取得
 *============================================================================*/
static int GetNumericCtrlAsDouble(int panel, int control, double *outVal)
{
    int dataType;
    int iVal;
    double dVal;

    if (!outVal) return -1;
    if (GetCtrlAttribute(panel, control, ATTR_DATA_TYPE, &dataType) < 0) return -1;

    if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT) {
        if (GetCtrlVal(panel, control, &dVal) < 0) return -1;
        *outVal = dVal;
        return 0;
    }

    if (dataType == VAL_INTEGER || dataType == VAL_SHORT_INTEGER ||
        dataType == VAL_UNSIGNED_INTEGER || dataType == VAL_CHAR) {
        if (GetCtrlVal(panel, control, &iVal) < 0) return -1;
        *outVal = (double)iVal;
        return 0;
    }

    return -1;
}

/*============================================================================
 * SetCtrlBooleanCompat / GetCtrlBooleanCompat - 真偽値入出力の型互換処理
 *============================================================================*/
static int SetCtrlBooleanCompat(int panel, int control, int value)
{
    int dataType;
    int iVal;
    double dVal;
    char strVal[8];

    if (GetCtrlAttribute(panel, control, ATTR_DATA_TYPE, &dataType) < 0) return -1;

    iVal = value ? 1 : 0;
    dVal = (double)iVal;

    if (dataType == VAL_STRING) {
        Fmt(strVal, "%i", iVal);
        return SetCtrlVal(panel, control, strVal);
    }

    if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT)
        return SetCtrlVal(panel, control, dVal);

    return SetCtrlVal(panel, control, iVal);
}

static int GetCtrlBooleanCompat(int panel, int control, int *outVal)
{
    int dataType;
    int iVal;
    double dVal;
    char strVal[64];

    if (!outVal) return -1;
    if (GetCtrlAttribute(panel, control, ATTR_DATA_TYPE, &dataType) < 0) return -1;

    if (dataType == VAL_STRING) {
        if (GetCtrlVal(panel, control, strVal) < 0) return -1;
        Scan(strVal, "%i", &iVal);
        *outVal = (iVal != 0);
        return 0;
    }

    if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT) {
        if (GetCtrlVal(panel, control, &dVal) < 0) return -1;
        *outVal = (dVal != 0.0);
        return 0;
    }

    if (GetCtrlVal(panel, control, &iVal) < 0) return -1;
    *outVal = (iVal != 0);
    return 0;
}

/*============================================================================
 * ApplyDefaultCtrlValues - UIコントロール型を確認して初期値を適用
 *
 * ThermoMonitor_ui.h と実UIRの型が不一致でも実行時クラッシュを回避する。
 *============================================================================*/
static void ApplyDefaultCtrlValues(void)
{
    int dataType;
    int iVal;

    if (GetCtrlAttribute(panelHandle, PANEL_IP_ADDR, ATTR_DATA_TYPE, &dataType) >= 0) {
        if (dataType == VAL_STRING)
            SetCtrlVal(panelHandle, PANEL_IP_ADDR, DEFAULT_IP);
        else
            LogMessage("警告: PANEL_IP_ADDR が文字列コントロールではありません。");
    }

    if (GetCtrlAttribute(panelHandle, PANEL_PORT, ATTR_DATA_TYPE, &dataType) >= 0) {
        if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT)
            SetCtrlVal(panelHandle, PANEL_PORT, (double)DEFAULT_PORT);
        else if (dataType == VAL_INTEGER || dataType == VAL_SHORT_INTEGER ||
                 dataType == VAL_UNSIGNED_INTEGER || dataType == VAL_CHAR) {
            iVal = DEFAULT_PORT;
            SetCtrlVal(panelHandle, PANEL_PORT, iVal);
        }
    }

    if (GetCtrlAttribute(panelHandle, PANEL_UNIT_ID, ATTR_DATA_TYPE, &dataType) >= 0) {
        if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT)
            SetCtrlVal(panelHandle, PANEL_UNIT_ID, (double)DEFAULT_UNIT_ID);
        else if (dataType == VAL_INTEGER || dataType == VAL_SHORT_INTEGER ||
                 dataType == VAL_UNSIGNED_INTEGER || dataType == VAL_CHAR) {
            iVal = DEFAULT_UNIT_ID;
            SetCtrlVal(panelHandle, PANEL_UNIT_ID, iVal);
        }
    }

    if (GetCtrlAttribute(panelHandle, PANEL_INTERVAL, ATTR_DATA_TYPE, &dataType) >= 0) {
        if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT)
            SetCtrlVal(panelHandle, PANEL_INTERVAL, (double)DEFAULT_INTERVAL_MS);
        else if (dataType == VAL_INTEGER || dataType == VAL_SHORT_INTEGER ||
                 dataType == VAL_UNSIGNED_INTEGER || dataType == VAL_CHAR) {
            iVal = DEFAULT_INTERVAL_MS;
            SetCtrlVal(panelHandle, PANEL_INTERVAL, iVal);
        }
    }

    if (GetCtrlAttribute(panelHandle, PANEL_TIME_RANGE, ATTR_DATA_TYPE, &dataType) >= 0) {
        if (dataType == VAL_DOUBLE || dataType == VAL_FLOAT)
            SetCtrlVal(panelHandle, PANEL_TIME_RANGE, DEFAULT_TIME_RANGE_SEC);
        else if (dataType == VAL_INTEGER || dataType == VAL_SHORT_INTEGER ||
                 dataType == VAL_UNSIGNED_INTEGER || dataType == VAL_CHAR) {
            iVal = (int)DEFAULT_TIME_RANGE_SEC;
            SetCtrlVal(panelHandle, PANEL_TIME_RANGE, iVal);
        }
    }

    /* センサタイプデフォルト: K型 */
    SetCtrlVal(panelHandle, PANEL_SENSOR_TYPE_SEL, (int)SENSOR_TYPE_K);
}

/*============================================================================
 * main - メイン関数
 *============================================================================*/
int main(int argc, char *argv[])
{
    int error = 0;
    int ch;

    nullChk(InitCVIRTE(0, argv, 0));

    /* UIR からパネルをロード (互換名も含めて探索) */
    errChk(panelHandle = LoadMainPanel());

    /* ===== 初期値設定 ===== */
    ApplyDefaultCtrlValues();

    /* 全チャネルのチェックボックスをON */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        SetCtrlBooleanCompat(panelHandle, ctrlChk[ch], 1);

    /* モニタリングボタン初期非表示 (接続前は非表示) */
    SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR, ATTR_VISIBLE, 0);

    /* タイマー初期設定 */
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_ENABLED, 0));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_INTERVAL,
                             DEFAULT_INTERVAL_MS / 1000.0));

    /* ストリップチャート初期設定 */
    InitStripChart();

    /* ログ表示設定: 黒背景 + 明るい緑文字 */
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_LOG, ATTR_TEXT_BGCOLOR, 0x00000000));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_LOG, ATTR_TEXT_COLOR,   0x0039FF14));

    /* パネル表示 */
    DisplayPanel(panelHandle);
    SetPanelAttribute(panelHandle, ATTR_SCALE_CONTENTS_ON_RESIZE, 1);
    SetPanelAttribute(panelHandle, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);

    /* ログファイルパスをカレントディレクトリで確定 */
    {
        char dir[MAX_PATHNAME_LEN];
        GetDir(dir);
        sprintf(g_logFilePath, "%s\\ThermoMonitor_log.txt", dir);
    }

    LogMessage("CPSN-SSI-4C 熱電対温度モニタ v3.0.3 起動");
    LogMessage("接続設定を確認して [接続] ボタンを押してください。");

    errChk(RunUserInterface());

Error:
    if (panelHandle > 0)
        DiscardPanel(panelHandle);
    Modbus_Disconnect();
    return error;
}

/*============================================================================
 * InitStripChart - ストリップチャート初期設定
 *
 * トレース色は UIR エディタで設定済みのため、ここでは
 * X軸スケール・表示点数・Y軸自動スケールのみ設定する。
 *============================================================================*/
static void InitStripChart(void)
{
    int top, left, height, width;

    /* UIR ストリップチャートの配置情報を取得してから破棄 */
    GetCtrlAttribute(panelHandle, PANEL_STRIPCHT, ATTR_TOP,    &top);
    GetCtrlAttribute(panelHandle, PANEL_STRIPCHT, ATTR_LEFT,   &left);
    GetCtrlAttribute(panelHandle, PANEL_STRIPCHT, ATTR_HEIGHT, &height);
    GetCtrlAttribute(panelHandle, PANEL_STRIPCHT, ATTR_WIDTH,  &width);
    DiscardCtrl(panelHandle, PANEL_STRIPCHT);

    /* CH毎に1つずつストリップチャートを作成（縦4等分） */
    {
        static const char * const chLabels[SSI4C_NUM_CHANNELS] = {"CH0", "CH1", "CH2", "CH3"};
        int chHeight = height / SSI4C_NUM_CHANNELS;
        int ch;
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            g_stripHandle[ch] = NewCtrl(panelHandle, CTRL_STRIP_CHART, chLabels[ch],
                                        top + ch * chHeight, left);
            SetCtrlAttribute(panelHandle, g_stripHandle[ch], ATTR_HEIGHT, chHeight);
            SetCtrlAttribute(panelHandle, g_stripHandle[ch], ATTR_WIDTH,  width);
            /* ラベルをチャート右上に配置（Y軸数値と重ならないよう右端寄せ） */
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_LABEL_TOP,  top + ch * chHeight));
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_LABEL_LEFT, left + width - 40));
            /* ラベル色・フォントサイズをトレース色に合わせる */
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_LABEL_COLOR, (int)g_traceColors[ch]));
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_LABEL_POINT_SIZE, 20));
            /* プロット背景を黒に設定（全トレース色を視認しやすくする） */
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_PLOT_BGCOLOR, 0x00000000));
            /* グリッド線を表示（黒背景に映えるグレー） */
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_XGRID_VISIBLE, 1));
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_YGRID_VISIBLE, 1));
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_GRID_COLOR, 0x00606060));
            /* トレース色を設定（SetTraceAttribute: CVI 2020 専用関数、トレース番号は1始まり） */
            SetTraceAttribute(panelHandle, g_stripHandle[ch], 1,
                              ATTR_TRACE_COLOR, (int)g_traceColors[ch]);
        }
    }

    /* X軸・表示点数設定 */
    ApplyTimeRange();

    /* Y軸レンジ初期値: K型デフォルト (-100 ~ 1400℃) */
    {
        int ch;
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
            SetAxisScalingMode(panelHandle, g_stripHandle[ch],
                               VAL_LEFT_YAXIS, VAL_MANUAL, -100.0, 1400.0);
    }

}

/*============================================================================
 * ApplyTimeRange - 表示時間幅をストリップチャートに反映
 *
 * PANEL_TIME_RANGE [秒] と PANEL_INTERVAL [ms] の現在値から
 * ストリップチャートの X軸スケールと表示点数を計算・設定する。
 *============================================================================*/
static void ApplyTimeRange(void)
{
    double timeRange;
    double dIntervalMs;
    double intervalSec;
    int    pointsPerScreen;

    if (GetNumericCtrlAsDouble(panelHandle, PANEL_TIME_RANGE, &timeRange) < 0)
        timeRange = DEFAULT_TIME_RANGE_SEC;
    if (GetNumericCtrlAsDouble(panelHandle, PANEL_INTERVAL, &dIntervalMs) < 0)
        dIntervalMs = DEFAULT_INTERVAL_MS;

    intervalSec = dIntervalMs / 1000.0;
    if (intervalSec < 0.1) intervalSec = 0.1;   /* 最小 100ms */

    /*
     * X軸スケール: 1サンプル = intervalSec 秒
     * → 横軸の目盛が「経過時間 [秒]」として表示される
     */
    /* 4チャートすべてに同じX軸設定を適用 */
    {
        int ch;
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_XAXIS_GAIN, intervalSec));
        }
    }

    /* 画面表示サンプル数 = 表示時間幅 [秒] / サンプル間隔 [秒] */
    pointsPerScreen = (int)(timeRange / intervalSec);
    if (pointsPerScreen < 10)   pointsPerScreen = 10;
    if (pointsPerScreen > 7200) pointsPerScreen = 7200;   /* 上限 7200点 */

    {
        int ch;
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            tryAttr(SetCtrlAttribute(panelHandle, g_stripHandle[ch],
                                     ATTR_POINTS_PER_SCREEN, pointsPerScreen));
        }
    }
}

/*============================================================================
 * UpdateStripChart - ストリップチャートに最新データを1点追加
 *
 * チェックボックスがOFFのチャネル、またはフォルト発生チャネルは
 * 前回値をそのまま追加して波形を水平維持する。
 * (全トレースが同数の点を持つことでスクロールが同期する)
 *============================================================================*/
static void UpdateStripChart(const ChannelData *chData)
{
    int    ch;
    int    chkVal;
    double val;

    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
        if (GetCtrlBooleanCompat(panelHandle, ctrlChk[ch], &chkVal) < 0)
            chkVal = 1;

        if (chkVal && chData[ch].isValid && !chData[ch].hasHardFault) {
            val = chData[ch].temperature;
            g_lastTemp[ch] = val;
        } else if (chkVal && chData[ch].hasHardFault) {
            val = FAULT_ERROR_TEMP;   /* ハードフォルト時は -999.0 をプロット */
        } else {
            val = g_lastTemp[ch];
        }

        PlotStripChart(panelHandle, g_stripHandle[ch], &val, 1, 0, 0, VAL_DOUBLE);
    }
}

/*============================================================================
 * PanelCallback - パネルイベントコールバック
 *============================================================================*/
int CVICALLBACK PanelCallback(int panel, int event, void *callbackData,
                               int eventData1, int eventData2)
{
    if (event == EVENT_CLOSE) {
        StopMonitoring();
        Modbus_Disconnect();
        QuitUserInterface(0);
    }
    return 0;
}

/*============================================================================
 * DetectAndApplyDataFormat - FC04 生データからデータ形式を自動判定して適用
 *
 * 接続成功直後に1回だけ呼び出す。
 * 判定結果を g_currentDataFormat に格納し、UI に反映する。
 *============================================================================*/
int DetectAndApplyDataFormat(void)
{
    double     dUnitId;
    int        unitId;
    int        confidence = 0;
    DataFormat fmt;
    char       infoMsg[128];
    char       confMsg[64];
    const char *confLevel;

    if (GetNumericCtrlAsDouble(panelHandle, PANEL_UNIT_ID, &dUnitId) < 0)
        dUnitId = DEFAULT_UNIT_ID;
    unitId = (int)dUnitId;

    fmt = SSI4C_DetectDataFormat(unitId, 0, &confidence);

    g_currentDataFormat = fmt;
    g_formatConfidence  = confidence;

    /* 信頼度レベル判定 */
    if (confidence >= 3)      confLevel = "高";
    else if (confidence >= 1) confLevel = "中";
    else                      confLevel = "低";

    /* PANEL_SENSOR_INFO: データ形式名 */
    if (confidence > 0) {
        Fmt(infoMsg, "データ形式: %s [自動判定]", DataFormatNames[fmt]);
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_CTRL_VAL, infoMsg));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_TEXT_COLOR, (int)COLOR_FAULT));   /* 赤 */
    } else {
        Fmt(infoMsg, "データ形式: %s [デフォルト]", DataFormatNames[fmt]);
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_CTRL_VAL, infoMsg));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_TEXT_COLOR, (int)COLOR_WARN));
    }

    /* PANEL_FORMAT_CONFIDENCE: 信頼度 */
    Fmt(confMsg, "信頼度: %s (%d/4ch一致)", confLevel, confidence);
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_FORMAT_CONFIDENCE,
                             ATTR_CTRL_VAL, confMsg));

    LogMessageFmt("データ形式判定: %s (信頼度: %s, %d/4ch)",
                  DataFormatNames[fmt], confLevel, confidence);
    return MODBUS_OK;
}

/*============================================================================
 * BtnConnect_Callback - 接続/切断ボタン
 *============================================================================*/
int CVICALLBACK BtnConnect_Callback(int panel, int control, int event,
                                     void *callbackData, int eventData1,
                                     int eventData2)
{
    char   ipAddr[64];
    double dPort;
    int    port;
    int    ret;

    if (event != EVENT_COMMIT) return 0;

    if (Modbus_IsConnected()) {
        /* 切断 */
        StopMonitoring();
        Modbus_Disconnect();
        UpdateConnectionStatus(0);
        SetCtrlAttribute(panel, PANEL_BTN_MONITOR, ATTR_VISIBLE, 0);
        tryAttr(SetCtrlAttribute(panel, PANEL_SENSOR_INFO,
                                 ATTR_CTRL_VAL, "認識待機中..."));
        tryAttr(SetCtrlAttribute(panel, PANEL_SENSOR_INFO,
                                 ATTR_TEXT_COLOR, (int)COLOR_DISCONNECT));
        tryAttr(SetCtrlAttribute(panel, PANEL_FORMAT_CONFIDENCE,
                                 ATTR_CTRL_VAL, "(判定待機中)"));
        LogMessage("切断しました。");
    } else {
        /* 接続 */
        /* 自動再接続中なら中止してから手動接続に移行 */
        if (g_isReconnecting) {
            g_isReconnecting = 0;
            tryAttr(SetCtrlAttribute(panel, PANEL_TIMER, ATTR_ENABLED, 0));
            LogMessage("自動再接続を中止しました。");
        }
        GetCtrlVal(panel, PANEL_IP_ADDR, ipAddr);
        if (GetNumericCtrlAsDouble(panel, PANEL_PORT, &dPort) < 0)
            dPort = DEFAULT_PORT;
        port = (int)dPort;

        LogMessageFmt("接続中... IP: %s, Port: %d", ipAddr, port);

        ret = Modbus_Connect(ipAddr, port, MODBUS_TCP_TIMEOUT_MS);
        if (ret == MODBUS_OK) {
            UpdateConnectionStatus(1);
            g_consecutiveErrors = 0;
            DetectAndApplyDataFormat();   /* 接続直後にデータ形式を自動判定 */
            SetCtrlAttribute(panel, PANEL_BTN_MONITOR, ATTR_VISIBLE, 1);
            SetCtrlAttribute(panel, PANEL_BTN_MONITOR, ATTR_DIMMED,   0);
            LogMessageFmt("接続成功: %s:%d", ipAddr, port);
        } else {
            UpdateConnectionStatus(0);
            LogMessageFmt("接続失敗: %s (%s:%d)",
                          Modbus_ErrorString(ret), ipAddr, port);
        }
    }
    return 0;
}

/*============================================================================
 * BtnMonitor_Callback - モニタリング開始/停止ボタン
 *============================================================================*/
int CVICALLBACK BtnMonitor_Callback(int panel, int control, int event,
                                     void *callbackData, int eventData1,
                                     int eventData2)
{
    if (event != EVENT_COMMIT) return 0;

    if (g_isMonitoring)
        StopMonitoring();
    else
        StartMonitoring();
    return 0;
}

/*============================================================================
 * BtnClear_Callback - ログクリアボタン
 *============================================================================*/
int CVICALLBACK BtnClear_Callback(int panel, int control, int event,
                                   void *callbackData, int eventData1,
                                   int eventData2)
{
    if (event != EVENT_COMMIT) return 0;
    ResetTextBox(panelHandle, PANEL_LOG, "");
    return 0;
}

/*============================================================================
 * BtnGraphClear_Callback - グラフクリアボタン
 *============================================================================*/
int CVICALLBACK BtnGraphClear_Callback(int panel, int control, int event,
                                        void *callbackData, int eventData1,
                                        int eventData2)
{
    int ch;

    if (event != EVENT_COMMIT) return 0;

    /* 全ストリップチャートのデータを消去 */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        ClearStripChart(panelHandle, g_stripHandle[ch]);

    /* 前回値バッファをリセット */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        g_lastTemp[ch] = 0.0;

    LogMessage("グラフをクリアしました。");
    return 0;
}

/*============================================================================
 * ChkChannel_Callback - チャネル表示チェックボックス
 *
 * チェックOFF: トレース色を背景色（黒）にして非表示。データは同期維持のため継続プロット。
 * チェックON : トレース色を元の色に戻して再表示。
 *============================================================================*/
int CVICALLBACK ChkChannel_Callback(int panel, int control, int event,
                                     void *callbackData, int eventData1,
                                     int eventData2)
{
    int ch, chkVal;

    if (event != EVENT_COMMIT) return 0;

    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
        if (control != ctrlChk[ch]) continue;
        GetCtrlBooleanCompat(panel, control, &chkVal);
        SetTraceAttribute(panelHandle, g_stripHandle[ch], 1,
                          ATTR_TRACE_COLOR,
                          chkVal ? (int)g_traceColors[ch] : 0x00000000);
        break;
    }
    return 0;
}

/*============================================================================
 * SensorTypeSel_Callback - センサタイプ選択リング
 *
 * 選択変更時に推奨グラフレンジをログに出力する。
 * センサタイプはModbusで取得不可のためユーザーが手動設定する。
 *============================================================================*/
int CVICALLBACK SensorTypeSel_Callback(int panel, int control, int event,
                                        void *callbackData, int eventData1,
                                        int eventData2)
{
    static const int chYMaxCtrl[SSI4C_NUM_CHANNELS] =
        {PANEL_CH0_YMAX, PANEL_CH1_YMAX, PANEL_CH2_YMAX, PANEL_CH3_YMAX};
    static const int chYMinCtrl[SSI4C_NUM_CHANNELS] =
        {PANEL_CH0_YMIN, PANEL_CH1_YMIN, PANEL_CH2_YMIN, PANEL_CH3_YMIN};
    int    sType, ch;
    double yMin, yMax;

    if (event != EVENT_COMMIT) return 0;

    GetCtrlVal(panel, PANEL_SENSOR_TYPE_SEL, &sType);
    if (sType < 0 || sType >= SENSOR_TYPE_COUNT)
        sType = SENSOR_TYPE_K;

    yMin = -100.0; yMax = 1400.0;
    if      (sType == SENSOR_TYPE_R || sType == SENSOR_TYPE_S) { yMin =    0.0; yMax = 1800.0; }
    else if (sType == SENSOR_TYPE_T)                            { yMin = -100.0; yMax =  400.0; }
    else if (sType == SENSOR_TYPE_E)                            { yMin = -100.0; yMax = 1000.0; }
    else if (sType == SENSOR_TYPE_J)                            { yMin = -100.0; yMax = 1200.0; }
    else if (sType == SENSOR_TYPE_N)                            { yMin = -100.0; yMax = 1300.0; }
    else if (sType == SENSOR_TYPE_Q)                            { yMin =    0.0; yMax =  150.0; }

    /* Y軸スケールコントロールに初期値をセット（実際の適用はApplyChYAxisScaling） */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
        SetCtrlVal(panelHandle, chYMaxCtrl[ch], yMax);
        SetCtrlVal(panelHandle, chYMinCtrl[ch], yMin);
    }

    LogMessageFmt("センサタイプ: %s / グラフレンジ: %.0f〜%.0f ℃",
                  SensorTypeNames[sType], yMin, yMax);
    return 0;
}




/*============================================================================
 * FetchShotTime - 実験ネットワークから time.txt を取得
 *
 * 取得成功時: time.txt の数値（秒）を返す
 * 取得失敗時: -9999.0 を返す
 *============================================================================*/
static int CVICALLBACK ShotTimeTCPCB(unsigned int handle, int event, int error, void *cbData)
{
    if (event == TCP_DISCONNECT)
        g_fetchServerClosed = 1;
    return 0;
}

static double FetchShotTime(void)
{
    unsigned int handle    = 0;
    char         req[256];
    char         buf[512];
    double       tVal      = -9999.0;
    char        *body;

    g_fetchServerClosed = 0;

    sprintf(req,
            "GET /expinfo/time.txt HTTP/1.0\r\n"
            "Host: csv02.exp.triam.kyushu-u.ac.jp\r\n"
            "Connection: close\r\n"
            "\r\n");

    if (ConnectToTCPServer(&handle, 80,
                           "csv02.exp.triam.kyushu-u.ac.jp",
                           ShotTimeTCPCB, NULL, 100) < 0)
        return -9999.0;

    if (ClientTCPWrite(handle, req, (int)strlen(req), 100) < 0)
        goto cleanup;

    memset(buf, 0, sizeof(buf));
    ClientTCPRead(handle, buf, (int)sizeof(buf) - 1, 100);

    body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        if (strlen(body) == 0) {
            /* ヘッダのみ受信: ボディが後続パケットの場合に追加読み取り */
            char bodyBuf[64];
            memset(bodyBuf, 0, sizeof(bodyBuf));
            ClientTCPRead(handle, bodyBuf, sizeof(bodyBuf) - 1, 100);
            body = bodyBuf;
        }
        if (sscanf(body, "%lf", &tVal) != 1)
            tVal = -9999.0;
    }

cleanup:
    if (!g_fetchServerClosed)
        DisconnectFromTCPServer(handle);
    return tVal;
}

/*============================================================================
 * FetchShotNumber - 実験ネットワークから shotNumber.txt を取得
 *
 * 取得成功時: ショットナンバー（整数）を返す
 * 取得失敗時: -1 を返す
 *============================================================================*/
static int CVICALLBACK ShotNumberTCPCB(unsigned int handle, int event, int error, void *cbData)
{
    if (event == TCP_DISCONNECT)
        g_fetchShotNumServerClosed = 1;
    return 0;
}

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

/*============================================================================
 * UpdateShotState - ショット状態を更新してSTART/STOPをログ出力
 *============================================================================*/
static void UpdateShotState(void)
{
    double    tNow;
    time_t    now;
    struct tm *tm_info;
    char      timeStr[32];
    int       ch;

    tNow = FetchShotTime();
    if (tNow == -9999.0) return;  /* FetchShotTimeエラー時のみスキップ */

    now     = time(NULL);
    tm_info = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_info);

    if (g_prevShotTime < 0.0 && tNow >= 0.0) {
        /* SHOT START 検出 */
        int shotNum;

        /* ショットナンバーを取得して START の前にログ出力 */
        shotNum = FetchShotNumber();
        if (shotNum >= 0)
            LogMessageFmt("[SHOT NUMBER] %d", shotNum);
        else
            LogMessageFmt("[SHOT NUMBER] (取得失敗)");

        g_postShotTracking = 1;
        g_peakStableCount  = 0;
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            g_truePeakTemp[ch] = g_lastTemp[ch];
            strncpy(g_truePeakTimeStr[ch], timeStr, sizeof(g_truePeakTimeStr[ch]) - 1);
        }

        LogMessageFmt("[SHOT START] %s  CH0=%.1f℃  CH1=%.1f℃  CH2=%.1f℃  CH3=%.1f℃",
                      timeStr,
                      g_lastTemp[0], g_lastTemp[1],
                      g_lastTemp[2], g_lastTemp[3]);
        g_shotActive = 1;

        /* CSVログ: 既存ファイルをクローズして新規作成 */
        {
            int csvOn = 0;
            GetCtrlVal(panelHandle, PANEL_CSV_LOG_ENABLE, &csvOn);
            if (csvOn) {
                CloseShotCsvFile();
                OpenShotCsvFile();
                strcpy(g_csvRowMarker, "SHOT START");
            }
        }

    } else if (g_shotActive && tNow < 0.0) {
        /* SHOT STOP 検出: STOP時点の現在温度を表示 */
        LogMessageFmt("[SHOT STOP ] %s  CH0=%.1f℃  CH1=%.1f℃  CH2=%.1f℃  CH3=%.1f℃",
                      timeStr,
                      g_lastTemp[0], g_lastTemp[1],
                      g_lastTemp[2], g_lastTemp[3]);
        g_shotActive = 0;
        /* g_postShotTracking継続: [PEAK TIME]はTimer_Callbackが温度下降確定後に出力 */

        /* CSVログ: マーカーを設定（ファイルはクローズしない） */
        if (g_csvFile)
            strcpy(g_csvRowMarker, "SHOT STOP");
    }

    g_prevShotTime = tNow;
}

/*============================================================================
 * OpenShotCsvFile - ショットCSVファイルを新規作成
 *============================================================================*/
static void OpenShotCsvFile(void)
{
    time_t     now;
    struct tm *tm_info;
    char       timeStr[32];

    now     = time(NULL);
    tm_info = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", tm_info);
    {
        char dir[MAX_PATHNAME_LEN];
        GetDir(dir);
        sprintf(g_csvFilePath, "%s\\ThermoMonitor_shot_%s.csv", dir, timeStr);
    }

    g_csvFile = fopen(g_csvFilePath, "w");
    if (!g_csvFile) {
        LogMessageFmt("CSVログ: ファイル作成失敗 %s", g_csvFilePath);
        return;
    }
    fprintf(g_csvFile, "\xEF\xBB\xBF");  /* UTF-8 BOM (Excel対応) */
    fprintf(g_csvFile, "時刻,CH0(℃),CH1(℃),CH2(℃),CH3(℃),マーカー\n");
    fflush(g_csvFile);
    LogMessageFmt("CSVログ開始: %s", g_csvFilePath);
}

/*============================================================================
 * CloseShotCsvFile - ショットCSVファイルをクローズ
 *============================================================================*/
static void CloseShotCsvFile(void)
{
    if (!g_csvFile) return;
    fclose(g_csvFile);
    g_csvFile = NULL;
    LogMessageFmt("CSVログ終了: %s", g_csvFilePath);
    g_csvFilePath[0] = '\0';
}

/*============================================================================
 * WriteCsvRow - CSVに現在温度を1行書き込む
 *============================================================================*/
static void WriteCsvRow(const char *marker)
{
    time_t     now;
    struct tm *tm_info;
    char       timeStr[32];

    if (!g_csvFile) return;

    now     = time(NULL);
    tm_info = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_info);

    fprintf(g_csvFile, "%s,%.1f,%.1f,%.1f,%.1f,%s\n",
            timeStr,
            g_lastTemp[0], g_lastTemp[1], g_lastTemp[2], g_lastTemp[3],
            marker ? marker : "");
    fflush(g_csvFile);
}

/*============================================================================
 * CsvLogEnable_Callback - ショットCSVログ チェックボックス
 *============================================================================*/
int CVICALLBACK CsvLogEnable_Callback(int panel, int control, int event,
                                       void *callbackData, int eventData1,
                                       int eventData2)
{
    if (event != EVENT_COMMIT) return 0;
    int val = 0;
    GetCtrlVal(panel, control, &val);
    if (!val) CloseShotCsvFile();  /* OFF時: 記録中ファイルをクローズ */
    return 0;
}

/*============================================================================
 * ApplyChYAxisScaling - CH毎Y軸レンジをコントロール値から適用
 *
 * Timer_Callback から毎周期呼ばれる。ユーザーがコントロールの値を
 * 変更するだけでリアルタイムにグラフY軸が更新される。
 *============================================================================*/
static void ApplyChYAxisScaling(void)
{
    static const int chYMaxCtrl[SSI4C_NUM_CHANNELS] =
        {PANEL_CH0_YMAX, PANEL_CH1_YMAX, PANEL_CH2_YMAX, PANEL_CH3_YMAX};
    static const int chYMinCtrl[SSI4C_NUM_CHANNELS] =
        {PANEL_CH0_YMIN, PANEL_CH1_YMIN, PANEL_CH2_YMIN, PANEL_CH3_YMIN};
    double yMax, yMin;
    int ch;
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
        if (g_stripHandle[ch] <= 0) continue;
        GetCtrlVal(panelHandle, chYMaxCtrl[ch], &yMax);
        GetCtrlVal(panelHandle, chYMinCtrl[ch], &yMin);
        if (yMin >= yMax) continue;
        SetAxisScalingMode(panelHandle, g_stripHandle[ch],
                           VAL_LEFT_YAXIS, VAL_MANUAL, yMin, yMax);
    }
}

/*============================================================================
 * Timer_Callback - 定期読み取りタイマー
 *============================================================================*/
int CVICALLBACK Timer_Callback(int panel, int control, int event,
                                void *callbackData, int eventData1,
                                int eventData2)
{
    if (event != EVENT_TIMER_TICK) return 0;
    if (g_isReconnecting) {
        AttemptReconnect();
        return 0;
    }
    if (!g_isMonitoring) return 0;
    g_blinkState = !g_blinkState;
    ReadAndDisplayTemperatures();
    /* 真のピーク追跡: SHOT START〜温度下降確定まで継続（SHOT STOP後も追跡） */
    if (g_postShotTracking) {
        int ch, updated = 0;
        time_t now2 = time(NULL);
        struct tm *tmi2 = localtime(&now2);
        char pts[32];
        strftime(pts, sizeof(pts), "%H:%M:%S", tmi2);
        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            if (g_lastTemp[ch] > g_truePeakTemp[ch]) {
                g_truePeakTemp[ch] = g_lastTemp[ch];
                strncpy(g_truePeakTimeStr[ch], pts, sizeof(g_truePeakTimeStr[ch]) - 1);
                updated = 1;
            }
        }
        if (updated) {
            g_peakStableCount = 0;
        } else if (!g_shotActive && ++g_peakStableCount >= TRUE_PEAK_STABLE_TICKS) {
            /* SHOT STOP後のみ発火（ショット中は発火しない） */
            LogMessageFmt("[PEAK TIME ] CH0=%.1f℃@%s  CH1=%.1f℃@%s  CH2=%.1f℃@%s  CH3=%.1f℃@%s",
                          g_truePeakTemp[0], g_truePeakTimeStr[0],
                          g_truePeakTemp[1], g_truePeakTimeStr[1],
                          g_truePeakTemp[2], g_truePeakTimeStr[2],
                          g_truePeakTemp[3], g_truePeakTimeStr[3]);
            g_postShotTracking = 0;
            g_peakStableCount  = 0;
        }
    }
    ApplyChYAxisScaling();
    UpdateShotState();
    WriteCsvRow(g_csvRowMarker[0] ? g_csvRowMarker : NULL);
    g_csvRowMarker[0] = '\0';
    return 0;
}

/*============================================================================
 * StartMonitoring - モニタリング開始
 *============================================================================*/
void StartMonitoring(void)
{
    double      dIntervalMs;
    double      intervalSec;
    int         intervalMs;
    ChannelData chData[SSI4C_NUM_CHANNELS];
    double      dUnitId;
    int         unitId, ch, ret;

    if (!Modbus_IsConnected()) {
        LogMessage("エラー: 接続されていません。");
        return;
    }

    if (GetNumericCtrlAsDouble(panelHandle, PANEL_INTERVAL, &dIntervalMs) < 0)
        dIntervalMs = DEFAULT_INTERVAL_MS;
    intervalMs  = (int)dIntervalMs;
    intervalSec = dIntervalMs / 1000.0;

    if (GetNumericCtrlAsDouble(panelHandle, PANEL_UNIT_ID, &dUnitId) < 0)
        dUnitId = DEFAULT_UNIT_ID;
    unitId = (int)dUnitId;

    /* --- 接続確認: タイマー有効化前にテスト読み取り --- */
    ret = SSI4C_ReadAllChannels(unitId, 0, chData, g_currentDataFormat);
    if (ret != MODBUS_OK || !Modbus_IsConnected()) {
        LogMessageFmt("接続確認失敗: %s", Modbus_ErrorString(ret));
        LogMessage("LANケーブルを確認して再接続してください。");
        Modbus_Disconnect();
        UpdateConnectionStatus(0);
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_CTRL_VAL, "認識待機中..."));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_TEXT_COLOR, (int)COLOR_DISCONNECT));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_FORMAT_CONFIDENCE,
                                 ATTR_CTRL_VAL, "(判定待機中)"));
        return;
    }

    /* タイマー間隔をUIの設定値に合わせて更新 */
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_INTERVAL, intervalSec));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_ENABLED,  1));

    /* モニタリング中は接続先・間隔の変更を禁止 */
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_CONNECT, ATTR_DIMMED, 1));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_INTERVAL,    ATTR_DIMMED, 1));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR,
                             ATTR_LABEL_TEXT, "モニタリング停止"));

    /* グラフX軸スケールを間隔設定に合わせて更新 */
    ApplyTimeRange();

    g_isMonitoring      = 1;
    g_readCount         = 1;   /* テスト読み取りを1回目としてカウント */
    g_consecutiveErrors = 0;
    LogMessageFmt("モニタリング開始 (更新間隔: %d ms)", intervalMs);

    /* テスト読み取り結果を初期表示 */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        UpdateChannelDisplay(ch, &chData[ch]);
    UpdateStripChart(chData);
}

/*============================================================================
 * StopMonitoring - モニタリング停止
 *============================================================================*/
void StopMonitoring(void)
{
    if (!g_isMonitoring) return;

    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER,       ATTR_ENABLED, 0));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_CONNECT, ATTR_DIMMED,  0));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_INTERVAL,    ATTR_DIMMED,  0));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR,
                             ATTR_LABEL_TEXT, "モニタリング開始"));

    g_isMonitoring     = 0;
    g_shotActive       = 0;
    g_postShotTracking = 0;
    g_peakStableCount  = 0;
    g_prevShotTime     = -9999.0;
    CloseShotCsvFile();  /* モニタリング停止時にCSVをクローズ */
    LogMessageFmt("モニタリング停止 (総読み取り回数: %d)", g_readCount);
}

/*============================================================================
 * TriggerAutoReconnect - 切断検知時に自動再接続シーケンスを開始
 *============================================================================*/
static void TriggerAutoReconnect(void)
{
    StopMonitoring();   /* タイマー停止・g_isMonitoring=0 */
    Modbus_Disconnect();
    UpdateConnectionStatus(0);
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                             ATTR_CTRL_VAL, "自動再接続待機中..."));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                             ATTR_TEXT_COLOR, (int)COLOR_WARN));
    g_isReconnecting    = 1;
    g_reconnectAttempts = 0;
    LogMessageFmt("自動再接続を開始します (%d秒間隔・最大%d回)。",
                  RECONNECT_INTERVAL_SEC, MAX_RECONNECT_ATTEMPTS);
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER,
                             ATTR_INTERVAL, (double)RECONNECT_INTERVAL_SEC));
    tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_ENABLED, 1));
}

/*============================================================================
 * AttemptReconnect - 自動再接続タイマーから呼び出される再接続試行
 *============================================================================*/
static void AttemptReconnect(void)
{
    char   ipAddr[64];
    double dPort;
    int    port, ret;

    g_reconnectAttempts++;
    LogMessageFmt("自動再接続試行 (%d/%d)...",
                  g_reconnectAttempts, MAX_RECONNECT_ATTEMPTS);

    GetCtrlVal(panelHandle, PANEL_IP_ADDR, ipAddr);
    if (GetNumericCtrlAsDouble(panelHandle, PANEL_PORT, &dPort) < 0)
        dPort = DEFAULT_PORT;
    port = (int)dPort;

    ret = Modbus_Connect(ipAddr, port, MODBUS_TCP_TIMEOUT_MS);
    if (ret == MODBUS_OK) {
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_ENABLED, 0));
        g_isReconnecting = 0;
        UpdateConnectionStatus(1);
        g_consecutiveErrors = 0;
        DetectAndApplyDataFormat();
        SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR, ATTR_VISIBLE, 1);
        SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR, ATTR_DIMMED,  0);
        LogMessage("自動再接続成功。モニタリングを再開します。");
        StartMonitoring();
        return;
    }

    if (g_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_TIMER, ATTR_ENABLED, 0));
        g_isReconnecting = 0;
        LogMessageFmt("自動再接続を %d 回試みましたが失敗しました。",
                      MAX_RECONNECT_ATTEMPTS);
        LogMessage("手動で [接続] ボタンを押して再接続してください。");
        return;
    }

    LogMessageFmt("再接続失敗 (%s)。%d秒後に再試行します...",
                  Modbus_ErrorString(ret), RECONNECT_INTERVAL_SEC);
}

/*============================================================================
 * ReadAndDisplayTemperatures - 温度データ読み取りと画面更新
 *============================================================================*/
void ReadAndDisplayTemperatures(void)
{
    ChannelData chData[SSI4C_NUM_CHANNELS];
    double      dUnitId;
    int         unitId;
    int         ret;
    int         ch;

    if (!Modbus_IsConnected()) {
        LogMessage("エラー: 接続が切断されました。");
        TriggerAutoReconnect();
        return;
    }

    if (GetNumericCtrlAsDouble(panelHandle, PANEL_UNIT_ID, &dUnitId) < 0)
        dUnitId = DEFAULT_UNIT_ID;
    unitId = (int)dUnitId;

    /* 判定済みデータ形式を渡して読み取り・解析 */
    ret = SSI4C_ReadAllChannels(unitId, 0, chData, g_currentDataFormat);

    if (ret != MODBUS_OK) {
        g_consecutiveErrors++;
        LogMessageFmt("通信エラー(%d/%d): %s",
                      g_consecutiveErrors, MAX_CONSECUTIVE_ERRORS,
                      Modbus_ErrorString(ret));
        if (g_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
            LogMessage("連続エラー上限に達しました。");
            TriggerAutoReconnect();
        }
        return;
    }

    g_consecutiveErrors = 0;   /* 成功時はカウンタリセット */

    /* ===== デバッグ用強制フォルト (使用時はコメントを外す) =====
    chData[0].hasSoftFault = 1;
    strncpy(chData[0].faultMsg, "センサ上限超過", sizeof(chData[0].faultMsg) - 1);
    chData[1].hasHardFault = 1; chData[1].hasSoftFault = 0;
    strncpy(chData[1].faultMsg, "センサハードFLT", sizeof(chData[1].faultMsg) - 1);
    ===== デバッグ用強制フォルト ここまで ===== */

    /* 現在値表示更新 */
    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        UpdateChannelDisplay(ch, &chData[ch]);

    /* グラフにデータを追加 */
    UpdateStripChart(chData);

    g_readCount++;
}

/*============================================================================
 * UpdateChannelDisplay - チャネル現在値表示更新
 *============================================================================*/
void UpdateChannelDisplay(int ch, const ChannelData *data)
{
    int bgColor;

    if (ch < 0 || ch >= SSI4C_NUM_CHANNELS || !data) return;

    SetCtrlVal(panelHandle, ctrlTemp[ch], data->temperature);

    if (data->hasHardFault || !data->isValid)
        bgColor = COLOR_FAULT;
    else if (data->hasSoftFault)
        bgColor = COLOR_WARN;
    else
        bgColor = COLOR_NORMAL;

    tryAttr(SetCtrlAttribute(panelHandle, ctrlTemp[ch], ATTR_TEXT_BGCOLOR, bgColor));

    if (data->hasHardFault || !data->isValid) {
        tryAttr(SetCtrlAttribute(panelHandle, ctrlLED[ch], ATTR_ON_COLOR, COLOR_FAULT));
        SetCtrlVal(panelHandle, ctrlLED[ch], g_blinkState);
        SetCtrlVal(panelHandle, ctrlFault[ch], data->faultMsg);
        tryAttr(SetCtrlAttribute(panelHandle, ctrlFault[ch], ATTR_TEXT_COLOR,
                                 g_blinkState ? COLOR_FAULT : 0x00000000));
    } else if (data->hasSoftFault) {
        tryAttr(SetCtrlAttribute(panelHandle, ctrlLED[ch], ATTR_ON_COLOR, COLOR_WARN));
        SetCtrlVal(panelHandle, ctrlLED[ch], g_blinkState);
        SetCtrlVal(panelHandle, ctrlFault[ch], data->faultMsg);
        tryAttr(SetCtrlAttribute(panelHandle, ctrlFault[ch], ATTR_TEXT_COLOR,
                                 g_blinkState ? COLOR_WARN : 0x00000000));
    } else {
        SetCtrlVal(panelHandle, ctrlLED[ch], 0);
        SetCtrlVal(panelHandle, ctrlFault[ch], data->faultMsg);
        tryAttr(SetCtrlAttribute(panelHandle, ctrlFault[ch],
                                 ATTR_TEXT_COLOR, 0x00008000));   /* 緑 */
    }
}

/*============================================================================
 * UpdateConnectionStatus - 接続ステータス表示更新
 *============================================================================*/
void UpdateConnectionStatus(int isConnected)
{
    int ch;

    if (isConnected) {
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_STATUS,
                                 ATTR_CTRL_VAL,   "接続中"));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_STATUS,
                                 ATTR_TEXT_COLOR, 0x00008000));   /* 緑 */
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_CONNECT,
                                 ATTR_LABEL_TEXT, "切断"));
    } else {
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_STATUS,
                                 ATTR_CTRL_VAL,   "未接続"));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_STATUS,
                                 ATTR_TEXT_COLOR, COLOR_DISCONNECT));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_BTN_CONNECT,
                                 ATTR_LABEL_TEXT, "接続"));
        SetCtrlAttribute(panelHandle, PANEL_BTN_MONITOR, ATTR_VISIBLE, 0);
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_CTRL_VAL, "認識待機中..."));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_SENSOR_INFO,
                                 ATTR_TEXT_COLOR, (int)COLOR_DISCONNECT));
        tryAttr(SetCtrlAttribute(panelHandle, PANEL_FORMAT_CONFIDENCE,
                                 ATTR_CTRL_VAL, "(判定待機中)"));
        g_currentDataFormat = DATA_FORMAT_RAW_SSI;
        g_formatConfidence  = 0;

        for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
            tryAttr(SetCtrlAttribute(panelHandle, ctrlTemp[ch],
                                     ATTR_TEXT_BGCOLOR, COLOR_DISCONNECT));
            SetCtrlVal(panelHandle, ctrlLED[ch], 0);
            tryAttr(SetCtrlAttribute(panelHandle, ctrlFault[ch],
                                     ATTR_CTRL_VAL,   "---"));
            tryAttr(SetCtrlAttribute(panelHandle, ctrlFault[ch],
                                     ATTR_TEXT_COLOR, COLOR_DISCONNECT));
        }
    }
}

/*============================================================================
 * LogMessage / LogMessageFmt - ログ出力
 *============================================================================*/
void LogMessage(const char *msg)
{
    char   timeBuf[32];
    char   logLine[512];
    time_t now;
    FILE  *fp;

    time(&now);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&now));
    sprintf(logLine, "[%s] %s\n", timeBuf, msg);
    InsertTextBoxLine(panelHandle, PANEL_LOG, -1, logLine);

    /* 最新ログへ自動スクロール */
    {
        int numLines;
        GetNumTextBoxLines(panelHandle, PANEL_LOG, &numLines);
        SetCtrlAttribute(panelHandle, PANEL_LOG, ATTR_FIRST_VISIBLE_LINE, numLines);
    }

    /* ファイルにも追記 */
    fp = fopen(g_logFilePath, "a");
    if (fp) {
        fprintf(fp, "%s", logLine);
        fclose(fp);
    }
}

void LogMessageFmt(const char *fmt, ...)
{
    char    buf[512];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    LogMessage(buf);
}
