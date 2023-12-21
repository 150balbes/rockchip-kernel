/******************************************************************************
  * @project: LT9211C
  * @file: ModPattern.c
  * @author: sxue
  * @company: LONTIUM COPYRIGHT and CONFIDENTIAL
  * @date: 2023.01.29
******************************************************************************/

#include    "include.h"

                                                    //hfp    hs     hbp     hact     htotal   vfp   vs   vbp   vact    vtotal 
struct video_pattern_timing pattern_640x480_15Hz   = {24,    96,    40,     640,     800,     33,   2,   10,   480,    525,     15};
struct video_pattern_timing pattern_640x480_60Hz   = {24,    96,    40,     640,     800,     33,   2,   10,   480,    525,     60};
struct video_pattern_timing pattern_720x480_60Hz   = {16,    62,    60,     720,     858,     9,    6,   30,   480,    525,     60};
struct video_pattern_timing pattern_1280x720_60Hz  = {110,   40,    220,    1280,    1650,    5,    5,   20,   720,    750,     60};
//struct video_pattern_timing pattern_1280x800_60Hz  = {50,    10,    50,    800,      910,    10,    18,   10,   1280,   1318,   60};
struct video_pattern_timing pattern_1280x800_60Hz  = {10,    18,    10,     1280,    1318,    50,   10,  50,   800,    910,     60};
struct video_pattern_timing pattern_1920x720_60Hz  = {48,    32,    80,     1920,    2080,    5,    5,   20,   720,    750,     60};
struct video_pattern_timing pattern_1920x1080_30Hz = {88,    44,    148,    1920,    2200,    4,    5,   36,   1080,   1125,    30};
//struct video_pattern_timing pattern_1920x1080_60Hz = {88,    44,    148,    1920,    2200,    4,    5,   36,   1080,   1125,    60};
//struct video_pattern_timing pattern_1920x1080_60Hz = {10,    18,    10,     1280,    1318,    50,   10,  50,   800,    910,     60};
struct video_pattern_timing pattern_1920x1080_60Hz = {50,  10,  50,   800,    910,    10,  18,  10,  1280,  1318,  60};
struct video_pattern_timing pattern_1920x1080_90Hz = {88,    44,    148,    1920,    2200,    4,    5,   36,   1080,   1125,    90};
struct video_pattern_timing pattern_1920x1080_100Hz= {88,    44,    148,    1920,    2200,    4,    5,   36,   1080,   1125,   100};
struct video_pattern_timing pattern_2560x1440_60Hz = {48,    32,    80,     2560,    2720,    3,    5,   33,   1440,   1481,    60};
struct video_pattern_timing pattern_3840x2160_30Hz = {176,   88,    296,    3840,    4400,    8,   10,  72,   2160,   2250,    30};


void Mod_SystemInt(void)
{
    /* system clock init */       

    LT9211C_write(0xff,0x85);
    LT9211C_write(0xe9,0x88); //sys clk sel from XTAL

}

