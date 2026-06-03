/*==============================================================================
 * modbus_tcp.c
 * Modbus TCP 通信ライブラリ実装
 * CPSN-MCB271 / CPSN-SSI-4C 熱電対モジュール用
 *
 * 26.04.23
 *
 * 通信仕様:
 *   - CPSN-MCB271 に Modbus TCP (port 502) で接続
 *   - CPSN-SSI-4C: Function Code 0x04 で入力レジスタを読み取り
 *   - データ形式は Web設定により3種類。FC04 生データのビットパターンで自動判定。
 *     DATA_FORMAT_RAW_SSI : D31-D24=フォルトバイト, D23-D0=温度/1024
 *     DATA_FORMAT_TEMP_01 : reg0=温度×10(0.1℃), reg1=フォルトフラグ
 *     DATA_FORMAT_TEMP_001: int32/1000 (0.001℃単位, フォルトビットなし)
 *============================================================================*/

#include <ansi_c.h>
#include <tcpsupp.h>
#include <utility.h>
#include "modbus_tcp.h"

/*--- 内部定数 ---------------------------------------------------------------*/
#define MBAP_HEADER_SIZE    6       /* Modbus TCP MBAP ヘッダサイズ */
#define MAX_ADU_SIZE        260     /* Modbus TCP ADU 最大サイズ */
#define RECV_TIMEOUT_MS     3000    /* 受信タイムアウト [ms] */

/*--- データ形式名称 ----------------------------------------------------------*/
const char *DataFormatNames[3] = {
    "Raw SSI (測定値)",    /* DATA_FORMAT_RAW_SSI  */
    "0.1℃+ステータス",
    "0.001℃"
};

/*--- 内部変数 ---------------------------------------------------------------*/
static unsigned  g_tcpHandle = 0;
static int       g_connected = 0;
static uint16_t  g_transId   = 0;

/*--- 内部関数プロトタイプ ---------------------------------------------------*/
static void     PackUint16BE(uint8_t *buf, uint16_t val);
static uint16_t UnpackUint16BE(const uint8_t *buf);
static int32_t  SignExtend24(uint32_t val24);

/*============================================================================
 * Modbus_Connect - Modbus TCP サーバへ接続
 *============================================================================*/
int Modbus_Connect(const char *ipAddr, int port, int timeoutMs)
{
    int ret;

    if (g_connected)
        Modbus_Disconnect();

    ret = ConnectToTCPServer(&g_tcpHandle, port, ipAddr, NULL, NULL,
                             (timeoutMs > 0) ? timeoutMs : RECV_TIMEOUT_MS);
    if (ret < 0) {
        g_connected = 0;
        g_tcpHandle = 0;
        return MODBUS_ERR_CONNECT;
    }

    g_connected = 1;
    g_transId   = 0;
    return MODBUS_OK;
}

/*============================================================================
 * Modbus_Disconnect - Modbus TCP 接続を切断
 *============================================================================*/
int Modbus_Disconnect(void)
{
    if (g_connected && g_tcpHandle) {
        DisconnectFromTCPServer(g_tcpHandle);
        g_tcpHandle = 0;
    }
    g_connected = 0;
    return MODBUS_OK;
}

/*============================================================================
 * Modbus_IsConnected - 接続状態を返す
 *============================================================================*/
int Modbus_IsConnected(void)
{
    return g_connected;
}

/*============================================================================
 * Modbus_ReadInputRegisters - 入力レジスタ読み取り (FC 0x04)
 *============================================================================*/
