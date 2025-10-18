// lcd_i2c.c - Kernel driver for HD44780 LCD with character device interface
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define DRIVER_NAME "lcd-i2c"
#define DEVICE_NAME "lcd0"

// LCD Commands
#define LCD_CHR 1
#define LCD_CMD 0
#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04

struct lcd_data {
    struct i2c_client *client;
    u8 backlight;
};

//static int major;
//static char text[64];

static dev_t dev_num;
static struct i2c_client *global_client;
static struct class *lcd_class;
static struct cdev lcd_cdev;

static void lcd_clear(struct i2c_client *client);
static void lcd_write_string(struct i2c_client *client, const char *str, u8 row);

static ssize_t lcd_write (struct file *filp, const char __user *user_buf, size_t len, loff_t *off){
	int row = 0;
    char kernel_buf[81];  // 20x4 = 80 chars + null terminator
    size_t to_copy;
    to_copy = len > 80 ? 80 : len;
    if (copy_from_user(kernel_buf, user_buf, to_copy))
        return -EFAULT;
    
    kernel_buf[to_copy] = '\0';
    if (!global_client)
        return -ENODEV;
    if (strcmp(kernel_buf, "clear") == 0) {
        lcd_clear(global_client);
        return to_copy;
    }
    switch (kernel_buf[0]) {
		case '0':
		case '1':
        case '2':
        case '3':
			row =  kernel_buf[0] - '0';
		default:
			break;
	}
    if(to_copy > 2){
        lcd_write_string(global_client, kernel_buf + 2, row);
    }
	
	return to_copy;
}


// File operations
static struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
};

// Function to toggle enable bit
static void lcd_toggle_enable(struct i2c_client *client, u8 bits)
{
    i2c_smbus_write_byte(client, bits | ENABLE);
    udelay(500);
    i2c_smbus_write_byte(client, bits & ~ENABLE);
    udelay(500);
}

// Send byte to LCD
static void lcd_byte(struct i2c_client *client, u8 bits, u8 mode)
{
    u8 bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    u8 bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;
    
    // High nibble
    i2c_smbus_write_byte(client, bits_high);
    lcd_toggle_enable(client, bits_high);
    
    // Low nibble
    i2c_smbus_write_byte(client, bits_low);
    lcd_toggle_enable(client, bits_low);
}

// Initialize LCD
static int lcd_init(struct i2c_client *client)
{
    msleep(50);
    
    // 4-bit mode initialization
    lcd_byte(client, 0x33, LCD_CMD);
    msleep(5);
    lcd_byte(client, 0x32, LCD_CMD);
    msleep(5);
    
    // Display configuration
    lcd_byte(client, 0x28, LCD_CMD);  // 2 line, 5x8 matrix
    msleep(5);
    lcd_byte(client, 0x0C, LCD_CMD);  // Display on, cursor off
    msleep(5);
    lcd_byte(client, 0x06, LCD_CMD);  // Entry mode
    msleep(5);
    lcd_byte(client, 0x01, LCD_CMD);  // Clear display
    msleep(5);
    
    return 0;
}

// Write character to LCD
static void lcd_write_char(struct i2c_client *client, char c)
{
    lcd_byte(client, c, LCD_CHR);
}

// Clear display
static void lcd_clear(struct i2c_client *client)
{
    lcd_byte(client, 0x01, LCD_CMD);
    msleep(2);
}

// Set cursor position (col, row)
static void lcd_set_cursor(struct i2c_client *client, u8 col, u8 row)
{
    u8 row_offsets[] = {0x00, 0x40, 0x14, 0x54};  // For 20x4 display
    u8 pos;
    
    if (row < 4) {
        pos = 0x80 | (col + row_offsets[row]);
        lcd_byte(client, pos, LCD_CMD);
    }
}

// Write string to LCD at specified row
static void lcd_write_string(struct i2c_client *client, const char *str, u8 row)
{
    lcd_set_cursor(client, 0, row);
    while (*str) {
        if(*str != '\0' && *str != '\n')
        lcd_write_char(client, *str++);
    }
}

// Probe function - called when device is found
static int lcd_probe(struct i2c_client *client)
{
    struct lcd_data *lcd;
    int ret;
    
    dev_info(&client->dev, "LCD I2C driver probing\n");
    
    // Allocate private data
    lcd = devm_kzalloc(&client->dev, sizeof(*lcd), GFP_KERNEL);
    if (!lcd)
        return -ENOMEM;
    
    lcd->client = client;
    lcd->backlight = LCD_BACKLIGHT;
    i2c_set_clientdata(client, lcd);
    
    // Initialize LCD
    ret = lcd_init(client);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to initialize LCD\n");
        return ret;
    }
    global_client = client;
    
    // Write to all 4 rows
    lcd_write_string(client, "Row 1: A", 0);
    lcd_write_string(client, "Row 2: B", 1);
    lcd_write_string(client, "Row 3: C", 2);
    lcd_write_string(client, "Row 4: D", 3);
    
    dev_info(&client->dev, "LCD I2C driver initialized successfully\n");
    
    return 0;
}

// Remove function - called when device is removed
static void lcd_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "LCD I2C driver removed\n");
    lcd_clear(client);
    global_client = NULL;
}

// Device tree match table
static const struct of_device_id lcd_of_match[] = {
    { .compatible = "hd44780,lcd-i2c" },
    { }
};
MODULE_DEVICE_TABLE(of, lcd_of_match);

// I2C device ID table
static const struct i2c_device_id lcd_id[] = {
    { "lcd-i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_id);

// I2C driver structure
static struct i2c_driver lcd_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = lcd_of_match,
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
    .id_table = lcd_id,
};

// Module init function
static int __init lcd_module_init(void)
{
    int ret;
    struct device *dev_ret;
    
    pr_info("LCD I2C module initializing\n");
    
    // Allocate character device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }
    
    // Create device class
    lcd_class = class_create(DEVICE_NAME);
    if (IS_ERR(lcd_class)) {
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create class\n");
        return PTR_ERR(lcd_class);
    }
    
    // Create device file
    dev_ret = device_create(lcd_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dev_ret)) {
        class_destroy(lcd_class);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create device\n");
        return PTR_ERR(dev_ret);
    }
    
    // Initialize and add character device
    cdev_init(&lcd_cdev, &lcd_fops);
    ret = cdev_add(&lcd_cdev, dev_num, 1);
    if (ret < 0) {
        device_destroy(lcd_class, dev_num);
        class_destroy(lcd_class);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to add cdev\n");
        return ret;
    }
    
    // Register I2C driver
    ret = i2c_add_driver(&lcd_driver);
    if (ret < 0) {
        cdev_del(&lcd_cdev);
        device_destroy(lcd_class, dev_num);
        class_destroy(lcd_class);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to register I2C driver\n");
        return ret;
    }
    
    pr_info("LCD I2C module loaded, device: /dev/%s\n", DEVICE_NAME);
    
    return 0;
}

// Module exit function
static void __exit lcd_module_exit(void)
{
    pr_info("LCD I2C module exiting\n");
    
    i2c_del_driver(&lcd_driver);
    cdev_del(&lcd_cdev);
    device_destroy(lcd_class, dev_num);
    class_destroy(lcd_class);
    unregister_chrdev_region(dev_num, 1);
    
    pr_info("LCD I2C module unloaded\n");
}

module_init(lcd_module_init);
module_exit(lcd_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("HD44780 LCD with PCF8574 I2C driver with char device");
MODULE_VERSION("1.0");