void Mod_DesscPll_ForPtn(struct ChipRxVidTiming *ptn_timing)
{
    u32 pclk_khz;
    u8 desscpll_pixck_div;
    u32 pcr_m, pcr_k;

    pclk_khz = ptn_timing->ulPclk_Khz; 
    
    
    /* dessc pll */
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x26,0x20); //[7:6]desscpll reference select Xtal clock as reference
                                   //[4]1'b0: dessc-pll power down
    
    
    if(pclk_khz < 11000)
    {
        LT9211C_write(0x2f,0x07);
        desscpll_pixck_div = 16;
        LT9211C_write(0x2c,0x02);    //lowf_pix_div = 4
        pclk_khz = 4 * pclk_khz;
    }
    else if(pclk_khz < 22000)
    {
        LT9211C_write(0x2f,0x07);
        desscpll_pixck_div = 16;
        LT9211C_write(0x2c,0x01);    //lowf_pix_div = 2
        pclk_khz = 2 * pclk_khz;
    }
    else if(pclk_khz < 44000)
    {
        LT9211C_write(0x2f,0x07);
        desscpll_pixck_div = 16;
    }
    else if(pclk_khz < 88000)
    {
        LT9211C_write(0x2f,0x06);
        desscpll_pixck_div = 8;
    }
    else if(pclk_khz < 176000)
    {
        LT9211C_write(0x2f,0x05);
        desscpll_pixck_div = 4;
    }
    else if(pclk_khz < 352000)
    {
        LT9211C_write(0x2f,0x04);
        desscpll_pixck_div = 2;
    }
    else
    {
        LT9211C_write(0x2f,0x04);
        desscpll_pixck_div = 2;
    }
    pcr_m = (pclk_khz * desscpll_pixck_div) /25;
    pcr_k = pcr_m%1000;
    pcr_m = pcr_m/1000;

    pcr_k <<= 14;
    
    //pixel clk
    LT9211C_write(0xff,0xd0); //pcr
    LT9211C_write(0x2d,0x7f);
    LT9211C_write(0x31,0x00);

    LT9211C_write(0x26,0x80|((u8)pcr_m));
    LT9211C_write(0x27,(u8)((pcr_k>>16)&0xff)); //K
    LT9211C_write(0x28,(u8)((pcr_k>>8)&0xff)); //K
    LT9211C_write(0x29,(u8)(pcr_k&0xff)); //K

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x03,0xfe); //desscpll rst
    msleep(10);
    LT9211C_write(0x03,0xff);
}

void Mod_VidChkDebug_ForPtn(void)
{
    u16 ushact, usvact;
    u16 ushs, usvs;
    u16 ushbp, usvbp;
    u16 ushtotal, usvtotal;
    u16 ushfp, usvfp;
    u8 ucsync_polarity;
    u8 ucframerate;

    LT9211C_write(0xff,0x86);
    LT9211C_write(0x3f,0x03); //video check clk src sel : pattern
    msleep(100);
    ucframerate = Drv_VidChk_FrmRt_Get();
    pr_info("Pattern ucframerate: %d",ucframerate);
    LT9211C_write(0xff,0x86);
    ucsync_polarity = LT9211C_read(0x4f);
    
    usvs = LT9211C_read(0x52); //[15:8]
    usvs = (usvs <<8 ) + LT9211C_read(0x53);

    ushs = LT9211C_read(0x50);
    ushs = (ushs << 8) + LT9211C_read(0x51);
    
    usvbp = LT9211C_read(0x57);
    usvfp = LT9211C_read(0x5b);

    ushbp = LT9211C_read(0x54);
    ushbp = (ushbp << 8) + LT9211C_read(0x55);

    ushfp = LT9211C_read(0x58);
    ushfp = (ushfp << 8) + LT9211C_read(0x59);

    usvtotal = LT9211C_read(0x62);
    usvtotal = (usvtotal << 8) + LT9211C_read(0x63);

    ushtotal = LT9211C_read(0x60);
    ushtotal = (ushtotal << 8) + LT9211C_read(0x61);

    usvact = LT9211C_read(0x5e);
    usvact = (usvact << 8)+ LT9211C_read(0x5f);

    ushact = LT9211C_read(0x5c);
    ushact = (ushact << 8) + LT9211C_read(0x5d);

    pr_info("Pattern sync_polarity = 0x%02x", ucsync_polarity);

    if ((ushact == pattern_640x480_60Hz.hact ) &&( usvact == pattern_640x480_60Hz.vact ))
    {
        pr_info("Video Pattern Set 640x480");
    }
    else if ((ushact == pattern_720x480_60Hz.hact ) &&(usvact == pattern_720x480_60Hz.vact ))
    {
        pr_info("Video Pattern Set 720x480_60Hz");
    }
    else if ((ushact == pattern_1280x720_60Hz.hact ) &&(usvact == pattern_1280x720_60Hz.vact ))
    {
        pr_info("Video Pattern Set 1280x720_60Hz");
    }
    else if ((ushact == pattern_1280x800_60Hz.hact ) &&(usvact == pattern_1280x800_60Hz.vact ))
    {
        pr_info("Video Pattern Set 1280x800_60Hz");
    }
    else if ((ushact == pattern_1920x720_60Hz.hact ) &&(usvact == pattern_1920x720_60Hz.vact ))
    {
        pr_info("Video Pattern Set 1920x720_60Hz");
    }
    else if ((ushact == pattern_1920x1080_60Hz.hact ) &&(usvact == pattern_1920x1080_60Hz.vact ))
    {
        if (ucframerate > (pattern_1920x1080_90Hz.ucFrameRate - 3) && ucframerate < (pattern_1920x1080_90Hz.ucFrameRate + 3))
        {
            pr_info("Video Pattern Set 1920x1080_90Hz");
        }
        else if (ucframerate > (pattern_1920x1080_100Hz.ucFrameRate - 3) && ucframerate < (pattern_1920x1080_100Hz.ucFrameRate + 3))
        {
            pr_info("Video Pattern Set 1920x1080_100Hz");
        }
        else
        {
            pr_info("Video Pattern Set 1920x1080_60Hz");
        }
    }
    else if ((ushact == pattern_2560x1440_60Hz.hact ) &&(usvact == pattern_2560x1440_60Hz.vact ))
    {
        pr_info("Video Pattern Set 2560x1440_60Hz");
    }
    else if ((ushact == pattern_3840x2160_30Hz.hact ) &&(usvact == pattern_3840x2160_30Hz.vact ))
    {
        pr_info("Video Pattern Set 3840x2160_30Hz");
    }
    else 
    {
        pr_info("No Video Set");
    }
    pr_info("Pattern hfp, hs, hbp, hact, htotal = %d %d %d %d %d",ushfp, ushs, ushbp, ushact, ushtotal);
    pr_info("Pattern vfp, vs, vbp, vact, vtotal = %d %d %d %d %d",usvfp, usvs, usvbp, usvact, usvtotal);
}

