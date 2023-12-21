// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * fixed.c
 *
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * Copyright (c) 2009 Nokia Corporation
 * Roger Quadros <ext-roger.quadros@nokia.com>
 *
 * This is useful for systems with mixed controllable and
 * non-controllable regulators, as well as for allowing testing on
 * systems with no controllable regulators.
 */

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

struct fixed_voltage_data {
	struct regulator_desc desc;
	struct regulator_dev *dev;
	struct clk *enable_clock;
	unsigned int clk_enable_counter;

	struct delayed_work handler;  
	unsigned int reset_time_ms;
	int gpio;
	struct platform_device *pdev;
};

struct fixed_dev_type {
	bool has_enable_clock;
};

static int reg_clock_enable(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = clk_prepare_enable(priv->enable_clock);
	if (ret)
		return ret;

	priv->clk_enable_counter++;

	return ret;
}

static int reg_clock_disable(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);

	clk_disable_unprepare(priv->enable_clock);
	priv->clk_enable_counter--;

	return 0;
}

static int reg_clock_is_enabled(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);

	return priv->clk_enable_counter > 0;
}


/**
 * of_get_fixed_voltage_config - extract fixed_voltage_config structure info
 * @dev: device requesting for fixed_voltage_config
 * @desc: regulator description
 *
 * Populates fixed_voltage_config structure by extracting data from device
 * tree node, returns a pointer to the populated structure of NULL if memory
 * alloc fails.
 */
static struct fixed_voltage_config *
of_get_fixed_voltage_config(struct device *dev,
			    const struct regulator_desc *desc)
{
	struct fixed_voltage_config *config;
	struct device_node *np = dev->of_node;
	struct regulator_init_data *init_data;
	
	config = devm_kzalloc(dev, sizeof(struct fixed_voltage_config),
								 GFP_KERNEL);
	if (!config)
		return ERR_PTR(-ENOMEM);

	config->init_data = of_get_regulator_init_data(dev, dev->of_node, desc);
	if (!config->init_data)
		return ERR_PTR(-EINVAL);

	init_data = config->init_data;
	init_data->constraints.apply_uV = 0;

	config->supply_name = init_data->constraints.name;
	if (init_data->constraints.min_uV == init_data->constraints.max_uV) {
		config->microvolts = init_data->constraints.min_uV;
	} else {
		dev_err(dev,
			 "Fixed regulator specified with variable voltages\n");
		return ERR_PTR(-EINVAL);
	}

	if (init_data->constraints.boot_on)
		config->enabled_at_boot = true;

	of_property_read_u32(np, "startup-delay-us", &config->startup_delay);
	of_property_read_u32(np, "off-on-delay-us", &config->off_on_delay);

	if (of_find_property(np, "vin-supply", NULL))
		config->input_supply = "vin";

	return config;
}

static const struct regulator_ops fixed_voltage_ops = {
};