int Modbus_ReadInputRegisters(int unitId, int startAddr, int quantity,
                               uint16_t *regValues)
{
    uint8_t  reqBuf[MAX_ADU_SIZE];
    uint8_t  rspBuf[MAX_ADU_SIZE];
    int      byteCount, ret, i, expectedLen;
    uint16_t rxTransId, rxLength;

    if (!g_connected || !regValues || quantity < 1 || quantity > 125)
        return MODBUS_ERR_INVALID;

    g_transId++;
    PackUint16BE(&reqBuf[0], g_transId);
    PackUint16BE(&reqBuf[2], 0x0000);
    PackUint16BE(&reqBuf[4], 0x0006);
    reqBuf[6] = (uint8_t)unitId;
    reqBuf[7] = FC_READ_INPUT_REG;
    PackUint16BE(&reqBuf[8],  (uint16_t)startAddr);
    PackUint16BE(&reqBuf[10], (uint16_t)quantity);

    ret = ClientTCPWrite(g_tcpHandle, reqBuf, 12, RECV_TIMEOUT_MS);
    if (ret < 0) {
        g_connected = 0;
        return MODBUS_ERR_SEND;
    }

    ret = ClientTCPRead(g_tcpHandle, rspBuf, 6, RECV_TIMEOUT_MS);
    if (ret < 0) {
        if (ret != kTCP_TimeOutErr) g_connected = 0;
        return (ret == kTCP_TimeOutErr) ? MODBUS_ERR_TIMEOUT : MODBUS_ERR_RECV;
    }
    if (ret != 6) {   /* 0バイト(接続クローズ)または部分読み取り */
        g_connected = 0;
        return MODBUS_ERR_RECV;
    }

    rxTransId = UnpackUint16BE(&rspBuf[0]);
    rxLength  = UnpackUint16BE(&rspBuf[4]);
    if (rxTransId != g_transId) {
        /* TCPバッファの同期を維持するためペイロードを読み捨て */
        if (rxLength > 0 && rxLength <= MAX_ADU_SIZE)
            ClientTCPRead(g_tcpHandle, rspBuf, (int)rxLength, RECV_TIMEOUT_MS);
        return MODBUS_ERR_INVALID;
    }

    ret = ClientTCPRead(g_tcpHandle, &rspBuf[6], (int)rxLength, RECV_TIMEOUT_MS);
    if (ret < 0) {
        if (ret != kTCP_TimeOutErr) g_connected = 0;
        return (ret == kTCP_TimeOutErr) ? MODBUS_ERR_TIMEOUT : MODBUS_ERR_RECV;
    }
    if (ret != (int)rxLength) {   /* 0バイト(接続クローズ)または部分読み取り */
        g_connected = 0;
        return MODBUS_ERR_RECV;
    }

    if (rspBuf[7] & 0x80) return MODBUS_ERR_EXCEPTION;

    byteCount   = rspBuf[8];
    expectedLen = quantity * 2;
    if (byteCount != expectedLen) return MODBUS_ERR_INVALID;

    for (i = 0; i < quantity; i++)
        regValues[i] = UnpackUint16BE(&rspBuf[9 + i * 2]);

    return MODBUS_OK;
}

/*============================================================================
 * SSI4C_DetectDataFormat - FC04 生データからデータ形式をヒューリスティック判定
 *
 * 全4チャンネルの上位バイト（D31-D24）を検査し、最多票の形式を返す。
 * *confidence : 判定に一致したチャンネル数 (0-4)
 *              0 = 全チャンネルがフォルト等で判定不能 → デフォルト(RAW_SSI)
 *
 * 判定根拠:
 *   Raw SSI 正常時: D31-D25=0(フォルトなし), D24=1(有効) → 上位バイト=0x01
 *   0.001℃ 正温度: 上位16bit < 0x001C (最大1800℃×1000=1800000<0x1B7740)
 *   0.1℃+ST 正温度: 上位16bit(reg0)が温度×10の16bit値, 下位16bit上位バイト=0x00
 *============================================================================*/
