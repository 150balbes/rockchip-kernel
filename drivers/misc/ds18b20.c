#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>	/* size_t.. */
#include <linux/fs.h>		/* everything.. */
#include <linux/cdev.h>
#include <linux/slab.h>		/* kmalloc() */
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
//#include <asm-generic/uaccess.h>	/* copy_*_user() */

#define DS1820_CMD_SKIP_ROM	(0xCC)
#define DS1820_CMD_CONVERT	(0x44)
#define DS1820_CMD_READSCR	(0xBE)
#define DS1820_CMD_CONFIG	(0x4E)

#define DS1820_TH_VALUE		(0x4B) //75
#define DS1820_TL_VALUE		(0x00) //0
#define DS1820_9_BIT_RESOLUTION		(0x1F)
#define DS1820_10_BIT_RESOLUTION	(0x3F)
#define DS1820_11_BIT_RESOLUTION	(0x5F)
#define DS1820_12_BIT_RESOLUTION	(0x7F)

#define DS1820_9_BIT_RES_MDEALY		(94)
#define DS1820_10_BIT_RES_MDEALY	(188)
#define DS1820_11_BIT_RES_MDEALY	(375)
#define DS1820_12_BIT_RES_MDEALY	(750)
 
#define make_dq_out() 	gpio_direction_output(ds1820_devp->dq_gpio, 1)
#define make_dq_in() 	gpio_direction_input(ds1820_devp->dq_gpio)
#define set_dq(_val)	gpio_set_value(ds1820_devp->dq_gpio, _val)
#define get_dq()		gpio_get_value(ds1820_devp->dq_gpio)

#define DATA_CHECK_MAX_TIMES	10
#define DEVICE_NAME "ds1820"
#define MAX_CONVERT_WAIT_TIMES	7
#define DS18B20_DATA_LEN	9
#define ds1820_existed() ds1820_reset()

struct ds1820_device
{
	struct mutex res_mutex;
	struct cdev cdev;
	int dq_gpio;
	int vcc_gpio;
};
static int ds1820_major = 0;
static struct ds1820_device *ds1820_devp;
static struct class *my_class;
static spinlock_t lock_ds1820;
static unsigned char current_resolvetion = DS1820_9_BIT_RESOLUTION;

static const unsigned char ds18b20_crc_table[] =
{
	0x00,0x5E,0xBC,0xE2,0x61,0x3F,0xDD,0x83,0xC2,0x9C,0x7E,0x20,0xA3,0xFD,0x1F,0x41,
	0x9D,0xC3,0x21,0x7F,0xFC,0xA2,0x40,0x1E,0x5F,0x01,0xE3,0xBD,0x3E,0x60,0x82,0xDC,
	0x23,0x7D,0x9F,0xC1,0x42,0x1C,0xFE,0xA0,0xE1,0xBF,0x5D,0x03,0x80,0xDE,0x3C,0x62,
	0xBE,0xE0,0x02,0x5C,0xDF,0x81,0x63,0x3D,0x7C,0x22,0xC0,0x9E,0x1D,0x43,0xA1,0xFF,
	0x46,0x18,0xFA,0xA4,0x27,0x79,0x9B,0xC5,0x84,0xDA,0x38,0x66,0xE5,0xBB,0x59,0x07,
	0xDB,0x85,0x67,0x39,0xBA,0xE4,0x06,0x58,0x19,0x47,0xA5,0xFB,0x78,0x26,0xC4,0x9A,
	0x65,0x3B,0xD9,0x87,0x04,0x5A,0xB8,0xE6,0xA7,0xF9,0x1B,0x45,0xC6,0x98,0x7A,0x24,
	0xF8,0xA6,0x44,0x1A,0x99,0xC7,0x25,0x7B,0x3A,0x64,0x86,0xD8,0x5B,0x05,0xE7,0xB9,
	0x8C,0xD2,0x30,0x6E,0xED,0xB3,0x51,0x0F,0x4E,0x10,0xF2,0xAC,0x2F,0x71,0x93,0xCD,
	0x11,0x4F,0xAD,0xF3,0x70,0x2E,0xCC,0x92,0xD3,0x8D,0x6F,0x31,0xB2,0xEC,0x0E,0x50,
	0xAF,0xF1,0x13,0x4D,0xCE,0x90,0x72,0x2C,0x6D,0x33,0xD1,0x8F,0x0C,0x52,0xB0,0xEE,
	0x32,0x6C,0x8E,0xD0,0x53,0x0D,0xEF,0xB1,0xF0,0xAE,0x4C,0x12,0x91,0xCF,0x2D,0x73,
	0xCA,0x94,0x76,0x28,0xAB,0xF5,0x17,0x49,0x08,0x56,0xB4,0xEA,0x69,0x37,0xD5,0x8B,
	0x57,0x09,0xEB,0xB5,0x36,0x68,0x8A,0xD4,0x95,0xCB,0x29,0x77,0xF4,0xAA,0x48,0x16,
	0xE9,0xB7,0x55,0x0B,0x88,0xD6,0x34,0x6A,0x2B,0x75,0x97,0xC9,0x4A,0x14,0xF6,0xA8,
	0x74,0x2A,0xC8,0x96,0x15,0x4B,0xA9,0xF7,0xB6,0xE8,0x0A,0x54,0xD7,0x89,0x6B,0x35
};

