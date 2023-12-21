#include "include.h"

StructPcrPara g_stPcrPara;
SrtuctMipiRx_VidTiming_Get g_stMipiRxVidTiming_Get;
#define MIPIRX_FORMAT_CNT   0x0f
char* g_szStrRxFormat[MIPIRX_FORMAT_CNT] = 
{
    "",
    "DSI YUV422 10bit",
    "DSI YUV422 12bit",
    "YUV422 8bit",
    "RGB 10bit",
    "RGB 12Bit",
    "YUV420 8bit",
    "RGB 565",
    "RGB 666",
    "DSI RGB 6L",
    "RGB 8Bit",
    "RAW8",
    "RAW10",
    "RAW12",
    "CSI YUV422 10",
};

void DRV_DesscPll_SdmCal(void)
{
    LT9211C_write(0xff,0xd0);//      
    LT9211C_write(0x08,0x00);//sel mipi rx sdm  

    LT9211C_write(0x26,0x80 | ((u8)g_stPcrPara.Pcr_M)); //m
    LT9211C_write(0x2d,g_stPcrPara.Pcr_UpLimit); //PCR M overflow limit setting.
    LT9211C_write(0x31,g_stPcrPara.Pcr_DownLimit); //PCR M underflow limit setting. 
    
    LT9211C_write(0x27,(u8)(g_stPcrPara.Pcr_K >> 16)); //k
    LT9211C_write(0x28,(u8)(g_stPcrPara.Pcr_K >> 8)); //k
    LT9211C_write(0x29,(u8)(g_stPcrPara.Pcr_K)); //k  
    LT9211C_write(0x26,(LT9211C_read(0x26) & 0x7f));
   
}

void Drv_MipiRx_DesscPll_Set(void)
{
    u8 ucdesscpll_pixck_div = 0;

    LT9211C_write(0xff,0x82);
    LT9211C_write(0x26,0x20); //[7:6]desscpll reference select Xtal clock as reference
                                   //[4]1'b0: dessc-pll power down
    LT9211C_write(0x27,0x40); //prediv = 0;

    pr_info("Mipi Rx PixClk: %d",g_stRxVidTiming.ulPclk_Khz);
    if (g_stRxVidTiming.ulPclk_Khz >= 352000)
    {
        LT9211C_write(0x2f,0x04);
        ucdesscpll_pixck_div = 2;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 176000 && g_stRxVidTiming.ulPclk_Khz < 352000)
    {
        LT9211C_write(0x2f,0x04);
        ucdesscpll_pixck_div = 2;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 88000 && g_stRxVidTiming.ulPclk_Khz < 176000)
    {
        LT9211C_write(0x2f,0x05);
        ucdesscpll_pixck_div = 4;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 44000 && g_stRxVidTiming.ulPclk_Khz < 88000)
    {
        LT9211C_write(0x2f,0x06);
        ucdesscpll_pixck_div = 8;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 22000 && g_stRxVidTiming.ulPclk_Khz < 44000)
    {
        LT9211C_write(0x2f,0x07);
        ucdesscpll_pixck_div = 16;
    }
    else 
    {
        LT9211C_write(0x2f,0x07);
        ucdesscpll_pixck_div = 16;
    }

    g_stPcrPara.Pcr_M = (g_stRxVidTiming.ulPclk_Khz * ucdesscpll_pixck_div) / 25;
    g_stPcrPara.Pcr_K = g_stPcrPara.Pcr_M % 1000;
    g_stPcrPara.Pcr_M = g_stPcrPara.Pcr_M / 1000;
    
    g_stPcrPara.Pcr_UpLimit   = g_stPcrPara.Pcr_M + 1;
    g_stPcrPara.Pcr_DownLimit = g_stPcrPara.Pcr_M - 1;

    g_stPcrPara.Pcr_K <<= 14;

    DRV_DesscPll_SdmCal();
    LT9211C_write(0xff,0x81);     
    LT9211C_write(0x03,0xfe); //desscpll rst
    msleep(1);
    LT9211C_write(0x03,0xff); //desscpll rst   
}

u8 Drv_MipiRx_PcrCali(void)
{    
    u8 ucRtn = LONTIUM_TRUE;
    u8 ucPcr_Cal_Cnt;

    LT9211C_write(0xff,0xd0);
    #if MIPIRX_INPUT_SEL == MIPI_DSI 
    LT9211C_write(0x0a,(LT9211C_read(0x0a) & 0x0f));
    LT9211C_write(0x1e,0x51); //0x51[7:4]pcr diff first step,[3:0]pcr diff second step
    LT9211C_write(0x23,0x80); //0x05 MIPIRX PCR capital data set for PCR second step
    LT9211C_write(0x24,0x70);
    LT9211C_write(0x25,0x80);
    LT9211C_write(0x2a,0x10); 
    LT9211C_write(0x21,0x4f);
    LT9211C_write(0x22,0xf0);
    
    LT9211C_write(0x38,0x04); //MIPIRX PCR de mode delay offset0
    LT9211C_write(0x39,0x08);
    LT9211C_write(0x3a,0x10);
    LT9211C_write(0x3b,0x20); //MIPIRX PCR de mode delay offset2

    LT9211C_write(0x3f,0x04); //PCR de mode step0 setting
    LT9211C_write(0x40,0x08);
    LT9211C_write(0x41,0x10);
    LT9211C_write(0x42,0x20);
    LT9211C_write(0x2b,0xA0);
//    LT9211C_write(0x4d,0x00); //0x00
    #elif MIPIRX_INPUT_SEL == MIPI_CSI
    LT9211C_write(0x0a,(LT9211C_read(0x0a) & 0x1f));
    LT9211C_write(0x1e,0x51); //0x51[7:4]pcr diff first step,[3:0]pcr diff second step
    LT9211C_write(0x23,0x10); //0x05 MIPIRX PCR capital data set for PCR second step
    LT9211C_write(0x24,0x70);
    LT9211C_write(0x25,0x80);
    LT9211C_write(0x2a,0x10); 
    LT9211C_write(0x21,0xc6); //[7]1'b1:CSI sel de sync mode;
    LT9211C_write(0x22,0x00);
    
    LT9211C_write(0x38,0x04); //MIPIRX PCR de mode delay offset0
    LT9211C_write(0x39,0x08);
    LT9211C_write(0x3a,0x10);
    LT9211C_write(0x3b,0x20); //MIPIRX PCR de mode delay offset2

    LT9211C_write(0x3f,0x04); //PCR de mode step0 setting
    LT9211C_write(0x40,0x08);
    LT9211C_write(0x41,0x10);
    LT9211C_write(0x42,0x20);
    LT9211C_write(0x2b,0xA0);
    #endif
    
    if (g_stRxVidTiming.ulPclk_Khz < 44000)
    {
        #if MIPIRX_CLK_BURST == ENABLED
//        LT9211C_write(0x0a,0x42);
        LT9211C_write(0x0c,0x40); //[7:0]rgd_vsync_dly(sram rd delay)
        LT9211C_write(0x1b,0x00); //pcr wr dly[15:0]
        LT9211C_write(0x1c,0x40); //pcr wr dly[7:0]
        #else
        LT9211C_write(0x0c,0x60); //[7:0]rgd_vsync_dly(sram rd delay)
        LT9211C_write(0x1b,0x00); //pcr wr dly[15:0]
        LT9211C_write(0x1c,0x60); //pcr wr dly[7:0]
        #endif
    }
    else 
    {
        LT9211C_write(0x0c,0x40); //[7:0]rgd_vsync_dly(sram rd delay)
        LT9211C_write(0x1b,0x00); //pcr wr dly[15:0]
        LT9211C_write(0x1c,0x40); //pcr wr dly[7:0]
    }
    
    LT9211C_write(0xff,0x81);     
    LT9211C_write(0x09,0xdb);
    LT9211C_write(0x09,0xdf); //pcr rst

    LT9211C_write(0xff,0xd0);
    LT9211C_write(0x08,0x80);
    LT9211C_write(0x08,0x00);
    msleep(10);
    ucPcr_Cal_Cnt = 0;
    
    do
    {
        msleep(100);
        ucPcr_Cal_Cnt++;
        pr_debug("PCR unstable M = 0x%02x",(LT9211C_read(0x94)&0x7F));
    }while((ucPcr_Cal_Cnt < 200) && ((LT9211C_read(0x87) & 0x18) != 0x18));

    if((LT9211C_read(0x87) & 0x18) != 0x18)
    {
        pr_info("LT9211C pcr unstable");
        ucRtn = FAIL;
    }
    
    return ucRtn;
}