DataFormat SSI4C_DetectDataFormat(int unitId, int startAddr, int *confidence)
{
    uint16_t  reg[SSI4C_NUM_CHANNELS * 2];
    int       votes[3] = {0, 0, 0};
    int       ch, ret;
    uint8_t   upperByte;
    uint16_t  regHigh, regLow;

    if (confidence) *confidence = 0;

    ret = Modbus_ReadInputRegisters(unitId, startAddr,
                                    SSI4C_NUM_CHANNELS * 2, reg);
    if (ret != MODBUS_OK)
        return DATA_FORMAT_RAW_SSI;   /* 読取失敗 → デフォルト */

    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++) {
        regHigh   = reg[ch * 2];
        regLow    = reg[ch * 2 + 1];
        upperByte = (uint8_t)(regHigh >> 8);

        /* Raw SSI 判定: 上位バイト=0x01(有効ビット) かつ regLow上位バイトが非ゼロ
         * TEMP_01 の reg1（フォルトレジスタ）は上位バイトが必ず0x00のため区別できる。
         * ただし Raw SSI で温度が極低温（<約0.25℃）の場合は regLow上位も0になるが実用上問題なし。 */
        if (upperByte == 0x01 && (regLow >> 8) != 0x00) {
            votes[DATA_FORMAT_RAW_SSI]++;
            continue;
        }

        /* 0.001℃ 判定: 上位16bit が小さい正値 (正温度 < 1800℃ なら regHigh <= 0x001B) */
        if (regHigh <= 0x001B) {
            votes[DATA_FORMAT_TEMP_001]++;
            continue;
        }

        /* 0.1℃+ステータス 判定: reg1 の上位バイトが 0x00 */
        if ((regLow >> 8) == 0x00) {
            votes[DATA_FORMAT_TEMP_01]++;
            continue;
        }

        /* フォルト・不定 → 票なし */
    }

    /* 最多票の形式を選択 */
    {
        DataFormat best = DATA_FORMAT_RAW_SSI;
        int        maxV = votes[0];
        int        f;
        for (f = 1; f < 3; f++) {
            if (votes[f] > maxV) {
                maxV = votes[f];
                best = (DataFormat)f;
            }
        }
        if (confidence) *confidence = maxV;
        return best;
    }
}

/*============================================================================
 * SSI4C_ReadAllChannels - SSI-4C の全チャンネル温度読み取り
 *============================================================================*/
int SSI4C_ReadAllChannels(int unitId, int startAddr, ChannelData *chData,
                           DataFormat fmt)
{
    uint16_t reg[SSI4C_NUM_CHANNELS * 2];
    int      ret, ch;

    if (!chData) return MODBUS_ERR_INVALID;

    ret = Modbus_ReadInputRegisters(unitId, startAddr,
                                    SSI4C_NUM_CHANNELS * 2, reg);
    if (ret != MODBUS_OK) return ret;

    for (ch = 0; ch < SSI4C_NUM_CHANNELS; ch++)
        SSI4C_ParseData(reg[ch * 2], reg[ch * 2 + 1], &chData[ch], fmt);

    return MODBUS_OK;
}

/*============================================================================
 * SSI4C_ParseData - レジスタペアを解析して温度・フォルト情報を取得
 *
 * rawHigh : 上位16bit レジスタ値 (reg0)
 * rawLow  : 下位16bit レジスタ値 (reg1)
 * fmt     : データ形式 (Web設定に対応)
 *============================================================================*/