unsigned char ds18b20_cal_crc_table(unsigned char *ptr, unsigned char len) 
{
    unsigned char  crc = 0x00;
 
    while (len--)
    {
        crc = ds18b20_crc_table[crc ^ *ptr++];
    }
    return (crc);
}

static unsigned int ds1820_reset(void)
{
	unsigned int ret = 1, i = 0;
	preempt_disable();
	spin_lock_irq(&lock_ds1820);
	make_dq_out();
	set_dq(0);	/* set dq low to info slave */
	udelay(500);	/* hold for 480~960us */
	set_dq(1);	/* release dq */
	make_dq_in();
	if(get_dq())
	{
		//udelay(60);	/* wait for 60us, ds1820 will hold dq low for 240us */
		do {
			udelay(10);
			ret = get_dq();/* ds1820 pulldown dq */
		} while(ret > 0 && i++ < 25);

		ret = get_dq();/* ds1820 pulldown dq */
		udelay(500);
		make_dq_out();
		set_dq(1);
	}
	spin_unlock_irq(&lock_ds1820);
	preempt_enable();
	return ret;
}

unsigned int ds1820_read_bit(void)
{
	make_dq_out();
	set_dq(1);
	udelay(2);
	set_dq(0);
	udelay(3);
	set_dq(1);
	udelay(5);
	make_dq_in();

	return get_dq();
}

unsigned char ds1820_read_byte(void)
{
	unsigned char temp = 0;
	unsigned int i;
	for(i=0; i<8; i++){
		if( ds1820_read_bit() ){
				temp |= (0x01<<i);
		}
		udelay(60);
	}

	return temp;
}

void ds1820_write_bit(char bitValue)
{
	make_dq_out();
	set_dq(0);
	udelay(15);
	if( bitValue == 1 ){
			set_dq(1);
	}else{
			set_dq(0); 
	}
	udelay(45);
	set_dq(1);
}

void ds1820_write_byte(char cmd)
{
	unsigned char i;
	unsigned char temp;

	for(i=0; i<8;i++){
			temp = cmd>>i;
			temp &= 0x01; 
			ds1820_write_bit(temp);
	}
}

void ds1820_init_config(void)
{
	preempt_disable();
	spin_lock_irq(&lock_ds1820);
	ds1820_write_byte(DS1820_CMD_SKIP_ROM);
	ds1820_write_byte(DS1820_CMD_CONFIG);
	ds1820_write_byte(DS1820_TH_VALUE);//TH Max
	ds1820_write_byte(DS1820_TL_VALUE);//TL Min
	ds1820_write_byte(current_resolvetion);//resolution	
	spin_unlock_irq(&lock_ds1820);
	preempt_enable();
}

static int ds1820_open(struct inode *inode, struct file *filp)
{
	struct ds1820_device *dev;
 
	dev = container_of(inode->i_cdev, struct ds1820_device, cdev);
	filp->private_data = dev;
	return 0;
}
 