void Mod_LvdsTx_PtnTiming_Set(struct ChipRxVidTiming *ptn_timing)
{

    /* CLK sel */
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x80,0x01); //[7:6]rx sram rd clk src sel ad dessc pcr clk
                                   //[5:4]rx sram wr clk src sel mlrx bytr clk
                                   //[1:0]video check clk sel from desscpll pix clk
    LT9211C_write(0x81,0x10); //[5]mlrx byte clock select from ad_mlrxb_byte_clk
                                   //[4]rx output pixel clock select from ad_desscpll_pix_clk
                                   
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x30,0x43); //Chip active RX select pattern
    
    LT9211C_write(0xff,0x86);
    LT9211C_write(0x25,0x11); //Test video pattern encode mode select
    LT9211C_write(0x26,0x04); //

    LT9211C_write(0x11,0x77); //pattern mode sel
    LT9211C_write(0x12,0xff); //Pattern data value set

    ptn_timing->usHtotal = ptn_timing->usHfp + ptn_timing->usHs + ptn_timing->usHbp + ptn_timing->usHact;
    ptn_timing->usVtotal = ptn_timing->usVfp + ptn_timing->usVs + ptn_timing->usVbp + ptn_timing->usVact;

    LT9211C_write(0x13,(u8)((ptn_timing->usHs+ptn_timing->usHbp)/256));
    LT9211C_write(0x14,(u8)((ptn_timing->usHs+ptn_timing->usHbp)%256)); //h_start

    LT9211C_write(0x16,(u8)((ptn_timing->usVs+ptn_timing->usVbp)%256)); //v_start

    LT9211C_write(0x17,(u8)(ptn_timing->usHact/256));
    LT9211C_write(0x18,(u8)(ptn_timing->usHact%256)); //hactive

    LT9211C_write(0x19,(u8)(ptn_timing->usVact/256));
    LT9211C_write(0x1a,(u8)(ptn_timing->usVact%256));  //vactive

    LT9211C_write(0x1b,(u8)(ptn_timing->usHtotal/256));
    LT9211C_write(0x1c,(u8)(ptn_timing->usHtotal%256)); //htotal

    LT9211C_write(0x1d,(u8)(ptn_timing->usVtotal/256));
    LT9211C_write(0x1e,(u8)(ptn_timing->usVtotal%256)); //vtotal

    LT9211C_write(0x1f,(u8)(ptn_timing->usHs/256)); 
    LT9211C_write(0x20,(u8)(ptn_timing->usHs%256)); //hs

    LT9211C_write(0x21,(u8)(ptn_timing->usVs%256)); //vs

}

