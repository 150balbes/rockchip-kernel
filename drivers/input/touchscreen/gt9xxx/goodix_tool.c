/* 
 * Goodix GT9xx touchscreen driver
 * 
 * Copyright  (C)  2010 - 2014 Goodix. Ltd.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be a reference 
 * to you, when you are integrating the GOODiX's CTP IC into your system, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 * 
 * Version: 2.4
 * Release Date: 2014/11/28
 */


#include "gt9xx.h"
#include <linux/miscdevice.h>


#define DATA_LENGTH_UINT    512
#define CMD_HEAD_LENGTH     (sizeof(st_cmd_head) - sizeof(u8*))
static char procname[20] = {0};

#define UPDATE_FUNCTIONS

#ifdef UPDATE_FUNCTIONS
extern s32 gup_enter_update_mode(struct i2c_client *client);
extern void gup_leave_update_mode(void);
extern s32 gup_update_proc(void *dir);
#endif

extern void gtp_irq_disable(struct goodix_ts_data *);
extern void gtp_irq_enable(struct goodix_ts_data *);

#pragma pack(1)
typedef struct{
    u8  wr;         //write read flag£¬0:R  1:W  2:PID 3:
    u8  flag;       //0:no need flag/int 1: need flag  2:need int
    u8 flag_addr[2];  //flag address
    u8  flag_val;   //flag val
    u8  flag_relation;  //flag_val:flag 0:not equal 1:equal 2:> 3:<
    u16 circle;     //polling cycle 
    u8  times;      //plling times
    u8  retry;      //I2C retry times
    u16 delay;      //delay befor read or after write
    u16 data_len;   //data length
    u8  addr_len;   //address length
    u8  addr[2];    //address
    u8  res[3];     //reserved
    u8* data;       //data pointer
}st_cmd_head;
#pragma pack()
st_cmd_head cmd_head1, cmd_head2;

static struct i2c_client *gt_client = NULL;

#if HOTKNOT_ENABLE
extern s32 gup_load_calibration1(void);
extern s32 gup_load_calibration2(void);
extern s32 gup_recovery_calibration0(void);
extern s32 gup_load_calibration0(char *filepath);
#endif

static struct proc_dir_entry *goodix_proc_entry;

static ssize_t goodix_tool_read(struct file *, char __user *, size_t, loff_t *);
//static ssize_t goodix_tool_write(struct file *, const char __user *, size_t, loff_t *);
static const struct proc_ops tool_ops = {
    .proc_read = goodix_tool_read,
//    .proc_write = goodix_tool_write,
};

static s32 (*tool_i2c_read)(u8 *, u16);
static s32 (*tool_i2c_write)(u8 *, u16);

#if GTP_ESD_PROTECT
extern void gtp_esd_switch(struct i2c_client *, s32);
#endif


s32 DATA_LENGTH_GT9XX = 0;
s8 IC_TYPE_GT9XX[16] = "GT9XX";

#if HOTKNOT_BLOCK_RW
DECLARE_WAIT_QUEUE_HEAD(bp_waiter);
u8 got_hotknot_state = 0;
u8 got_hotknot_extra_state = 0;
u8 wait_hotknot_state = 0;
u8 force_wake_flag = 0;
#endif

#if HOTKNOT_ENABLE

#define HOTKNOTNAME  "hotknot"
u8 hotknot_enabled;

static int hotknot_open(struct inode *node, struct file *flip)
{
    GTP_DEBUG("Hotknot is enable.");
    hotknot_enabled = 1;
    return 0;
}

static int hotknot_release(struct inode *node, struct file *filp)
{
    GTP_DEBUG("Hotknot is disable.");
    hotknot_enabled = 0;
    return 0;  
}
//static ssize_t hotknot_write(struct file *, const char __user *, size_t , loff_t *);
static ssize_t hotknot_read(struct file *, char __user *, size_t, loff_t *);

static struct file_operations hotknot_fops =
{
	.open = hotknot_open,
	.release = hotknot_release,
	.read = hotknot_read,
//    .write = hotknot_write,
};

static struct miscdevice hotknot_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = HOTKNOTNAME,
    .fops = &hotknot_fops,
};
#endif

static void tool_set_proc_name(char * procname)
{
#if HOTKNOT_ENABLE
	sprintf(procname, HOTKNOTNAME);  
#else
    char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", 
        "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char date[20] = {0};
    char month[4] = {0};
    int i = 0, n_month = 1, n_day = 0, n_year = 0;
    
    sprintf(date, "%s", __DATE__);
    
    //GTP_DEBUG("compile date: %s", date);
    
    sscanf(date, "%s %d %d", month, &n_day, &n_year);
    
    for (i = 0; i < 12; ++i)
    {
        if (!memcmp(months[i], month, 3))
        {
            n_month = i+1;
            break;
        }
    }
    
    sprintf(procname, "gmnode%04d%02d%02d", n_year, n_month, n_day);    

#endif    
    //GTP_DEBUG("procname = %s", procname);
}


