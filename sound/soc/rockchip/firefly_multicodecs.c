/*
 * Rockchip machine ASoC driver for Rockchip Multi-codecs audio
 *
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd
 *
 * Authors: Sugar Zhang <sugar.zhang@rock-chips.com>,
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/extcon-provider.h>
#include <linux/gpio.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#define DRV_NAME "firefly-multicodecs"
#define MAX_CODECS	2
#define WAIT_CARDS	(SNDRV_CARDS - 1)
#define DEFAULT_MCLK_FS	256
#define hw_ver_1 1
#define hw_ver_0 0

enum mic_link{
	MIC_LINK_OFF,
	MIC_LINK_ON,
};

enum INPUT_DEV{
	INPUT_LIN1,
	INPUT_LIN2,        //mic line
	INPUT_LIN1_DIFF,   //mic differential
	INPUT_LIN2_DIFF,   //mic differential
};

enum LINEIN_TYPE{
	LINEIN_TYPE0,    //ITX-3588J USE
	LINEIN_TYPE1,    //ROC-RK3588S-PC USE
	LINEIN_TYPE2,    //AIO-3588SJD4 USE
	LINEIN_TYPE3,    //ROC-RK3588-PC USE
	LINEIN_TPYE4,   //AIO-3588Q USE
	LINEIN_TPYE6,   //AIO-3588L USE
};

#define POLL_VAL 80
#define INPUT_LIN1_ADC 900
#define INPUT_LIN2_ADC 5
#define INPUT_LIN2_DIFF_ADC 1800
int first_init_status = 0;
int first_init_adc_flag = 0;

struct multicodecs_data {
	struct snd_soc_card snd_card;
	struct snd_soc_dai_link dai_link;
	struct snd_soc_jack *jack_headset;
	struct gpio_desc *hp_ctl_gpio;
	struct gpio_desc *spk_ctl_gpio;
	struct gpio_desc *hp_det_gpio;
	struct iio_channel *adc;
	struct extcon_dev *extcon;
	struct delayed_work handler;
	struct delayed_work mic_work;
	unsigned int mclk_fs;
	bool not_use_dapm;
	bool hp_enable_state; //1: headphone inserting  0: headphone unpluging
	bool codec_hp_det;
	bool hw_ver;
	int hw_ver_flag;
	int hp_det_adc_value;
	u32 linein_type;     //0:ITX-3588J(mic) 1:ROC-RK3588S-PC(mic)
	u32 last_key;
	u32 mic_status;
	u32 keyup_voltage;
	struct input_dev *input;
	struct input_dev_mic *mic;
	struct mutex gpio_lock;
};

struct multicodecs_data *global_mc_data = NULL;

static struct snd_soc_jack_pin jack_pins[] = {
	{
		.pin = "Headphone",
		.mask = SND_JACK_HEADPHONE,
	}, {
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	}, {
		.pin = "Speaker",
		.mask = SND_JACK_LINEOUT,
	},
};

struct jack_zone {
	unsigned int min_mv;
	unsigned int max_mv;
	unsigned int type;
};

static struct jack_zone lin1_lin2_zone[] ={
	{
		.min_mv = 0,
		.max_mv = 100,
		.type = INPUT_LIN1,
	}, {
		.min_mv = 1250,
		.max_mv = 1350,
		.type = INPUT_LIN1,
	}, {
		.min_mv = 1550,
		.max_mv = UINT_MAX,
		.type = INPUT_LIN2_DIFF,
	}
};

static struct jack_zone lin1_lin2_lin2diff_zone[] ={
	{
		.min_mv = 0,
		.max_mv = 100,
		.type = INPUT_LIN2,
	}, {
		.min_mv = 800,
		.max_mv = 1000,
		.type = INPUT_LIN1,
	}, {
		.min_mv = 1700,
		.max_mv = UINT_MAX,
		.type = INPUT_LIN2_DIFF,
	}
};

static struct jack_zone aio_3588sjd4_zone[] ={
	{
		.min_mv = 0,
		.max_mv = 100,
		.type = INPUT_LIN1,
	}, {
		.min_mv = 800,
		.max_mv = 1000,
		.type = INPUT_LIN1_DIFF,
	}, {
		.min_mv = 1700,
		.max_mv = UINT_MAX,
		.type = INPUT_LIN2_DIFF,
	}
};


static struct jack_zone aio_3588q_zone[] ={
	{
		.min_mv = 0,
		.max_mv = 200,
		.type = INPUT_LIN2_DIFF,
	}, {
		.min_mv = 300,
		.max_mv = 800,
		.type = INPUT_LIN2,
	}, {
		.min_mv = 1000,
		.max_mv = UINT_MAX,
		.type = INPUT_LIN1,

	}
};

static struct jack_zone aio_3588l_zone[] ={
       {
               .min_mv = 2500,
               .max_mv = UINT_MAX,
               .type = INPUT_LIN1,
       }
};

static struct jack_zone lin1_leftonly_lin2diff_zone[] ={
	{
		.min_mv = 1700,
		.max_mv = UINT_MAX,
		.type = INPUT_LIN1,
	}
};

static int jack_get_type(struct jack_zone *zone, int count, int micbias_voltage)
{
	int i;
	for(i = 0; i< count; i++){
		if (micbias_voltage >= zone[i].min_mv &&
			micbias_voltage < zone[i].max_mv)
				return zone[i].type;
	}
	return 0;
}

static const unsigned int headset_extcon_cable[] = {
	EXTCON_JACK_MICROPHONE,
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

extern void es8323_line1_line2_line2diff_switch(int value);

static void mic_det_work(struct work_struct *work)
{
	struct multicodecs_data *mc_data = container_of(work,struct multicodecs_data,mic_work.work);
	int value, ret ,status;

	ret = iio_read_channel_processed(mc_data->adc, &value);
	if (unlikely(ret < 0)) {
		/* Forcibly release key if any was pressed */
		value = mc_data->keyup_voltage;
		status = INPUT_LIN2_DIFF;
	} else {
		if(mc_data->linein_type == LINEIN_TYPE1){
			status = jack_get_type(lin1_lin2_zone,ARRAY_SIZE(lin1_lin2_zone),value);
		}else if (mc_data->linein_type == LINEIN_TYPE2){
			status = jack_get_type(aio_3588sjd4_zone,ARRAY_SIZE(aio_3588sjd4_zone),value);
		}else if (mc_data->linein_type == LINEIN_TYPE3){
			status = jack_get_type(lin1_leftonly_lin2diff_zone,ARRAY_SIZE(lin1_leftonly_lin2diff_zone),value);
		}else if (mc_data->linein_type == LINEIN_TPYE4){
			status = jack_get_type(aio_3588q_zone,ARRAY_SIZE(aio_3588q_zone),value);
		}else if (mc_data->linein_type == LINEIN_TPYE6){
			status = jack_get_type(aio_3588l_zone,ARRAY_SIZE(aio_3588l_zone),value);
		}else{
			status = jack_get_type(lin1_lin2_lin2diff_zone,ARRAY_SIZE(lin1_lin2_lin2diff_zone),value);
		}
		//printk("mic_det_work value:%d,status:%d\n",value,status);
	}

	if(mc_data->mic_status != status || first_init_status == 0 ){
		if(status == INPUT_LIN1){
			es8323_line1_line2_line2diff_switch(INPUT_LIN1);
		}else if(status == INPUT_LIN2){
			es8323_line1_line2_line2diff_switch(INPUT_LIN2);
		}else if(status == INPUT_LIN1_DIFF){
			es8323_line1_line2_line2diff_switch(INPUT_LIN1_DIFF);
		}else{
			es8323_line1_line2_line2diff_switch(INPUT_LIN2_DIFF);
		}
		mc_data->mic_status = status;
		first_init_status = 1;
	}
	queue_delayed_work(system_freezable_wq, &mc_data->mic_work, msecs_to_jiffies(500));
}

