/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. *
 */
 
#include "include.h"

StructChipRxVidTiming mipi_timing;
LvdsOutParameter lvds_parameter; 

struct LT9211C {
    struct device *     dev;
    struct i2c_client * i2c_client;
    u8          i2c_addr;

    u32         reset_gpio;
    u32         power_gpio;
};

static struct LT9211C *lt9211c_pdata;

static struct delayed_work test_delayed_work;

static struct i2c_client *lt9211c_client;

StructChipRx g_stChipRx;
StructChipTx g_stChipTx;
StructVidChkTiming g_stVidChk;
StructChipRxVidTiming g_stRxVidTiming;
u8 g_b1CscTodo = LONTIUM_FALSE;

void Mod_SystemTx_SetState(u8 ucState)
{
    if(ucState != g_stChipTx.ucTxState)
    {        
        g_stChipTx.ucTxState = ucState;
        g_stChipTx.b1TxStateChanged = LONTIUM_TRUE;
        pr_debug("TxState = 0x%02x", ucState);
    }
}

void Mod_SystemRx_SetState(u8 ucState)
{
    u8 ucLastState;
    if(ucState != g_stChipRx.ucRxState)
    {   
        ucLastState = g_stChipRx.ucRxState;
        g_stChipRx.ucRxState = ucState;
        g_stChipRx.b1RxStateChanged = LONTIUM_TRUE;
        pr_debug("RxState = 0x%02x", ucState);

        // other state-->STATE_HDMIRX_PLAY_BACK,need notify video on
        if(g_stChipRx.ucRxState == STATE_CHIPRX_PLAY_BACK)
        {
            g_stChipRx.pHdmiRxNotify(MIPIRX_VIDEO_ON_EVENT);
        }

        //STATE_HDMIRX_PLAY_BACK-->other state,need notify video off
        if(ucLastState == STATE_CHIPRX_PLAY_BACK)
        {
            g_stChipRx.pHdmiRxNotify(MIPIRX_VIDEO_OFF_EVENT);
        }
    }
}

void Mod_SystemRx_NotifyRegister(void (*pFunction)(EnumChipRxEvent ucEvent))
{
    g_stChipRx.pHdmiRxNotify  = pFunction;
}

void Mod_SystemTx_PowerOnInit(void)
{
    memset(&g_stChipTx, 0, sizeof(StructChipTx));
    g_stChipTx.ucTxState = STATE_CHIPTX_POWER_ON;
}

void Mod_SystemRx_PowerOnInit(void)
{
    memset(&g_stChipRx,0 ,sizeof(StructChipRx));
    memset(&g_stVidChk,0 ,sizeof(StructVidChkTiming));
    memset(&g_stRxVidTiming,0 ,sizeof(StructChipRxVidTiming));
    
    g_stChipRx.ucRxState = STATE_CHIPRX_POWER_ON;    

    Mod_SystemRx_NotifyRegister(Mod_System_RxNotifyHandle);
}

void Mod_System_RxNotifyHandle(EnumChipRxEvent ucEvent)
{
    switch (ucEvent)
    {
        case MIPIRX_VIDEO_ON_EVENT:
            g_stChipTx.b1UpstreamVideoReady = LONTIUM_TRUE;
            break;
        case MIPIRX_VIDEO_OFF_EVENT:
            g_stChipTx.b1UpstreamVideoReady = LONTIUM_FALSE;
            break;
        case MIPIRX_CSC_EVENT:
            g_b1CscTodo = LONTIUM_TRUE;
        default:
            break;
    }
}

void Drv_SystemActRx_Sel(u8 ucSrc)
{
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x30,(LT9211C_read(0x30) & 0xf8));

    switch(ucSrc)
    {
        case LVDSRX:
            LT9211C_write(0x30,(LT9211C_read(0x30) | LVDSRX));
            LT9211C_write(0x30,(LT9211C_read(0x30) & 0xcf)); //[5:4]00: LVDSRX
        break;
        case MIPIRX:
            LT9211C_write(0x30,(LT9211C_read(0x30) | MIPIRX));
            LT9211C_write(0x30,(LT9211C_read(0x30) | BIT4_1)); //[5:4]01: MIPIRX
        break;
        case TTLRX:
            LT9211C_write(0x30,(LT9211C_read(0x30) | TTLRX));
        break;
        case PATTERN:
            LT9211C_write(0x30,(LT9211C_read(0x30) | PATTERN));
        break;
        default:
            LT9211C_write(0x30,(LT9211C_read(0x30) | LVDSRX));
        break;
        
    }
}