static const struct regulator_ops fixed_voltage_clkenabled_ops = {
	.enable = reg_clock_enable,
	.disable = reg_clock_disable,
	.is_enabled = reg_clock_is_enabled,
};
static void regulator_handler(struct work_struct *work){
	
	struct delayed_work  *dwork= container_of(work, struct delayed_work, work);
	struct fixed_voltage_data *drvdata = container_of(dwork, struct fixed_voltage_data, handler);

	struct platform_device *pdev = drvdata->pdev;
	struct device *dev = &(drvdata->pdev->dev);
	struct fixed_voltage_config *config;
	const struct fixed_dev_type *drvtype = of_device_get_match_data(dev);
	struct regulator_config cfg = { };
	enum gpiod_flags gflags;
	int ret;
	if (pdev->dev.of_node) {
		config = of_get_fixed_voltage_config(&pdev->dev,
						     &drvdata->desc);
		if (IS_ERR(config))
			goto regulator_init_fail;
	} else {
		config = dev_get_platdata(&pdev->dev);
	}

	if (!config)
		return;

	drvdata->desc.name = devm_kstrdup(&pdev->dev,
					  config->supply_name,
					  GFP_KERNEL);
	if (drvdata->desc.name == NULL) {
		dev_err(&pdev->dev, "Failed to allocate supply name\n");
		goto regulator_init_fail;
	}
	drvdata->desc.type = REGULATOR_VOLTAGE;
	drvdata->desc.owner = THIS_MODULE;

	if (drvtype && drvtype->has_enable_clock) {
		drvdata->desc.ops = &fixed_voltage_clkenabled_ops;

		drvdata->enable_clock = devm_clk_get(dev, NULL);
		if (IS_ERR(drvdata->enable_clock)) {
			dev_err(dev, "Can't get enable-clock from devicetree\n");
			goto regulator_init_fail;
		}
	} else {
		drvdata->desc.ops = &fixed_voltage_ops;
	}

	drvdata->desc.enable_time = config->startup_delay;
	drvdata->desc.off_on_delay = config->off_on_delay;

	if (config->input_supply) {
		drvdata->desc.supply_name = devm_kstrdup(&pdev->dev,
					    config->input_supply,
					    GFP_KERNEL);
		if (!drvdata->desc.supply_name) {
			dev_err(&pdev->dev,
				"Failed to allocate input supply\n");
			goto regulator_init_fail;
		}
	}

	if (config->microvolts)
		drvdata->desc.n_voltages = 1;

	drvdata->desc.fixed_uV = config->microvolts;

	/*
	 * The signal will be inverted by the GPIO core if flagged so in the
	 * descriptor.
	 */
	if (config->enabled_at_boot)
		gflags = GPIOD_OUT_HIGH;
	else
		gflags = GPIOD_OUT_LOW;

	/*
	 * Some fixed regulators share the enable line between two
	 * regulators which makes it necessary to get a handle on the
	 * same descriptor for two different consumers. This will get
	 * the GPIO descriptor, but only the first call will initialize
	 * it so any flags such as inversion or open drain will only
	 * be set up by the first caller and assumed identical on the
	 * next caller.
	 *
	 * FIXME: find a better way to deal with this.
	 */
	gflags |= GPIOD_FLAGS_BIT_NONEXCLUSIVE;

	/*
	 * Do not use devm* here: the regulator core takes over the
	 * lifecycle management of the GPIO descriptor.
	 */
	cfg.ena_gpiod = gpiod_get_optional(&pdev->dev, NULL, gflags);
	if (IS_ERR(cfg.ena_gpiod))
		goto regulator_init_fail;

	cfg.dev = &pdev->dev;
	cfg.init_data = config->init_data;
	cfg.driver_data = drvdata;
	cfg.of_node = pdev->dev.of_node;

	drvdata->dev = devm_regulator_register(&pdev->dev, &drvdata->desc,
					       &cfg);
	if (IS_ERR(drvdata->dev)) {
		ret = PTR_ERR(drvdata->dev);
		dev_err(&pdev->dev, "Failed to register regulator: %d\n", ret);
		goto regulator_init_fail;
	}

	platform_set_drvdata(pdev, drvdata);

	dev_dbg(&pdev->dev, "%s supplying %duV\n", drvdata->desc.name,
		drvdata->desc.fixed_uV);

	return ;

regulator_init_fail:
	printk("error:regulator-fixed-firefly init fail");
	return;
}
static int reg_fixed_voltage_probe_firefly(struct platform_device *pdev)
{

	struct fixed_voltage_data *drvdata;
	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct fixed_voltage_data),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->reset_time_ms = 0;
	drvdata->pdev = pdev;

	if(of_property_read_bool(pdev->dev.of_node, "regulator-fixed-kernel-reset-ms")){
		struct device_node *gpio_node;
		enum of_gpio_flags flag;
		gpio_node = pdev->dev.of_node;
		drvdata->gpio = of_get_named_gpio_flags(gpio_node, "gpio", 0, &flag);
		if (!gpio_is_valid(drvdata->gpio)) {
			printk("firefly-gpio: %d is invalid\n", drvdata->gpio); 
			return -ENODEV;
		}
		if (gpio_request(drvdata->gpio, "gpio")) {
			printk("gpio %d request failed!\n", !flag);
			gpio_free(drvdata->gpio);
			return -ENODEV;
		}
		printk("lzf debug %s-%d\n",__func__,__LINE__);
		gpio_direction_output(drvdata->gpio, (flag == OF_GPIO_ACTIVE_LOW) ? 1:0);
		gpio_free(drvdata->gpio);
		of_property_read_u32(pdev->dev.of_node, "regulator-fixed-kernel-reset-ms", &drvdata->reset_time_ms);		
	}
	INIT_DELAYED_WORK(&drvdata->handler, regulator_handler);
	queue_delayed_work(system_freezable_wq, &drvdata->handler, msecs_to_jiffies(drvdata->reset_time_ms));

	return 0;
}

#if defined(CONFIG_OF)
static const struct fixed_dev_type fixed_voltage_data = {
	.has_enable_clock = false,
};

static const struct fixed_dev_type fixed_clkenable_data = {
	.has_enable_clock = true,
};

static const struct of_device_id fixed_of_match[] = {
	{
		.compatible = "regulator-fixed-firefly",
		.data = &fixed_voltage_data,
	},
	{
		.compatible = "regulator-fixed-clock-firefly",
		.data = &fixed_clkenable_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, fixed_of_match);
#endif

static struct platform_driver regulator_fixed_voltage_driver = {
	.probe		= reg_fixed_voltage_probe_firefly,
	.driver		= {
		.name		= "reg-fixed-voltage-firefly",
		.of_match_table = of_match_ptr(fixed_of_match),
	},
};

static int __init regulator_fixed_voltage_init(void)
{
	return platform_driver_register(&regulator_fixed_voltage_driver);
}
module_init(regulator_fixed_voltage_init);

static void __exit regulator_fixed_voltage_exit(void)
{
	platform_driver_unregister(&regulator_fixed_voltage_driver);
}
module_exit(regulator_fixed_voltage_exit);

MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_DESCRIPTION("Fixed voltage regulator");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:reg-fixed-voltage-firefly");