u8 Drv_MipiRx_VidFmtUpdate(void)
{
    u8 ucRxFmt;
    
    ucRxFmt = g_stMipiRxVidTiming_Get.ucFmt;
    LT9211C_write(0xff,0xd0);
    g_stMipiRxVidTiming_Get.ucFmt = (LT9211C_read(0x84) & 0x0f);
    
    if (ucRxFmt != g_stMipiRxVidTiming_Get.ucFmt)
    {
        return LONTIUM_TRUE;
    }
    else
    {
        return LONTIUM_FALSE;
    }
}


void Drv_MipiRx_HsSettleSet(void)
{
    if((g_stMipiRxVidTiming_Get.ucLane0SetNum > 0x10) && (g_stMipiRxVidTiming_Get.ucLane0SetNum < 0x50))
    {
        pr_debug( "Set Mipi Rx Settle: 0x%02x", (g_stMipiRxVidTiming_Get.ucLane0SetNum - 5));
        LT9211C_write(0xff,0xd0);
        LT9211C_write(0x02,(g_stMipiRxVidTiming_Get.ucLane0SetNum - 5));
    }
    else
    {
        pr_debug( "Set Mipi Rx Settle: 0x0e"); //mipi rx cts test need settle 0x0e
        LT9211C_write(0xff,0xd0);
        LT9211C_write(0x02,0x08);
    }
}

void Drv_MipiRx_SotGet(void)
{
    LT9211C_write(0xff,0xd0);
    g_stMipiRxVidTiming_Get.ucLane0SetNum  = LT9211C_read(0x88);
    g_stMipiRxVidTiming_Get.ucLane0SotData = LT9211C_read(0x89);
    g_stMipiRxVidTiming_Get.ucLane1SetNum  = LT9211C_read(0x8a);
    g_stMipiRxVidTiming_Get.ucLane1SotData = LT9211C_read(0x8b);
    g_stMipiRxVidTiming_Get.ucLane2SetNum  = LT9211C_read(0x8c);
    g_stMipiRxVidTiming_Get.ucLane2SotData = LT9211C_read(0x8d);
    g_stMipiRxVidTiming_Get.ucLane3SetNum  = LT9211C_read(0x8e);
    g_stMipiRxVidTiming_Get.ucLane3SotData = LT9211C_read(0x8f);

    pr_debug("Sot Num = 0x%02x, 0x%02x, 0x%02x, 0x%02x", g_stMipiRxVidTiming_Get.ucLane0SetNum,g_stMipiRxVidTiming_Get.ucLane1SetNum,
                                                                    g_stMipiRxVidTiming_Get.ucLane2SetNum,g_stMipiRxVidTiming_Get.ucLane3SetNum);
    pr_debug("Sot Dta = 0x%02x, 0x%02x, 0x%02x, 0x%02x", g_stMipiRxVidTiming_Get.ucLane0SotData,g_stMipiRxVidTiming_Get.ucLane1SotData,
                                                                    g_stMipiRxVidTiming_Get.ucLane2SotData,g_stMipiRxVidTiming_Get.ucLane3SotData); 
}

void Drv_MipiRx_HactGet(void)
{
    LT9211C_write(0xff,0xd0);
    g_stMipiRxVidTiming_Get.usVact = (LT9211C_read(0x85) << 8) +LT9211C_read(0x86);
    g_stMipiRxVidTiming_Get.ucFmt  = (LT9211C_read(0x84) & 0x0f);
    g_stMipiRxVidTiming_Get.ucPa_Lpn = LT9211C_read(0x9c);
    g_stMipiRxVidTiming_Get.uswc = (LT9211C_read(0x82) << 8) + LT9211C_read(0x83); //

    switch (g_stMipiRxVidTiming_Get.ucFmt)
    {
        case 0x01: //DSI-YUV422-10bpc
        case 0x0e: //CSI-YUV422-10bpc
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*10) / 25)/10; //wc = hact * 20bpp/8
        break;
        case 0x02: //DSI-YUV422-12bpc
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 3; //wc = hact * 24bpp/8
        break;
        case 0x03: //YUV422-8bpc
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 2; //wc = hact * 16bpp/8
        break; 
        case 0x04: //RGB10bpc
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*100) / 375)/100; //wc = hact * 30bpp/8
        break;
        case 0x05: //RGB12bpc
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*10) / 45)/10; //wc = hact * 36bpp/8
        break;
        case 0x06: //YUV420-8bpc
        case 0x0a: //RGB8bpc
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 3; //wc = hact * 24bpp/8
        break;
        case 0x07: //RGB565
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 2; //wc = hact * 16bpp/8
        break;
        case 0x08: //RGB6bpc
        case 0x09: //RGB6bpc_losely
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*100) / 225)/100; //wc = hact * 18bpp/8
        break;
        case 0x0b: //RAW8
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 1; //wc = hact * 8bpp/8
        break;
        case 0x0c: //RAW10
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*100) / 125)/10; //wc = hact * 10bpp/8
        break;
        case 0x0d: //RAW12
            g_stMipiRxVidTiming_Get.usHact = ((g_stMipiRxVidTiming_Get.uswc*10) / 15)/10; //wc = hact * 12bpp/8
        break;
        default: 
            g_stMipiRxVidTiming_Get.usHact = g_stMipiRxVidTiming_Get.uswc / 3; //wc = hact * 24bpp/8
        break;
    }
    
}

