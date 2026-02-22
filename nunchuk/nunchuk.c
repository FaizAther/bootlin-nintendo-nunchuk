// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>

#include <linux/platform_device.h>

#include <linux/of.h>


struct device;

struct nunchuk_dev {
	struct i2c_client *i2c_client;
};

// extern const struct regmap_config nunchuk_regmap;
int nunchuk_probe(struct device *dev, struct regmap *regmap);
int nunchuk_remove(struct device *dev);
int __maybe_unused nunchuk_suspend(struct device *dev);
int __maybe_unused nunchuk_resume(struct device *dev);

static const struct i2c_device_id nunchuk_i2c_id[] = {
	{ "nunchuk" },
	{ }
};

// static int __init nunchuk_init(void)
// {
// 	pr_alert("%s\n", __func__);
// 	return 0;
// }

// static void __exit nunchuk_exit(void)
// {
// 	pr_alert("%s\n", __func__);
// }

int __maybe_unused nunchuk_suspend(struct device *dev)
{
	pr_alert("%s\n", __func__);
	return 0;
}

int __maybe_unused nunchuk_resume(struct device *dev)
{
	pr_alert("%s\n", __func__);
	return 0;
}

const struct of_device_id nunchuk_of_match[] = {
	{ .compatible = "nintendo,nunchuk", },
	{ }
};
MODULE_DEVICE_TABLE(of, nunchuk_of_match);

struct nunchuk_pressed {
	unsigned x;
	unsigned y;
	bool z;
	bool c;
	int err;
};

static struct nunchuk_pressed nunchuk_read_registers(struct i2c_client *i2c)
{
	pr_alert("%s\n", __func__);
	struct nunchuk_pressed nunchuk_pressed = {0};

	fsleep(10 * 1000); // 10 ms delay between i2c operations

	const char buf_s[] = {0x00};
	if ((nunchuk_pressed.err = i2c_master_send(i2c, &buf_s[0], 1)) < 0) {
		pr_alert("Failed to send read command: %d\n", nunchuk_pressed.err);
		return nunchuk_pressed;
	}

	fsleep(10 * 1000); // 10 ms delay between i2c operations

	char buf_r[6];
	if ((nunchuk_pressed.err = i2c_master_recv(i2c, buf_r, 6)) < 0) {
		pr_alert("Failed to read data: %d\n", nunchuk_pressed.err);
		return nunchuk_pressed;
	}
	// int i = 0;
	// do {
	// 	// pr_info("Read data: %d: {%x}\n", i, buf_r[i]);
	// } while(++i < 6);
	nunchuk_pressed.x = (unsigned)buf_r[0];
	nunchuk_pressed.y = (unsigned)buf_r[1];
	nunchuk_pressed.z = (buf_r[5] & 0x01) == 0;
	nunchuk_pressed.c = (buf_r[5] & 0x02) == 0;
	pr_info("x{0x%02X} button\n", nunchuk_pressed.x);
	pr_info("y{0x%02X} button\n", nunchuk_pressed.y);
	if (nunchuk_pressed.z) {
		pr_info("Z button is pressed\n");
	}
	if (nunchuk_pressed.c) {
		pr_info("C button is pressed\n");
	}
	return nunchuk_pressed;
}

void nunchuk_poll(struct input_dev *input)
{
	pr_alert("%s\n", __func__);
	struct nunchuk_dev *nunchuk = input_get_drvdata(input);
	struct nunchuk_pressed pressed = nunchuk_read_registers(nunchuk->i2c_client);
	if (pressed.err < 0) {
		pr_alert("Failed to read registers: %d\n", pressed.err);
		return;
	}
	input_report_key(input, ABS_X, pressed.x);
	input_report_key(input, ABS_Y, pressed.y);
	input_report_key(input, BTN_C, pressed.c);
	input_report_key(input, BTN_Z, pressed.z);
/*
	307 (0x133)	BTN_NORTH	BTN_X TOP		x=0x7c,y=0xff +- 5
	304 (0x130)	BTN_SOUTH	BTN_A BOTTOM	x=0x7c,y=0x00 +- 5
	305 (0x131)	BTN_EAST	BTN_B RIGHT		x=0xfe,y=0x80 +- 5
	308 (0x134)	BTN_WEST	BTN_Y LEFT		x=0x00,y=0x80 +- 5
*/
	// 0x80,0x80 center.
	if (pressed.x >= 0x77 && pressed.x <= 0x81 && pressed.y >= 0xf5 && pressed.y <= 0xff) {
		// input_report_key(input, BTN_X, 1);
		// input_report_key(input, KEY_UP, 1);
	} else {
		// input_report_key(input, BTN_X, 0);
		// input_report_key(input, KEY_UP, 0);
	}
	if (pressed.x >= 0x77 && pressed.x <= 0x81 && pressed.y >= 0x00 && pressed.y <= 0x05) {
		// input_report_key(input, BTN_A, 1);
		// input_report_key(input, KEY_DOWN, 1);
	} else {
		// input_report_key(input, BTN_A, 0);
		// input_report_key(input, KEY_DOWN, 0);
	}
	if (pressed.x >= 0xf5 && pressed.x <= 0xff && pressed.y >= 0x77 && pressed.y <= 0x81) {
		// input_report_key(input, BTN_B, 1);
		// input_report_key(input, KEY_RIGHT, 1);
	} else {
		// input_report_key(input, BTN_B, 0);
		// input_report_key(input, KEY_RIGHT, 0);
	}
	if (pressed.x >= 0x00 && pressed.x <= 0x05 && pressed.y >= 0x77 && pressed.y <= 0x81) {
		// input_report_key(input, BTN_Y, 1);
		// input_report_key(input, KEY_LEFT, 1);
	} else {
		// input_report_key(input, BTN_Y, 0);
		// input_report_key(input, KEY_LEFT, 0);
	}

	input_sync(input);
}