void Mod_LvdsTx_DesscPll_ForPtn(struct ChipRxVidTiming *ptn_timing)
{
    u32 pclk_khz;
    u8 desscpll_pixck_div;
    u32 pcr_m, pcr_k;

    pclk_khz = ptn_timing->ulPclk_Khz; 
 
    
    /* dessc pll */
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x26,0x20); //[7:6]desscpll reference select Xtal clock as reference
                                   //[4]1'b0: dessc-pll power down
    if(pclk_khz < 44000)
    {
        LT9211C_write(0x2f,0x07);
        desscpll_pixck_div = 16;
    }

    else if(pclk_khz < 88000)
    {
        LT9211C_write(0x2f,0x06);
        desscpll_pixck_div = 8;
    }

    else if(pclk_khz < 176000)
    {
        LT9211C_write(0x2f,0x05);
        desscpll_pixck_div = 4;
    }

    else if(pclk_khz < 352000)
    {
        LT9211C_write(0x2f,0x04);
        desscpll_pixck_div = 2;
    }

    pcr_m = (pclk_khz * desscpll_pixck_div) /25;
    pcr_k = pcr_m%1000;
    pcr_m = pcr_m/1000;

    pcr_k <<= 14;
    
    //pixel clk
    LT9211C_write(0xff,0xd0); //pcr
    LT9211C_write(0x2d,0x7f);
    LT9211C_write(0x31,0x00);

    LT9211C_write(0x26,0x80|((u8)pcr_m));
    LT9211C_write(0x27,(u8)((pcr_k>>16)&0xff)); //K
    LT9211C_write(0x28,(u8)((pcr_k>>8)&0xff)); //K
    LT9211C_write(0x29,(u8)(pcr_k&0xff)); //K

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x03,0xfe); //desscpll rst
    LT9211C_write(0x03,0xff);
}

void Mod_LvdsTxPhy_ForPtn(LvdsOutParameter *lvds_parameter)
{       

/*
#if LVDSTX_PORT_SEL  == PORTA
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,0x01); //lvds enable
    LT9211C_write(0x37,0x40);
    
#elif LVDSTX_PORT_SEL  == PORTB
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,0x02); //lvds enable
    LT9211C_write(0x37,0x04);
    
#elif LVDSTX_PORT_SEL  == DOU_PORT
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,0x03); //lvds enable
    LT9211C_write(0x37,0x44); //port rterm enable
#endif*/

    if(strstr(lvds_parameter->lvdstx_port_sel, "porta")!= NULL){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x01); //lvds enable
        LT9211C_write(0x37,0x40);
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "portb")!= NULL){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x02); //lvds enable
        LT9211C_write(0x37,0x04);
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "dou_port")){
        LT9211C_write(0xff,0x82);
        LT9211C_write(0x36,0x03); //lvds enable
        LT9211C_write(0x37,0x44); //port rterm enable
    }
    
    LT9211C_write(0x38,0x14);
    LT9211C_write(0x39,0x31); //0x31
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

void Mod_LvdsTxpll_ForPtn(struct ChipRxVidTiming *ptn_timing)
{
    
    u8 ucPreDiv = 0;
    u8 ucSericlkDiv = 0;
    u8 ucDivSet = 0;
    u16 ucPixClkDiv = 0;
    u32 ulLvdsTXPhyClk = 0;
    
    u32 pclk_khz;
    pclk_khz = ptn_timing->ulPclk_Khz; 

    LT9211C_write(0xff,0x82);
    LT9211C_write(0x30,0x00); //[7]0:txpll normal work; txpll ref clk sel pix clk
    
    /* txphyclk = vco clk * ucSericlkDiv */
#if (LVDSTX_PORT_SEL == DOU_PORT)
    ulLvdsTXPhyClk = (u32)(pclk_khz * 7 / 2); //2 port: byte clk = pix clk / 2;
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT0_1)); //htotal -> 2n
    
    #if ((LVDSTX_COLORSPACE == YUV422) && (LVDSTX_COLORDEPTH == DEPTH_8BIT) && (LVDSTX_LANENUM == FIVE_LANE))
    ulLvdsTXPhyClk = (u32)(pclk_khz * 7 / 4); //2 port YUV422 8bit 5lane: byte clk = pix clk / 4;
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT1_1)); //htotal -> 4n
    #endif 