static s32 tool_i2c_read_no_extra(u8* buf, u16 len)
{
    s32 ret = -1;
    s32 i = 0;
    struct i2c_msg msgs[2];
    
    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = gt_client->addr;
    msgs[0].len   = cmd_head1.addr_len;
    msgs[0].buf   = &buf[0];
    
    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = gt_client->addr;
    msgs[1].len   = len;
    msgs[1].buf   = &buf[GTP_ADDR_LENGTH];

    for (i = 0; i < cmd_head1.retry; i++)
    {
        ret=i2c_transfer(gt_client->adapter, msgs, 2);
        if (ret > 0)
        {
            break;
        }
    }
    return ret;
}

static s32 tool_i2c_write_no_extra(u8* buf, u16 len)
{
    s32 ret = -1;
    s32 i = 0;
    struct i2c_msg msg;

    msg.flags = !I2C_M_RD;
    msg.addr  = gt_client->addr;
    msg.len   = len;
    msg.buf   = buf;

    for (i = 0; i < cmd_head1.retry; i++)
    {
        ret=i2c_transfer(gt_client->adapter, &msg, 1);
        if (ret > 0)
        {
            break;
        }
    }
    return ret;
}

static s32 tool_i2c_read_with_extra(u8* buf, u16 len)
{
    s32 ret = -1;
    u8 pre[2] = {0x0f, 0xff};
    u8 end[2] = {0x80, 0x00};

    tool_i2c_write_no_extra(pre, 2);
    ret = tool_i2c_read_no_extra(buf, len);
    tool_i2c_write_no_extra(end, 2);

    return ret;
}

static s32 tool_i2c_write_with_extra(u8* buf, u16 len)
{
    s32 ret = -1;
    u8 pre[2] = {0x0f, 0xff};
    u8 end[2] = {0x80, 0x00};

    tool_i2c_write_no_extra(pre, 2);
    ret = tool_i2c_write_no_extra(buf, len);
    tool_i2c_write_no_extra(end, 2);

    return ret;
}

static void register_i2c_func(void)
{
//    if (!strncmp(IC_TYPE_GT9XX, "GT818", 5) || !strncmp(IC_TYPE_GT9XX, "GT816", 5) 
//        || !strncmp(IC_TYPE_GT9XX, "GT811", 5) || !strncmp(IC_TYPE_GT9XX, "GT818F", 6) 
//        || !strncmp(IC_TYPE_GT9XX, "GT827", 5) || !strncmp(IC_TYPE_GT9XX,"GT828", 5)
//        || !strncmp(IC_TYPE_GT9XX, "GT813", 5))
    if (strncmp(IC_TYPE_GT9XX, "GT8110", 6) && strncmp(IC_TYPE_GT9XX, "GT8105", 6)
        && strncmp(IC_TYPE_GT9XX, "GT801", 5) && strncmp(IC_TYPE_GT9XX, "GT800", 5)
        && strncmp(IC_TYPE_GT9XX, "GT801PLUS", 9) && strncmp(IC_TYPE_GT9XX, "GT811", 5)
        && strncmp(IC_TYPE_GT9XX, "GTxxx", 5) && strncmp(IC_TYPE_GT9XX, "GT9XX", 5))
    {
        tool_i2c_read = tool_i2c_read_with_extra;
        tool_i2c_write = tool_i2c_write_with_extra;
        GTP_DEBUG("I2C function: with pre and end cmd!");
    }
    else
    {
        tool_i2c_read = tool_i2c_read_no_extra;
        tool_i2c_write = tool_i2c_write_no_extra;
        GTP_INFO("I2C function: without pre and end cmd!");
    }
}

static void unregister_i2c_func(void)
{
    tool_i2c_read = NULL;
    tool_i2c_write = NULL;
    GTP_INFO("I2C function: unregister i2c transfer function!");
}

