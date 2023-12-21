// SPDX-License-Identifier: GPL-2.0
/*
 * xc7160 driver
 * V0.0X01.0X02
 * 1. fix compile error in kernel5.10
 * V0.0X01.0X03
 * 1.add cmd to get channel info and ioctl
 * 2.adjust bus format to UYVY for hal3 to get media device without isp
 * V0.0X01.0X04
 * 1.v4l2_fwnode_endpoint_parse get resource failed, so ignore it
 * 2.adjust init reg to power on interface, fix up preview and recording failed. 
 *
 */


#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include "xc7160_regs.h"

//#define 		FIREFLY_DEBUG 0
#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x04)
#define XC7160_MEDIA_BUS_FMT MEDIA_BUS_FMT_UYVY8_2X8 //MEDIA_BUS_FMT_YUYV8_2X8

#define XC7160_REG_HIGH_SELECT 0xfffd
#define XC7160_REG_PAGE_SELECT  0xfffe

#define XC7160_CHIP_REG_ID1_ZYK		0xfffb
#define XC7160_CHIP_REG_ID2_ZYK		0xfffc

#define XC7160_CHIP_ID1_ZYK			0x71
#define XC7160_CHIP_ID2_ZYK			0x60

#define SC8238_CHIP_REG_ID1_ZYK		0x3107
#define SC8238_CHIP_REG_ID2_ZYK		0x3108

#define SC8238_CHIP_ID1_ZYK			0x82
#define SC8238_CHIP_ID2_ZYK			0x35

#define XC7160_REG_VALUE_08BIT		1
#define XC7160_REG_VALUE_16BIT		2
#define XC7160_REG_VALUE_24BIT		3

static DEFINE_MUTEX(xc7160_power_mutex);

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define XC7160_NAME			"xc7160"
#define XC7160_LANES 			4

#define XC7160_LINK_FREQ_XXX_MHZ_L	504000000U
#define XC7160_LINK_FREQ_XXX_MHZ_H      576000000U

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define XC7160_PIXEL_RATE_LOW		336000000U // (XC7160_LINK_FREQ_XXX_MHZ_L *2 *4/12)
#define XC7160_PIXEL_RATE_HIGH		384000000U // (XC7160_LINK_FREQ_XXX_MHZ_H *2 *4/12)

#define XC7160_XVCLK_FREQ		24000000

#define SC8238_RETRY_STABLE_TIME 5

static const struct regval *xc7160_global_regs = isp_xc7160_1080p_30fps_2022617_regs;
static const struct regval *sc8238_global_regs = sensor_xc7160_1080p_30fps_2022617_regs;
static u32 clkout_enabled_index = 1;

static const char * const xc7160_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define XC7160_NUM_SUPPLIES ARRAY_SIZE(xc7160_supply_names)



struct xc7160_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	const struct regval *isp_reg_list;
	const struct regval *sensor_reg_list;
	u32 vc[PAD_MAX];
};

struct xc7160 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*mipi_pwr_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[XC7160_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl    *pixel_rate;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	bool            isp_out_colorbar;	//Mark whether the color bar should be output 
	bool			initial_status;  //Whether the isp has been initialized 
	const struct xc7160_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32 lane_data_num;
};


#define to_xc7160(sd) container_of(sd, struct xc7160, subdev)



static const struct xc7160_mode supported_modes[] = {
	 {
		.width = 1920,
		.height = 1080,
		.max_fps = {
				.numerator = 10000,
				.denominator = 300000,
		},
		.bus_fmt = XC7160_MEDIA_BUS_FMT,
		.isp_reg_list = isp_xc7160_1080p_30fps_2022617_regs,
		.sensor_reg_list = sensor_xc7160_1080p_30fps_2022617_regs,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.bus_fmt = XC7160_MEDIA_BUS_FMT,
		.isp_reg_list = isp_xc7160_4k_25fps_2022617_regs,
		.sensor_reg_list= sensor_xc7160_4k_25fps_2022617_regs,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},

		//driver setting
};

static const s64 link_freq_menu_items[] = {
	XC7160_LINK_FREQ_XXX_MHZ_H,
	XC7160_LINK_FREQ_XXX_MHZ_L,
};