static int ds1820_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t ds1820_read(struct file *filp, char *buf, size_t count,
		loff_t *pos)
{
	unsigned char tmp[DS18B20_DATA_LEN] = {0};
	unsigned int i;
	int ret, try = DATA_CHECK_MAX_TIMES;
	unsigned int err;
	struct ds1820_device *dev;
	unsigned char crc8;

	dev = filp->private_data;
	ret = mutex_lock_interruptible(&dev->res_mutex);

	ds1820_reset();
	ds1820_init_config();

	ds1820_reset();
	/* send temperature convert command */
	preempt_disable();
	spin_lock_irq(&lock_ds1820);
	ds1820_write_byte(DS1820_CMD_SKIP_ROM);
	ds1820_write_byte(DS1820_CMD_CONVERT);
	spin_unlock_irq(&lock_ds1820);
	preempt_enable();
	
	mutex_unlock(&dev->res_mutex);
	
	/* wait convert finish */
	switch(current_resolvetion)
	{
		case DS1820_9_BIT_RESOLUTION:mdelay(DS1820_9_BIT_RES_MDEALY);break;
		case DS1820_10_BIT_RESOLUTION:mdelay(DS1820_10_BIT_RES_MDEALY);break;
		case DS1820_11_BIT_RESOLUTION:mdelay(DS1820_11_BIT_RES_MDEALY);break;
		case DS1820_12_BIT_RESOLUTION:mdelay(DS1820_12_BIT_RES_MDEALY);break;
		default:break;
	}

	ret =  mutex_lock_interruptible(&dev->res_mutex);

retry:
	ds1820_reset();
	/* send read scratchpad command */
	preempt_disable();
	spin_lock_irq(&lock_ds1820);
	ds1820_write_byte(DS1820_CMD_SKIP_ROM);
	ds1820_write_byte(DS1820_CMD_READSCR);

	/*read temp*/
	for(i=0; i<DS18B20_DATA_LEN; i++)
	{
		tmp[i] = ds1820_read_byte();
	}

	spin_unlock_irq(&lock_ds1820);	
	preempt_enable();

	/*crc*/
	crc8 = ds18b20_cal_crc_table(tmp, DS18B20_DATA_LEN-1);
	if((--try) && (crc8 != tmp[DS18B20_DATA_LEN-1]))
	{
		//printk("ds18b20 crc err %x %x try = %d\r\n", crc8, tmp[DS18B20_DATA_LEN-1], try);
		goto retry;
	}

	/*singal bit check， tmp[1] only equal to 00000xxx or 11111xxx */
	if((--try) &&
		((tmp[1]&0xf8)!=0xf8) && 
			((tmp[1]>>3)!=0))
	{
		printk("ds18b20 signal err %x try = %d\r\n", tmp[1], try);
		goto retry;
	}
	
	if(try <= 0)
	{
		printk("ds18b20 data try_max exit ..\r\n");
		mutex_unlock(&dev->res_mutex);
		return -EFAULT;
	}

	err = copy_to_user(buf, tmp, sizeof(tmp));
	mutex_unlock(&dev->res_mutex);
	return err? -EFAULT: min(sizeof(tmp), count);
}

static const struct file_operations ds1820_fops = {
    .owner 	= THIS_MODULE,
    .open	= ds1820_open,
    .release= ds1820_release,
    .read	= ds1820_read,
};