s32 init_wr_node(struct i2c_client *client)
{
    s32 i;

    gt_client = client;
    memset(&cmd_head1, 0, sizeof(cmd_head1));
	memset(&cmd_head2, 0, sizeof(cmd_head2));
    cmd_head1.data = NULL;
	cmd_head2.data = NULL;

    i = 5;
    while ((!cmd_head1.data) && i){
        cmd_head1.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
        if (NULL != cmd_head1.data) {
            break;
        }
        i--;
    }
    if (i) {
        DATA_LENGTH_GT9XX = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
        GTP_INFO("Applied memory size:%d.", DATA_LENGTH_GT9XX);
    } else {
        GTP_ERROR("Apply for memory failed.");
        return FAIL;
    }

    i = 5;
    while ((!cmd_head2.data) && i){
        cmd_head2.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
        if (NULL != cmd_head2.data) {
            break;
        }
        i--;
    }
    if (i) {
        DATA_LENGTH_GT9XX = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
        GTP_INFO("Applied memory size:%d.", DATA_LENGTH_GT9XX);
    } else {
        GTP_ERROR("Apply for memory failed.");
        return FAIL;
    }
	
    cmd_head1.addr_len = 2;
    cmd_head1.retry = 5;
    cmd_head2.addr_len = 2;
    cmd_head2.retry = 5;

	

    register_i2c_func();

#if HOTKNOT_ENABLE
	if (misc_register(&hotknot_misc_device))
    {
        GTP_ERROR("hotknot_device register failed!");
        return FAIL;
    }
#endif

    tool_set_proc_name(procname);
    goodix_proc_entry = proc_create(procname, 0666, NULL, &tool_ops);
    if (goodix_proc_entry == NULL)
    {
        GTP_ERROR("Couldn't create proc entry!");
        return FAIL;
    }
    else
    {
        GTP_INFO("Create proc entry success!");
    }
/*
	if (misc_register(&hotknot_misc_device))
	{
		  printk("mtk_tpd: hotknot_device register failed\n");
		  return FAIL;
	}
*/
    return SUCCESS;
}

void uninit_wr_node(void)
{
    kfree(cmd_head1.data);
    cmd_head1.data = NULL;
    unregister_i2c_func();
	kfree(cmd_head2.data);
	cmd_head2.data = NULL;
#if HOTKNOT_ENABLE
	misc_deregister(&hotknot_misc_device);
#endif
    remove_proc_entry(procname, NULL);
}

static u8 relation(u8 src, u8 dst, u8 rlt)
{
    u8 ret = 0;
    
    switch (rlt)
    {
    case 0:
        ret = (src != dst) ? true : false;
        break;

    case 1:
        ret = (src == dst) ? true : false;
        GTP_DEBUG("equal:src:0x%02x   dst:0x%02x   ret:%d.", src, dst, (s32)ret);
        break;

    case 2:
        ret = (src > dst) ? true : false;
        break;

    case 3:
        ret = (src < dst) ? true : false;
        break;

    case 4:
        ret = (src & dst) ? true : false;
        break;

    case 5:
        ret = (!(src | dst)) ? true : false;
        break;

    default:
        ret = false;
        break;    
    }

    return ret;
}

/*******************************************************    
Function:
    Comfirm function.
Input:
  None.
Output:
    Return write length.
********************************************************/
static u8 comfirm(void)
{
    s32 i = 0;
    u8 buf[32];
    
//    memcpy(&buf[GTP_ADDR_LENGTH - cmd_head1.addr_len], &cmd_head1.flag_addr, cmd_head1.addr_len);
//    memcpy(buf, &cmd_head1.flag_addr, cmd_head1.addr_len);//Modified by Scott, 2012-02-17
    memcpy(buf, cmd_head1.flag_addr, cmd_head1.addr_len);
   
    for (i = 0; i < cmd_head1.times; i++)
    {
        if (tool_i2c_read(buf, 1) <= 0)
        {
            GTP_ERROR("Read flag data failed!");
            return FAIL;
        }
        if (true == relation(buf[GTP_ADDR_LENGTH], cmd_head1.flag_val, cmd_head1.flag_relation))
        {
            GTP_DEBUG("value at flag addr:0x%02x.", buf[GTP_ADDR_LENGTH]);
            GTP_DEBUG("flag value:0x%02x.", cmd_head1.flag_val);
            break;
        }

        msleep(cmd_head1.circle);
    }

    if (i >= cmd_head1.times)
    {
        GTP_ERROR("Didn't get the flag to continue!");
        return FAIL;
    }

    return SUCCESS;
}