static const char * const xc7160_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int xc7160_write_reg(struct i2c_client *client, u16 reg,
			     u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];


	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int xc7160_write_array(struct i2c_client * client,
			       const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;
	u8 value = 0;
	struct xc7160* xc7160 = NULL;

	xc7160 = to_xc7160(dev_get_drvdata(&client->dev));

	while(true) {
		if (regs[i].addr == REG_NULL) {
			if (regs[i].val == REG_DL) 
				msleep(5);
			else
				break;
	
		}
		value = regs[i].val;
		if(regs[i].addr == 0x2023 && xc7160 != NULL){
			if(xc7160->lane_data_num == 2 )
				value = 0x03; //isp mipi data uses 2 lanes
			else
				value = 0x0f; //isp mipi data uses 4 lanes
		}
		ret = xc7160_write_reg(client, regs[i].addr, XC7160_REG_VALUE_08BIT, value);
		if(ret){
			dev_err(&client->dev,"%s: write xc7160 array reg 0x%02x failed\n",__func__,regs[i].addr);
			return ret;
		}
		i++;
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int xc7160_read_reg(struct i2c_client *client, u16 reg,
			    unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];


	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}


static int sc8238_write_reg(struct i2c_client *client, u16 reg,
			     u32 len, u32 val)
{
	struct i2c_msg msg;
	u8 sc8238_buf[4];
	int ret;
	u32 buf_i, val_i;
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	sc8238_buf[0]= reg >> 8;
	sc8238_buf[1]= reg & 0xff;

	while (val_i < 4)
		sc8238_buf[buf_i++] = val_p[val_i++];

	/* Write register address */
	msg.addr = 0x30;
	msg.flags = 0;
	msg.len =3;
	msg.buf = sc8238_buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1){
		dev_err(&client->dev,"%s: xc7160_8238 i2c transfer failed\n",__func__);
		return -EIO;
	}

	return 0;
}

static int sc8238_read_reg(struct i2c_client *client, u16 reg,
			    unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = 0x30;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = 0x30;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int sc8238_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i,j;
	u32 value;
	struct device* dev = &client->dev;
	int ret = 0;

	value =0;
	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++){
		for(j = 0 ; j < SC8238_RETRY_STABLE_TIME ; j++){
			ret = sc8238_write_reg(client, regs[i].addr,
					XC7160_REG_VALUE_08BIT,
					regs[i].val);

			if(ret && j == SC8238_RETRY_STABLE_TIME){
				dev_err(dev,"xc7160_8238 write sc8238 regs array get failed, 0x%02x\n",regs[i].addr);
				return ret;
			}else if(ret){
				dev_err(dev,"xc7160_8238 write sc8238 regs array retry 0x%02x, %d\n",regs[i].addr,j+1);
				msleep(1);
				continue;	
			}else
				break;
		}
#ifdef 		FIREFLY_DEBUG
		sc8238_read_reg(client,regs[i].addr,XC7160_REG_VALUE_08BIT, &value);
		if(regs[i].val != value){
			dev_info(dev,"firefly->debug: xc7160_8238 reg0x%02x write and read are different\n",regs[i].addr);
		}
#endif
	}
	return ret;
}


static int sc8238_check_sensor_id(struct xc7160 *xc7160,
				   struct i2c_client *client)
{
	struct device *dev = &xc7160->client->dev;
	u32 id_H = 0,id_L=0;
	int ret,i=0;

	ret = xc7160_write_array(xc7160->client, xc7160_i2c_bypass_on_regs);
	if (ret) {
		dev_err(dev, "%s: could not set bypass on registers\n",__func__);
		return ret;
	}

