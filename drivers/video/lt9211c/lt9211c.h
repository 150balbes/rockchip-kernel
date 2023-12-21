#ifndef _MAIN_H
#define _MAIN_H

/*
#define         TX_PATTERN_SRC_SEL          LVDSTX_PATTERN   
#define         TX_VID_PATTERN_SEL          PTN_1920x1080_60  //NO_PATTERN/PTN_640x480_15/PTN_640x480_60/PTN_720x480_60/PTN_1280x720_60/PTN_1920x720_60/PTN_1920x1080_30
*/                                                    //PTN_1920x1080_60/PTN_1920x1080_90/PTN_1920x1080_100/PTN_2560x1440_60/PTN_3840x2160_30


/*==============LVDSTX PATTERN SETTING==============*/

/*
#define     LVDSTX_PORT_SEL         DOU_PORT       //PORTA/PORTB/DOU_PORT
#define     LVDSTX_PORT_SWAP        DISABLED       //ENABLED/DISABLED*/

#define     LVDSTX_PORT_COPY        DISABLED         //ENABLED/DISABLED
#define     LVDSTX_LANENUM          FOUR_LANE       //FOUR_LANE/FIVE_LANE
#define     LVDSTX_MODE             SYNC_MODE       //SYNC_MODE/DE_MODE
#define     LVDSTX_DATAFORMAT       VESA            //VESA/JEIDA
#define     LVDSTX_COLORSPACE       RGB             //RGB/YUV422
#define     LVDSTX_COLORDEPTH       DEPTH_8BIT      //DEPTH_6BIT/DEPTH_8BIT/DEPTH_10BIT
#define     LVDSTX_SYNC_INTER_MODE  DISABLED         //ENABLED/DISABLED
#define     LVDSTX_VIDEO_FORMAT     P_FORMAT        //P_FORMAT/I_FORMAT
#define     LVDSTX_SYNC_CODE_SEND   NON_REPECTIVE       ///NON_REPECTIVE/REPECTIVE
#define     LVDS_SSC_SEL            NO_SSC          //NO_SSC/SSC_1920x1080_30k5/SSC_3840x2160_30k5

//vedio settings
#define     TTLTX_VIDEO_FORMAT          P_FORMAT        //P_FORMAT/I_FORMAT
#define     TTLTX_VSYNC_POLARITY        PLUS            //PLUS/MINUS
#define     TTLTX_HSYNC_POLARITY        PLUS            //PLUS/MINUS

typedef enum
{  
    STATE_CHIPRX_POWER_ON = 1,      //0x01
    STATE_CHIPRX_WAITE_SOURCE,      //0x02
    STATE_CHIPRX_VIDTIMING_CONFIG,  //0x03      
    STATE_CHIPRX_PLL_CONFIG,        //0x04 
    STATE_CHIPRX_VIDEO_CHECK,       //0x05
    STATE_CHIPRX_PLAY_BACK          //0x06
}EnumChipRxState;

typedef enum
{
    STATE_CHIPTX_POWER_ON = 1,          //0x01
    STATE_CHIPTX_UPSTREAM_VIDEO_READY,  //0x02
    STATE_CHIPTX_CONFIG_VIDEO,          //0x03
    STATE_CHIPTX_VIDEO_OUT,             //0x04
    STATE_CHIPTX_PLAY_BACK              //0x05
}EnumChipTxState;

typedef enum
{
    MIPIRX_VIDEO_ON_EVENT = 1,
    MIPIRX_VIDEO_OFF_EVENT,
    MIPIRX_CSC_EVENT,
}EnumChipRxEvent;

typedef struct ChipRx
{
    u8 b1RxStateChanged;
    u8 b1VidChkScanFlg;
    u8 ucPixelEncoding;
    u8 ucRxFormat;
    u8 ucRxState;
    void (*pHdmiRxNotify)(EnumChipRxEvent ucEvent); 
}StructChipRx;


typedef struct ChipTx
{
    u8 b1TxStateChanged;
    u8 b1UpstreamVideoReady;
    u8 ucTxState;
}StructChipTx;

typedef enum
{
    LVDSRX  = 0x00,     
    MIPIRX  = 0x01,     //pcr recover video timing
    TTLRX   = 0x02,
    PATTERN = 0x03,
    LVDSDEBUG = 0x04,
    MIPIDEBUG = 0x05,
    TTLDEBUG  = 0x06,
    
}EnumChipRxSrc;

typedef enum
{
    LVDSTX  = 0x00,     
    MIPITX  = 0x01,
    
}EnumChipTxSel;

typedef enum
{
    AD_MLTX_READ_CLK    = 0x08,   //0x08
    AD_MLTX_WRITE_CLK   = 0x09,   //0x09
    AD_DESSCPLL_PIX_CLK = 0x10,   //0x10
    AD_RXPLL_PIX_CLK    = 0x1a,   //0x1a
    AD_DESSCPLL_PCR_CLK = 0x14,   //0x14
    AD_MLRXA_BYTE_CLK   = 0x18,
    AD_MLRXB_BYTE_CLK   = 0x1e,
}Enum_FM_CLK;


typedef  enum
{
    HTOTAL_POS    =    0,
    HACTIVE_POS,
    HFP_POS,
    HSW_POS,
    HBP_POS,
    
    VTOTAL_POS,
    VACTIVE_POS,
    VFP_POS,
    VSW_POS,
    VBP_POS,
    
    HSPOL_POS,
    VSPOL_POS,
}POS_INDEX;

typedef enum
{
    RXPLL_PIX_CLK     = 0x00,
    DESSCPLL_PIX_CLK  = 0x01,
    RXPLL_DEC_DDR_CLK = 0x02,
    MLRX_BYTE_CLK     = 0x03,
    
}Enum_VIDCHK_PIXCLK_SRC_SEL;

typedef struct LvdsOutParameter
{
    const char * lvdstx_port_sel;
    const char * lvdstx_port_swap;
    const char * lt9211c_mode_sel;
    
}LvdsOutParameter;

extern StructChipRx g_stChipRx;
extern StructChipTx g_stChipTx;
extern StructVidChkTiming g_stVidChk;
extern StructChipRxVidTiming g_stRxVidTiming;

extern void Drv_SystemActRx_Sel(u8 ucSrc);
extern void Drv_SystemTxSram_Sel(u8 ucSrc);
extern u8 Drv_System_GetPixelEncoding(void);
extern void Drv_System_VidChk_SrcSel(u8 ucSrc);
extern void Drv_System_VidChkClk_SrcSel(u8 ucSrc);
extern void Drv_VidChkAll_Get(StructVidChkTiming *video_time);

extern u8 Drv_VidChk_FrmRt_Get(void);
extern u32 Drv_System_FmClkGet(u8 ucSrc);
extern int LT9211C_write(u8 reg, u8 val);
extern int LT9211C_read(u8 reg);

extern void Mod_System_RxNotifyHandle(EnumChipRxEvent ucEvent);
extern void Mod_SystemRx_NotifyRegister(void (*pFunction)(EnumChipRxEvent ucEvent));
extern void Mod_SystemTx_PowerOnInit(void);
extern void Mod_SystemRx_PowerOnInit(void);
extern void Mod_SystemRx_SetState(u8 ucState);
extern void Mod_SystemTx_SetState(u8 ucState);
extern int LT9211C_write(u8 reg, u8 val);
extern int LT9211C_read(u8 reg);

extern void LT9211C_resume(void);
extern void LT9211C_suspend(void);

#endif


