#include    "include.h"

#ifndef     _MIPIRX_LVDSTX_H
#define     _MIPIRX_LVDSTX_H

#define     LVDSTX_PORT_SEL         DOU_PORT       //PORTA/PORTB/DOU_PORT
#define     LVDSTX_PORT_SWAP        ENABLED      //ENABLED/DISABLED
#define     LVDSTX_PORT_COPY        DISABLED        //ENABLED/DISABLED
#define     LVDSTX_LANENUM          FOUR_LANE       //FOUR_LANE/FIVE_LANE
#define     LVDSTX_MODE             SYNC_MODE       //SYNC_MODE/DE_MODE
#define     LVDSTX_DATAFORMAT       VESA            //VESA/JEIDA
#define     LVDSTX_COLORSPACE       RGB             //RGB/YUV422
#define     LVDSTX_COLORDEPTH       DEPTH_8BIT      //DEPTH_6BIT/DEPTH_8BIT/DEPTH_10BIT
#define     LVDSTX_SYNC_INTER_MODE  DISABLED         //ENABLED/DISABLED
#define     LVDSTX_VIDEO_FORMAT     P_FORMAT        //P_FORMAT/I_FORMAT
#define     LVDSTX_SYNC_CODE_SEND   NON_REPECTIVE       ///NON_REPECTIVE/REPECTIVE
#define     LVDS_SSC_SEL            NO_SSC          //NO_SSC/SSC_1920x1080_30k5/SSC_3840x2160_30k5
#define     MIPIRX_INPUT_SEL           MIPI_DSI            //MIPI_DSI/MIPI_CSI
#define     MIPIRX_PORT_SEL            PORTA               //PORTA/PORTB/DOU_PORT
#define     MIPIRX_LANE_NUM            MIPIRX_4LANE        //MIPIRX_4LANE/MIPIRX_3LANE/MIPIRX_2LANE/MIPIRX_1LANE
#define     MIPIRX_PORT_SWAP           DISABLED             //ENABLED/DISABLED
#define     MIPIRX_CLK_BURST           DISABLED             //ENABLED/DISABLED

typedef enum
{
    RGB_6Bit = 0,
    RGB_8Bit,
    RGB_10Bit,
    RGB_12Bit,
    YUV444_8Bit,
    YUV444_10Bit,
    YUV422_8bit,
    YUV422_10bit,
    YUV422_12bit,
    YUV420_8bit,
    YUV420_10bit,
    MIPITX_FORMAT_CNT,
}Enum_MIPI_FORMAT;
//MIPI RX Videotiming Debug
typedef struct VideoTiming_Get
{   
    u16 uswc;
    u16 usHact;
    u16 usVact;
    u8 ucFmt;
    u8 ucPa_Lpn;
    u8 ucFrameRate;
    u8 ucLane0SetNum;
    u8 ucLane1SetNum;
    u8 ucLane2SetNum;
    u8 ucLane3SetNum;
    u8 ucLane0SotData;
    u8 ucLane1SotData;
    u8 ucLane2SotData;
    u8 ucLane3SotData;
}SrtuctMipiRx_VidTiming_Get;

typedef struct VideoTimingList
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
    
    u8  ucFrameRate;

}VideoTimingList;


typedef enum
{
    MIPIRX_4LANE = 0x00,
    MIPIRX_1LANE = 0x01,
    MIPIRX_2LANE = 0x02,
    MIPIRX_3LANE = 0x03,
}Enum_MIPIRXPORTLANE_NUM;


extern StructPcrPara g_stPcrPara;
extern SrtuctMipiRx_VidTiming_Get g_stMipiRxVidTiming_Get;


extern void Mod_LvdsTx_Handler(LvdsOutParameter *lvds_parameter);
void Mod_LvdsTx_StateJudge(void);
extern void Mod_MipiRx_Handler(StructChipRxVidTiming *ptn_timing);

#endif