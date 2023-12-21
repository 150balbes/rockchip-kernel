
#ifndef _TYPE_H
#define _TYPE_H

#include<linux/types.h>

#define BIT0_1  0x01
#define BIT1_1  0x02
#define BIT2_1  0x04
#define BIT3_1  0x08
#define BIT4_1  0x10
#define BIT5_1  0x20
#define BIT6_1  0x40
#define BIT7_1  0x80

#define BIT0_0  0xFE
#define BIT1_0  0xFD
#define BIT2_0  0xFB
#define BIT3_0  0xF7
#define BIT4_0  0xEF
#define BIT5_0  0xDF
#define BIT6_0  0xBF
#define BIT7_0  0x7F
//state
#define     LONTIUM_FALSE                      0
#define     LONTIUM_TRUE                       1
#define     LOW                        0
#define     HIGH                       1
#define     OFF                        0
#define     ON                         1
#define     LED_ON                     0
#define     LED_OFF                    1
#define     ENABLED                    1
#define     DISABLED                   0
#define     NEGITVE                    0
#define     POSITIVE                   1
#define     FAIL                       0
#define     SUCCESS                    1

#define     UNSTABLE                   0
#define     STABLE                     1
#define     STABLE_UNKNOWN             2

#define     LVDS_IN_LVDS_OUT           0
#define     LVDS_IN_MIPI_OUT           1
#define     LVDS_IN_TTL_OUT            2
#define     MIPI_IN_LVDS_OUT           3
#define     MIPI_IN_MIPI_OUT           4
#define     MIPI_IN_TTL_OUT            5
#define     TTL_IN_LVDS_OUT            6
#define     TTL_IN_MIPI_OUT            7
#define     TTL_IN_TTL_OUT             8
#define     MIPI_REPEATER              9   
#define     MIPI_LEVEL_SHIFT           10
#define     PATTERN_OUT                11

#define     NO_TX_PATTERN              0
#define     MIPITX_PATTERN             1
#define     LVDSTX_PATTERN             2
#define     TTLTX_PATTERN              3

#define     RGB                        0
#define     YUV422                     1
#define     YUV444                     2
#define     YUV420                     3

#define     P_MODE                     0
#define     I_MODE                     1

#define     DEEP_8                     8
#define     DEEP_10                    10
#define     DEEP_12                    12

//---------------------------LVDS MODE------------------------
#define     LVDS_No_SSC                0
#define     LVDS_W_SSC                 1

#define     FOUR_LANE                     0
#define     FIVE_LANE                     1

#define     DE_MODE                       0
#define     SYNC_MODE                     1

#define     VESA                          0
#define     JEIDA                         1

#define     DEPTH_6BIT                    0
#define     DEPTH_8BIT                    1
#define     DEPTH_10BIT                   2
#define     DEPTH_12BIT                   3

#define     P_FORMAT                      0
#define     I_FORMAT                      1

#define     NON_REPECTIVE                 0
#define     REPECTIVE                     1

#define     VID_640x480_60Hz            0
#define     VID_720x480_60Hz            1
#define     VID_1280x720_60Hz           2
#define     VID_1366x768_60Hz           3
#define     VID_1280x720_30Hz           4
#define     VID_1920x720_60Hz           5
#define     VID_1920x1080_30Hz          6
#define     VID_1920x1080_60Hz          7
#define     VID_1920x1200_60Hz          8
#define     VID_3840x2160_30Hz          9

#define     NO_SSC                        0
#define     SSC_1920x1080_30k5         1
#define     SSC_3840x2160_30k5         2

//---------------------------MIPI MODE------------------------
#define     MIPI_DSI                 0
#define     MIPI_CSI                 1

#define     MIPI_DPHY                0
#define     MIPI_CPHY                1

#define     MIPITX_DCS                 0
#define     MIPITX_CCS                 1

#define     PORTA                      0
#define     PORTB                      1
#define     DOU_PORT                   2
#define     NO_COPY                    3

#define     MIPI_NEGITVE               1
#define     MIPI_POSITIVE              0

#define     MIPITX_PLL_LOW             80000
#define     MIPITX_PLL_HIGH            2700000