u8 Drv_MipiRx_VidTiming_Get(void)
{
    Drv_MipiRx_SotGet();
    Drv_MipiRx_HsSettleSet();
    Drv_MipiRx_HactGet();
    if((g_stMipiRxVidTiming_Get.usHact < 400) || (g_stMipiRxVidTiming_Get.usVact < 400))
    {
        pr_err("RX No Video Get");
        return FAIL;
    }else{
        pr_info("hact = %d",g_stMipiRxVidTiming_Get.usHact);
        pr_info("vact = %d",g_stMipiRxVidTiming_Get.usVact);    
        pr_info("fmt = 0x%02x", g_stMipiRxVidTiming_Get.ucFmt);
        pr_info("pa_lpn = 0x%02x", g_stMipiRxVidTiming_Get.ucPa_Lpn);
        return SUCCESS;
    }
    return SUCCESS;
}
void Drv_MipiRx_VidTiming_Set(void)
{
    LT9211C_write(0xff,0xd0);
    LT9211C_write(0x0d,(u8)(g_stRxVidTiming.usVtotal >> 8));     //vtotal[15:8]
    LT9211C_write(0x0e,(u8)(g_stRxVidTiming.usVtotal));          //vtotal[7:0]
    LT9211C_write(0x0f,(u8)(g_stRxVidTiming.usVact >> 8));       //vactive[15:8]
    LT9211C_write(0x10,(u8)(g_stRxVidTiming.usVact));            //vactive[7:0]
    LT9211C_write(0x15,(u8)(g_stRxVidTiming.usVs));              //vs[7:0]
    LT9211C_write(0x17,(u8)(g_stRxVidTiming.usVfp >> 8));        //vfp[15:8]
    LT9211C_write(0x18,(u8)(g_stRxVidTiming.usVfp));             //vfp[7:0]    

    LT9211C_write(0x11,(u8)(g_stRxVidTiming.usHtotal >> 8));     //htotal[15:8]
    LT9211C_write(0x12,(u8)(g_stRxVidTiming.usHtotal));          //htotal[7:0]
    LT9211C_write(0x13,(u8)(g_stRxVidTiming.usHact >> 8));       //hactive[15:8]
    LT9211C_write(0x14,(u8)(g_stRxVidTiming.usHact));            //hactive[7:0]
    LT9211C_write(0x4c,(u8)(g_stRxVidTiming.usHs >> 8));         //hs[15:8]
    LT9211C_write(0x16,(u8)(g_stRxVidTiming.usHs));              //hs[7:0]
    LT9211C_write(0x19,(u8)(g_stRxVidTiming.usHfp >> 8));        //hfp[15:8]
    LT9211C_write(0x1a,(u8)(g_stRxVidTiming.usHfp));             //hfp[7:0]
}

u8 Drv_MipiRx_VidTiming_Sel(StructChipRxVidTiming *ptn_timing)
{   
    u8 rtn = LONTIUM_FALSE;
    if ((g_stMipiRxVidTiming_Get.usHact == ptn_timing->usHact ) && ( g_stMipiRxVidTiming_Get.usVact == ptn_timing->usVact )){
        g_stMipiRxVidTiming_Get.ucFrameRate = Drv_VidChk_FrmRt_Get();
        pr_info("FrameRate %d", g_stMipiRxVidTiming_Get.ucFrameRate);
        if ((g_stMipiRxVidTiming_Get.ucFrameRate > (ptn_timing->ucFrameRate - 3)) && 
               (g_stMipiRxVidTiming_Get.ucFrameRate < (ptn_timing->ucFrameRate + 3))){
                    g_stRxVidTiming.usVtotal = ptn_timing->usVtotal;
                    g_stRxVidTiming.usVact   = ptn_timing->usVact;
                    g_stRxVidTiming.usVs     = ptn_timing->usVs;
                    g_stRxVidTiming.usVfp    = ptn_timing->usVfp;
                    g_stRxVidTiming.usVbp    = ptn_timing->usVbp;

                    g_stRxVidTiming.usHtotal = ptn_timing->usHtotal;
                    g_stRxVidTiming.usHact   = ptn_timing->usHact;
                    g_stRxVidTiming.usHs     = ptn_timing->usHs;
                    g_stRxVidTiming.usHfp    = ptn_timing->usHfp;
                    g_stRxVidTiming.usHbp    = ptn_timing->usHbp;

                    g_stRxVidTiming.ulPclk_Khz = (u32)((u32)(ptn_timing->usHtotal) * (ptn_timing->usVtotal) * (ptn_timing->ucFrameRate) / 1000);
                    Drv_MipiRx_VidTiming_Set();
                    pr_info("Video Timing Set %dx%d_%d",g_stRxVidTiming.usHact,g_stRxVidTiming.usVact,g_stMipiRxVidTiming_Get.ucFrameRate);
                    rtn = LONTIUM_TRUE;
               }
 
        
        
    }
    return rtn;
    
}



u8 Drv_MipiRx_VidFmt_Get(u8 VidFmt)
{
    u8 ucRxVidFmt;
    
    switch (VidFmt)
    {
        case 0x01: //DSI-YUV422-10bpc
            ucRxVidFmt = YUV422_10bit;
        break;
        case 0x02: //DSI-YUV422-12bpc
            ucRxVidFmt = YUV422_12bit;
        break;
        case 0x03: //YUV422-8bpc
            ucRxVidFmt = YUV422_8bit;
        break;
        case 0x04: //RGB30bpp
            ucRxVidFmt = RGB_10Bit;
        break;
        case 0x05: //RGB36bpp
            ucRxVidFmt = RGB_12Bit;
        break;
        case 0x06: //YUV420-8bpc
            ucRxVidFmt = YUV420_8bit;
        break;
        case 0x07: //RGB565
        break;
        case 0x08: //RGB666
            ucRxVidFmt = RGB_6Bit;
        break;
        case 0x09: //DSI-RGB6L
        break;
        case 0x0a: //RGB888
            ucRxVidFmt = RGB_8Bit;
        break;
        case 0x0b: //RAW8
        break;
        case 0x0c: //RAW10
        break;
        case 0x0d: //RAW12
        break;
        case 0x0e: //CSI-YUV422-10
            ucRxVidFmt = YUV422_10bit;
        break;
        default:
            ucRxVidFmt = RGB_8Bit;
        break;    
    }

    pr_info("MipiRx Input Format: %s",g_szStrRxFormat[VidFmt]);
    return ucRxVidFmt;
}