	do{
			i++;
			ret = sc8238_read_reg(client, SC8238_CHIP_REG_ID1_ZYK	,
						XC7160_REG_VALUE_08BIT, &id_H);
			if (id_H == SC8238_CHIP_ID1_ZYK && !ret ) {
				//dev_info(dev, "sensor chip is SC8238, id_H is 0x%02x\n",id);
				ret = sc8238_read_reg(client, SC8238_CHIP_REG_ID2_ZYK	,
					XC7160_REG_VALUE_08BIT, &id_L);
				if (id_L != SC8238_CHIP_ID2_ZYK || ret) {
					dev_err(dev, "Unexpected SC8238_CHIP_ID_REG2, id(%06x), ret(%d), but SC8238_CHIP_ID_REG1 success\n", id_L, ret);
				}else{
					xc7160->isp_out_colorbar = false;
					dev_info(dev, "sensor chip is SC8238\n");
					break;
				}			
			}else
				dev_err(dev, "Unexpected sensor of SC8238_CHIP_ID_REG1, id(%06x), ret(%d)\n", id_H, ret);	
			
			if(i == SC8238_RETRY_STABLE_TIME){
				dev_err(dev, "cannot check sc8238 sensor id\n");
				xc7160->isp_out_colorbar = true;
				break;	
			}
			dev_info(dev, "check sensor of SC8238_CHIP_ID retry %d\n",i );	
			msleep(1);
	}while((id_H != SC8238_CHIP_ID1_ZYK)||(id_L != SC8238_CHIP_ID2_ZYK));


	ret = xc7160_write_array(client, xc7160_i2c_bypass_off_regs);
	if (ret) 
		dev_err(dev, "%s: could not set bypass off registers\n", __func__);

	return ret;
	
}

static int camera_isp_sensor_initial(struct xc7160 *xc7160)
{
	struct device *dev = &xc7160->client->dev;
	int ret,i=0;

	dev_info(dev,"xc7160 res wxh: %dx%d@%dfps\n",
				 xc7160->cur_mode->width,xc7160->cur_mode->height,(xc7160->cur_mode->max_fps.denominator/xc7160->cur_mode->max_fps.numerator));

	ret = xc7160_write_array(xc7160->client, xc7160_global_regs);
	if(ret){
		dev_err(dev, "could not set init registers\n");
		return ret;
	}

	xc7160->initial_status = true;
	ret=sc8238_check_sensor_id(xc7160,xc7160->client);
	if(ret)
		return ret;

	if( xc7160->isp_out_colorbar != true ){
		xc7160_write_array(xc7160->client, xc7160_stream_off_regs);
		ret = xc7160_write_array(xc7160->client, xc7160_i2c_bypass_on_regs);
		if (ret) {
			dev_err(dev, "could not set bypass on registers\n");
			return ret;
		} 	
		msleep(1);
		i = sc8238_write_array(xc7160->client, sc8238_global_regs);
		if (i){ 
			dev_err(dev,  "could not set sensor_initial_regs\n");
			xc7160->isp_out_colorbar = true;
		}
		ret = xc7160_write_array(xc7160->client, xc7160_i2c_bypass_off_regs);
		if (ret) {
			dev_err(dev, "could not set bypass off registers\n");
			i = ret;
		}			
	}
	return i;
}

static int xc7160_get_reso_dist(const struct xc7160_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct xc7160_mode *
xc7160_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = xc7160_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int xc7160_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	const struct xc7160_mode *mode;

	mutex_lock(&xc7160->mutex);

	mode = xc7160_find_best_fit(fmt);
	fmt->format.code = XC7160_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;

	if((fmt->format.width == 3840)&&(fmt->format.height == 2160)){
		__v4l2_ctrl_s_ctrl(xc7160->link_freq, 1);
		__v4l2_ctrl_s_ctrl_int64(xc7160->pixel_rate, XC7160_PIXEL_RATE_LOW);
	}else{
		__v4l2_ctrl_s_ctrl(xc7160->link_freq, 0);
		__v4l2_ctrl_s_ctrl_int64(xc7160->pixel_rate, XC7160_PIXEL_RATE_HIGH);
	}
	xc7160_global_regs = mode->isp_reg_list;
	sc8238_global_regs = mode->sensor_reg_list;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&xc7160->mutex);
		return -ENOTTY;
#endif
	} else {
		xc7160->cur_mode = mode;
		if (xc7160->streaming){
			mutex_unlock(&xc7160->mutex);
			return -EBUSY;
		}

	}
	//camera_isp_sensor_initial(xc7160);
	mutex_unlock(&xc7160->mutex);

	return 0;
}