#define     CTS_DATARATE               1900000


//MIPI LEVEL SHIFT
#define     RX_LS                       0
#define     TX_LS                       1

#define     HS_PORTA_LP_PORTB           0
#define     LP_PORTA_HS_PORTB           1

//---------------------------TTL MODE------------------------
#define     SDR                 0
#define     DDR                 1
#define     DPI                 2

#define     BT1120              0
#define     BTA_T1004           1
#define     BT656               2

#define     PLUS                0
#define     MINUS               1

#define     TTL_NORMAL_MODE     0
#define     TTL_SYNC_MODE       1
#define     TTL_DE_MODE         2

#define     INPUT_RGB888               0
#define     INPUT_RGB666               1
#define     INPUT_RGB565               2
#define     INPUT_YCBCR444             3
#define     INPUT_YCBCR422_8BIT        4
#define     INPUT_YCBCR422_10BIT       5
#define     INPUT_YCBCR422_12BIT       6
#define     INPUT_YCBCR422_16BIT       7
#define     INPUT_YCBCR422_20BIT       8
#define     INPUT_YCBCR422_24BIT       9
#define     INPUT_BT656_8BIT           10
#define     INPUT_BT656_10BIT          11
#define     INPUT_BT656_12BIT          12
#define     INPUT_BT1120_8BIT          13
#define     INPUT_BT1120_10BIT         14
#define     INPUT_BT1120_12BIT         15
#define     INPUT_BTA_T1004_16BIT      16
#define     INPUT_BTA_T1004_20BIT      17
#define     INPUT_BTA_T1004_24BIT      18
#define     INPUT_BT1120_16BIT         19
#define     INPUT_BT1120_20BIT         20
#define     INPUT_BT1120_24BIT         21

#define     OUTPUT_RGB888               0
#define     OUTPUT_RGB666               1
#define     OUTPUT_RGB565               2
#define     OUTPUT_YCBCR444             3
#define     OUTPUT_YCBCR422_8BIT        4
#define     OUTPUT_YCBCR422_10BIT       5
#define     OUTPUT_YCBCR422_12BIT       6
#define     OUTPUT_YCBCR422_16BIT       7
#define     OUTPUT_YCBCR422_20BIT       8
#define     OUTPUT_YCBCR422_24BIT       9
#define     OUTPUT_BT656_8BIT           10
#define     OUTPUT_BT656_10BIT          11
#define     OUTPUT_BT656_12BIT          12
#define     OUTPUT_BT1120_8BIT          13
#define     OUTPUT_BT1120_10BIT         14
#define     OUTPUT_BT1120_12BIT         15
#define     OUTPUT_BTA_T1004_16BIT      16
#define     OUTPUT_BTA_T1004_20BIT      17
#define     OUTPUT_BTA_T1004_24BIT      18
#define     OUTPUT_BT1120_16BIT         19
#define     OUTPUT_BT1120_20BIT         20
#define     OUTPUT_BT1120_24BIT         21


//TX
typedef struct VidChkTiming
{
    u16 usHfp;
    u16 usHs;
    u16 usHbp;
    u16 usHact;
    u16 usHtotal;
    
    u16 usVfp;
    u16 usVs;
    u16 usVbp;
    u16 usVact;
    u16 usVtotal;
    
    u32 ulPclk_Khz;
    u8  ucFrameRate;
    
    u8 ucHspol;
    u8 ucVspol;
}StructVidChkTiming;

//RX
typedef struct ChipRxVidTiming
{
    u32 usHfp;
    u32 usHs;
    u32 usHbp;
    u32 usHact;
    u32 usHtotal;
    
    u32 usVfp;
    u32 usVs;
    u32 usVbp;
    u32 usVact;
    u32 usVtotal;
    
    u32 ulPclk_Khz;
    u32  ucFrameRate;
}StructChipRxVidTiming;


//PCR setting
typedef struct
{
    u32 Pcr_M;
    u32 Pcr_K;
    u8  Pcr_UpLimit;
    u8  Pcr_DownLimit;
}StructPcrPara;
#endif