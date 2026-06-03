/*==============================================================================
 * ThermoMonitor_app.h
 * CPSN-SSI-4C 熱電対温度モニタ - アプリケーションヘッダ
 *
 * 26.04.23
 *
 * インクルード構造:
 *   ThermoMonitor.c
 *     └─ ThermoMonitor_app.h  (このファイル)
 *          ├─ ThermoMonitor.h     (CVI自動生成: PANEL_PANEL_XXX 定数・コールバック宣言)
 *          ├─ ThermoMonitor_ui.h  (エイリアス: PANEL_XXX → PANEL_PANEL_XXX)
 *          └─ 内部関数プロトタイプ・アプリ定数
 *============================================================================*/

#ifndef THERMOMONITOR_APP_H
#define THERMOMONITOR_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <userint.h>
#include "modbus_tcp.h"
#include "ThermoMonitor.h"      /* CVI自動生成: PANEL_PANEL_XXX, コールバック宣言 */
#include "ThermoMonitor_ui.h"   /* エイリアス: PANEL_XXX → PANEL_PANEL_XXX        */

/*--- アプリ定数 -------------------------------------------------------------*/
#define PROG_TITLE               "CPSN-SSI-4C 熱電対温度モニタ"
#define DEFAULT_IP               "192.168.52.248"
#define DEFAULT_PORT             502
#define DEFAULT_UNIT_ID          1
#define DEFAULT_INTERVAL_MS      500
#define DEFAULT_TIME_RANGE_SEC   60.0

/* 自動再接続設定 */
#define RECONNECT_INTERVAL_SEC   5    /* 再接続試行間隔 [秒] */
#define MAX_RECONNECT_ATTEMPTS   10   /* 最大再接続試行回数 */

/* 色定義 */
#define COLOR_NORMAL             0x00C0E0FF   /* 正常: 水色   */
#define COLOR_FAULT              0x00FF4040   /* フォルト: 赤 */
#define COLOR_WARN               0x00FFCC00   /* 警告: 黄色   */
#define COLOR_DISCONNECT         0x00808080   /* 未接続: グレー*/

/* センサタイプ名称 */
#define SENSOR_TYPE_COUNT        8
extern const char *SensorTypeNames[SENSOR_TYPE_COUNT];

/*--- Timer_Callback (CVI生成ヘッダに含まれないため手動宣言) -----------------*/
int CVICALLBACK Timer_Callback(int panel, int control, int event,
                               void *callbackData, int eventData1,
                               int eventData2);

/*--- ショットCSVログ コールバック -------------------------------------------*/
int CVICALLBACK CsvLogEnable_Callback(int panel, int control, int event,
                                       void *callbackData, int eventData1,
                                       int eventData2);

/*--- 内部関数プロトタイプ ---------------------------------------------------*/
void UpdateChannelDisplay(int ch, const ChannelData *data);
void UpdateConnectionStatus(int isConnected);
void LogMessage(const char *msg);
void LogMessageFmt(const char *fmt, ...);
int  DetectAndApplyDataFormat(void);
void StartMonitoring(void);
void StopMonitoring(void);
void ReadAndDisplayTemperatures(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOMONITOR_APP_H */
