/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  PANEL                            1       /* callback function: PanelCallback */
#define  PANEL_PANEL_INTERVAL             2       /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_BTN_MONITOR          3       /* control type: command, callback function: BtnMonitor_Callback */
#define  PANEL_TEXTMSG_2                  4       /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_TIME_RANGE           5       /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_CHK_CH0              6       /* control type: radioButton, callback function: ChkChannel_Callback */
#define  PANEL_PANEL_CHK_CH1              7       /* control type: radioButton, callback function: ChkChannel_Callback */
#define  PANEL_PANEL_CHK_CH2              8       /* control type: radioButton, callback function: ChkChannel_Callback */
#define  PANEL_PANEL_CHK_CH3              9       /* control type: radioButton, callback function: ChkChannel_Callback */
#define  PANEL_TEXTMSG_3                  10      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_LED_CH0              11      /* control type: LED, callback function: (none) */
#define  PANEL_PANEL_TEMP_CH0             12      /* control type: numeric, callback function: (none) */
#define  PANEL_TEXTMSG_4                  13      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_LED_CH1              14      /* control type: LED, callback function: (none) */
#define  PANEL_PANEL_TEMP_CH1             15      /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_FAULT_CH0            16      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_LED_CH2              17      /* control type: LED, callback function: (none) */
#define  PANEL_PANEL_TEMP_CH2             18      /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_FAULT_CH1            19      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_TEMP_CH3             20      /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_LED_CH3              21      /* control type: LED, callback function: (none) */
#define  PANEL_PANEL_FAULT_CH2            22      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_FAULT_CH3            23      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_BTN_GRAPH_CLEAR      24      /* control type: command, callback function: BtnGraphClear_Callback */
#define  PANEL_TEXTMSG_5                  25      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_6                  26      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_LOG                  27      /* control type: textBox, callback function: (none) */
#define  PANEL_PANEL_BTN_LOG_CLEAR        28      /* control type: command, callback function: BtnClear_Callback */
#define  PANEL_PANEL_IP_ADDR              29      /* control type: string, callback function: (none) */
#define  PANEL_PANEL_PORT                 30      /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_UNIT_ID              31      /* control type: numeric, callback function: (none) */
#define  PANEL_PANEL_BTN_CONNECT          32      /* control type: command, callback function: BtnConnect_Callback */
#define  PANEL_PANEL_STATUS               33      /* control type: textMsg, callback function: (none) */
#define  PANEL_PANEL_TIMER                34      /* control type: timer, callback function: Timer_Callback */
#define  PANEL_PANEL_STRIPCHT             35      /* control type: strip, callback function: (none) */
#define  PANEL_TEXTMSG_7                  36      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_8                  37      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_9                  38      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_11                 39      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_10                 40      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG                    41      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_12                 42      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_13                 43      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_14                 44      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_15                 45      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_16                 46      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_17                 47      /* control type: textMsg, callback function: (none) */
#define  PANEL_TEXTMSG_18                 48      /* control type: textMsg, callback function: (none) */
#define  PANEL_SENSOR_TYPE_SEL            49      /* control type: ring, callback function: SensorTypeSel_Callback */
#define  PANEL_SENSOR_INFO                50      /* control type: textMsg, callback function: (none) */
#define  PANEL_FORMAT_CONFIDENCE          51      /* control type: textMsg, callback function: (none) */
#define  PANEL_CH3_YMIN                   52      /* control type: numeric, callback function: (none) */
#define  PANEL_CH3_YMAX                   53      /* control type: numeric, callback function: (none) */
#define  PANEL_CH2_YMIN                   54      /* control type: numeric, callback function: (none) */
#define  PANEL_CH2_YMAX                   55      /* control type: numeric, callback function: (none) */
#define  PANEL_CH1_YMIN                   56      /* control type: numeric, callback function: (none) */
#define  PANEL_CH1_YMAX                   57      /* control type: numeric, callback function: (none) */
#define  PANEL_CH0_YMIN                   58      /* control type: numeric, callback function: (none) */
#define  PANEL_CH0_YMAX                   59      /* control type: numeric, callback function: (none) */
#define  PANEL_CSV_LOG_ENABLE             60      /* control type: radioButton, callback function: CsvLogEnable_Callback */
#define  PANEL_DECORATION                 61      /* control type: deco, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK BtnClear_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BtnConnect_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BtnGraphClear_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BtnMonitor_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ChkChannel_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CsvLogEnable_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK PanelCallback(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK SensorTypeSel_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK Timer_Callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif