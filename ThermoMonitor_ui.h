/*==============================================================================
 * ThermoMonitor_ui.h  - UIR 定数エイリアス
 *
 * CVI は UIR 保存時にパネル名 "PANEL_" をコントロール定数名の前に付加するため、
 * 例: UIR に PANEL_INTERVAL と設定 → ThermoMonitor.h に PANEL_PANEL_INTERVAL と生成
 *
 * このファイルはコード側の PANEL_XXX を実際の PANEL_PANEL_XXX に対応させる。
 * ThermoMonitor.h (CVI生成) をインクルード後に使用すること。
 *============================================================================*/

#ifndef THERMOMONITOR_UI_H
#define THERMOMONITOR_UI_H

/*--- 接続設定 ----------------------------------------------------------------*/
#define PANEL_IP_ADDR           PANEL_PANEL_IP_ADDR
#define PANEL_PORT              PANEL_PANEL_PORT
#define PANEL_UNIT_ID           PANEL_PANEL_UNIT_ID
#define PANEL_BTN_CONNECT       PANEL_PANEL_BTN_CONNECT
#define PANEL_STATUS            PANEL_PANEL_STATUS
/* 以下3つは ThermoMonitor.h に直接定義済み（エイリアス不要）
 *   PANEL_SENSOR_TYPE_SEL (ring,  ID=49) : センサタイプ手動選択
 *   PANEL_SENSOR_INFO     (text,  ID=50) : データ形式 自動判定結果表示
 *   PANEL_FORMAT_CONFIDENCE(text, ID=51) : 判定信頼度表示              */

/*--- モニタリング設定 --------------------------------------------------------*/
#define PANEL_INTERVAL          PANEL_PANEL_INTERVAL
#define PANEL_BTN_MONITOR       PANEL_PANEL_BTN_MONITOR
#define PANEL_TIMER             PANEL_PANEL_TIMER

/*--- グラフ設定 --------------------------------------------------------------*/
#define PANEL_TIME_RANGE        PANEL_PANEL_TIME_RANGE
#define PANEL_CHK_CH0           PANEL_PANEL_CHK_CH0
#define PANEL_CHK_CH1           PANEL_PANEL_CHK_CH1
#define PANEL_CHK_CH2           PANEL_PANEL_CHK_CH2
#define PANEL_CHK_CH3           PANEL_PANEL_CHK_CH3
#define PANEL_BTN_GRAPH_CLEAR   PANEL_PANEL_BTN_GRAPH_CLEAR

/*--- 現在値表示 (CH0-CH3) ---------------------------------------------------*/
#define PANEL_TEMP_CH0          PANEL_PANEL_TEMP_CH0
#define PANEL_TEMP_CH1          PANEL_PANEL_TEMP_CH1
#define PANEL_TEMP_CH2          PANEL_PANEL_TEMP_CH2
#define PANEL_TEMP_CH3          PANEL_PANEL_TEMP_CH3
#define PANEL_LED_CH0           PANEL_PANEL_LED_CH0
#define PANEL_LED_CH1           PANEL_PANEL_LED_CH1
#define PANEL_LED_CH2           PANEL_PANEL_LED_CH2
#define PANEL_LED_CH3           PANEL_PANEL_LED_CH3
#define PANEL_FAULT_CH0         PANEL_PANEL_FAULT_CH0
#define PANEL_FAULT_CH1         PANEL_PANEL_FAULT_CH1
#define PANEL_FAULT_CH2         PANEL_PANEL_FAULT_CH2
#define PANEL_FAULT_CH3         PANEL_PANEL_FAULT_CH3

/*--- ストリップチャート ------------------------------------------------------*/
#define PANEL_STRIPCHT          PANEL_PANEL_STRIPCHT

/*--- ログ -------------------------------------------------------------------*/
#define PANEL_LOG               PANEL_PANEL_LOG
#define PANEL_BTN_LOG_CLEAR     PANEL_PANEL_BTN_LOG_CLEAR

/* PANEL_CSV_LOG_ENABLE は ThermoMonitor.h に直接定義済み（エイリアス不要）
 *   PANEL_CSV_LOG_ENABLE (radioButton, ID=60) : ショットCSVログ有効/無効 */

#endif /* THERMOMONITOR_UI_H */