/*******************************************************    
Function:
    Goodix tool write function.
Input:
  standard proc write function param.
Output:
    Return write length.
********************************************************/
#if 0
ssize_t goodix_tool_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
    s32 ret = 0;
    struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
    
    GTP_DEBUG_FUNC();
    GTP_DEBUG_ARRAY((u8*)buff, len);
    
    if(ts->gtp_is_suspend == 1||gtp_resetting == 1)
    {
        //GTP_ERROR("[Write]tpd_halt =1 fail!");
    return FAIL;
    }
    ret = copy_from_user(&cmd_head1, buff, CMD_HEAD_LENGTH);
    if(ret)
    {
        GTP_ERROR("copy_from_user failed.");
        return -EPERM;
    }

    /*
    GTP_DEBUG("[Operation]wr: %02X", cmd_head1.wr);
    GTP_DEBUG("[Flag]flag: %02X, addr: %02X%02X, value: %02X, relation: %02X", cmd_head1.flag, cmd_head1.flag_addr[0], 
                        cmd_head1.flag_addr[1], cmd_head1.flag_val, cmd_head1.flag_relation);
    GTP_DEBUG("[Retry]circle: %d, times: %d, retry: %d, delay: %d", (s32)cmd_head1.circle, (s32)cmd_head1.times, 
                        (s32)cmd_head1.retry, (s32)cmd_head1.delay);
    GTP_DEBUG("[Data]data len: %d, addr len: %d, addr: %02X%02X, buffer len: %d, data[0]: %02X", (s32)cmd_head1.data_len, 
                        (s32)cmd_head1.addr_len, cmd_head1.addr[0], cmd_head1.addr[1], (s32)len, buff[CMD_HEAD_LENGTH]);
    */                    
    if (1 == cmd_head1.wr)
    {
        ret = copy_from_user(&cmd_head1.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);
        if(ret)
        {
            GTP_ERROR("copy_from_user failed.");
            return -EPERM;
        }
        memcpy(&cmd_head1.data[GTP_ADDR_LENGTH - cmd_head1.addr_len], cmd_head1.addr, cmd_head1.addr_len);

        GTP_DEBUG_ARRAY(cmd_head1.data, cmd_head1.data_len + cmd_head1.addr_len);
        GTP_DEBUG_ARRAY((u8*)&buff[CMD_HEAD_LENGTH], cmd_head1.data_len);

        if (1 == cmd_head1.flag)
        {
            if (FAIL == comfirm())
            {
                GTP_ERROR("[WRITE]Comfirm fail!");
                return -EPERM;
            }
        }
        else if (2 == cmd_head1.flag)
        {
            //Need interrupt!
        }
        if (tool_i2c_write(&cmd_head1.data[GTP_ADDR_LENGTH - cmd_head1.addr_len],
            cmd_head1.data_len + cmd_head1.addr_len) <= 0)
        {
            GTP_ERROR("[WRITE]Write data failed!");
            return -EPERM;
        }

        GTP_DEBUG_ARRAY(&cmd_head1.data[GTP_ADDR_LENGTH - cmd_head1.addr_len],cmd_head1.data_len + cmd_head1.addr_len);
        if (cmd_head1.delay)
        {
            msleep(cmd_head1.delay);
        }
    }
    else if (3 == cmd_head1.wr)  //Write ic type
    {
        ret = copy_from_user(&cmd_head1.data[0], &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);
        if(ret)
        {
            GTP_ERROR("copy_from_user failed.");
            return -EPERM;
        }
        memcpy(IC_TYPE_GT9XX, cmd_head1.data, cmd_head1.data_len);

        register_i2c_func();
    }
    else if (5 == cmd_head1.wr)
    {
        //memcpy(IC_TYPE_GT9XX, cmd_head1.data, cmd_head1.data_len);
    }
    else if (7 == cmd_head1.wr)//disable irq!
    {
        gtp_irq_disable(i2c_get_clientdata(gt_client));
        
    #if GTP_ESD_PROTECT
        gtp_esd_switch(gt_client, SWITCH_OFF);
    #endif
    }
    else if (9 == cmd_head1.wr) //enable irq!
    {
        gtp_irq_enable(i2c_get_clientdata(gt_client));

    #if GTP_ESD_PROTECT
        gtp_esd_switch(gt_client, SWITCH_ON);
    #endif
    }
    else if(17 == cmd_head1.wr)
    {
        struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
        ret = copy_from_user(&cmd_head1.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);
        if(ret)
        {
            GTP_DEBUG("copy_from_user failed.");
            return -EPERM;
        }
        if(cmd_head1.data[GTP_ADDR_LENGTH])
        {
            GTP_INFO("gtp enter rawdiff.");
            ts->gtp_rawdiff_mode = true;
        }
        else
        {
            ts->gtp_rawdiff_mode = false;
            GTP_INFO("gtp leave rawdiff.");
        }
    }
#ifdef UPDATE_FUNCTIONS
    else if (11 == cmd_head1.wr)//Enter update mode!
    {
        if (FAIL == gup_enter_update_mode(gt_client))
        {
            return -EPERM;
        }
    }
    else if (13 == cmd_head1.wr)//Leave update mode!
    {
        gup_leave_update_mode();
    }
    else if (15 == cmd_head1.wr) //Update firmware!
    {
        show_len = 0;
        total_len = 0;
        memset(cmd_head1.data, 0, cmd_head1.data_len + 1);
        memcpy(cmd_head1.data, &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);

        if (FAIL == gup_update_proc((void*)cmd_head1.data))
        {
            return -EPERM;
        }
    }

#endif
#if HOTKNOT_ENABLE
    else if (19 == cmd_head1.wr)  //load subsystem
    {
        ret = copy_from_user(&cmd_head1.data[0], &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);
        if(0 == cmd_head1.data[0])
        {
            if (FAIL == gup_load_calibration1())
            {
                return FAIL;
            }
        }
        else if(1 == cmd_head1.data[0])
        {
            if (FAIL == gup_load_calibration2())
            {
                return FAIL;
            }       
        }
        else if(2 == cmd_head1.data[0])
        {
            if (FAIL == gup_recovery_calibration0())
            {
                return FAIL;
            }
        }
        else if(3 == cmd_head1.data[0])
        {
            if (FAIL == gup_load_calibration0(NULL))
            {
                return FAIL;
            }
        }
    }   