#else
    ulLvdsTXPhyClk = (u32)(pclk_khz * 7); //1 port: byte clk = pix clk;
    
    #if ((LVDSTX_COLORSPACE == YUV422) && (LVDSTX_COLORDEPTH == DEPTH_8BIT) && (LVDSTX_LANENUM == FIVE_LANE))
    ulLvdsTXPhyClk = (u32)(pclk_khz * 7 / 2); //1 port YUV422 8bit 5lane: byte clk = pix clk / 2;
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT0_1)); //htotal -> 2n
    #endif
#endif
    
    /*txpll prediv sel*/
    LT9211C_write(0xff,0x82);
    if (pclk_khz < 20000)
    {
        LT9211C_write(0x31,0x28); //[2:0]3'b000: pre div set div1
        ucPreDiv = 1;
    }
    else if (pclk_khz >= 20000 && pclk_khz < 40000)
    {
        LT9211C_write(0x31,0x28); //[2:0]3'b000: pre div set div1
        ucPreDiv = 1;
    }
    else if (pclk_khz >= 40000 && pclk_khz < 80000)
    {
        LT9211C_write(0x31,0x29); //[2:0]3'b001: pre div set div2
        ucPreDiv = 2;
    }
    else if (pclk_khz >= 80000 && pclk_khz < 160000)
    {
        LT9211C_write(0x31,0x2a); //[2:0]3'b010: pre div set div4
        ucPreDiv = 4;
    }
    else if (pclk_khz >= 160000 && pclk_khz < 320000)
    {
        LT9211C_write(0x31,0x2b); //[2:0]3'b011: pre div set div8
        ucPreDiv = 8;
//        ulLvdsTXPhyClk = ulDessc_Pix_Clk * 3.5;
    }
    else if (pclk_khz >= 320000)
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
    if (pclk_khz > 150000)
    {
        LT9211C_write(0x33,0x04); //pixclk > 150000, pixclk mux sel (vco clk / 3.5)
        ucPixClkDiv = 3.5;
    }
    else
    {
        ucPixClkDiv = (u8)((ulLvdsTXPhyClk * ucSericlkDiv) / (pclk_khz * 7));

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
    
    ucDivSet = (u8)((ulLvdsTXPhyClk * ucSericlkDiv) / (pclk_khz / ucPreDiv));
    
    LT9211C_write(0x34,0x01); //txpll div set software output enable
    LT9211C_write(0x35,ucDivSet);
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

void Mod_LvdsTxDigital_Forptn(void)
{  
    
    LT9211C_write(0xff,0x85); 
#if LVDSTX_MODE == SYNC_MODE
        LT9211C_write(0X6e,(LT9211C_read(0x6e) & 0xf7));
#elif LVDSTX_MODE == DE_MODE
        LT9211C_write(0X6e,(LT9211C_read(0x6e) | 0x08)); //[3]lvdstx de mode
#endif        
    
#if ((LVDSTX_PORT_SEL == PORTA) || (LVDSTX_PORT_SEL == PORTB))
        printk("Pattern :%s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
        pr_info("Pattern LVDS Output Port Num: 1Port");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x80)); //[7]lvds function enable
                                                                  //[4]0:output 1port; [4]1:output 2port;
#elif (LVDSTX_PORT_SEL == DOU_PORT)
        pr_info("LVDS Output Port Num: 2Port");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x90)); //[7]lvds function enable
                                                                   //[4]0:output 1port; [4]1:output 2port;
#endif                                     
                                       
#if (LVDSTX_DATAFORMAT == JEIDA)
        pr_info("Data Format: JEIDA");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) | BIT6_1)); //[6]1:JEIDA MODE