void Drv_MipiRx_InputSel(void)
{
    LT9211C_write(0xff,0xd0);
    #if (MIPIRX_INPUT_SEL == MIPI_CSI)
    LT9211C_write(0x04,0x10); //[4]1: CSI enable
    LT9211C_write(0x21,0xc6); //[7](dsi: hsync_level(for pcr adj) = hsync_level; csi:hsync_level(for pcr adj) = de_level)
    pr_info("Mipi CSI Input");
    #else 
    LT9211C_write(0x04,0x00); //[4]0: DSI enable
    LT9211C_write(0x21,0x46); //[7](dsi: hsync_level(for pcr adj) = hsync_level; csi:hsync_level(for pcr adj) = de_level)
    pr_info("Mipi DSI Input");
    #endif
}

void Drv_MipiRx_LaneSet(void)
{
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x3f,0x08); //MLRX HS/LP control conmand enable
    LT9211C_write(0x40,0x04); //[2:0]pa_ch0_src_sel ch4 data
    LT9211C_write(0x41,0x03); //[2:0]pa_ch1_src_sel ch3 data
    LT9211C_write(0x42,0x02); //[2:0]pa_ch2_src_sel ch2 data
    LT9211C_write(0x43,0x01); //[2:0]pa_ch3_src_sel ch1 data

    LT9211C_write(0x45,0x04); //[2:0]pb_ch0_src_sel ch9 data
    LT9211C_write(0x46,0x03); //[2:0]pb_ch1_src_sel ch8 data
    LT9211C_write(0x47,0x02); //[2:0]pb_ch2_src_sel ch7 data
    LT9211C_write(0x48,0x01); //[2:0]pb_ch3_src_sel ch6 data
    
    #if MIPIRX_PORT_SWAP == ENABLED
    LT9211C_write(0x44,0x40); //[6]mlrx port A output select port B;[2:0]pa_ch4_src_sel ch0 data
    LT9211C_write(0x49,0x00); //[6]mlrx port B output select port B;[2:0]pb_ch4_src_sel ch5 data
    #else
    LT9211C_write(0x44,0x00); //[6]mlrx port A output select port A;[2:0]pa_ch4_src_sel ch0 data
    LT9211C_write(0x49,0x00); //[6]mlrx port B output select port A;[2:0]pb_ch4_src_sel ch5 data
    #endif

    #if MIPIRX_PORT_SEL == PORTB
    LT9211C_write(0x44,0x40); //[6]mlrx port A output select port B;[2:0]pa_ch4_src_sel ch0 data
    LT9211C_write(0x49,0x00); //[6]mlrx port B output select port B;[2:0]pb_ch4_src_sel ch5 data
    #endif

}

void Drv_MipiRxClk_Sel(void)
{
    /* CLK sel */
    LT9211C_write(0xff,0x85);
    LT9211C_write(0xe9,0x88); //sys clk sel from XTAL
    
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x80,0x51); //[7:6]rx sram rd clk src sel ad dessc pcr clk
                                   //[5:4]rx sram wr clk src sel mlrx bytr clk
                                   //[1:0]video check clk sel from desscpll pix clk
    #if MIPIRX_PORT_SEL == PORTA
    LT9211C_write(0x81,0x10); //[5]0: mlrx byte clock select from ad_mlrxa_byte_clk
                                   //[4]1: rx output pixel clock select from ad_desscpll_pix_clk
    #elif MIPIRX_PORT_SEL == PORTB
    LT9211C_write(0x81,0x30); //[5]1: mlrx byte clock select from ad_mlrxb_byte_clk
                                   //[4]1: rx output pixel clock select from ad_desscpll_pix_clk
    #endif
    LT9211C_write(0xff,0x86);
    LT9211C_write(0x32,0x03); //video check frame cnt set: 3 frame
}

void Drv_MipiRx_PhyPowerOn(void)
{
    LT9211C_write(0xff,0xd0);
    LT9211C_write(0x00,(LT9211C_read(0x00) | MIPIRX_LANE_NUM));    // 0: 4 Lane / 1: 1 Lane / 2 : 2 Lane / 3: 3 Lane

    LT9211C_write(0xff,0x82);
    LT9211C_write(0x01,0x11); //MIPI RX portA & B disable

    #if MIPIRX_PORT_SEL == PORTA
    LT9211C_write(0x01,0x91); //MIPI RX portA enable
    LT9211C_write(0x02,0x00); //[5][1]:0 mipi mode, no swap
    LT9211C_write(0x03,0xcc); //port A & B eq current reference 
    LT9211C_write(0x09,0x21); //[3]0: select link clk from port-A, [1]0: mlrx_clk2pll disable
    LT9211C_write(0x13,0x0c); //MIPI port A clk lane rterm & high speed en
    
    #if MIPIRX_CLK_BURST == ENABLED
    LT9211C_write(0x13,0x00); //MIPI port A clk lane rterm & high speed en
    #endif
    pr_info("MIPI Input PortA");
    #elif MIPIRX_PORT_SEL == PORTB
    LT9211C_write(0x01,0x19); //MIPI RX portB enable
    LT9211C_write(0x02,0x00); //[5][1]:0 mipi mode, no swap
    LT9211C_write(0x03,0xcc); //port A & B eq current reference 
    LT9211C_write(0x09,0x29); //[3]1: select link clk from port-B, [1]0: mlrx_clk2pll disable
    LT9211C_write(0x14,0x03); //Port-B clk lane software enable
    
    #if MIPIRX_CLK_BURST == ENABLED
    LT9211C_write(0x14,0x00); //MIPI port A clk lane rterm & high speed en
    #endif
    pr_info("MIPI Input PortB");
    #else
    LT9211C_write(0x01,0x99); //MIPI RX portB enable
    LT9211C_write(0x02,0x00); //[5][1]:0 mipi mode, no swap
    LT9211C_write(0x03,0xcc); //port A & B eq current reference 
    LT9211C_write(0x09,0x29); //[3]1: select link clk from port-B, [1]0: mlrx_clk2pll disable
    LT9211C_write(0x13,0x0c); //MIPI port A clk lane rterm & high speed en
    LT9211C_write(0x14,0x03); //Port-B clk lane software enable
    
    #if MIPIRX_CLK_BURST == ENABLED
    LT9211C_write(0x13,0x00); //MIPI port A clk lane rterm use hardware mode
    LT9211C_write(0x14,0x00); //Port-B clk lane use hardware mode
    #endif
    #endif

    LT9211C_write(0xff,0xd0);
    LT9211C_write(0x01,0x00); //mipi rx data lane term enable time: 39ns;
    LT9211C_write(0x02,0x0e); //mipi rx hs settle time defult set: 0x05;
    LT9211C_write(0x05,0x00); //mipi rx lk lane term enable time: 39ns;
    LT9211C_write(0x0a,0x52);
    LT9211C_write(0x0b,0x30);
    
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x09,0xde); //mipi rx dphy reset
    LT9211C_write(0x09,0xdf); //mipi rx dphy release
}

