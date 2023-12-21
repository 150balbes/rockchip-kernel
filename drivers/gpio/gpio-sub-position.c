/*
 * Driver for pwm demo on Firefly board.
 *
 * Copyright (C) 2016, Zhongshan T-chip Intelligent Technology Co.,ltd.
 * Copyright 2006  JC.Lin
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
#include <linux/platform_device.h>
#include <linux/uaccess.h>

int sub_position_bit0;
int sub_position_bit1;
int sub_position_bit2;
int sub_position_bit3;

int sub_position_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int sub_position_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t sub_position_read(struct file *filp, char __user *buf, size_t count,loff_t *f_pos)
{
	char sub_position=0;
	
    if (!buf || count < sizeof(sub_position)) {
        return -EFAULT;
	}
	if (*f_pos >= sizeof(sub_position))
		return 1;

	sub_position = (gpio_get_value(sub_position_bit3) << 3) \
					| (gpio_get_value(sub_position_bit2) << 2) \
					| (gpio_get_value(sub_position_bit1) << 1) \
					| (gpio_get_value(sub_position_bit0) << 0);
	if (copy_to_user(buf, &sub_position, sizeof(sub_position)) != 0)
		return -EFAULT;

	*f_pos = *f_pos + sizeof(sub_position);
	return 1;
}

struct file_operations sub_position_fops = {
    .owner = THIS_MODULE,
    .read = sub_position_read,
    .open = sub_position_open,
    .release = sub_position_release,
};

static int sub_position_gpio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;
	int major;
	static struct class *cls;
	static struct device *dev;

	major = register_chrdev(0, "sub_position", &sub_position_fops);
    if (major < 0)
    {
        printk("Unable to register sub position character device !\n");
        return major;
    }
    cls = class_create(THIS_MODULE, "sub_position");
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, "sub_position");

    sub_position_bit0 = of_get_named_gpio(np, "core-add-0", 0);
    if ( !gpio_is_valid(sub_position_bit0) ) { 
        printk("Can not read property core-add-0\n");
        return -ENODEV;
    }
	ret = devm_gpio_request(&pdev->dev, sub_position_bit0, "sub_position_bit0");
	if(ret < 0){
		dev_err(&pdev->dev, "sub_position_bit0 request ERROR\n");
		devm_gpio_free(&pdev->dev, sub_position_bit0);
		return -ENODEV;
	}
	gpio_direction_input(sub_position_bit0);


    sub_position_bit1 = of_get_named_gpio(np, "core-add-1", 0);
    if ( !gpio_is_valid(sub_position_bit1) ) { 
        printk("Can not read property core-add-1\n");
        return -ENODEV;
    }
	ret = devm_gpio_request(&pdev->dev, sub_position_bit1, "sub_position_bit1");
	if(ret < 0){
		dev_err(&pdev->dev, "sub_position_bit1 request ERROR\n");
		devm_gpio_free(&pdev->dev, sub_position_bit1);
		return -ENODEV;
	}
	gpio_direction_input(sub_position_bit1);


    sub_position_bit2 = of_get_named_gpio(np, "core-add-2", 0);
    if ( !gpio_is_valid(sub_position_bit2) ) { 
        printk("Can not read property core-add-2\n");
        return -ENODEV;
    }
	ret = devm_gpio_request(&pdev->dev, sub_position_bit2, "sub_position_bit2");
	if(ret < 0){
		dev_err(&pdev->dev, "sub_position_bit2 request ERROR\n");
		devm_gpio_free(&pdev->dev, sub_position_bit2);
		return -ENODEV;
	}
	gpio_direction_input(sub_position_bit2);


    sub_position_bit3 = of_get_named_gpio(np, "core-add-3", 0);
    if ( !gpio_is_valid(sub_position_bit3) ) { 
        printk("Can not read property core-add-3\n");
        return -ENODEV;
    }
	ret = devm_gpio_request(&pdev->dev, sub_position_bit3, "sub_position_bit3");
	if(ret < 0){
		dev_err(&pdev->dev, "sub_position_bit3 request ERROR\n");
		devm_gpio_free(&pdev->dev, sub_position_bit3);
		return -ENODEV;
	}
	gpio_direction_input(sub_position_bit3);

	printk("%s: sub position gpio init ok!\n", __func__);
    return 0;
}

static int sub_position_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id sub_position_match_table[] = {
	{ .compatible = "firefly,sub_position",},
	{},
};

static struct platform_driver sub_position_driver = {
	.driver = {
		.name = "sub_position",
		.owner = THIS_MODULE,
		.of_match_table = sub_position_match_table,
	},
	.probe = sub_position_gpio_probe,
	.remove = sub_position_gpio_remove,
};

static int sub_position_init(void)
{
	return platform_driver_register(&sub_position_driver);
}
module_init(sub_position_init);

static void sub_position_exit(void)
{
	platform_driver_unregister(&sub_position_driver);
}
module_exit(sub_position_exit);

MODULE_AUTHOR("lisd <service@t-firefly.com>");
MODULE_DESCRIPTION("firefly cluster_server_r2' sub position gpio driver");
MODULE_LICENSE("GPL");