static int ds1820_probe(struct platform_device *pdev) {
	struct device_node *ds1820_node = pdev->dev.of_node;
	int result;
	dev_t devno = 0;
	
	spin_lock_init(&lock_ds1820);
	
	if (ds1820_major) {
		devno = MKDEV(ds1820_major, 0);
		result = register_chrdev_region(devno, 1, DEVICE_NAME);
	} else {
		result = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
		ds1820_major = MAJOR(devno);
	}
 
	if (result < 0) {
		printk(KERN_ERR "ds1820: init error!\n");
		return result;
	}
 
	ds1820_devp = kzalloc(sizeof(struct ds1820_device), GFP_KERNEL);
	if (!ds1820_devp) {
		result = -ENOMEM;
		goto fail;
	}
 
	ds1820_devp->dq_gpio = of_get_named_gpio_flags(ds1820_node, "ds1820-dq", 0, 0);
	if (!gpio_is_valid(ds1820_devp->dq_gpio)) { 
		printk("ds1820-dq gpio: %d is invalid\n", ds1820_devp->dq_gpio);
		goto fail; 
    } 
    if (gpio_request(ds1820_devp->dq_gpio, "ds1820-dq")) { 
		printk("ds1820-dq gpio %d request failed!\n", ds1820_devp->dq_gpio);
        gpio_free(ds1820_devp->dq_gpio); 
        goto fail;
    }

    gpio_direction_output(ds1820_devp->dq_gpio, 1); 
	
	ds1820_devp->vcc_gpio = of_get_named_gpio_flags(ds1820_node, "vcc-gpio", 0, 0);
	if (!gpio_is_valid(ds1820_devp->vcc_gpio)) { 
		printk("ds1820 vcc-gpio: %d is invalid\n", ds1820_devp->vcc_gpio);
    } 
	else
	{
		if (gpio_request(ds1820_devp->vcc_gpio, "ds1820-vcc_gpio")) { 
			printk("ds1820 vcc-gpio %d request failed!\n", ds1820_devp->vcc_gpio);
			gpio_free(ds1820_devp->vcc_gpio); 
			goto fail;
		} 
		gpio_direction_output(ds1820_devp->vcc_gpio, 1);
	}

	if(ds1820_existed())
	{
		printk (KERN_INFO"%s not exist! free ds1820-dq gpio:%d now\n", DEVICE_NAME, ds1820_devp->dq_gpio);
		gpio_free(ds1820_devp->dq_gpio);

		ds1820_devp->dq_gpio = of_get_named_gpio_flags(ds1820_node, "ds1820-dq-1", 0, 0);
		printk (KERN_INFO"try request another ds1820-dq-1 gpio:%d\n", ds1820_devp->dq_gpio);
		if (!gpio_is_valid(ds1820_devp->dq_gpio)) {
			printk("ds1820-dq-1 gpio: %d is invalid\n", ds1820_devp->dq_gpio);
			goto fail;
		}
		if (gpio_request(ds1820_devp->dq_gpio, "ds1820-dq")) {
			printk("ds1820-dq-1 gpio %d request failed!\n", ds1820_devp->dq_gpio);
			gpio_free(ds1820_devp->dq_gpio);
			goto fail;
		}

		gpio_direction_output(ds1820_devp->dq_gpio, 1);

		if(ds1820_existed())
		{
			printk (KERN_INFO"%s not exist! free ds1820-dq-1 gpio:%d now\n", DEVICE_NAME, ds1820_devp->dq_gpio);
			gpio_free(ds1820_devp->dq_gpio);
			if (gpio_is_valid(ds1820_devp->vcc_gpio)) {
				gpio_free(ds1820_devp->vcc_gpio);
			}
			goto fail;
		}
	}

	mutex_init(&ds1820_devp->res_mutex);
	/* setup cdev */
	cdev_init(&ds1820_devp->cdev, &ds1820_fops);
	ds1820_devp->cdev.owner = THIS_MODULE;
	ds1820_devp->cdev.ops = &ds1820_fops;
	if (cdev_add(&ds1820_devp->cdev, devno, 1))
		printk(KERN_NOTICE "Error adding ds1820 cdev!\n");

	/* setup device node */
	my_class = class_create(THIS_MODULE, "ds1820_class");
	device_create(my_class, NULL, MKDEV(ds1820_major, 0), \
				NULL, DEVICE_NAME);

    printk (KERN_INFO"%s init sucess!\n", DEVICE_NAME);
	return 0;

fail:
	unregister_chrdev_region(devno, 1);
	return result;
}

static int ds1820_remove(struct platform_device *dev) {
	dev_t devno = MKDEV(ds1820_major, 0);
 
	if (ds1820_devp)
		cdev_del(&ds1820_devp->cdev);
 
	gpio_free(ds1820_devp->dq_gpio);
	gpio_free(ds1820_devp->vcc_gpio);
	kfree(ds1820_devp);
	device_destroy(my_class, devno);
	class_destroy(my_class);

    printk (KERN_INFO"%s removed\n", DEVICE_NAME);
    return 0;
}

static const struct of_device_id of_ds1820_match[] = {
    { .compatible = "firefly,ds1820" },
};

static struct platform_driver ds1820_driver = {
    .probe      = ds1820_probe,
    .remove     = ds1820_remove,
    .driver     = {
        .owner  = THIS_MODULE,
        .of_match_table = of_ds1820_match,
        .name   = "ds1820",
    },
};

static int __init ds1820_init(void) {
    return platform_driver_register(&ds1820_driver);
}

static void __exit ds1820_exit(void) {
    platform_driver_unregister(&ds1820_driver);
}

module_init(ds1820_init);
module_exit(ds1820_exit);

MODULE_AUTHOR("lsd <service@t-firefly.com>");
MODULE_LICENSE("GPL");