void Drv_LvdsTxSw_Rst(void)
{
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x08,0x6f); //LVDS TX SW reset
    msleep(2);
    LT9211C_write(0x08,0x7f);
    pr_info("LVDS Tx Video Out");
}

void Drv_LVDSTxPhy_PowerOff(void)
{
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,0x00); //lvds enable
    LT9211C_write(0x37,0x00);
}

void Drv_LvdsTxPhy_Poweron(LvdsOutParameter *lvds_parameter)
{

    if(strstr(lvds_parameter->lvdstx_port_sel, "porta")){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x01); //lvds enable
        LT9211C_write(0x37,0x40);
        pr_info("LVDS Output PortA");
        #if LVDSTX_LANENUM == FIVE_LANE
        LT9211C_write(0x36,0x03);
        #endif 
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "porta")){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x02); //lvds enable
        LT9211C_write(0x37,0x04);
        pr_info("LVDS Output PortB");
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "dou_port")){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x03); //lvds enable
        LT9211C_write(0x37,0x44); //port rterm enable
        pr_info("LVDS Output PortA&B");
    }
    LT9211C_write(0x38,0x14);
    LT9211C_write(0x39,0x31);
    LT9211C_write(0x3a,0xc8);
    LT9211C_write(0x3b,0x00);
    LT9211C_write(0x3c,0x0f);

    LT9211C_write(0x46,0x40);
    LT9211C_write(0x47,0x40);
    LT9211C_write(0x48,0x40);
    LT9211C_write(0x49,0x40);
    LT9211C_write(0x4a,0x40);
    LT9211C_write(0x4b,0x40);
    LT9211C_write(0x4c,0x40);
    LT9211C_write(0x4d,0x40);
    LT9211C_write(0x4e,0x40);
    LT9211C_write(0x4f,0x40);
    LT9211C_write(0x50,0x40);
    LT9211C_write(0x51,0x40);

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x03,0xbf); //mltx reset
    LT9211C_write(0x03,0xff); //mltx release
}

void Drv_LvdsTxPll_RefPixClk_Set(void)
{
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x30,0x00); //[7]0:txpll normal work; txpll ref clk sel pix clk
}

void Drv_LvdsTxPll_Config(LvdsOutParameter *lvds_parameter)
{
    u8 ucPreDiv = 0;
    u8 ucSericlkDiv = 0;
    u8 ucDivSet = 0;
    u8 ucPixClkDiv = 0;
    u32 ulLvdsTXPhyClk = 0;

    /* txphyclk = vco clk * ucSericlkDiv */
    if(strstr(lvds_parameter->lvdstx_port_sel, "dou_port")){
        ulLvdsTXPhyClk = (u32)(g_stRxVidTiming.ulPclk_Khz * 7 / 2); //2 port: byte clk = pix clk / 2;
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT0_1)); //htotal -> 2n
        
        #if ((LVDSTX_COLORSPACE == YUV422) && (LVDSTX_COLORDEPTH == DEPTH_8BIT) && (LVDSTX_LANENUM == FIVE_LANE))
        ulLvdsTXPhyClk = (u32)(g_stRxVidTiming.ulPclk_Khz * 7 / 4); //2 port YUV422 8bit 5lane: byte clk = pix clk / 4;
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT1_1)); //htotal -> 4n
        #endif
    }else{
        ulLvdsTXPhyClk = (u32)(g_stRxVidTiming.ulPclk_Khz * 7); //1 port: byte clk = pix clk;
        
        #if ((LVDSTX_COLORSPACE == YUV422) && (LVDSTX_COLORDEPTH == DEPTH_8BIT) && (LVDSTX_LANENUM == FIVE_LANE))
        ulLvdsTXPhyClk = (u32)(g_stRxVidTiming.ulPclk_Khz * 7 / 2); //1 port YUV422 8bit 5lane: byte clk = pix clk / 2;
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT0_1)); //htotal -> 2n
        #endif
    }
    
    /*txpll prediv sel*/
    LT9211C_write(0xff,0x82);
    if (g_stRxVidTiming.ulPclk_Khz < 20000)
    {
        LT9211C_write(0x31,0x28); //[2:0]3'b000: pre div set div1
        ucPreDiv = 1;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 20000 && g_stRxVidTiming.ulPclk_Khz < 40000)
    {
        LT9211C_write(0x31,0x28); //[2:0]3'b000: pre div set div1
        ucPreDiv = 1;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 40000 && g_stRxVidTiming.ulPclk_Khz < 80000)
    {
        LT9211C_write(0x31,0x29); //[2:0]3'b001: pre div set div2
        ucPreDiv = 2;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 80000 && g_stRxVidTiming.ulPclk_Khz < 160000)
    {
        LT9211C_write(0x31,0x2a); //[2:0]3'b010: pre div set div4
        ucPreDiv = 4;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 160000 && g_stRxVidTiming.ulPclk_Khz < 320000)
    {
        LT9211C_write(0x31,0x2b); //[2:0]3'b011: pre div set div8
        ucPreDiv = 8;
//        ulLvdsTXPhyClk = ulDessc_Pix_Clk * 3.5;
    }
    else if (g_stRxVidTiming.ulPclk_Khz >= 320000)
    {
        LT9211C_write(0x31,0x2f); //[2:0]3'b111: pre div set div16
        ucPreDiv = 16;
//        ulLvdsTXPhyClk = ulDessc_Pix_Clk * 3.5;
    }

    /*txpll serick_divsel*/
    LT9211C_write(0xff,0x82);
    if (ulLvdsTXPhyClk >= 640000 )//640M~1.28G
    {
        LT9211C_write(0x32,0x42);
        ucSericlkDiv = 1; //sericlk div1 [6:4]:0x40
    }
    else if (ulLvdsTXPhyClk >= 320000 && ulLvdsTXPhyClk < 640000)
    {
        LT9211C_write(0x32,0x02);
        ucSericlkDiv = 2; //sericlk div2 [6:4]:0x00
    }
    else if (ulLvdsTXPhyClk >= 160000 && ulLvdsTXPhyClk < 320000)
    {
        LT9211C_write(0x32,0x12);
        ucSericlkDiv = 4; //sericlk div4 [6:4]:0x10
    }
    else if (ulLvdsTXPhyClk >= 80000 && ulLvdsTXPhyClk < 160000)
    {
        LT9211C_write(0x32,0x22);
        ucSericlkDiv = 8; //sericlk div8 [6:4]:0x20
    }
    else //40M~80M
    {
        LT9211C_write(0x32,0x32);
        ucSericlkDiv = 16; //sericlk div16 [6:4]:0x30
    }

    /* txpll_pix_mux_sel & txpll_pixdiv_sel*/
    LT9211C_write(0xff,0x82);
    if (g_stRxVidTiming.ulPclk_Khz > 150000)
    {
        LT9211C_write(0x33,0x04); //pixclk > 150000, pixclk mux sel (vco clk / 3.5)
        ucPixClkDiv = 3.5;
    }
    else
    {
        ucPixClkDiv = (u8)((ulLvdsTXPhyClk * ucSericlkDiv) / (g_stRxVidTiming.ulPclk_Khz * 7));

        if (ucPixClkDiv <= 1)
        {
            LT9211C_write(0x33,0x00); //pixclk div sel /7
        }
        else if ((ucPixClkDiv > 1) && (ucPixClkDiv <= 2))
        {
            LT9211C_write(0x33,0x01); //pixclk div sel /14
        }
        else if ((ucPixClkDiv > 2) && (ucPixClkDiv <= 4))
        {
            LT9211C_write(0x33,0x02); //pixclk div sel /28
        }
        else if ((ucPixClkDiv > 4) && (ucPixClkDiv <= 8))
        {
            LT9211C_write(0x33,0x03); //pixclk div sel /56
        }
        else
        {
            LT9211C_write(0x33,0x03); //pixclk div sel /56
        }
    }
    
    ucDivSet = (u8)((ulLvdsTXPhyClk * ucSericlkDiv) / (g_stRxVidTiming.ulPclk_Khz / ucPreDiv));
    
    LT9211C_write(0x34,0x01); //txpll div set software output enable
    LT9211C_write(0x35,ucDivSet);
    pr_debug("ulPclk_Khz: %d, ucPreDiv: %d, ucSericlkDiv: %d, ucPixClkDiv: %d, ucDivSet: %d",
                    g_stRxVidTiming.ulPclk_Khz, ucPreDiv, ucSericlkDiv, ucPixClkDiv, ucDivSet);
    
    #if LVDS_SSC_SEL != NO_SSC

    LT9211C_write(0xff,0x82);
    LT9211C_write(0x34,0x00); //hardware mode
    
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x6e,0x10); //sram select
    
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x81,0x15); //clk select
    
    LT9211C_write(0xff,0x87);

    LT9211C_write(0x1e,0x00); //modulation disable
    
    LT9211C_write(0x17,0x02); //2 order

    
    LT9211C_write(0x18,0x04);
    LT9211C_write(0x19,0xd4); //ssc_prd

    #if LVDS_SSC_SEL == SSC_1920x1080_30k5
        LT9211C_write(0x1a,0x00);
        LT9211C_write(0x1b,0x12); //delta1
        LT9211C_write(0x1c,0x00);
        LT9211C_write(0x1d,0x24); //delta
        
        LT9211C_write(0x1f,0x1c); //M
        LT9211C_write(0x20,0x00);
        LT9211C_write(0x21,0x00);
        pr_info("LVDS Output SSC 1920x1080 30k5%");
    #elif LVDS_SSC_SEL == SSC_3840x2160_30k5
        LT9211C_write(0x1a,0x00);
        LT9211C_write(0x1b,0x12); //delta1
        LT9211C_write(0x1c,0x00);
        LT9211C_write(0x1d,0x24); //delta
        
        LT9211C_write(0x1f,0x1c); //M
        LT9211C_write(0x20,0x00);
        LT9211C_write(0x21,0x00);
        pr_info("LVDS Output SSC 3840x2160 30k5%");
    #endif

    LT9211C_write(0x1e,0x02); //modulation enable
    
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x0c,0xfe); //txpll reset
    LT9211C_write(0x0c,0xff); //txpll release
    
    #endif
}