void Drv_SystemTxSram_Sel(u8 ucSrc)
{
    //[7:6]2'b00: TX Sram sel MIPITX; others sel LVDSTX
    LT9211C_write(0xff,0x85);
    LT9211C_write(0x30,(LT9211C_read(0x30) & 0x3f)); 

    switch(ucSrc)
    {
        case LVDSTX:
            LT9211C_write(0x30,(LT9211C_read(0x30) | BIT6_1));
        break;
        case MIPITX:
            LT9211C_write(0x30,(LT9211C_read(0x30) & BIT6_0));
        break;
    }
}

u8 Drv_System_GetPixelEncoding(void)
{
    return g_stChipRx.ucRxFormat;
}


void Drv_System_VidChkClk_SrcSel(u8 ucSrc)
{
    LT9211C_write(0xff,0x81);
    LT9211C_write(0x80,(LT9211C_read(0x80) & 0xfc));

    switch (ucSrc)
    {
        case RXPLL_PIX_CLK:
            LT9211C_write(0x80,(LT9211C_read(0x80) | RXPLL_PIX_CLK));
        break;
        case DESSCPLL_PIX_CLK:
            LT9211C_write(0x80,(LT9211C_read(0x80) | DESSCPLL_PIX_CLK));
        break;
        case RXPLL_DEC_DDR_CLK:
            LT9211C_write(0x80,(LT9211C_read(0x80) | RXPLL_DEC_DDR_CLK));
        break;
        case MLRX_BYTE_CLK:
            LT9211C_write(0x80,(LT9211C_read(0x80) | MLRX_BYTE_CLK));
        break;    
        
    }

}

void Drv_System_VidChk_SrcSel(u8 ucSrc)
{
    LT9211C_write(0xff,0x86);
    LT9211C_write(0x3f,(LT9211C_read(0x80) & 0xf8));

    switch (ucSrc)
    {
        case LVDSRX:
            LT9211C_write(0x3f,LVDSRX);
        break;
        case MIPIRX:
            LT9211C_write(0x3f,MIPIRX);
        break;
        case TTLRX:
            LT9211C_write(0x3f,TTLRX);
        break;
        case PATTERN:
            LT9211C_write(0x3f,PATTERN);
        break;
        case LVDSDEBUG:
            LT9211C_write(0x3f,LVDSDEBUG);
        case MIPIDEBUG:
            LT9211C_write(0x3f,MIPIDEBUG);
        break;
        case TTLDEBUG:
            LT9211C_write(0x3f,TTLDEBUG);
        break;    
        
    }

}


u16 Drv_VidChkSingle_Get(u8 ucPara)
{ 
    u16 usRtn = 0;

    LT9211C_write(0xff,0x81);
    LT9211C_write(0x0b,0x7f);
    LT9211C_write(0x0b,0xff);
    msleep(80);
    LT9211C_write(0xff,0x86);
    switch(ucPara)
    {
        case HTOTAL_POS:
            usRtn = (LT9211C_read(0x60) << 8) + LT9211C_read(0x61);
        break;
        case HACTIVE_POS:
            usRtn = (LT9211C_read(0x5c) << 8) + LT9211C_read(0x5d);  
        break;
        case HFP_POS:
            usRtn = (LT9211C_read(0x58) << 8) + LT9211C_read(0x59);
        break;
        case HSW_POS:
            usRtn = (LT9211C_read(0x50) << 8) + LT9211C_read(0x51);
        break;    
        case HBP_POS:
            usRtn = (LT9211C_read(0x54) << 8) + LT9211C_read(0x55);
        break;
        case VTOTAL_POS:
            usRtn = (LT9211C_read(0x62) << 8) + LT9211C_read(0x63);
        break;
        case VACTIVE_POS:
            usRtn = (LT9211C_read(0x5e) << 8) + LT9211C_read(0x5f);
        break;
        case VFP_POS:
            usRtn = (LT9211C_read(0x5a) << 8) + LT9211C_read(0x5b);
        break;
        case VSW_POS:
            usRtn = (LT9211C_read(0x52) << 8) + LT9211C_read(0x53);
        break;
        case VBP_POS:
            usRtn = (LT9211C_read(0x56) << 8) + LT9211C_read(0x57);
        break;
        case HSPOL_POS:
            usRtn = (LT9211C_read(0x4f) & 0x01);
        break;
        case VSPOL_POS:
            usRtn = (LT9211C_read(0x4f) & 0x02);
        break;
        default:
        break;
    }
    return usRtn;
}