static int xc7160_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	const struct xc7160_mode *mode = xc7160->cur_mode;

	mutex_lock(&xc7160->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&xc7160->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = XC7160_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&xc7160->mutex);

	return 0;
}

static int xc7160_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = XC7160_MEDIA_BUS_FMT;

	return 0;
}

static int xc7160_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != XC7160_MEDIA_BUS_FMT/*MEDIA_BUS_FMT_YUYV8_2X8*/)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

//static int xc7160_enable_test_pattern(struct xc7160 *xc7160, u32 pattern)
//{
//	return 0;
//}

static int xc7160_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	const struct xc7160_mode *mode = xc7160->cur_mode;

	mutex_lock(&xc7160->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&xc7160->mutex);

	return 0;
}

static void xc7160_get_module_inf(struct xc7160 *xc7160,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, XC7160_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, xc7160->module_name,
		sizeof(inf->base.module));
	strlcpy(inf->base.lens, xc7160->len_name, sizeof(inf->base.lens));
}

static int xc7160_get_channel_info(struct xc7160 *xc7160, struct rkmodule_channel_info *ch_info)
{
       if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
               return -EINVAL;
       ch_info->vc = xc7160->cur_mode->vc[ch_info->index];
       ch_info->width = xc7160->cur_mode->width;
       ch_info->height = xc7160->cur_mode->height;
       ch_info->bus_fmt = xc7160->cur_mode->bus_fmt;
       return 0;
}

