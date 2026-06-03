/*==============================================================================
 * modbus_tcp.h
 * Modbus TCP 通信ライブラリ
 * CPSN-MCB271 / CPSN-SSI-4C 熱電対モジュール用
 *============================================================================*/

#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*--- 定数定義 ---------------------------------------------------------------*/
#define MODBUS_PORT_DEFAULT     502
#define MODBUS_UNIT_ID_DEFAULT  1
#define MODBUS_TCP_TIMEOUT_MS   10000   /* 接続タイムアウト [ms] */

/* ファンクションコード */
#define FC_READ_COILS           0x01
#define FC_READ_DISCRETE        0x02
#define FC_READ_HOLDING_REG     0x03
#define FC_READ_INPUT_REG       0x04
#define FC_WRITE_SINGLE_COIL    0x05
#define FC_WRITE_SINGLE_REG     0x06
#define FC_WRITE_MULTIPLE_REG   0x10

/* エラーコード */
#define MODBUS_OK               0
#define MODBUS_ERR_CONNECT     -1
#define MODBUS_ERR_SEND        -2
#define MODBUS_ERR_RECV        -3
#define MODBUS_ERR_TIMEOUT     -4
#define MODBUS_ERR_EXCEPTION   -5
#define MODBUS_ERR_INVALID     -6

/* SSI-4C チャネル数 */
#define SSI4C_NUM_CHANNELS      4

/* フォルトビット定義 (D24-D31) ― Raw SSI 形式のみ有効 */
#define FAULT_VALID_BIT         (1 << 24)   /* D24: 有効ビット (1=有効) */
#define FAULT_ADC_RANGE         (1 << 25)   /* D25: ADC範囲外 */
#define FAULT_SENSOR_LOW        (1 << 26)   /* D26: センサ下限超過 */
#define FAULT_SENSOR_HIGH       (1 << 27)   /* D27: センサ上限超過 */
#define FAULT_CJ_SOFT           (1 << 28)   /* D28: CJソフトフォルト */
#define FAULT_CJ_HARD           (1 << 29)   /* D29: CJハードフォルト */
#define FAULT_ADC_HARD          (1 << 30)   /* D30: ADCハードフォルト */
#define FAULT_SENSOR_HARD       (1U << 31)  /* D31: センサハードフォルト */

#define FAULT_HARD_MASK         (FAULT_SENSOR_HARD | FAULT_ADC_HARD | FAULT_CJ_HARD)
#define FAULT_SOFT_MASK         (FAULT_CJ_SOFT | FAULT_SENSOR_HIGH | FAULT_SENSOR_LOW | FAULT_ADC_RANGE)
#define FAULT_ERROR_TEMP        (-999.0)    /* フォルト時の出力温度 */

/*--- Modbus データ形式 -------------------------------------------------------*/
/* CPSN-SSI-4C の Web設定「Modbusデータ形式」に対応する3種類。
 * FC04 で返る 32bit データのビット構造が形式ごとに異なる。         */
typedef enum {
    DATA_FORMAT_RAW_SSI  = 0,  /* 測定値(0～4294967295): D24=有効, 温度/1024 */
    DATA_FORMAT_TEMP_01  = 1,  /* 温度(0.1℃)+ステータス: reg0=温度×10, reg1=フォルト */
    DATA_FORMAT_TEMP_001 = 2   /* 温度(0.001℃): int32/1000, フォルトビットなし */
} DataFormat;

extern const char *DataFormatNames[3];  /* 形式名称文字列配列 */

/*--- センサタイプ ------------------------------------------------------------*/
typedef enum {
    SENSOR_TYPE_J = 0x00,
    SENSOR_TYPE_K = 0x01,
    SENSOR_TYPE_E = 0x02,
    SENSOR_TYPE_N = 0x03,
    SENSOR_TYPE_R = 0x04,
    SENSOR_TYPE_S = 0x05,
    SENSOR_TYPE_T = 0x06,
    SENSOR_TYPE_Q = 0x07
} SensorType;

/*--- チャネルデータ構造体 ----------------------------------------------------*/
typedef struct {
    double   temperature;    /* 温度 (℃) */
    uint8_t  faultByte;      /* フォルトバイト (Raw SSI: D24-D31, 他形式: 0) */
    int      isValid;        /* データ有効フラグ */
    int      hasHardFault;   /* ハードフォルト有無 */
    int      hasSoftFault;   /* ソフトフォルト有無 */
    char     faultMsg[128];  /* フォルトメッセージ */
} ChannelData;

/*--- 関数プロトタイプ -------------------------------------------------------*/

/* Modbus TCP 接続 / 切断 / 状態取得 */
int  Modbus_Connect(const char *ipAddr, int port, int timeoutMs);
int  Modbus_Disconnect(void);
int  Modbus_IsConnected(void);

/* 入力レジスタ読み取り (Function Code 0x04) */
int  Modbus_ReadInputRegisters(int unitId, int startAddr, int quantity,
                                uint16_t *regValues);

/* データ形式ヒューリスティック検出
 *   戻り値 : 判定した DataFormat
 *   *confidence : 判定に一致したチャンネル数 (0-4)  */
DataFormat SSI4C_DetectDataFormat(int unitId, int startAddr, int *confidence);

/* SSI-4C 全チャンネル温度読み取り */
int  SSI4C_ReadAllChannels(int unitId, int startAddr, ChannelData *chData,
                            DataFormat fmt);

/* レジスタペア → チャンネルデータ解析 */
void SSI4C_ParseData(uint16_t rawHigh, uint16_t rawLow,
                     ChannelData *chData, DataFormat fmt);

/* フォルトメッセージ生成 (Raw SSI 形式用) */
void SSI4C_GetFaultMessage(uint8_t faultByte, char *msgBuf, int bufLen);

/* エラーコード → 文字列変換 */
const char* Modbus_ErrorString(int errCode);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TCP_H */