void Drv_VidChkAll_Get(StructVidChkTiming *video_time)
{
    video_time->usHtotal    =     Drv_VidChkSingle_Get(HTOTAL_POS);
    video_time->usHact      =     Drv_VidChkSingle_Get(HACTIVE_POS);
    video_time->usHfp       =     Drv_VidChkSingle_Get(HFP_POS);
    video_time->usHs        =     Drv_VidChkSingle_Get(HSW_POS);
    video_time->usHbp       =     Drv_VidChkSingle_Get(HBP_POS);
    
    video_time->usVtotal    =     Drv_VidChkSingle_Get(VTOTAL_POS);
    video_time->usVact      =     Drv_VidChkSingle_Get(VACTIVE_POS);
    video_time->usVfp       =     Drv_VidChkSingle_Get(VFP_POS);
    video_time->usVs        =     Drv_VidChkSingle_Get(VSW_POS);
    video_time->usVbp       =     Drv_VidChkSingle_Get(VBP_POS);
    
    video_time->ucHspol     =     Drv_VidChkSingle_Get(HSPOL_POS);
    video_time->ucVspol     =     Drv_VidChkSingle_Get(VSPOL_POS);        
    video_time->ucFrameRate =     Drv_VidChk_FrmRt_Get(); 
}

u8 Drv_VidChk_FrmRt_Get(void)
{
    u8 ucframerate = 0; 
    u32 ulframetime = 0;

    LT9211C_write(0xff,0x86);
    ulframetime = LT9211C_read(0x43);
    ulframetime = (ulframetime << 8) + LT9211C_read(0x44);
    ulframetime = (ulframetime << 8) + LT9211C_read(0x45);
    ucframerate = (u8)((250000000 / (ulframetime*10)) + 1); //2500000/ulframetime
    return ucframerate;
}

u32 Drv_System_FmClkGet(u8 ucSrc)
{
    u32 ulRtn = 0;
    LT9211C_write(0xff,0x86);
    LT9211C_write(0X90,ucSrc);
    msleep(5);
    LT9211C_write(0x90,(ucSrc | BIT7_1));
    ulRtn = (LT9211C_read(0x98) & 0x0f);
    ulRtn = (ulRtn << 8) + LT9211C_read(0x99);
    ulRtn = (ulRtn << 8) + LT9211C_read(0x9a);
    LT9211C_write(0x90,(LT9211C_read(0x90) & BIT7_0));
    return ulRtn;
}

int LT9211C_write(u8 reg, u8 val)
{
    int rc = 0;
    struct i2c_client *client = lt9211c_client;
    struct i2c_msg msg;
    u8 buf[2] = { reg, val };
    int read_data;
    int i = 0;
    if (!client) {
        printk("%s: Invalid params\n", __func__);
        return -EINVAL;
    }

    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = 2;
    msg.buf = buf;

    for(i=0; i<3;i++){
        if (i2c_transfer(client->adapter, &msg, 1) < 1) {
            rc = -EIO;
        }
        if(reg != 0xff && rc == 0) {
            read_data = LT9211C_read(reg);
            if (read_data == val)
                break;
        }
    }


    return rc;
}