u8 Drv_LvdsTxPll_Cali(void)
{
    u8 ucloopx;
    u8 ucRtn = FAIL;

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x0c,0xfe); //txpll reset
    msleep(1);
    LT9211C_write(0x0c,0xff); //txpll release

    do
    {
        LT9211C_write(0xff,0x87);
        LT9211C_write(0x0f,0x00);
        LT9211C_write(0x0f,0x01);
        msleep(20);

        ucloopx++;
    }while((ucloopx < 3) && ((LT9211C_read(0x39) & 0x01) != 0x01));

    if(LT9211C_read(0x39) & 0x04)
    {
        ucRtn = SUCCESS;
        pr_info("Tx Pll Lock");
    }
    else
    {
        pr_err("Tx Pll Unlocked");
    }

    return ucRtn;
}

void Drv_LvdsTx_VidTiming_Set(void)
{
    u16 vss,eav,sav;
    msleep(100);
    LT9211C_write(0xff,0x85); 
    
    vss = g_stVidChk.usVs + g_stVidChk.usVbp;
    eav = g_stVidChk.usHs + g_stVidChk.usHbp + g_stVidChk.usHact + 4;
    sav = g_stVidChk.usHs + g_stVidChk.usHbp;
    
    
    LT9211C_write(0x5f,0x00);    
    LT9211C_write(0x60,0x00);  
    LT9211C_write(0x62,(u8)(g_stVidChk.usVact>>8));         //vact[15:8]
    LT9211C_write(0x61,(u8)(g_stVidChk.usVact));            //vact[7:0]
    LT9211C_write(0x63,(u8)(vss));                           //vss[7:0]
    LT9211C_write(0x65,(u8)(eav>>8));                        //eav[15:8]
    LT9211C_write(0x64,(u8)(eav));                           //eav[7:0]
    LT9211C_write(0x67,(u8)(sav>>8));                        //sav[15:8]
    LT9211C_write(0x66,(u8)(sav));                           //sav[7:0] 
    
}


void Drv_LvdsTxPort_Set(LvdsOutParameter *lvds_parameter)
{
    LT9211C_write(0xff,0x85);
    if(strstr(lvds_parameter->lvdstx_port_sel, "porta") || strstr(lvds_parameter->lvdstx_port_sel, "portb"))
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x80)); //[7]lvds function enable //[4]0:output 1port; [4]1:output 2port;
    //only portb output must use port copy from porta, so lvds digtial output port sel 2ports.
    else if(strstr(lvds_parameter->lvdstx_port_sel, "dou_port"))
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x90)); //[7]lvds function enable //[4]0:output 1port; [4]1:output 2port;
}

void Drv_LvdsTxVidFmt_Set(void)
{
    LT9211C_write(0xff,0x85); 
#if (LVDSTX_MODE == SYNC_MODE)
    LT9211C_write(0X6e,(LT9211C_read(0x6e) & BIT3_0));
#elif (LVDSTX_MODE == DE_MODE)
    LT9211C_write(0X6e,(LT9211C_read(0x6e) | BIT3_1)); //[3]lvdstx de mode
#endif

#if (LVDSTX_DATAFORMAT == JEIDA)
    LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT6_1)); //[6]1:JEIDA MODE
    pr_info("Data Format: JEIDA");
#elif (LVDSTX_DATAFORMAT == VESA)
    LT9211C_write(0x6f,(LT9211C_read(0x6f) & BIT6_0)); //[6]0:VESA MODE;
    pr_info("Data Format: VESA");
#endif