int nunchuk_probe(struct device *dev, struct regmap *regmap)
{
	pr_alert("%s\n", __func__);
	struct input_dev *input;
	struct nunchuk_dev *nunchuk;
	int err;
	// struct input_dev __must_check *devm_input_allocate_device(struct device *);
	input = devm_input_allocate_device(dev);
	if (!input) {
		pr_alert("Failed to allocate input device\n");
		return -ENOMEM;
	}
	input->name = "Nintendo Wii Nunchuk";
	input->id.bustype = BUS_I2C;
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);
	
	set_bit(ABS_X, input->absbit);
	set_bit(ABS_Y, input->absbit);
	input_set_abs_params(input, ABS_X, 0, 255, 4, 8);
	input_set_abs_params(input, ABS_Y, 0, 255, 4, 8);

	/* Classic buttons */

	// set_bit(BTN_TL, input->keybit);
	// set_bit(BTN_SELECT, input->keybit);
	// set_bit(BTN_MODE, input->keybit);
	// set_bit(BTN_START, input->keybit);
	// set_bit(BTN_TR, input->keybit);
	// set_bit(BTN_TL2, input->keybit);
	// set_bit(BTN_B, input->keybit);
	// set_bit(BTN_Y, input->keybit);
	// set_bit(BTN_A, input->keybit);
	// set_bit(BTN_X, input->keybit);
	// set_bit(BTN_TR2, input->keybit);

	// set_bit(KEY_LEFT, input->keybit);
	// set_bit(KEY_RIGHT, input->keybit);
	// set_bit(KEY_UP, input->keybit);
	// set_bit(KEY_DOWN, input->keybit);

	// int input_setup_polling(struct input_dev *dev,
	// 		void (*poll_fn)(struct input_dev *dev));

	nunchuk = devm_kzalloc(dev, sizeof(*nunchuk), GFP_KERNEL);
	if (!nunchuk)
		return -ENOMEM;
	nunchuk->i2c_client = to_i2c_client(dev);
	input_set_drvdata(input, nunchuk);

	if ((err = input_setup_polling(input, nunchuk_poll)) < 0) {
		pr_alert("Failed to setup polling: %d\n", err);
		return err;
	}
	//int __must_check input_register_device(struct input_dev *);
	if ((err = input_register_device(input)) < 0) {
		pr_alert("Failed to register input device: %d\n", err);
		return err;
	}
	return 0;
}

static int nunchuk_i2c_probe(struct i2c_client *i2c)
{
	// struct regmap_config config;
	pr_alert("%s\n", __func__);
	int err;

	unsigned long us_delay[] = {0 * 1000, 0 * 1000, 1 * 1000};
	char buf[] = {0x40, 0x00, 0x00};

	pr_alert("i2c addr: %x\n", i2c->addr);
	int i = 0;
	do {
		pr_info("Sending init command: %x\n", buf[i]);
		int err;
		if ((err = i2c_master_send(i2c, &buf[i], 1)) < 0) {
			pr_alert("Failed to send init command: %d\n", err);
			return err;
		}
		pr_info("sleeping for %lu us\n", us_delay[i]);
		fsleep(us_delay[i]);
	} while (++i < 3);

	if ((err = nunchuk_read_registers(i2c).err) < 0) {
		pr_alert("Failed to read registers: %d\n", err);
		return err;
	}
	if ((err = nunchuk_read_registers(i2c).err) < 0) {
		pr_alert("Failed to read registers: %d\n", err);
		return err;
	}

	// config = nunchuk_regmap;
	return nunchuk_probe(&i2c->dev, NULL);


	// return nunchuk_probe(&i2c->dev, devm_regmap_init_i2c(i2c, &config));
}

int nunchuk_remove(struct device *dev)
{
	pr_alert("%s\n", __func__);
	void input_unregister_device(struct input_dev *);
	return 0;
}

static void nunchuk_i2c_remove(struct i2c_client *i2c)
{
	pr_alert("%s\n", __func__);
	nunchuk_remove(&i2c->dev);
}

static const struct dev_pm_ops nunchuk_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(nunchuk_suspend, nunchuk_resume)
};

static struct i2c_driver nunchuk_i2c_driver = {
	.driver = {
		.name = "nunchuk",
		.of_match_table = nunchuk_of_match,
		.pm = &nunchuk_pm_ops,
	},
	.probe = nunchuk_i2c_probe,
	.remove = nunchuk_i2c_remove,
	.id_table = nunchuk_i2c_id,
};

module_i2c_driver(nunchuk_i2c_driver);

// module_init(nunchuk_init);
// module_exit(nunchuk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Nunchuk");
MODULE_AUTHOR("Faiz Ather");
