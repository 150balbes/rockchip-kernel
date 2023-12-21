/*
 * Driver for Firefly board.
 *
 * Copyright (C) 2016, Zhongshan T-chip Intelligent Technology Co.,ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>


struct firefly_gpio_info {
	struct gpio_desc* firefly_gpiod;
};



static int firefly_gl3321g_gpio_probe(struct platform_device *pdev)
{
    int ret;
	int time;
    struct firefly_gpio_info *gpio_info;
    struct device_node *firefly_gpio_node = pdev->dev.of_node;

	gpio_info = devm_kzalloc(&pdev->dev,sizeof(struct firefly_gpio_info), GFP_KERNEL);
    if (!gpio_info) {
        dev_err(&pdev->dev, "devm_kzalloc failed!\n");
                return -ENOMEM;
    }

	gpio_info->firefly_gpiod = devm_gpiod_get_optional(&pdev->dev, "firefly-gl3321g", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_info->firefly_gpiod)) {
        dev_err(&pdev->dev, "failed to get firefly gpio\n");
		return -ENODEV;
    }
	pdev->dev.driver_data = gpio_info;
    ret=of_property_read_u32(firefly_gpio_node,"firefly-mdelay",&time);
	if(!ret && (time > 0)){
		printk("firefly gl3321g gpio delay %d ms\n",time);
		msleep(time);
	}
	printk("firefly gl3321g gpio set high\n");
	gpiod_set_value_cansleep(gpio_info->firefly_gpiod, 1);

	return 0;
}

static void firefly_gl3321g_gpio_shutdown(struct platform_device *pdev)
{
/*
	struct firefly_gpio_info *gpio_info;
	gpio_info = pdev->dev.driver_data;
	if(gpio_info != NULL)
		gpiod_set_value_cansleep(gpio_info->firefly_gpiod,0);

	printk("firefly gl3321g gpio set low\n");
*/
	return;
}

static struct of_device_id firefly_match_table[] = {
        { .compatible = "firefly,gl3321g-gpio",},
        {},
};

static struct platform_driver firefly_gl3321g_gpio_driver = {
        .driver = {
                .name = "gl3321g-gpio",
                .owner = THIS_MODULE,
                .of_match_table = firefly_match_table,
        },
        .probe = firefly_gl3321g_gpio_probe,
		.shutdown = firefly_gl3321g_gpio_shutdown,
};

static int firefly_gl3321g_gpio_init(void)
{
        return platform_driver_register(&firefly_gl3321g_gpio_driver);
}
late_initcall(firefly_gl3321g_gpio_init);

static void firefly_gl3321g_gpio_exit(void)
{
        platform_driver_unregister(&firefly_gl3321g_gpio_driver);
}
module_exit(firefly_gl3321g_gpio_exit);

MODULE_AUTHOR("zyk <zyk@t-chip.com.cn>");
MODULE_DESCRIPTION("Firefly GL3321G GPIO driver");
MODULE_ALIAS("platform:firefly-gl3321g-gpio");
MODULE_LICENSE("GPL");