#if (LVDSTX_COLORSPACE == RGB)
    pr_info("ColorSpace: RGB");
    #if (LVDSTX_COLORDEPTH == DEPTH_6BIT)
        pr_info("ColorDepth: 6Bit");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x40)); //RGB666 [6]RGB666 output must select jeida mode
        LT9211C_write(0x6f,(LT9211C_read(0x6f) & 0xf3));
    #elif (LVDSTX_COLORDEPTH == DEPTH_8BIT)
        pr_info("ColorDepth: 8Bit");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0X04));
    #elif (LVDSTX_COLORDEPTH == DEPTH_10BIT)
        pr_info("ColorDepth: 10Bit");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0X0c));
    #endif

#elif (LVDSTX_COLORSPACE == YUV422)
    pr_info("ColorSpace: YUV422");    
    LT9211C_write(0xff,0x85);
    #if (LVDSTX_COLORDEPTH == DEPTH_8BIT)
        pr_info("ColorDepth: 8Bit");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0X04));
        #if (LVDSTX_LANENUM == FIVE_LANE)

                pr_info("LvdsLaneNum: 5Lane");
                LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0X40)); //YUV422-8bpc-5lane mode output must sel jeida mode
                LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0X28));  //YUV422-8bpc-5lane mode set
        #else
                pr_info("LvdsLaneNum: 4Lane");
                LT9211C_write(0x6f,(LT9211C_read(0x6f) & 0Xbf)); //YUV422-8bpc-4lane mode output must sel vesa mode
        #endif
    #endif
#endif

#if (LVDSTX_SYNC_INTER_MODE == ENABLED)
    Drv_LvdsTx_VidTiming_Set();    
    pr_info("Lvds Sync Code Mode: Internal"); //internal sync code mode
    LT9211C_write(0x68,(LT9211C_read(0x68) | 0X01));
    #if (LVDSTX_VIDEO_FORMAT == I_FORMAT)
        pr_info("Lvds Video Format: interlaced"); //internal sync code mode
        LT9211C_write(0x68,(LT9211C_read(0x68) | 0X02));
    #endif
    #if (LVDSTX_SYNC_CODE_SEND == REPECTIVE)
        pr_info("Lvds Sync Code Send: respectively."); //sync code send method sel respectively
        LT9211C_write(0x68,(LT9211C_read(0x68) | 0X04));
    #endif

#else
    LT9211C_write(0x68,0x00);
#endif 

}

void Drv_LvdsTxLaneNum_Set(void)
{
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x4a,0x01); //[0]hl_swap_en; [7:6]tx_pt0_src_sel: 0-pta;1-ptb
    LT9211C_write(0x4b,0x00);
    LT9211C_write(0x4c,0x10);
    LT9211C_write(0x4d,0x20);
    LT9211C_write(0x4e,0x50);
    LT9211C_write(0x4f,0x30);

#if (LVDSTX_LANENUM  == FOUR_LANE)
        LT9211C_write(0x50,0x46); //[7:6]tx_pt1_src_sel: 0-pta;1-ptb
        LT9211C_write(0x51,0x10);
        LT9211C_write(0x52,0x20);
        LT9211C_write(0x53,0x50);
        LT9211C_write(0x54,0x30);
        LT9211C_write(0x55,0x00); //[7:4]pt1_tx4_src_sel
        LT9211C_write(0x56,0x20); //[3:0]pt1_tx5_src_sel
                                       //[6:5]rgd_mltx_src_sel: 0-mipitx;1-lvdstx
#elif (LVDSTX_LANENUM == FIVE_LANE)
        LT9211C_write(0x50,0x44); //[7:6]tx_pt1_src_sel: 0-pta;1-ptb
        LT9211C_write(0x51,0x00);
        LT9211C_write(0x52,0x10);
        LT9211C_write(0x53,0x20);
        LT9211C_write(0x54,0x50);
        LT9211C_write(0x55,0x30); //[7:4]pt1_tx4_src_sel
        LT9211C_write(0x56,0x24); //[3:0]pt1_tx5_src_sel
                                       //[6:5]rgd_mltx_src_sel: 0-mipitx;1-lvdstx

#if 0
        //swap when 9211 lvdstx_to_lvdsrx 2port 5lane link
        LT9211C_write(0x54,0x30);
        LT9211C_write(0x55,0x40);
#endif

#endif

}

void Drv_LvdsTxPort_Swap(LvdsOutParameter *lvds_parameter)
{

    if(strstr(lvds_parameter->lvdstx_port_swap, "enabled") || strstr(lvds_parameter->lvdstx_port_sel, "portb")){
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x4a,0x41);
        LT9211C_write(0x50,(LT9211C_read(0x50) & BIT6_0));
    }else{
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x4a,0x01);
        LT9211C_write(0x50,(LT9211C_read(0x50) | BIT6_1));

    }
}

void Drv_LvdsTxPort_Copy(LvdsOutParameter *lvds_parameter)
{

    #if (LVDSTX_PORT_COPY == ENABLED)
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,(LT9211C_read(0x36) | 0x03)); //port swap enable when porta & portb enable

    LT9211C_write(0xff,0x85);

    if(strstr(lvds_parameter->lvdstx_port_sel, "porta")){
        LT9211C_write(0x4a,(LT9211C_read(0x4a) & 0xbf));
        LT9211C_write(0x50,(LT9211C_read(0x50) & 0xbf));
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "portb")){
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x10)); //[7]lvds function enable //[4]0:output 1port; [4]1:output 2port;
        LT9211C_write(0x4a,(LT9211C_read(0x4a) | 0x40));
        LT9211C_write(0x50,(LT9211C_read(0x50) | 0x40));
        pr_info("Port B Copy");
    }
    
    #endif

}

void Drv_LvdsTxCsc_Set(void)
{
    #if LVDSTX_COLORSPACE == RGB
        pr_info("Csc Set:    RGB");
    #elif LVDSTX_COLORSPACE == YUV422
        LT9211C_write(0xff,0x86);
        if((LT9211C_read(0x87) & 0x10) == 0)
        {
            LT9211C_write(0x85,LT9211C_read(0x85) | 0x10);
        }
        else
        {
            LT9211C_write(0x87,LT9211C_read(0x87) & 0xef);
        }
        if((LT9211C_read(0x86) & 0x04) == 0)
        {
            LT9211C_write(0x86,LT9211C_read(0x86) | 0x40);
        }
        else
        {
            LT9211C_write(0x86,LT9211C_read(0x86) & 0xfb);
        }
        pr_info("Csc Set:    YUV422");
    #endif
}

void Mod_LvdsTxPll_RefPixClk_Get(void)
{

    /*mipi to lvds use desscpll pix clk as txpll ref clk*/
    g_stRxVidTiming.ulPclk_Khz = Drv_System_FmClkGet(AD_DESSCPLL_PIX_CLK);

}