static int mic_det_fun(struct multicodecs_data *mc_data)
{
	INIT_DELAYED_WORK(&mc_data->mic_work, mic_det_work);
	queue_delayed_work(system_freezable_wq, &mc_data->mic_work, msecs_to_jiffies(1000));
	mc_data->mic_status = INPUT_LIN2_DIFF;
	return 0;
}

/*set es8323 output mute*/
extern void firefly_multircodecs_mute_es8323(int mute);

/*
 *  firefly_multicodecs_control_gpio() will be called by firefly_multircodecs_mute_es8323(),
 *  and that is always called by the codec snd_soc_dai_ops mute_stream()
 * */
void firefly_multicodecs_control_gpio(int sound_mute)
{
	if(global_mc_data == NULL){
		printk("[zyk debug] %s: can't get global_mc_data!\n",__func__);
		return;
	}

	if(global_mc_data->not_use_dapm != true)
		return;

	mutex_lock(&global_mc_data->gpio_lock);
	if(sound_mute){
		gpiod_set_value_cansleep(global_mc_data->hp_ctl_gpio,0);
		gpiod_set_value_cansleep(global_mc_data->spk_ctl_gpio,0);
	}else{
		if(global_mc_data->hp_enable_state == true){
			gpiod_set_value_cansleep(global_mc_data->spk_ctl_gpio,0);
			gpiod_set_value_cansleep(global_mc_data->hp_ctl_gpio,1);
		}else{
			gpiod_set_value_cansleep(global_mc_data->hp_ctl_gpio,0);
			gpiod_set_value_cansleep(global_mc_data->spk_ctl_gpio,1);
		}
	}
	mutex_unlock(&global_mc_data->gpio_lock);
	return;
}