#if HOTKNOT_BLOCK_RW
    else if (21 == cmd_head1.wr)
    {
        u16 wait_hotknot_timeout = 0;
        u8  rqst_hotknot_state;
                
        ret = copy_from_user(&cmd_head1.data[GTP_ADDR_LENGTH], 
            &buff[CMD_HEAD_LENGTH], cmd_head1.data_len);

        if (ret)
        {
            GTP_ERROR("copy_from_user failed.");
        }
        
        rqst_hotknot_state = cmd_head1.data[GTP_ADDR_LENGTH];
        wait_hotknot_state |= rqst_hotknot_state;
        wait_hotknot_timeout = (cmd_head1.data[GTP_ADDR_LENGTH + 1]<<8) + 
            cmd_head1.data[GTP_ADDR_LENGTH + 2];
        GTP_DEBUG("Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
            rqst_hotknot_state, wait_hotknot_timeout, wait_hotknot_state);
        got_hotknot_state &= (~rqst_hotknot_state);
        //got_hotknot_extra_state = 0;
        switch(rqst_hotknot_state)
        {
            set_current_state(TASK_INTERRUPTIBLE);
            case HN_DEVICE_PAIRED:
                hotknot_paired_flag = 0;
                wait_event_interruptible(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state));
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state != (got_hotknot_state&rqst_hotknot_state))
                {
                    GTP_ERROR("Wait 0x%x block polling waiter failed.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            case HN_MASTER_SEND:
            case HN_SLAVE_RECEIVED:
                wait_event_interruptible_timeout(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state),
                    wait_hotknot_timeout);
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state))
                {
                    return got_hotknot_extra_state;
                }
                else
                {
                    GTP_ERROR("Wait 0x%x block polling waiter timeout.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            case HN_MASTER_DEPARTED:
            case HN_SLAVE_DEPARTED:
                wait_event_interruptible_timeout(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state),
                    wait_hotknot_timeout);
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state != (got_hotknot_state&rqst_hotknot_state))
                {
                    GTP_ERROR("Wait 0x%x block polling waitor timeout.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            default:
                GTP_ERROR("Invalid rqst_hotknot_state in goodix_tool.");
            break;
        }
        force_wake_flag = 0;
    }
    else if(23 == cmd_head1.wr)
    {
        GTP_DEBUG("Manual wakeup all block polling waiter!");
        got_hotknot_state = 0;
        wait_hotknot_state = 0;
        force_wake_flag = 1;
        hotknot_paired_flag = 0;
        wake_up_interruptible(&bp_waiter);
    }
#endif
#endif
    return len;
}
#endif