#elif (LVDSTX_DATAFORMAT == VESA)
        pr_info("Pattern Data Format: VESA");
        LT9211C_write(0x6f,(LT9211C_read(0x6f) & BIT6_0)); //[6]0:VESA MODE; 
#endif
    
#if (LVDSTX_COLORSPACE == RGB)
        pr_info("Pattern ColorSpace: RGB");
    #if (LVDSTX_COLORDEPTH == DEPTH_6BIT)
            pr_info("ColorDepth: 6Bit");
            LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x40)); //RGB666 [6]RGB666 output must select jeida mode
            LT9211C_write(0x6f,(LT9211C_read(0x6f) & 0xf3));
    #elif (LVDSTX_COLORDEPTH == DEPTH_8BIT)
            pr_info("Pattern ColorDepth: 8Bit");
            LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x04));
    #elif (LVDSTX_COLORDEPTH == DEPTH_10BIT)
            pr_info("ColorDepth: 10Bit");
            LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x0c));
    #endif

#elif (LVDSTX_COLORSPACE == YUV422)

        pr_info("ColorSpace: YUV422");
        LT9211C_write(0xff,0x86);
        LT9211C_write(0x85,0x10); //[4]1: csc RGB to YUV enable
        LT9211C_write(0x86,0x40); //[6]1: csc YUV444 to YUV422 enable
        
        LT9211C_write(0xff,0x85);
    #if (LVDSTX_COLORDEPTH == DEPTH_8BIT)
            pr_info("ColorDepth: 8Bit");
            LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x04));
        #if (LVDSTX_LANENUM == FIVE_LANE)

                pr_info("LvdsLaneNum: 5Lane");
                LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x40)); //YUV422-8bpc-5lane mode output must sel jeida mode
                LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x28));  ////YUV422-8bpc-5lane mode set
        #else
                pr_info("LvdsLaneNum: 4Lane");
                LT9211C_write(0x6f,(LT9211C_read(0x6f) & 0xbf)); //YUV422-8bpc-5lane mode output must sel jeida mode
        #endif
    #endif
#endif

#if (LVDSTX_SYNC_INTER_MODE == ENABLED)
        pr_info("Lvds Sync Code Mode: Internal"); //internal sync code mode
        LT9211C_write(0x68,(LT9211C_read(0x68) | 0x01));
    #if (LVDSTX_VIDEO_FORMAT == I_FORMAT)
            pr_info("Lvds Video Format: interlaced"); //internal sync code mode
            LT9211C_write(0xff,0x86);
            LT9211C_write(0x11,LT9211C_read(0x11) | 0x80); //Pattern Interlace format
            LT9211C_write(0xff,0x85);
            LT9211C_write(0x68,(LT9211C_read(0x68) | 0x02));   
    #endif
    #if (LVDSTX_SYNC_CODE_SEND == REPECTIVE)
            pr_info("Lvds Sync Code Send: respectively."); //sync code send method sel respectively
            LT9211C_write(0x68,(LT9211C_read(0x68) | 0x04));
    #endif
    
    u16 vss = 0;
    u16 eav = 0;
    u16 sav = 0;
    vss = ptn_timing.usVs + ptn_timing.usVbp;
    eav = ptn_timing.usHs + ptn_timing.usHbp + ptn_timing.usHact + 4;
    sav = ptn_timing.usHs + ptn_timing.usHbp;
    
    
    LT9211C_write(0x5f,0x00);    
    LT9211C_write(0x60,0x00);  
    LT9211C_write(0x62,(u8)(ptn_timing.usVact>>8));         //vact[15:8]
    LT9211C_write(0x61,(u8)(ptn_timing.usVact));            //vact[7:0]
    LT9211C_write(0x63,(u8)(vss));                         //vss[7:0]
    LT9211C_write(0x65,(u8)(eav>>8));                      //eav[15:8]
    LT9211C_write(0x64,(u8)(eav));                         //eav[7:0]
    LT9211C_write(0x67,(u8)(sav>>8));                      //sav[15:8]
    LT9211C_write(0x66,(u8)(sav));                         //sav[7:0]
     
    #if LVDSTX_VIDEO_FORMAT == I_FORMAT
        #if TX_VID_PATTERN_SEL == PTN_1920x1080_30
            LT9211C_write(0x61,0x1c);
            LT9211C_write(0x62,0x02);
            LT9211C_write(0x63,0x14);
        #endif
    #endif