void SSI4C_ParseData(uint16_t rawHigh, uint16_t rawLow,
                     ChannelData *chData, DataFormat fmt)
{
    uint32_t ssiData;
    int32_t  tempInt;

    if (!chData) return;

    switch (fmt) {

    /* ---------------------------------------------------------------------- */
    case DATA_FORMAT_RAW_SSI:
        /* D31-D24: フォルトバイト / D23-D0: 温度(24bit符号付き) / 除数1024 */
        ssiData = ((uint32_t)rawHigh << 16) | (uint32_t)rawLow;

        chData->faultByte    = (uint8_t)((ssiData >> 24) & 0xFF);
        chData->isValid      = (ssiData & FAULT_VALID_BIT) ? 1 : 0;
        chData->hasHardFault = ((ssiData & FAULT_HARD_MASK) != 0) ? 1 : 0;
        chData->hasSoftFault = ((ssiData & FAULT_SOFT_MASK) != 0) ? 1 : 0;
        SSI4C_GetFaultMessage(chData->faultByte, chData->faultMsg,
                              sizeof(chData->faultMsg));

        if (chData->hasHardFault || !chData->isValid) {
            chData->temperature = FAULT_ERROR_TEMP;
            return;
        }
        tempInt = SignExtend24(ssiData & 0x00FFFFFF);
        chData->temperature = (double)tempInt / 1024.0;
        break;

    /* ---------------------------------------------------------------------- */
    case DATA_FORMAT_TEMP_01:
        /* reg0: 温度×10 (16bit符号付き) / reg1下位バイト: フォルトフラグ */
        chData->faultByte    = (uint8_t)(rawLow & 0xFF);
        chData->hasHardFault = ((chData->faultByte & 0xE0) != 0) ? 1 : 0;
        chData->hasSoftFault = ((chData->faultByte & 0x1E) != 0) ? 1 : 0;
        chData->isValid      = 1;
        SSI4C_GetFaultMessage(chData->faultByte, chData->faultMsg,
                              sizeof(chData->faultMsg));

        if (chData->hasHardFault) {
            chData->temperature = FAULT_ERROR_TEMP;
            return;
        }
        chData->temperature = (double)(int16_t)rawHigh / 10.0;
        break;

    /* ---------------------------------------------------------------------- */
    case DATA_FORMAT_TEMP_001:
        /* int32/1000 (フォルトビットなし) */
        chData->faultByte    = 0;
        chData->isValid      = 1;
        chData->hasHardFault = 0;
        chData->hasSoftFault = 0;
        strncpy(chData->faultMsg, "正常", sizeof(chData->faultMsg) - 1);
        chData->faultMsg[sizeof(chData->faultMsg) - 1] = '\0';

        tempInt = (int32_t)(((uint32_t)rawHigh << 16) | (uint32_t)rawLow);
        chData->temperature = (double)tempInt / 1000.0;
        break;
    }
}

/*============================================================================
 * SSI4C_GetFaultMessage - フォルトメッセージ文字列生成 (Raw SSI 形式用)
 *============================================================================*/
void SSI4C_GetFaultMessage(uint8_t faultByte, char *msgBuf, int bufLen)
{
    uint32_t fault32;

    if (!msgBuf || bufLen <= 0) return;

    msgBuf[0] = '\0';
    fault32   = (uint32_t)faultByte << 24;

    if (!(fault32 & FAULT_VALID_BIT)) {
        strncpy(msgBuf, "データ無効", bufLen - 1);
        msgBuf[bufLen - 1] = '\0';
        return;
    }

    if (fault32 & FAULT_SENSOR_HARD)
        strncat(msgBuf, "センサハードFLT ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_ADC_HARD)
        strncat(msgBuf, "ADCハードFLT ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_CJ_HARD)
        strncat(msgBuf, "CJハードFLT ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_CJ_SOFT)
        strncat(msgBuf, "CJソフトFLT ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_SENSOR_HIGH)
        strncat(msgBuf, "センサ上限超過 ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_SENSOR_LOW)
        strncat(msgBuf, "センサ下限超過 ", bufLen - strlen(msgBuf) - 1);
    if (fault32 & FAULT_ADC_RANGE)
        strncat(msgBuf, "ADC範囲外 ", bufLen - strlen(msgBuf) - 1);

    if (msgBuf[0] == '\0')
        strncpy(msgBuf, "正常", bufLen - 1);

    msgBuf[bufLen - 1] = '\0';
}

/*============================================================================
 * Modbus_ErrorString - エラーコードを文字列に変換
 *============================================================================*/
const char* Modbus_ErrorString(int errCode)
{
    switch (errCode) {
        case MODBUS_OK:            return "正常";
        case MODBUS_ERR_CONNECT:   return "接続エラー";
        case MODBUS_ERR_SEND:      return "送信エラー";
        case MODBUS_ERR_RECV:      return "受信エラー";
        case MODBUS_ERR_TIMEOUT:   return "タイムアウト";
        case MODBUS_ERR_EXCEPTION: return "Modbus例外エラー";
        case MODBUS_ERR_INVALID:   return "無効なデータ";
        default:                    return "不明なエラー";
    }
}

/*============================================================================
 * 内部関数: PackUint16BE / UnpackUint16BE / SignExtend24
 *============================================================================*/
static void PackUint16BE(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static uint16_t UnpackUint16BE(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

static int32_t SignExtend24(uint32_t val24)
{
    if (val24 & 0x00800000)
        return (int32_t)(val24 | 0xFF000000);
    return (int32_t)val24;
}