static long xc7160_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	struct rkmodule_channel_info *ch_info;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		xc7160_get_module_inf(xc7160, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = xc7160_get_channel_info(xc7160, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long xc7160_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_channel_info *ch_info;
	long ret;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = xc7160_ioctl(sd, cmd, inf);
		if (!ret)
			ret = copy_to_user(up, inf, sizeof(*inf));
                       if (ret)
                               ret = -EFAULT;
		kfree(inf);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}
		ret = xc7160_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

#ifdef FIREFLY_DEBUG
static int xc7160_check_isp_reg(struct xc7160 *xc7160)
{
	u32 val;
	int ret;
	struct device *dev = &xc7160->client->dev;

	ret = xc7160_write_reg(xc7160->client, 0xfffd, XC7160_REG_VALUE_08BIT, 0x80);
	if (ret){
		dev_err(dev, "write reg0xfffd failed\n");
		return ret;
	}

	ret = xc7160_write_reg(xc7160->client, 0xfffe, XC7160_REG_VALUE_08BIT, 0x26);
	if (ret){
		dev_err(dev, "write reg0xfffe failed\n");
		return ret;
	}
	ret = xc7160_read_reg(xc7160->client, 0x8012, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x8012=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x8013, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x8013=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x8014, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x8014=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x8015, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x8015=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x8010, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x8010=0x%02x\n", val);

	ret = xc7160_write_reg(xc7160->client, 0xfffe, XC7160_REG_VALUE_08BIT, 0x50);
	if (ret){
		dev_err(dev, "write reg0xfffe failed\n");
		return ret;
	}
	ret = xc7160_read_reg(xc7160->client, 0x0090, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x0090=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x0033, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x0033=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x0058, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x0058=0x%02x\n", val);

	ret = xc7160_write_reg(xc7160->client, 0xfffe, XC7160_REG_VALUE_08BIT, 0x30);
	if (ret){
		dev_err(dev, "write reg0xfffe failed\n");
		return ret;
	}
	ret = xc7160_read_reg(xc7160->client, 0x2f06, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x2f06=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x2f07, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x2f07=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x2f08, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x2f08=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x2f09, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x2f09=0x%02x\n", val);

	ret = xc7160_read_reg(xc7160->client, 0x0028, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x0028=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x0029, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x0029=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x002a, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x002a=0x%02x\n", val);
	ret = xc7160_read_reg(xc7160->client, 0x002b, XC7160_REG_VALUE_08BIT, &val);
	dev_info(dev, "reg0x002b=0x%02x\n", val);

	return ret;

}
#endif // DEBUG


static int __xc7160_start_stream(struct xc7160 *xc7160)
{
	int ret;
	struct device *dev = &xc7160->client->dev;
	
	//xc7160->isp_out_colorbar = true;
	if(xc7160->isp_out_colorbar == true){
		dev_info(dev, "colorbar on !!!\n");
		ret = xc7160_write_array(xc7160->client, xc7160_colorbar_on_regs);
	}else
		ret = xc7160_write_array(xc7160->client, xc7160_stream_on_regs);

#ifdef FIREFLY_DEBUG
		xc7160_check_isp_reg(xc7160);
#endif // DEBUG
	
	if(ret)
		dev_err(dev, "xc7160 write stream or colorbar regs failed\n");

	/* In case these controls are set before streaming */
	mutex_unlock(&xc7160->mutex);
	ret = v4l2_ctrl_handler_setup(&xc7160->ctrl_handler);
	mutex_lock(&xc7160->mutex);
	if (ret)
		return ret;


	return 0;
}

static int __xc7160_stop_stream(struct xc7160 *xc7160)
{	
	int ret;

	ret = xc7160_write_array(xc7160->client, xc7160_stream_off_regs);
	if(ret)
		printk("%s: write stream off failed\n",__func__);
		
	return ret;
}

static int xc7160_check_isp_id(struct xc7160 *xc7160,
				   struct i2c_client *client)
{
	struct device *dev = &xc7160->client->dev;
	u32 id = 0;
	int ret;

	ret = xc7160_write_reg(client, XC7160_REG_HIGH_SELECT,XC7160_REG_VALUE_08BIT, 0x80);
	if (ret){
		dev_err(dev, "write XC7160_REG_HIGH_SELECT failed\n");
		return ret;
	}

	ret = xc7160_read_reg(client, XC7160_CHIP_REG_ID1_ZYK	,
			       XC7160_REG_VALUE_08BIT, &id);
	if (id == XC7160_CHIP_ID1_ZYK	) {
		dev_info(dev, "isp chip is xc7160\n");
		ret = xc7160_read_reg(client, XC7160_CHIP_REG_ID2_ZYK	,
		       XC7160_REG_VALUE_08BIT, &id);
		if (id != XC7160_CHIP_ID2_ZYK	) {
			dev_err(dev, "Unexpected sensor of XC7160_CHIP_ID_REG2, id(%06x), ret(%d)\n", id, ret);
			return ret;
		}
	}
	
	return 0;
	
}


static int __xc7160_power_on(struct xc7160 *xc7160);
static void __xc7160_power_off(struct xc7160 *xc7160);
static int xc7160_s_power(struct v4l2_subdev *sd, int on)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	struct i2c_client *client = xc7160->client;
	struct device *dev = &xc7160->client->dev;
	int ret = 0;

	mutex_lock(&xc7160->mutex);
	/* If the power state is not modified - no work to do. */
	if (xc7160->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __xc7160_power_on(xc7160);
		if(ret){
			dev_err(dev, "xc7160 power on failed\n");
		}
		xc7160->power_on = true;
/*
		if(xc7160->initial_status != true){
			ret = camera_isp_sensor_initial(xc7160);
		}
*/
		ret = xc7160_check_isp_id(xc7160,xc7160->client);
		if (ret){
			dev_err(dev, "write XC7160_REG_HIGH_SELECT failed\n");
			goto unlock_and_return;
		}
		
		// if(xc7160->initial_status != true){
		// 	xc7160_global_regs = xc7160->cur_mode->isp_reg_list;
		// 	sc8238_global_regs = xc7160->cur_mode->sensor_reg_list;	
		// 	camera_isp_sensor_initial(xc7160);
		// }
	
		// /* export gpio */
		// if (!IS_ERR(xc7160->reset_gpio))
		// 	gpiod_export(xc7160->reset_gpio, false);
		// if (!IS_ERR(xc7160->pwdn_gpio))
		// 	gpiod_export(xc7160->pwdn_gpio, false);
	} else {
		pm_runtime_put(&client->dev);
		__xc7160_power_off(xc7160);
		xc7160->power_on = false;
		/* unexport gpio */
		if (!IS_ERR(xc7160->reset_gpio))
			gpiod_unexport(xc7160->reset_gpio);
		if (!IS_ERR(xc7160->pwdn_gpio))
			gpiod_unexport(xc7160->pwdn_gpio);

	}

unlock_and_return:
	ret = xc7160_write_array(xc7160->client, xc7160_i2c_bypass_off_regs);
	mutex_unlock(&xc7160->mutex);

	return ret;
}


static int xc7160_s_stream(struct v4l2_subdev *sd, int on)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	struct i2c_client *client = xc7160->client;
	int ret = 0;

	if(xc7160->initial_status != true){
			xc7160_global_regs = xc7160->cur_mode->isp_reg_list;
			sc8238_global_regs = xc7160->cur_mode->sensor_reg_list;	
			camera_isp_sensor_initial(xc7160);
		}
	
	/* export gpio */
	if (!IS_ERR(xc7160->reset_gpio))
		gpiod_export(xc7160->reset_gpio, false);
	if (!IS_ERR(xc7160->pwdn_gpio))
		gpiod_export(xc7160->pwdn_gpio, false);

	mutex_lock(&xc7160->mutex);
	on = !!on;
	if (on == xc7160->streaming){
		goto unlock_and_return;
	}
	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __xc7160_start_stream(xc7160);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__xc7160_stop_stream(xc7160);
		pm_runtime_put(&client->dev);
	}

	xc7160->streaming = on;

unlock_and_return:
	mutex_unlock(&xc7160->mutex);

	return ret;
}


/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 xc7160_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, XC7160_XVCLK_FREQ / 1000 / 1000);
}