/*******************************************************    
Function:
    Goodix tool read function.
Input:
  standard proc read function param.
Output:
    Return read length.
********************************************************/
ssize_t goodix_tool_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    s32 ret = 0;
    struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
        
    GTP_DEBUG_FUNC();

    if(ts->gtp_is_suspend == 1||gtp_resetting == 1)
    {
        //GTP_ERROR("[READ]tpd_halt =1 fail!");
        return FAIL;
    }
    if (*ppos)      // ADB call again
    {
        //GTP_DEBUG("[HEAD]wr: %d", cmd_head1.wr);
        //GTP_DEBUG("[PARAM]size: %d, *ppos: %d", size, (int)*ppos);
        //GTP_DEBUG("[TOOL_READ]ADB call again, return it.");
		*ppos = 0;
        return 0;
    }
    
    if (cmd_head1.wr % 2)
    {
        return -EPERM;
    }
    else if (!cmd_head1.wr)
    {
        u16 len = 0;
        s16 data_len = 0;
        u16 loc = 0;
        
        if (1 == cmd_head1.flag)
        {
            if (FAIL == comfirm())
            {
                GTP_ERROR("[READ]Comfirm fail!");
                return -EPERM;
            }
        }
        else if (2 == cmd_head1.flag)
        {
            //Need interrupt!
        }

        memcpy(cmd_head1.data, cmd_head1.addr, cmd_head1.addr_len);
/*
        GTP_DEBUG("[CMD HEAD DATA] ADDR:0x%02x%02x.", cmd_head1.data[0], cmd_head1.data[1]);
        GTP_DEBUG("[CMD HEAD ADDR] ADDR:0x%02x%02x.", cmd_head1.addr[0], cmd_head1.addr[1]);
    */    
        if (cmd_head1.delay)
        {
            msleep(cmd_head1.delay);
        }
        
        data_len = cmd_head1.data_len;

        while(data_len > 0)
        {
            if (data_len > DATA_LENGTH_GT9XX)
            {
                len = DATA_LENGTH_GT9XX;
            }
            else
            {
                len = data_len;
            }
            data_len -= len;

            if (tool_i2c_read(cmd_head1.data, len) <= 0)
            {
                GTP_ERROR("[READ]Read data failed!");
                return -EPERM;
            }

            //memcpy(&page[loc], &cmd_head1.data[GTP_ADDR_LENGTH], len);
            ret = simple_read_from_buffer(&page[loc], size, ppos, &cmd_head1.data[GTP_ADDR_LENGTH], len);
            if (ret < 0)
            {
                return ret;
            }
            loc += len;

            GTP_DEBUG_ARRAY(&cmd_head1.data[GTP_ADDR_LENGTH], len);
            GTP_DEBUG_ARRAY(page, len);
        }
        return cmd_head1.data_len; 
    }
    else if (2 == cmd_head1.wr)
    {
        ret = simple_read_from_buffer(page, size, ppos, IC_TYPE_GT9XX, sizeof(IC_TYPE_GT9XX));
        return ret;
    }
    else if (4 == cmd_head1.wr)
    {
        u8 progress_buf[4];
        progress_buf[0] = show_len >> 8;
        progress_buf[1] = show_len & 0xff;
        progress_buf[2] = total_len >> 8;
        progress_buf[3] = total_len & 0xff;
        
        ret = simple_read_from_buffer(page, size, ppos, progress_buf, 4);
        return ret;
    }
    else if (6 == cmd_head1.wr)
    {
        //Read error code!
    }
    else if (8 == cmd_head1.wr)  //Read driver version
    {
        ret = simple_read_from_buffer(page, size, ppos, GTP_DRIVER_VERSION, strlen(GTP_DRIVER_VERSION));
        return ret;
    }
    return -EPERM;
}
#if 1
/*******************************************************    
Function:
    Goodix tool write function.
Input:
  standard proc write function param.
Output:
    Return write length.
********************************************************/
#if 0 
static ssize_t hotknot_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
    s32 ret = 0;
    struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
    
    GTP_DEBUG_FUNC();
    GTP_DEBUG_ARRAY((u8*)buff, len);
    
    if(ts->gtp_is_suspend == 1||gtp_resetting == 1)
    {
        //GTP_ERROR("[Write]tpd_halt =1 fail!");
    return FAIL;
    }
    ret = copy_from_user(&cmd_head2, buff, CMD_HEAD_LENGTH);
    if(ret)
    {
        GTP_ERROR("copy_from_user failed.");
        return -EPERM;
    }

    /*
    GTP_DEBUG("[Operation]wr: %02X", cmd_head1.wr);
    GTP_DEBUG("[Flag]flag: %02X, addr: %02X%02X, value: %02X, relation: %02X", cmd_head1.flag, cmd_head1.flag_addr[0], 
                        cmd_head1.flag_addr[1], cmd_head1.flag_val, cmd_head1.flag_relation);
    GTP_DEBUG("[Retry]circle: %d, times: %d, retry: %d, delay: %d", (s32)cmd_head1.circle, (s32)cmd_head1.times, 
                        (s32)cmd_head1.retry, (s32)cmd_head1.delay);
    GTP_DEBUG("[Data]data len: %d, addr len: %d, addr: %02X%02X, buffer len: %d, data[0]: %02X", (s32)cmd_head1.data_len, 
                        (s32)cmd_head1.addr_len, cmd_head1.addr[0], cmd_head1.addr[1], (s32)len, buff[CMD_HEAD_LENGTH]);
    */                    
    if (1 == cmd_head2.wr)
    {
        ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);
        if(ret)
        {
            GTP_ERROR("copy_from_user failed.");
            return -EPERM;
        }
        memcpy(&cmd_head2.data[GTP_ADDR_LENGTH - cmd_head2.addr_len], cmd_head2.addr, cmd_head2.addr_len);

        GTP_DEBUG_ARRAY(cmd_head2.data, cmd_head2.data_len + cmd_head2.addr_len);
        GTP_DEBUG_ARRAY((u8*)&buff[CMD_HEAD_LENGTH], cmd_head2.data_len);

        if (1 == cmd_head2.flag)
        {
            if (FAIL == comfirm())
            {
                GTP_ERROR("[WRITE]Comfirm fail!");
                return -EPERM;
            }
        }
        else if (2 == cmd_head2.flag)
        {
            //Need interrupt!
        }
        if (tool_i2c_write(&cmd_head2.data[GTP_ADDR_LENGTH - cmd_head2.addr_len],
            cmd_head2.data_len + cmd_head2.addr_len) <= 0)
        {
            GTP_ERROR("[WRITE]Write data failed!");
            return -EPERM;
        }

        GTP_DEBUG_ARRAY(&cmd_head2.data[GTP_ADDR_LENGTH - cmd_head2.addr_len],cmd_head2.data_len + cmd_head2.addr_len);
        if (cmd_head2.delay)
        {
            msleep(cmd_head2.delay);
        }
    }
    else if (3 == cmd_head2.wr)  //Write ic type
    {
        ret = copy_from_user(&cmd_head2.data[0], &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);
        if(ret)
        {
            GTP_ERROR("copy_from_user failed.");
            return -EPERM;
        }
        memcpy(IC_TYPE_GT9XX, cmd_head2.data, cmd_head2.data_len);

        register_i2c_func();
    }
    else if (5 == cmd_head2.wr)
    {
        //memcpy(IC_TYPE_GT9XX, cmd_head.data, cmd_head.data_len);
    }
    else if (7 == cmd_head2.wr)//disable irq!
    {
        gtp_irq_disable(i2c_get_clientdata(gt_client));
        
    #if GTP_ESD_PROTECT
        gtp_esd_switch(gt_client, SWITCH_OFF);
    #endif
    }
    else if (9 == cmd_head1.wr) //enable irq!
    {
        gtp_irq_enable(i2c_get_clientdata(gt_client));

    #if GTP_ESD_PROTECT
        gtp_esd_switch(gt_client, SWITCH_ON);
    #endif
    }
    else if(17 == cmd_head2.wr)
    {
        struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
        ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);
        if(ret)
        {
            GTP_DEBUG("copy_from_user failed.");
            return -EPERM;
        }
        if(cmd_head2.data[GTP_ADDR_LENGTH])
        {
            GTP_INFO("gtp enter rawdiff.");
            ts->gtp_rawdiff_mode = true;
        }
        else
        {
            ts->gtp_rawdiff_mode = false;
            GTP_INFO("gtp leave rawdiff.");
        }
    }