static void adc_jack_poll_handler(struct work_struct *work)
{
	struct multicodecs_data *mc_data = container_of(to_delayed_work(work),
						  struct multicodecs_data,
						  handler);
	struct snd_soc_jack *jack_headset = mc_data->jack_headset;
	int value, ret;
	bool hp_state;

	ret = iio_read_channel_raw(mc_data->adc, &value);
	//printk("debug %s-%d : use adc to detect headphone,adc rawvalue  = %d \n",__func__,__LINE__,value);
	if( value < (mc_data->hp_det_adc_value -100)  || value > (mc_data->hp_det_adc_value +100) )
		hp_state = false;
	else
		hp_state = true;

	if(mc_data->hp_enable_state != hp_state || first_init_adc_flag == 0){
		if(hp_state){
			firefly_multircodecs_mute_es8323(mc_data->hp_enable_state);
			snd_soc_jack_report(jack_headset, SND_JACK_HEADPHONE, SND_JACK_HEADSET);
			snd_soc_jack_report(jack_headset, 0, SND_JACK_LINEOUT);
			extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_HEADPHONE, true);
			extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_MICROPHONE, false);

		}else{
			snd_soc_jack_report(jack_headset, 0, SND_JACK_HEADSET);
			snd_soc_jack_report(jack_headset, SND_JACK_LINEOUT, SND_JACK_LINEOUT);
			extcon_set_state_sync(mc_data->extcon,EXTCON_JACK_HEADPHONE, false);
			extcon_set_state_sync(mc_data->extcon,EXTCON_JACK_MICROPHONE, false);
		}
		first_init_adc_flag = 1;
	}

	mc_data->hp_enable_state = hp_state;
	queue_delayed_work(system_freezable_wq, &mc_data->handler, msecs_to_jiffies(500));
}