#else
        LT9211C_write(0x68,0x00);
#endif

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
#endif

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x08,0x6f); //LVDS TX SW reset
    LT9211C_write(0x08,0x7f);

}

void Mod_LvdsTxPort_Swap_Forptn(LvdsOutParameter *lvds_parameter)
{
    /*
#if (LVDSTX_PORT_SWAP == ENABLED) || (LVDSTX_PORT_SEL == PORTB)
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x4a,0x41);
    LT9211C_write(0x50,(LT9211C_read(0x50) & BIT6_0));
#else
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x4a,0x01);
    LT9211C_write(0x50,(LT9211C_read(0x50) | BIT6_1));
#endif*/

    if(strstr(lvds_parameter->lvdstx_port_swap, "enabled") || strstr(lvds_parameter->lvdstx_port_sel, "portb")){
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x4a,0x41);
        LT9211C_write(0x50,(LT9211C_read(0x50) & BIT6_0));
        pr_info("lvdstx_port_swap=%s\n",lvds_parameter->lvdstx_port_swap);
    }else{
        LT9211C_write(0xff,0x85);
        LT9211C_write(0x4a,0x01);
        LT9211C_write(0x50,(LT9211C_read(0x50) | BIT6_1));
    }
}

void Mod_LvdsTxPort_Copy_Forptn(void)
{

    #if (LVDSTX_PORT_COPY == ENABLED)

    /*
    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,(LT9211C_read(0x36) | 0x03)); //port swap enable when porta & portb enable

    LT9211C_write(0xff,0x85);
    #if (LVDSTX_PORT_SEL == PORTA)
    LT9211C_write(0x4a,(LT9211C_read(0x4a) & 0xbf));
    LT9211C_write(0x50,(LT9211C_read(0x50) & 0xbf));
    pr_info("Port A Copy");
    #elif (LVDSTX_PORT_SEL == PORTB)
    LT9211C_write(0x6f,(LT9211C_read(0x6f) | 0x10)); //[7]lvds function enable //[4]0:output 1port; [4]1:output 2port;
    LT9211C_write(0x4a,(LT9211C_read(0x4a) | 0x40));
    LT9211C_write(0x50,(LT9211C_read(0x50) | 0x40));
    pr_info("Port B Copy");
    #endif*/

    LT9211C_write(0xff,0x82);
    LT9211C_write(0x36,(LT9211C_read(0x36) | 0x03)); //port swap enable when porta & portb enable

    LT9211C_write(0xff,0x85);

    if(strstr(lvds_parameter->lvdstx_port_sel, "porta")){
        LT9211C_write(0x4a,(LT9211C_read(0x4a) & 0xbf));
        LT9211C_write(0x50,(LT9211C_read(0x50) & 0xbf));
    }else if(strstr(lvds_parameter->lvdstx_port_sel, "portb")){
        LT9211C_write(0x4a,(LT9211C_read(0x4a) & 0xbf));
        LT9211C_write(0x50,(LT9211C_read(0x50) & 0xbf));
    }
    
    #endif


}


void Mod_LvdsTxPattern_Out(StructChipRxVidTiming *ptn_timings, LvdsOutParameter *lvds_parameter)
{
    pr_info("*************LT9211C LVDSTX Pattern Config*************");
    Mod_SystemInt();
    
    Mod_LvdsTx_PtnTiming_Set(ptn_timings);
    Mod_DesscPll_ForPtn(ptn_timings);
    
    Mod_LvdsTxPhy_ForPtn(lvds_parameter);
    Mod_LvdsTxpll_ForPtn(ptn_timings);
    Mod_LvdsTxDigital_Forptn();
    Mod_LvdsTxPort_Swap_Forptn(lvds_parameter);
    Mod_LvdsTxPort_Copy_Forptn();
    Mod_VidChkDebug_ForPtn();
    pr_info("*************LT9211C LVDSTX Vid Pattern Out************");
    
}
