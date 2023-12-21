/*
* Driver for adc demo on Firefly board.
*
* Copyright (C) 2016, Zhongshan T-chip Intelligent Technology Co.,ltd.
* Copyright 2006 ZL.Guan
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* NOTE: This demo is only applicable to: Firefly-RK3588 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>

#define FIREFLY_ADC_SAMPLE_JIFFIES      (50 / (MSEC_PER_SEC / HZ))
#define FIREFLY_FAN_ABSOLUTE_VALUE  500
#define VREF 1800

struct iio_channel *chan;
struct delayed_work adc_poll_work;
bool fan_insert;
int count; 

static void firefly_demo_adc_poll(struct work_struct *work)
{
    int ret,raw;
    int result = -1;

    ret = iio_read_channel_raw(chan, &raw); 
    if (ret < 0) {
        printk("read hook adc channel() error: %d\n", ret);
        return;
    }
    if(raw < FIREFLY_FAN_ABSOLUTE_VALUE && !fan_insert){
        if(count < 8) count++;
        else{
            fan_insert = true;
            count = 0;
            result = (VREF*raw)/4095    ;//Vref / (2^n-1) = Vresult / raw
            printk("Fan insert! raw= %d Voltage= %dmV\n",raw,result);
        }
    }
    else if(raw > FIREFLY_FAN_ABSOLUTE_VALUE && fan_insert){
        if(count < 8) count++;
        else{	
            fan_insert = false;
            count = 0;
            result = (VREF*raw)/4095;
            printk("Fan out! raw= %d Voltage=%dmV\n",raw,result);
        }
    }
    else count = 0;
    schedule_delayed_work(&adc_poll_work, FIREFLY_ADC_SAMPLE_JIFFIES);
}

static int firefly_adc_probe(struct platform_device *pdev)
{
    printk("firefly_adc_probe!\n");

    count = 0;
    chan = iio_channel_get(&(pdev->dev), NULL);
    if (IS_ERR(chan))
        {
            chan = NULL;
            printk("%s() have not set adc chan\n", __FUNCTION__);
            return -1;
        }

    fan_insert = false;
    if (chan) {
        INIT_DELAYED_WORK(&adc_poll_work, firefly_demo_adc_poll);
        schedule_delayed_work(&adc_poll_work,1000);
        }
    return 0;
    
}

static int firefly_adc_remove(struct platform_device *pdev)
{
    printk("firefly_adc_remove!\n");
    iio_channel_release(chan);
    return 0;
}

static const struct of_device_id firefly_adc_match[] = { 
    { .compatible = "firefly,rk3588-adc" },
    {},
};

static struct platform_driver firefly_adc_driver = { 
    .probe      = firefly_adc_probe,
    .remove     = firefly_adc_remove,
    .driver     = { 
        .name   = "firefly_adc",
        .owner  = THIS_MODULE,
        .of_match_table = firefly_adc_match,
    },  
};

static int firefly_adc_init(void)
{
    int ret ;
    printk("Firefly adc init\n");
    printk("Firefly adc-->FIREFLY_ADC_SAMPLE_JIFFIES:  %ld \n",FIREFLY_ADC_SAMPLE_JIFFIES);
    printk("Firefly adc-->MSEC_PER_SEC:  %ld \n",MSEC_PER_SEC);
    printk("Firefly adc-->HZ:  %d \n",HZ);
    ret = platform_driver_register(&firefly_adc_driver);
    printk("Firefly adc-->ret:  %d \n",ret);
    return 0;
}

static void firefly_adc_exit(void)
{
    printk("Firefly adc exit\n");
    platform_driver_unregister(&firefly_adc_driver);
}

module_init(firefly_adc_init);
module_exit(firefly_adc_exit);

MODULE_AUTHOR("maocl <service@t-firefly.com>");
MODULE_DESCRIPTION("Firefly adc demo driver");
MODULE_ALIAS("platform:firefly-adc");
MODULE_LICENSE("GPL");