static void adc_jack_handler(struct work_struct *work)
{
	struct multicodecs_data *mc_data = container_of(to_delayed_work(work),
						  struct multicodecs_data,
						  handler);
	struct snd_soc_jack *jack_headset = mc_data->jack_headset;


	int hp_det_gpio=0;

	if(mc_data->hw_ver){
  		if(mc_data->hw_ver_flag == hw_ver_1)
  		 	hp_det_gpio = gpiod_get_value(mc_data->hp_det_gpio);
		else
		 	hp_det_gpio = !gpiod_get_value(mc_data->hp_det_gpio);

		if (hp_det_gpio) {
			mc_data->hp_enable_state = false;
			snd_soc_jack_report(jack_headset, 0, SND_JACK_HEADSET);
			snd_soc_jack_report(jack_headset, SND_JACK_LINEOUT, SND_JACK_LINEOUT);
			extcon_set_state_sync(mc_data->extcon,
					EXTCON_JACK_HEADPHONE, false);
			extcon_set_state_sync(mc_data->extcon,
					EXTCON_JACK_MICROPHONE, false);
			es8323_line1_line2_line2diff_switch(INPUT_LIN2_DIFF);
			return;
		}
		mc_data->hp_enable_state = true;
		firefly_multircodecs_mute_es8323(!mc_data->hp_enable_state);
		snd_soc_jack_report(jack_headset, SND_JACK_HEADPHONE, SND_JACK_HEADSET);
		snd_soc_jack_report(jack_headset, 0, SND_JACK_LINEOUT);
		extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_HEADPHONE, true);
		extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_MICROPHONE, false);
		es8323_line1_line2_line2diff_switch(INPUT_LIN1);
		return;
	}

	if (!gpiod_get_value(mc_data->hp_det_gpio)) {
		mc_data->hp_enable_state = false;
		snd_soc_jack_report(jack_headset, 0, SND_JACK_HEADSET);
		snd_soc_jack_report(jack_headset, SND_JACK_LINEOUT, SND_JACK_LINEOUT);
		extcon_set_state_sync(mc_data->extcon,
				EXTCON_JACK_HEADPHONE, false);
		extcon_set_state_sync(mc_data->extcon,
				EXTCON_JACK_MICROPHONE, false);
		return;
	}
	/* no ADC, so is headphone */
	mc_data->hp_enable_state = true;
	/* make sure the es8323 will mute first time, or the speaker may get sonic boom */
	firefly_multircodecs_mute_es8323(!mc_data->hp_enable_state);

	snd_soc_jack_report(jack_headset, SND_JACK_HEADPHONE, SND_JACK_HEADSET);
	snd_soc_jack_report(jack_headset, 0, SND_JACK_LINEOUT);
	extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_HEADPHONE, true);
	extcon_set_state_sync(mc_data->extcon, EXTCON_JACK_MICROPHONE, false);

	return;
};

static irqreturn_t headset_det_irq_thread(int irq, void *data)
{
	struct multicodecs_data *mc_data = (struct multicodecs_data *)data;

	queue_delayed_work(system_power_efficient_wq, &mc_data->handler, msecs_to_jiffies(200));

	return IRQ_HANDLED;
};

static int mc_hp_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct multicodecs_data *mc_data = snd_soc_card_get_drvdata(card);

	if(mc_data->not_use_dapm == true)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		gpiod_set_value_cansleep(mc_data->hp_ctl_gpio, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		gpiod_set_value_cansleep(mc_data->hp_ctl_gpio, 0);
		break;
	default:
		return 0;

	}

	return 0;
}

static int mc_spk_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct multicodecs_data *mc_data = snd_soc_card_get_drvdata(card);

	if(mc_data->not_use_dapm == true)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		gpiod_set_value_cansleep(mc_data->spk_ctl_gpio, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		gpiod_set_value_cansleep(mc_data->spk_ctl_gpio, 0);
		break;
	default:
		return 0;

	}

	return 0;
}

static const struct snd_soc_dapm_widget mc_dapm_widgets[] = {

	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_SUPPLY("Speaker Power",
			    SND_SOC_NOPM, 0, 0,
			    mc_spk_event,
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("Headphone Power",
			    SND_SOC_NOPM, 0, 0,
			    mc_hp_event,
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),
};

static const struct snd_kcontrol_new mc_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static int rk_multicodecs_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct multicodecs_data *mc_data = snd_soc_card_get_drvdata(rtd->card);
	unsigned int mclk;
	int ret;

	mclk = params_rate(params) * mc_data->mclk_fs;

	ret = snd_soc_dai_set_sysclk(codec_dai, substream->stream, mclk,
				     SND_SOC_CLOCK_IN);
	if (ret && ret != -ENOTSUPP) {
		pr_err("Set codec_dai sysclk failed: %d\n", ret);
		goto out;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, substream->stream, mclk,
				     SND_SOC_CLOCK_OUT);
	if (ret && ret != -ENOTSUPP) {
		pr_err("Set cpu_dai sysclk failed: %d\n", ret);
		goto out;
	}

	return 0;

out:
	return ret;
}