void Mod_LvdsTxDig_Set(LvdsOutParameter *lvds_parameter)
{
    Drv_LvdsTxPort_Set(lvds_parameter);
    Drv_LvdsTxVidFmt_Set();
    Drv_LvdsTxLaneNum_Set();
    Drv_LvdsTxPort_Swap(lvds_parameter);
    Drv_LvdsTxPort_Copy(lvds_parameter);
    Drv_LvdsTxSw_Rst();
    Drv_LvdsTxCsc_Set();
}

void Mod_LvdsTx_StateJudge(void)
{
    //monitor upstream video stable.
    if(g_stChipTx.ucTxState > STATE_CHIPTX_UPSTREAM_VIDEO_READY)
    {
        if(g_stChipTx.b1UpstreamVideoReady == LONTIUM_FALSE)
        {
            Mod_SystemTx_SetState(STATE_CHIPTX_UPSTREAM_VIDEO_READY);
        }
    }
}

void Mod_LvdsTx_StateHandler(LvdsOutParameter *lvds_parameter)
{
    switch (g_stChipTx.ucTxState)
    {
        case STATE_CHIPTX_POWER_ON:
            Mod_SystemTx_SetState(STATE_CHIPTX_UPSTREAM_VIDEO_READY);
        break;

        case STATE_CHIPTX_UPSTREAM_VIDEO_READY:
            if(g_stChipTx.b1TxStateChanged == LONTIUM_TRUE)
            {
                Drv_LVDSTxPhy_PowerOff();
                g_stChipTx.b1TxStateChanged = LONTIUM_FALSE;
            }
        
            if(g_stChipTx.b1UpstreamVideoReady == LONTIUM_TRUE) 
            {
                Drv_SystemTxSram_Sel(LVDSTX);
                Drv_LvdsTxPhy_Poweron(lvds_parameter);
                Mod_SystemTx_SetState(STATE_CHIPTX_CONFIG_VIDEO);
            }
        break;

        case STATE_CHIPTX_CONFIG_VIDEO:
            Mod_LvdsTxPll_RefPixClk_Get();
            Drv_LvdsTxPll_RefPixClk_Set();
            Drv_LvdsTxPll_Config(lvds_parameter);
            if(Drv_LvdsTxPll_Cali() == SUCCESS)
            {
                Mod_SystemTx_SetState(STATE_CHIPTX_VIDEO_OUT);
            }
            else
            {
                pr_err("fail to Drv_LvdsTxPll_Cali ");
                Mod_SystemTx_SetState(STATE_CHIPTX_CONFIG_VIDEO);
            }
        break;

        case STATE_CHIPTX_VIDEO_OUT:
            Drv_VidChkAll_Get(&g_stVidChk);
            Mod_LvdsTxDig_Set(lvds_parameter);
            Mod_SystemTx_SetState(STATE_CHIPTX_PLAY_BACK);
        break;

        case STATE_CHIPTX_PLAY_BACK:
        break;
        
    }
}

void Mod_LvdsTx_Handler(LvdsOutParameter *lvds_parameter)
{
    Mod_LvdsTx_StateHandler(lvds_parameter);
}

void Mod_MipiRx_Init(void)
{
    memset(&g_stPcrPara,0 ,sizeof(StructPcrPara));
    memset(&g_stMipiRxVidTiming_Get,0 ,sizeof(SrtuctMipiRx_VidTiming_Get));
}

void Mod_MipiRxDig_Set(void)
{
    Drv_MipiRx_InputSel();
    Drv_MipiRx_LaneSet();
}

u8 Mod_MipiRx_VidChk_Stable(void)
{
    LT9211C_write(0xff, 0x86);
    if((LT9211C_read(0x40) & 0x01) == 0x01)
    {
        return LONTIUM_TRUE;
    }
    else
    {
        return LONTIUM_FALSE;
    }

}

void Mod_MipiRx_StateHandler(StructChipRxVidTiming *ptn_timings)
{
    switch (g_stChipRx.ucRxState)
    {
        case STATE_CHIPRX_POWER_ON :
            Mod_MipiRx_Init();
            Mod_SystemRx_SetState(STATE_CHIPRX_WAITE_SOURCE);
        break;
        
        case STATE_CHIPRX_WAITE_SOURCE:
            Drv_MipiRx_PhyPowerOn();
            Drv_MipiRxClk_Sel();
            Drv_System_VidChkClk_SrcSel(MLRX_BYTE_CLK);
            Drv_System_VidChk_SrcSel(MIPIDEBUG);
            Drv_SystemActRx_Sel(MIPIRX);
            Mod_MipiRxDig_Set();
            Mod_SystemRx_SetState(STATE_CHIPRX_VIDTIMING_CONFIG);
        break;
        
        case STATE_CHIPRX_VIDTIMING_CONFIG:
            if(Drv_MipiRx_VidTiming_Get() == LONTIUM_TRUE)
            {
                g_stChipRx.ucRxFormat = Drv_MipiRx_VidFmt_Get(g_stMipiRxVidTiming_Get.ucFmt);
                if (Drv_MipiRx_VidTiming_Sel(ptn_timings) == LONTIUM_TRUE)
                {
                    Mod_SystemRx_SetState(STATE_CHIPRX_PLL_CONFIG);
                }
                else
                {
                    pr_err("No Video Timing Matched");
                    Mod_SystemRx_SetState(STATE_CHIPRX_WAITE_SOURCE);
                }
            }
        break;
  
        case STATE_CHIPRX_PLL_CONFIG: 
            Drv_MipiRx_DesscPll_Set();
            if(Drv_MipiRx_PcrCali() == SUCCESS)
            {
                pr_info("mipirx LT9211C pcr stable");
                Drv_System_VidChkClk_SrcSel(DESSCPLL_PIX_CLK);
                Drv_System_VidChk_SrcSel(MIPIRX);
                Mod_SystemRx_SetState(STATE_CHIPRX_VIDEO_CHECK);
            }
            else
            {
                Mod_SystemRx_SetState(STATE_CHIPRX_VIDTIMING_CONFIG);
            }
        break;

        case STATE_CHIPRX_VIDEO_CHECK:
            
            if (Mod_MipiRx_VidChk_Stable() == LONTIUM_TRUE)
            {
                pr_info("mipirx Video Check Stable");
                g_stChipRx.pHdmiRxNotify(MIPIRX_VIDEO_ON_EVENT);
                Mod_SystemRx_SetState(STATE_CHIPRX_PLAY_BACK);
            }

            pr_info("mipirx Video Check Stable");
            g_stChipRx.pHdmiRxNotify(MIPIRX_VIDEO_ON_EVENT);
            Mod_SystemRx_SetState(STATE_CHIPRX_PLAY_BACK);

        break;

        case STATE_CHIPRX_PLAY_BACK:
        break;
    }
}

void Mod_MipiRx_Handler(StructChipRxVidTiming *ptn_timings)
{
      Mod_MipiRx_StateHandler(ptn_timings);
}