#ifdef UPDATE_FUNCTIONS
    else if (11 == cmd_head2.wr)//Enter update mode!
    {
        if (FAIL == gup_enter_update_mode(gt_client))
        {
            return -EPERM;
        }
    }
    else if (13 == cmd_head2.wr)//Leave update mode!
    {
        gup_leave_update_mode();
    }
    else if (15 == cmd_head2.wr) //Update firmware!
    {
        show_len = 0;
        total_len = 0;
        memset(cmd_head2.data, 0, cmd_head2.data_len + 1);
        memcpy(cmd_head2.data, &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);

        if (FAIL == gup_update_proc((void*)cmd_head2.data))
        {
            return -EPERM;
        }
    }

#endif
#if HOTKNOT_ENABLE
    else if (19 == cmd_head2.wr)  //load subsystem
    {
        ret = copy_from_user(&cmd_head2.data[0], &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);
        if(0 == cmd_head2.data[0])
        {
            if (FAIL == gup_load_calibration1())
            {
                return FAIL;
            }
        }
        else if(1 == cmd_head2.data[0])
        {
            if (FAIL == gup_load_calibration2())
            {
                return FAIL;
            }       
        }
        else if(2 == cmd_head2.data[0])
        {
            if (FAIL == gup_recovery_calibration0())
            {
                return FAIL;
            }
        }
        else if(3 == cmd_head2.data[0])
        {
            if (FAIL == gup_load_calibration0(NULL))
            {
                return FAIL;
            }
        }
    }   
#if HOTKNOT_BLOCK_RW
    else if (21 == cmd_head2.wr)
    {
        u16 wait_hotknot_timeout = 0;
        u8  rqst_hotknot_state;
                
        ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH], 
            &buff[CMD_HEAD_LENGTH], cmd_head2.data_len);

        if (ret)
        {
            GTP_ERROR("copy_from_user failed.");
        }
        
        rqst_hotknot_state = cmd_head2.data[GTP_ADDR_LENGTH];
        wait_hotknot_state |= rqst_hotknot_state;
        wait_hotknot_timeout = (cmd_head2.data[GTP_ADDR_LENGTH + 1]<<8) + 
            cmd_head2.data[GTP_ADDR_LENGTH + 2];
        GTP_DEBUG("Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
            rqst_hotknot_state, wait_hotknot_timeout, wait_hotknot_state);
        got_hotknot_state &= (~rqst_hotknot_state);
        //got_hotknot_extra_state = 0;
        switch(rqst_hotknot_state)
        {
            set_current_state(TASK_INTERRUPTIBLE);
            case HN_DEVICE_PAIRED:
                hotknot_paired_flag = 0;
                wait_event_interruptible(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state));
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state != (got_hotknot_state&rqst_hotknot_state))
                {
                    GTP_ERROR("Wait 0x%x block polling waiter failed.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            case HN_MASTER_SEND:
            case HN_SLAVE_RECEIVED:
                wait_event_interruptible_timeout(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state),
                    wait_hotknot_timeout);
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state))
                {
                    return got_hotknot_extra_state;
                }
                else
                {
                    GTP_ERROR("Wait 0x%x block polling waiter timeout.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            case HN_MASTER_DEPARTED:
            case HN_SLAVE_DEPARTED:
                wait_event_interruptible_timeout(bp_waiter, force_wake_flag || 
                    rqst_hotknot_state == (got_hotknot_state&rqst_hotknot_state),
                    wait_hotknot_timeout);
                wait_hotknot_state &= (~rqst_hotknot_state);
                if(rqst_hotknot_state != (got_hotknot_state&rqst_hotknot_state))
                {
                    GTP_ERROR("Wait 0x%x block polling waitor timeout.", rqst_hotknot_state);
                    force_wake_flag = 0;
                    return FAIL;
                }
            break;
            default:
                GTP_ERROR("Invalid rqst_hotknot_state in goodix_tool.");
            break;
        }
        force_wake_flag = 0;
    }
    else if(23 == cmd_head2.wr)
    {
        GTP_DEBUG("Manual wakeup all block polling waiter!");
        got_hotknot_state = 0;
        wait_hotknot_state = 0;
        force_wake_flag = 1;
        hotknot_paired_flag = 0;
        wake_up_interruptible(&bp_waiter);
    }