static int rk_dailink_init(struct snd_soc_pcm_runtime *rtd)
{
	struct multicodecs_data *mc_data = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_jack *jack_headset;
	int ret, irq;

	jack_headset = devm_kzalloc(card->dev, sizeof(*jack_headset), GFP_KERNEL);
	if (!jack_headset)
		return -ENOMEM;

	ret = snd_soc_card_jack_new(card, "Headset",
				    SND_JACK_HEADSET,
				    jack_headset,
				    jack_pins, ARRAY_SIZE(jack_pins));
	if (ret)
		return ret;

	mc_data->jack_headset = jack_headset;

	if (mc_data->codec_hp_det) {
		struct snd_soc_component *component = asoc_rtd_to_codec(rtd, 0)->component;

		snd_soc_component_set_jack(component, jack_headset, NULL);
	}else if(mc_data->hp_det_adc_value >= 0){
		// do nothing
	}else {
		irq = gpiod_to_irq(mc_data->hp_det_gpio);
		if (irq >= 0) {
			ret = devm_request_threaded_irq(card->dev, irq, NULL,
							headset_det_irq_thread,
							IRQF_TRIGGER_RISING |
							IRQF_TRIGGER_FALLING |
							IRQF_ONESHOT,
							"headset_detect",
							mc_data);
			if (ret) {
				dev_err(card->dev, "Failed to request headset detect irq");
				return ret;
			}

			queue_delayed_work(system_power_efficient_wq,
					   &mc_data->handler, msecs_to_jiffies(50));
		} else {
			dev_warn(card->dev, "Failed to map headset detect gpio to irq");
		}
	}

	return 0;
}

static int rk_multicodecs_parse_daifmt(struct device_node *node,
				       struct device_node *codec,
				       struct multicodecs_data *mc_data,
				       const char *prefix)
{
	struct snd_soc_dai_link *dai_link = &mc_data->dai_link;
	struct device_node *bitclkmaster = NULL;
	struct device_node *framemaster = NULL;
	unsigned int daifmt;

	daifmt = snd_soc_of_parse_daifmt(node, prefix,
					 &bitclkmaster, &framemaster);

	daifmt &= ~SND_SOC_DAIFMT_MASTER_MASK;

	if (strlen(prefix) && !bitclkmaster && !framemaster) {
		/*
		 * No dai-link level and master setting was not found from
		 * sound node level, revert back to legacy DT parsing and
		 * take the settings from codec node.
		 */
		pr_debug("%s: Revert to legacy daifmt parsing\n", __func__);

		daifmt = snd_soc_of_parse_daifmt(codec, NULL, NULL, NULL) |
			(daifmt & ~SND_SOC_DAIFMT_CLOCK_MASK);
	} else {
		if (codec == bitclkmaster)
			daifmt |= (codec == framemaster) ?
				SND_SOC_DAIFMT_CBM_CFM : SND_SOC_DAIFMT_CBM_CFS;
		else
			daifmt |= (codec == framemaster) ?
				SND_SOC_DAIFMT_CBS_CFM : SND_SOC_DAIFMT_CBS_CFS;
	}

	/*
	 * If there is NULL format means that the format isn't specified, we
	 * need to set i2s format by default.
	 */
	if (!(daifmt & SND_SOC_DAIFMT_FORMAT_MASK))
		daifmt |= SND_SOC_DAIFMT_I2S;

	dai_link->dai_fmt = daifmt;

	of_node_put(bitclkmaster);
	of_node_put(framemaster);

	return 0;
}

static int wait_locked_card(struct device_node *np, struct device *dev)
{
	char *propname = "rockchip,wait-card-locked";
	u32 cards[WAIT_CARDS];
	int num;
	int ret;
#ifndef MODULE
	int i;
#endif

	ret = of_property_count_u32_elems(np, propname);
	if (ret < 0) {
		if (ret == -EINVAL) {
			/*
			 * -EINVAL means the property does not exist, this is
			 * fine.
			 */
			return 0;
		}

		dev_err(dev, "Property '%s' elems could not be read: %d\n",
			propname, ret);
		return ret;
	}

	num = ret;
	if (num > WAIT_CARDS)
		num = WAIT_CARDS;

	ret = of_property_read_u32_array(np, propname, cards, num);
	if (ret < 0) {
		if (ret == -EINVAL) {
			/*
			 * -EINVAL means the property does not exist, this is
			 * fine.
			 */
			return 0;
		}

		dev_err(dev, "Property '%s' could not be read: %d\n",
			propname, ret);
		return ret;
	}

	ret = 0;
#ifndef MODULE
	for (i = 0; i < num; i++) {
		if (!snd_card_locked(cards[i])) {
			dev_warn(dev, "card: %d has not been locked, re-probe again\n",
				 cards[i]);
			ret = -EPROBE_DEFER;
			break;
		}
	}
#endif

	return ret;
}