static int __xc7160_power_on(struct xc7160 *xc7160)
{
	int ret;
	u32 delay_us;
	struct device *dev = &xc7160->client->dev;
	

	if (!IS_ERR_OR_NULL(xc7160->pins_default)) {
		ret = pinctrl_select_state(xc7160->pinctrl,
					   xc7160->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	if (!IS_ERR(xc7160->reset_gpio))
		gpiod_set_value_cansleep(xc7160->reset_gpio, 0);	


	if (!IS_ERR(xc7160->pwdn_gpio))
		gpiod_set_value_cansleep(xc7160->pwdn_gpio, 0);

	msleep(4);

	if (clkout_enabled_index){
		ret = clk_prepare_enable(xc7160->xvclk);
		if (ret < 0) {
			dev_err(dev, "Failed to enable xvclk\n");
			return ret;
		}
	}

	ret = regulator_bulk_enable(XC7160_NUM_SUPPLIES, xc7160->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(xc7160->mipi_pwr_gpio))
		gpiod_set_value_cansleep(xc7160->mipi_pwr_gpio, 1);


	usleep_range(500, 1000);
	if (!IS_ERR(xc7160->reset_gpio))
		gpiod_set_value_cansleep(xc7160->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(xc7160->pwdn_gpio))
		gpiod_set_value_cansleep(xc7160->pwdn_gpio, 1);

	//  msleep(25);
	// /* 8192 cycles prior to first SCCB transaction */
	delay_us = xc7160_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);
	// xc7160->power_on = true;

	return 0;

disable_clk:
	if (clkout_enabled_index)
		clk_disable_unprepare(xc7160->xvclk);

	return ret;
}

static void __xc7160_power_off(struct xc7160 *xc7160)
{
	int ret;
	struct device *dev = &xc7160->client->dev;
	xc7160->initial_status = false;

	if (!IS_ERR(xc7160->reset_gpio))
		gpiod_set_value_cansleep(xc7160->reset_gpio, 1);
	if (!IS_ERR(xc7160->pwdn_gpio))
		gpiod_set_value_cansleep(xc7160->pwdn_gpio,1);
	if (!IS_ERR(xc7160->mipi_pwr_gpio))
		gpiod_set_value_cansleep(xc7160->mipi_pwr_gpio,1);
	if (clkout_enabled_index)
		clk_disable_unprepare(xc7160->xvclk);
	if (!IS_ERR_OR_NULL(xc7160->pins_sleep)) {
		ret = pinctrl_select_state(xc7160->pinctrl,
					   xc7160->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(XC7160_NUM_SUPPLIES, xc7160->supplies);
	
}
static int xc7160_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct xc7160 *xc7160 = to_xc7160(sd);

	if(xc7160->power_on == false)
		return __xc7160_power_on(xc7160);
	else
		printk("xc7160 is power on, nothing to do\n");

	return 0;
}

static int xc7160_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct xc7160 *xc7160 = to_xc7160(sd);

	if(xc7160->power_on == true){
		__xc7160_power_off(xc7160);
		xc7160->power_on = false;
	}
	return 0;
}



static int xc7160_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int xc7160_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct xc7160 *xc7160 = to_xc7160(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct xc7160_mode *def_mode = &supported_modes[0];

	mutex_lock(&xc7160->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = XC7160_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&xc7160->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static const struct dev_pm_ops xc7160_pm_ops = {
	SET_RUNTIME_PM_OPS(xc7160_runtime_suspend,
			   xc7160_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops xc7160_internal_ops = {
	.open = xc7160_open,
};
#endif

static const struct v4l2_subdev_core_ops xc7160_core_ops = {
        .log_status = v4l2_ctrl_subdev_log_status,
        .subscribe_event = v4l2_ctrl_subdev_subscribe_event,
        .unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.s_power = xc7160_s_power,
	.ioctl = xc7160_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = xc7160_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops xc7160_video_ops = {
	.s_stream = xc7160_s_stream,
	.g_frame_interval = xc7160_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops xc7160_pad_ops = {
	.enum_mbus_code = xc7160_enum_mbus_code,
	.enum_frame_size = xc7160_enum_frame_sizes,
	.enum_frame_interval = xc7160_enum_frame_interval,
	.get_fmt = xc7160_get_fmt,
	.set_fmt = xc7160_set_fmt,
};

static const struct v4l2_subdev_ops xc7160_subdev_ops = {
	.core	= &xc7160_core_ops,
	.video	= &xc7160_video_ops,
	.pad	= &xc7160_pad_ops,
};

static int xc7160_initialize_controls(struct xc7160 *xc7160)
{
	const struct xc7160_mode *mode;
	struct v4l2_ctrl_handler *handler;
	int ret;

	handler = &xc7160->ctrl_handler;
	mode = xc7160->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &xc7160->mutex;

	xc7160->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	if (xc7160->link_freq)
		xc7160->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	xc7160->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, XC7160_PIXEL_RATE_HIGH, 1, XC7160_PIXEL_RATE_HIGH);

	if (handler->error) {
		ret = handler->error;
		dev_err(&xc7160->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	xc7160->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}



static int xc7160_configure_regulators(struct xc7160 *xc7160)
{
	unsigned int i;

	for (i = 0; i < XC7160_NUM_SUPPLIES; i++)
		xc7160->supplies[i].supply = xc7160_supply_names[i];

	return devm_regulator_bulk_get(&xc7160->client->dev,
				       XC7160_NUM_SUPPLIES,
				       xc7160->supplies);
}

static void free_gpio(struct xc7160 *xc7160)
{
	if (!IS_ERR(xc7160->pwdn_gpio))
		gpio_free(desc_to_gpio(xc7160->pwdn_gpio));
        if (!IS_ERR(xc7160->reset_gpio))
                gpio_free(desc_to_gpio(xc7160->reset_gpio));
        if (!IS_ERR(xc7160->mipi_pwr_gpio))
		gpio_free(desc_to_gpio(xc7160->mipi_pwr_gpio));
}

static int xc7160_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct device_node *endpoint_node = NULL;
	struct v4l2_fwnode_endpoint vep = {0};
	struct xc7160 *xc7160;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "T-chip firefly camera driver version: %02x.%02x.%02x",
			DRIVER_VERSION >> 16,
			(DRIVER_VERSION & 0xff00) >> 8,
			DRIVER_VERSION & 0x00ff);

	xc7160 = devm_kzalloc(dev, sizeof(*xc7160), GFP_KERNEL);
	if (!xc7160)
		return -ENOMEM;

/* Model info */
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
			&xc7160->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
			&xc7160->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
			&xc7160->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
			&xc7160->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

/* Parse dts Clk、Pwr、PDN、Rest */
	xc7160->client = client;
	xc7160->cur_mode = &supported_modes[0];

	if (clkout_enabled_index){
		xc7160->xvclk = devm_clk_get(dev, "xvclk");
		if (IS_ERR(xc7160->xvclk)) {
			dev_err(dev, "Failed to get xvclk\n");
			return -EINVAL;
		}
		ret = clk_set_rate(xc7160->xvclk, XC7160_XVCLK_FREQ);
		if (ret < 0) {
			dev_err(dev, "Failed to set xvclk rate (24MHz)\n");
			return ret;
		}
		if (clk_get_rate(xc7160->xvclk) != XC7160_XVCLK_FREQ)
			dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	}

	xc7160->mipi_pwr_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(xc7160->mipi_pwr_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	xc7160->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(xc7160->reset_gpio)) {
		dev_info(dev, "Failed to get reset-gpios, maybe no use\n");
	}

	xc7160->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(xc7160->pwdn_gpio)) {
		dev_info(dev, "Failed to get pwdn-gpios, maybe no use\n");
	}

	ret = xc7160_configure_regulators(xc7160);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	xc7160->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(xc7160->pinctrl)) {
		xc7160->pins_default =
			pinctrl_lookup_state(xc7160->pinctrl,
					OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(xc7160->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		xc7160->pins_sleep =
			pinctrl_lookup_state(xc7160->pinctrl,
					OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(xc7160->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

/* Parse lane number */ 
	endpoint_node = of_find_node_by_name(node,"endpoint");
	if(endpoint_node != NULL){
		//printk("xc7160 get endpoint node success\n");
		ret=v4l2_fwnode_endpoint_parse(&endpoint_node->fwnode, &vep);
		if(ret){
			dev_info(dev, "Failed to get xc7160 endpoint data lanes, set a default value\n");
			xc7160->lane_data_num = 4;
		}else{
			dev_info(dev, "Success to get xc7160 endpoint data lanes, dts uses %d lanes\n", vep.bus.mipi_csi2.num_data_lanes);
			xc7160->lane_data_num = vep.bus.mipi_csi2.num_data_lanes;
		}
	}else{
		dev_info(dev,"xc7160 get endpoint node failed\n");
		return -ENOENT;
	}

	mutex_init(&xc7160->mutex);
	
/* Init v4l2 subdev */
	sd = &xc7160->subdev;
	v4l2_i2c_subdev_init(sd, client, &xc7160_subdev_ops);
	
	ret = xc7160_initialize_controls(xc7160);
	if (ret)
		goto err_destroy_mutex;

/* Check chip id */
	ret = __xc7160_power_on(xc7160);
	if (ret) {
		dev_err(dev, "--xc--__xc7160_power_on failed\n");
		goto err_power_off;
	}

	ret = xc7160_check_isp_id(xc7160, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &xc7160_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	xc7160->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &xc7160->pad);
	if (ret < 0)
		goto err_power_off;

#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(xc7160->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
			xc7160->module_index, facing,
			XC7160_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__xc7160_power_off(xc7160);
	free_gpio(xc7160);
	//err_free_handler:
	v4l2_ctrl_handler_free(&xc7160->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&xc7160->mutex);

	return ret;
}

static int xc7160_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct xc7160 *xc7160 = to_xc7160(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&xc7160->ctrl_handler);
	mutex_destroy(&xc7160->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__xc7160_power_off(xc7160);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id xc7160_of_match[] = {
	{ .compatible = "firefly,xc7160" },
	{},
};
MODULE_DEVICE_TABLE(of, xc7160_of_match);
#endif

static const struct i2c_device_id xc7160_match_id[] = {
	{ "firefly,xc7160", 0 },
	{ },
};

static struct i2c_driver xc7160_i2c_driver = {
	.driver = {
		.name = "xc7160",
		.pm = &xc7160_pm_ops,
		.of_match_table = of_match_ptr(xc7160_of_match),
	},
	.probe		= &xc7160_probe,
	.remove		= &xc7160_remove,
	.id_table	= xc7160_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&xc7160_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&xc7160_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision xc7160 sensor driver");
MODULE_LICENSE("GPL v2");