#endif
#endif
    return len;
}
#endif

/*******************************************************    
Function:
    Goodix tool read function.
Input:
  standard proc read function param.
Output:
    Return read length.
********************************************************/
static ssize_t _hotknot_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    s32 ret = 0;
    struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
        
    GTP_DEBUG_FUNC();

    if(ts->gtp_is_suspend == 1||gtp_resetting == 1)
    {
        //GTP_ERROR("[READ]tpd_halt =1 fail!");
        return FAIL;
    }
    if (*ppos)      // ADB call again
    {
        //GTP_DEBUG("[HEAD]wr: %d", cmd_head1.wr);
        //GTP_DEBUG("[PARAM]size: %d, *ppos: %d", size, (int)*ppos);
        //GTP_DEBUG("[TOOL_READ]ADB call again, return it.");
		*ppos = 0;
        return 0;
    }
    
    if (cmd_head2.wr % 2)
    {
        return -EPERM;
    }
    else if (!cmd_head2.wr)
    {
        u16 len = 0;
        s16 data_len = 0;
        u16 loc = 0;
        
        if (1 == cmd_head2.flag)
        {
            if (FAIL == comfirm())
            {
                GTP_ERROR("[READ]Comfirm fail!");
                return -EPERM;
            }
        }
        else if (2 == cmd_head2.flag)
        {
            //Need interrupt!
        }

        memcpy(cmd_head2.data, cmd_head2.addr, cmd_head2.addr_len);
/*
        GTP_DEBUG("[CMD HEAD DATA] ADDR:0x%02x%02x.", cmd_head.data[0], cmd_head.data[1]);
        GTP_DEBUG("[CMD HEAD ADDR] ADDR:0x%02x%02x.", cmd_head.addr[0], cmd_head.addr[1]);
    */    
        if (cmd_head2.delay)
        {
            msleep(cmd_head2.delay);
        }
        
        data_len = cmd_head2.data_len;

        while(data_len > 0)
        {
            if (data_len > DATA_LENGTH_GT9XX)
            {
                len = DATA_LENGTH_GT9XX;
            }
            else
            {
                len = data_len;
            }
            data_len -= len;

            if (tool_i2c_read(cmd_head2.data, len) <= 0)
            {
                GTP_ERROR("[READ]Read data failed!");
                return -EPERM;
            }

            //memcpy(&page[loc], &cmd_head1.data[GTP_ADDR_LENGTH], len);
            ret = simple_read_from_buffer(&page[loc], size, ppos, &cmd_head2.data[GTP_ADDR_LENGTH], len);
            if (ret < 0)
            {
                return ret;
            }
            loc += len;

            GTP_DEBUG_ARRAY(&cmd_head2.data[GTP_ADDR_LENGTH], len);
            GTP_DEBUG_ARRAY(page, len);
        }
        return cmd_head2.data_len; 
    }
    else if (2 == cmd_head2.wr)
    {
        ret = simple_read_from_buffer(page, size, ppos, IC_TYPE_GT9XX, sizeof(IC_TYPE_GT9XX));
        return ret;
    }
    else if (4 == cmd_head2.wr)
    {
        u8 progress_buf[4];
        progress_buf[0] = show_len >> 8;
        progress_buf[1] = show_len & 0xff;
        progress_buf[2] = total_len >> 8;
        progress_buf[3] = total_len & 0xff;
        
        ret = simple_read_from_buffer(page, size, ppos, progress_buf, 4);
        return ret;
    }
    else if (6 == cmd_head2.wr)
    {
        //Read error code!
    }
    else if (8 == cmd_head2.wr)  //Read driver version
    {
        ret = simple_read_from_buffer(page, size, ppos, GTP_DRIVER_VERSION, strlen(GTP_DRIVER_VERSION));
        return ret;
    }
    return -EPERM;
}

static ssize_t hotknot_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	*ppos = 0;
	return _hotknot_read(file, page, size, ppos);
}
#endif