static struct snd_soc_ops rk_ops = {
	.hw_params = rk_multicodecs_hw_params,
};

static int rk_multicodecs_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_dai_link *link;
	struct snd_soc_dai_link_component *cpus;
	struct snd_soc_dai_link_component *platforms;
	struct snd_soc_dai_link_component *codecs;
	struct multicodecs_data *mc_data;
	struct of_phandle_args args;
	struct device_node *node;
	u32 val;
	int count;
	int ret = 0, i = 0, idx = 0;
	const char *prefix = "rockchip,";
	int adc_value;
	u32 hw_ver ;

	ret = wait_locked_card(np, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "check_lock_card failed: %d\n", ret);
		return ret;
	}

	mc_data = devm_kzalloc(&pdev->dev, sizeof(*mc_data), GFP_KERNEL);
	if (!mc_data)
		return -ENOMEM;

	mc_data->hp_det_adc_value = -1;

	cpus = devm_kzalloc(&pdev->dev, sizeof(*cpus), GFP_KERNEL);
	if (!cpus)
		return -ENOMEM;

	platforms = devm_kzalloc(&pdev->dev, sizeof(*platforms), GFP_KERNEL);
	if (!platforms)
		return -ENOMEM;

	card = &mc_data->snd_card;
	card->dev = &pdev->dev;

	/* Parse the card name from DT */
	ret = snd_soc_of_parse_card_name(card, "rockchip,card-name");
	if (ret < 0)
		return ret;

	link = &mc_data->dai_link;
	link->name = "dailink-multicodecs";
	link->stream_name = link->name;
	link->init = rk_dailink_init;
	link->ops = &rk_ops;
	link->cpus = cpus;
	link->platforms	= platforms;
	link->num_cpus	= 1;
	link->num_platforms = 1;
	link->ignore_pmdown_time = 1;

	card->dai_link = link;
	card->num_links = 1;
	card->dapm_widgets = mc_dapm_widgets;
	card->num_dapm_widgets = ARRAY_SIZE(mc_dapm_widgets);
	card->controls = mc_controls;
	card->num_controls = ARRAY_SIZE(mc_controls);
	card->num_aux_devs = 0;

	count = of_count_phandle_with_args(np, "rockchip,codec", NULL);
	if (count < 0 || count > MAX_CODECS)
		return -EINVAL;

	/* refine codecs, remove unavailable node */
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "rockchip,codec", i);
		if (!node)
			return -ENODEV;
		if (of_device_is_available(node))
			idx++;
	}

	if (!idx)
		return -ENODEV;

	codecs = devm_kcalloc(&pdev->dev, idx,
			      sizeof(*codecs), GFP_KERNEL);
	link->codecs = codecs;
	link->num_codecs = idx;
	idx = 0;
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "rockchip,codec", i);
		if (!node)
			return -ENODEV;
		if (!of_device_is_available(node))
			continue;

		ret = of_parse_phandle_with_fixed_args(np, "rockchip,codec",
						       0, i, &args);
		if (ret)
			return ret;

		codecs[idx].of_node = node;
		ret = snd_soc_get_dai_name(&args, &codecs[idx].dai_name);
		if (ret)
			return ret;
		idx++;
	}

	/* Only reference the codecs[0].of_node which maybe as master. */
	rk_multicodecs_parse_daifmt(np, codecs[0].of_node, mc_data, prefix);

	link->cpus->of_node = of_parse_phandle(np, "rockchip,cpu", 0);
	if (!link->cpus->of_node)
		return -ENODEV;

	link->platforms->of_node = link->cpus->of_node;

	mc_data->mclk_fs = DEFAULT_MCLK_FS;
	if (!of_property_read_u32(np, "rockchip,mclk-fs", &val))
		mc_data->mclk_fs = val;

	if (!of_property_read_u32(np, "linein-type", &val))
		mc_data->linein_type = val;

	mc_data->codec_hp_det =
		of_property_read_bool(np, "rockchip,codec-hp-det");

	mc_data->adc = devm_iio_channel_get(&pdev->dev, "adc-detect");

	if (IS_ERR(mc_data->adc)) {
		if (PTR_ERR(mc_data->adc) != -EPROBE_DEFER) {
			mc_data->adc = NULL;
			dev_warn(&pdev->dev, "Failed to get ADC channel");
		}
	} else {
		if (mc_data->adc->channel->type != IIO_VOLTAGE)
			return -EINVAL;
		mic_det_fun(mc_data);
	}

	if (!of_property_read_u32(np, "hp-det-adc-value", &adc_value)){
		mc_data->hp_det_adc_value = adc_value;
	}
	if(mc_data->hp_det_adc_value >= 0){
		INIT_DELAYED_WORK(&mc_data->handler, adc_jack_poll_handler);
		queue_delayed_work(system_freezable_wq, &mc_data->handler, msecs_to_jiffies(1000));
	}else{
		INIT_DEFERRABLE_WORK(&mc_data->handler, adc_jack_handler);
	}


	if (of_property_read_u32(np, "hw-ver", &hw_ver))
	{
		mc_data->hw_ver = false;
		dev_warn(&pdev->dev, "Failed to get hw-ver");
	}
	else
	{
		mc_data->hw_ver = true;
		if(hw_ver == hw_ver_0)
		{
			printk("multicodecs : this is roc-rk3588s-pc v0.1\n");	
			mc_data->hw_ver_flag = hw_ver_0;
		}
		else if(hw_ver == hw_ver_1)
		{
			printk("multicodecs : this is roc-rk3588s-pc v1.0 or later \n");
			mc_data->hw_ver_flag = hw_ver_1;
		}
	}

	mc_data->not_use_dapm =
		of_property_read_bool(np, "firefly,not-use-dapm");
	if(!mc_data->not_use_dapm)
		dev_warn(&pdev->dev, "using dapm to control gpio");


	mc_data->spk_ctl_gpio = devm_gpiod_get_optional(&pdev->dev,
							"spk-con",
							GPIOD_OUT_LOW);
	if (IS_ERR(mc_data->spk_ctl_gpio))
		return PTR_ERR(mc_data->spk_ctl_gpio);

	mc_data->hp_ctl_gpio = devm_gpiod_get_optional(&pdev->dev,
						       "hp-con",
						       GPIOD_OUT_LOW);
	if (IS_ERR(mc_data->hp_ctl_gpio))
		return PTR_ERR(mc_data->hp_ctl_gpio);

	mc_data->hp_det_gpio = devm_gpiod_get_optional(&pdev->dev, "hp-det", GPIOD_IN);
	if (IS_ERR(mc_data->hp_det_gpio))
		return PTR_ERR(mc_data->hp_det_gpio);

	mc_data->extcon = devm_extcon_dev_allocate(&pdev->dev, headset_extcon_cable);
	if (IS_ERR(mc_data->extcon)) {
		dev_err(&pdev->dev, "allocate extcon failed\n");
		return PTR_ERR(mc_data->extcon);
	}

	ret = devm_extcon_dev_register(&pdev->dev, mc_data->extcon);
	if (ret) {
		dev_err(&pdev->dev, "failed to register extcon: %d\n", ret);
		return ret;
	}

	ret = snd_soc_of_parse_audio_routing(card, "rockchip,audio-routing");
	if (ret < 0)
		dev_warn(&pdev->dev, "Audio routing invalid/unspecified\n");

	snd_soc_card_set_drvdata(card, mc_data);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (ret) {
		dev_err(&pdev->dev, "card register failed %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, card);

	mutex_init(&mc_data->gpio_lock);
	global_mc_data = mc_data;

	return ret;
}

static const struct of_device_id rockchip_multicodecs_of_match[] = {
	{ .compatible = "firefly,multicodecs-card", },
	{},
};

MODULE_DEVICE_TABLE(of, rockchip_multicodecs_of_match);

static struct platform_driver rockchip_multicodecs_driver = {
	.probe = rk_multicodecs_probe,
	.driver = {
		.name = DRV_NAME,
		.pm = &snd_soc_pm_ops,
		.of_match_table = rockchip_multicodecs_of_match,
	},
};

module_platform_driver(rockchip_multicodecs_driver);

MODULE_AUTHOR("service <service@t-firefly.com>");
MODULE_DESCRIPTION("Firefly General Multicodecs ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