int LT9211C_read(u8 reg)
{
    int rc = 0;
    struct i2c_client *client = lt9211c_client;
    struct i2c_msg msg[2];
    u8 buf[2] = {reg};
    if (!client) {
        printk("%s: Invalid params\n", __func__);
        return -EINVAL;
    }

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &buf[0];

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = &buf[1];

    if (i2c_transfer(client->adapter, msg, 2) != 2) {
        rc = -EIO;
    }

    if(rc == 0)
         return buf[1];
    else
         return rc;
}

int LT9211C_get_display_timings(struct device_node *np){
    int ret = 0;

    u32 ulPclk;
    if (of_property_read_u32(np, "clock-frequency", &ulPclk)){
        pr_err("vactive not ulPclk_Khz\n");
        ret = -EINVAL;
    }
    mipi_timing.ulPclk_Khz = ulPclk/1000;
    if (of_property_read_u32(np, "hfront-porch", &mipi_timing.usHfp)){
        pr_err("hfront-porch not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "hsync-len", &mipi_timing.usHs)){
        pr_err("hsync-len not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "hback-porch", &mipi_timing.usHbp)){
        pr_err("hback-porch not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "hactive", &mipi_timing.usHact)){
        pr_err("hactive not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "vfront-porch", &mipi_timing.usVfp)){
        pr_err("vfront-porch not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "vsync-len", &mipi_timing.usVs)){
        pr_err("vsync-len not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "vback-porch", &mipi_timing.usVbp)){
        pr_err("vback-porch not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "vactive", &mipi_timing.usVact)){
        pr_err("vactive not specified\n");
        ret = -EINVAL;
    }
    if (of_property_read_u32(np, "frameRate", &mipi_timing.ucFrameRate)){
        pr_err("ucFrameRate not specified\n");
        ret = -EINVAL;
    }
    mipi_timing.usHtotal = mipi_timing.usHfp + mipi_timing.usHs + mipi_timing.usHbp + mipi_timing.usHact;
    mipi_timing.usVtotal = mipi_timing.usVfp + mipi_timing.usVs + mipi_timing.usVbp + mipi_timing.usVact;


    pr_debug("ulPclk_Khz=%d", mipi_timing.ulPclk_Khz);
    pr_debug("ucFrameRate=%d", mipi_timing.ucFrameRate);
    pr_debug("hfront-porch=%d,hsync-len=%d,hback-porch=%d,hactive=%d\n",
    mipi_timing.usHfp,mipi_timing.usHs,mipi_timing.usHbp,mipi_timing.usHact);
    pr_debug("vfront-porch=%d,vsync-len=%d,vback-porch=%d,vactive=%d\n",
    mipi_timing.usVfp,mipi_timing.usVs,mipi_timing.usVbp,mipi_timing.usVact);

    return ret;
}

static int LT9211C_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    int ret = 0;
    u32 flag_reset, flag_power;

    lt9211c_pdata->reset_gpio = of_get_named_gpio_flags(np, "reset-gpio", 0, &flag_reset);

    if (!gpio_is_valid(lt9211c_pdata->reset_gpio)) {
        pr_err("reset gpio not specified\n");
        ret = -EINVAL;
    }

    lt9211c_pdata->power_gpio = of_get_named_gpio_flags(np, "power-gpio", 0, &flag_power);

    if (!gpio_is_valid(lt9211c_pdata->power_gpio)) {
        pr_err("power gpio not specified\n");
        ret = -EINVAL;
    }
    of_property_read_string(np, "mod", &lvds_parameter.lt9211c_mode_sel);
    of_property_read_string(np, "lvdstx_port_sel", &lvds_parameter.lvdstx_port_sel);
    of_property_read_string(np, "lvdstx_port_swap", &lvds_parameter.lvdstx_port_swap);

    pr_debug("reset_gpio=%d\n",lt9211c_pdata->reset_gpio);
    pr_debug("power_gpio=%d\n",lt9211c_pdata->power_gpio);
    pr_debug("lvds_parameter.lt9211c_mode_sel=%s\n",lvds_parameter.lt9211c_mode_sel);
    pr_debug("lvdstx_port_sel=%s\n",lvds_parameter.lvdstx_port_sel);
    pr_debug("lvdstx_port_swap=%s\n",lvds_parameter.lvdstx_port_swap);

    LT9211C_get_display_timings(np);

    return ret;
}

static int LT9211C_gpio_configure(bool on)
{
    int ret = 0;

    if (on) {
        ret = gpio_request(lt9211c_pdata->reset_gpio,
                   "LT9211C-reset-gpio");
        if (ret) {
            pr_err("LT9211C reset gpio request failed\n");
            goto error;
        }
        ret = gpio_request(lt9211c_pdata->power_gpio,
                   "LT9211C-power-gpio");
        if (ret) {
            pr_err("LT9211C power gpio request failed\n");
            goto reset_error;
        }
    } else {
		goto power_error;
    }

    return ret;

power_error:
    gpio_free(lt9211c_pdata->power_gpio);
reset_error:
    gpio_free(lt9211c_pdata->reset_gpio);
error:
    return ret;
}

static void Mod_LT9211C_Reset(void)
{
    gpio_direction_output(lt9211c_pdata->power_gpio,1);
    msleep(100);
    gpio_direction_output(lt9211c_pdata->reset_gpio,0);
    msleep(100);
    gpio_direction_output(lt9211c_pdata->reset_gpio,1);
}

static int Mod_ChipID_Read(void)
{
    u8 retry = 0;
    u8 chip_id_h = 0, chip_id_m = 0, chip_id_l = 0;
    int ret = -EAGAIN;

    while(retry++ < 3) {
        ret = LT9211C_write(0xff, 0x81);

        if(ret < 0) {
            pr_err( "LT9211C i2c test write addr:0xff failed\n");
            continue;
        }
        ret = LT9211C_write(0x08, 0x7f);

        if(ret < 0) {
            pr_err( "LT9211C i2c test write addr:0x08 failed\n");
            continue;
        }

        chip_id_l = LT9211C_read(0x00);
        chip_id_m = LT9211C_read(0x01);
        chip_id_h = LT9211C_read(0x02);
        pr_info( "LT9211C i2c test success chipid: 0x%x%x%x\n", chip_id_l, chip_id_m, chip_id_h);

        if (chip_id_h == 0 && chip_id_l == 0 && chip_id_m == 0) {
            pr_err( "LT9211C i2c test failed time %d\n", retry);
            continue;
        }
        ret = 0;
        break;
    }

    return ret;
}
static int lt9211c_config(void)
{
    int count = 0;
    Mod_LT9211C_Reset();
    Mod_ChipID_Read();

    if(!strstr(lvds_parameter.lt9211c_mode_sel, "pattern_out")){
        Mod_SystemRx_PowerOnInit();
        Mod_SystemTx_PowerOnInit();
    }else
        Mod_LvdsTxPattern_Out(&mipi_timing, &lvds_parameter);

    if(!strstr(lvds_parameter.lt9211c_mode_sel, "pattern_out")){
        while(1) {
            count++;
            if(strstr(lvds_parameter.lt9211c_mode_sel, "mipi_in_lvds_out")){
                Mod_MipiRx_Handler(&mipi_timing);
                Mod_LvdsTx_Handler(&lvds_parameter);
            }
            if((g_stChipRx.ucRxState == STATE_CHIPRX_PLAY_BACK) && (g_stChipTx.ucTxState == STATE_CHIPTX_PLAY_BACK))
                break;
            if (count>30) {
                pr_err("No video source access!\n");
                break;
            }
        }
    }
    return 0;
}

static __attribute__((unused)) void delayed_func_for_lt9211c_state_check(struct work_struct *work)
{   
    //pr_info("enter delayed_func_for_lt9211c_state_check!\n");
    if(!strstr(lvds_parameter.lt9211c_mode_sel, "pattern_out"))
        Mod_LvdsTx_StateJudge();
    if(g_stChipTx.ucTxState == STATE_CHIPTX_UPSTREAM_VIDEO_READY)
        lt9211c_config();
    schedule_delayed_work(&test_delayed_work,1*HZ);
}

EXPORT_SYMBOL(LT9211C_suspend);
void LT9211C_suspend(void)
{
    if(lt9211c_client == NULL)
        return;
    gpio_set_value(lt9211c_pdata->power_gpio, 0);
    msleep(100);
}

EXPORT_SYMBOL(LT9211C_resume);
void LT9211C_resume(void)
{
    if(lt9211c_client == NULL)
        return;
    lt9211c_config();
}

static int LT9211C_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;

    if (!client || !client->dev.of_node) {
        pr_err("Lontium invalid input\n");
        return -EINVAL;
    }

    lt9211c_client = devm_kzalloc(&client->dev,
                 sizeof(struct i2c_client), GFP_KERNEL);
    if (!lt9211c_client) {
        pr_err( "Lontium Failed alloc lt9211c_client memory\n");
        return -ENOMEM;
    }

   lt9211c_client = client; 

    if(!i2c_check_functionality(lt9211c_client->adapter, I2C_FUNC_I2C)) {  
        pr_err( "Lontium Failed check I2C functionality\n");
        return -ENODEV;
    }
 
    lt9211c_pdata = devm_kzalloc(&lt9211c_client->dev,
                 sizeof(struct LT9211C), GFP_KERNEL);
    if (!lt9211c_pdata) {
        pr_err( "Lontium Failed alloc LT9211C memory\n");
        return -ENOMEM;
    }

    lt9211c_pdata->i2c_addr = lt9211c_client->addr;
    lt9211c_pdata->i2c_client = lt9211c_client;
    lt9211c_pdata->dev = &lt9211c_client->dev;
    pr_info( "Lontium I2C address is 0x%x\n",lt9211c_pdata->i2c_addr);

    ret = LT9211C_parse_dt(&lt9211c_client->dev);
    if (ret) {
        pr_err( "Lontium Failed to parse device tree\n");
        goto err_dt_parse;
    }

    i2c_set_clientdata(lt9211c_client, lt9211c_pdata);

    ret = LT9211C_gpio_configure(LONTIUM_TRUE);
    if (ret) {
        pr_err( "Lontium Failed to configure GPIOs\n");
    }

    //lt9211c_config();
    
    //INIT_DELAYED_WORK(&test_delayed_work,delayed_func_for_lt9211c_state_check);
    //schedule_delayed_work(&test_delayed_work,1*HZ);

    return ret;

//err_i2c_prog:
//    LT9211C_gpio_configure(LONTIUM_FALSE);
err_dt_parse:
    devm_kfree(&lt9211c_client->dev, lt9211c_pdata);

    return ret;
}

static int LT9211C_remove(struct i2c_client *client)
{
    struct LT9211C *LT9211C = i2c_get_clientdata(client);

    if(gpio_is_valid(LT9211C->reset_gpio))
        gpio_free(LT9211C->reset_gpio);

    pr_info( "gpio for LT9211C driver release");
    i2c_set_clientdata(client, NULL);

    devm_kfree(&client->dev, LT9211C);

    pr_info( "remove LT9211C driver successfully\n");

    return 0;
}

static struct i2c_device_id LT9211C_id[] = {
    { "LT9211C", 0 },
    {}
};

static const struct of_device_id LT9211C_match_table[] = {
    { .compatible = "loutium, LT9211C" },
    {}
};

MODULE_DEVICE_TABLE(of, LT9211C_match_table);

static struct i2c_driver LT9211C_driver = {
    .driver         = {
        .name       = "LT9211C",
        .owner      = THIS_MODULE,
        .of_match_table = LT9211C_match_table,
    },
    .probe          = LT9211C_probe,
    .remove         = LT9211C_remove,
    .id_table       = LT9211C_id,
};

static int __init LT9211C_init(void)
{
    int ret = 0;
    pr_info("Lontium LT9211C driver installing....\n");
    ret = i2c_add_driver(&LT9211C_driver);
    return ret;
}

subsys_initcall(LT9211C_init);

static void __exit LT9211C_exit(void)
{
    pr_info("Lontium LT9211C driver exited\n");
    i2c_del_driver(&LT9211C_driver);
}

module_init(LT9211C_init);
module_exit(LT9211C_exit);

MODULE_AUTHOR("zyfang@lontium.com");
MODULE_DESCRIPTION("Lontium bridge IC LT9211C that convert ttl/lvds/mipi to ttl/lvds/mipi)");
MODULE_LICENSE("GPL");
