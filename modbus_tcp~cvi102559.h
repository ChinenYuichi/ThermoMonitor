/*==============================================================================
 * modbus_tcp.h
 * Modbus TCP 通信ライブラリ
 * CPSN-MCB271 / CPSN-SSI-4C 熱電対モジュール用
 *
 * 26.04.20
 *
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
#define MODBUS_TCP_TIMEOUT_MS   3000

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

/* フォルトビット定義 (D24-D31) */
#define FAULT_VALID_BIT         (1 << 24)   /* D24: 有効ビット (1=有効) */
#define FAULT_ADC_RANGE         (1 << 25)   /* D25: ADC範囲外 */
#define FAULT_SENSOR_LOW        (1 << 26)   /* D26: センサ下限超過 */
#define FAULT_SENSOR_HIGH       (1 << 27)   /* D27: センサ上限超過 */
#define FAULT_CJ_SOFT           (1 << 28)   /* D28: CJソフトフォルト */
#define FAULT_CJ_HARD           (1 << 29)   /* D29: CJハードフォルト */
#define FAULT_ADC_HARD          (1 << 30)   /* D30: ADCハードフォルト */
#define FAULT_SENSOR_HARD       (1 << 31)   /* D31: センサハードフォルト */

#define FAULT_HARD_MASK         (FAULT_SENSOR_HARD | FAULT_ADC_HARD | FAULT_CJ_HARD)
#define FAULT_SOFT_MASK         (FAULT_CJ_SOFT | FAULT_SENSOR_HIGH | FAULT_SENSOR_LOW | FAULT_ADC_RANGE)
#define FAULT_ERROR_TEMP        (-999.0)    /* ハードフォルト時の出力温度 */

/* センサタイプ */
typedef enum {
    SENSOR_TYPE_J = 0x00,
    SENSOR_TYPE_K = 0x01,
    SENSOR_TYPE_E = 0x02,
    SENSOR_TYPE_N = 0x03,
    SENSOR_TYPE_R = 0x04,
    SENSOR_TYPE_S = 0x05,
    SENSOR_TYPE_T = 0x06
} SensorType;

/* チャネルデータ構造体 */
typedef struct {
    double   temperature;    /* 温度 (℃) */
    uint8_t  faultByte;      /* フォルトバイト (D24-D31) */
    int      isValid;        /* データ有効フラグ (D24) */
    int      hasHardFault;   /* ハードフォルト有無 */
    int      hasSoftFault;   /* ソフトフォルト有無 */
    char     faultMsg[128];  /* フォルトメッセージ */
} ChannelData;

/*--- 関数プロトタイプ -------------------------------------------------------*/

/* Modbus TCP 接続 */
int  Modbus_Connect(const char *ipAddr, int port, int timeoutMs);

/* Modbus TCP 切断 */
int  Modbus_Disconnect(void);

/* 接続状態取得 */
int  Modbus_IsConnected(void);

/* 入力レジスタ読み取り (Function Code 0x04) */
int  Modbus_ReadInputRegisters(int unitId, int startAddr, int quantity,
                                uint16_t *regValues);

/* SSI-4C 全チャネル温度読み取り */
int  SSI4C_ReadAllChannels(int unitId, int startAddr, ChannelData *chData);

/* SSI データ解析 */
void SSI4C_ParseData(uint32_t ssiData, ChannelData *chData);

/* フォルトメッセージ生成 */
void SSI4C_GetFaultMessage(uint8_t faultByte, char *msgBuf, int bufLen);

/* エラーコード → 文字列変換 */
const char* Modbus_ErrorString(int errCode);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TCP_H */
