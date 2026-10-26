/*
 * Anbernic RG35XX (original) gamepad: buttons on SoC GPIOs, polled.
 *
 * The input device matches the one the Garlic OS kernel exposes ("RG35XX
 * Gamepad"), so SDL and RetroArch see the same buttons and axes:
 * - the d-pad is hat 0 (ABS_HAT0X/ABS_HAT0Y, -1 = left/up)
 * - L2/R2 are ABS_Z/ABS_RZ, 0 or 255
 * - every other button is a key (BTN_A, BTN_SELECT, ...)
 * ABS_X/Y/RX/RY and BTN_Z are declared but never sent: SDL numbers axes and
 * buttons in code order, and they keep L2/R2 at axes 2/5 and L1 at button 5.
 *
 * Device tree:
 *	gamepad {
 *		compatible = "anbernic,rg35xx-gamepad";
 *		poll-interval = <10>;		(ms)
 *		gpios = <&gpio 86 1>, ...;	(flag 1 = active low)
 *		linux,codes = <KEY_UP ...>;	(one per GPIO)
 *	};
 * Several GPIOs may share a code: board revisions wire START, SELECT, MENU
 * and the volume keys to different pins, and the unused pins read active.
 * At probe, a GPIO that reads active while another one with its code does
 * not is dropped (Garlic's driver picks the pin set the same way, on its
 * first poll). Of the GPIOs left, the button is pressed when any is active.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define DRV_NAME "rg35xx-gamepad"
#define DEFAULT_POLL_INTERVAL 10

struct rg35xx_button {
	unsigned int gpio;
	bool active_low;
	unsigned int code;
};

struct rg35xx_gamepad {
	struct input_polled_dev *poll_dev;
	int nbuttons;
	struct rg35xx_button buttons[];
};

static bool rg35xx_active(struct rg35xx_button *b)
{
	return !!gpio_get_value_cansleep(b->gpio) != b->active_low;
}

/* The d-pad and L2/R2 are reported as axes, not keys */
static bool rg35xx_is_axis(unsigned int code)
{
	switch (code) {
	case KEY_UP:
	case KEY_DOWN:
	case KEY_LEFT:
	case KEY_RIGHT:
	case BTN_TL2:
	case BTN_TR2:
		return true;
	}
	return false;
}

static void rg35xx_gamepad_poll(struct input_polled_dev *poll_dev)
{
	struct rg35xx_gamepad *pad = poll_dev->private;
	struct input_dev *input = poll_dev->input;
	DECLARE_BITMAP(pressed, KEY_CNT) = { 0 };
	int i;

	/* A code is pressed when any of its GPIOs is active */
	for (i = 0; i < pad->nbuttons; i++) {
		struct rg35xx_button *b = &pad->buttons[i];

		if (rg35xx_active(b))
			__set_bit(b->code, pressed);
	}

	/* The input core drops repeated reports of an unchanged state */
	for (i = 0; i < pad->nbuttons; i++) {
		unsigned int code = pad->buttons[i].code;

		if (!rg35xx_is_axis(code))
			input_report_key(input, code, test_bit(code, pressed));
	}
	input_report_abs(input, ABS_HAT0X, test_bit(KEY_RIGHT, pressed) -
			 test_bit(KEY_LEFT, pressed));
	input_report_abs(input, ABS_HAT0Y, test_bit(KEY_DOWN, pressed) -
			 test_bit(KEY_UP, pressed));
	input_report_abs(input, ABS_Z, test_bit(BTN_TL2, pressed) ? 255 : 0);
	input_report_abs(input, ABS_RZ, test_bit(BTN_TR2, pressed) ? 255 : 0);
	input_sync(input);
}

/* Drops the GPIOs of the other board revision (see the top comment) */
static void rg35xx_drop_unused(struct device *dev, struct rg35xx_gamepad *pad)
{
	bool active[pad->nbuttons];
	int i, j, n = 0;

	for (i = 0; i < pad->nbuttons; i++)
		active[i] = rg35xx_active(&pad->buttons[i]);

	for (i = 0; i < pad->nbuttons; i++) {
		struct rg35xx_button *b = &pad->buttons[i];
		bool unused = false;

		for (j = 0; j < pad->nbuttons && active[i]; j++)
			if (pad->buttons[j].code == b->code && !active[j])
				unused = true;
		if (unused) {
			dev_info(dev, "gpio %u unused (reads active)\n", b->gpio);
			devm_gpio_free(dev, b->gpio);
			continue;
		}
		pad->buttons[n++] = *b;
	}
	pad->nbuttons = n;
}

static int rg35xx_gamepad_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rg35xx_gamepad *pad;
	struct input_polled_dev *poll_dev;
	struct input_dev *input;
	u32 poll_interval = DEFAULT_POLL_INTERVAL;
	struct property *codes;
	int nbuttons, i, error;

	if (!np)
		return -ENODEV;

	nbuttons = of_gpio_count(np);
	if (nbuttons <= 0) {
		dev_err(dev, "no gpios\n");
		return -EINVAL;
	}
	codes = of_find_property(np, "linux,codes", NULL);
	if (!codes || codes->length != nbuttons * sizeof(u32)) {
		dev_err(dev, "linux,codes must have one code per gpio\n");
		return -EINVAL;
	}
	of_property_read_u32(np, "poll-interval", &poll_interval);

	pad = devm_kzalloc(dev, sizeof(*pad) +
			   nbuttons * sizeof(struct rg35xx_button), GFP_KERNEL);
	if (!pad)
		return -ENOMEM;
	pad->nbuttons = nbuttons;

	poll_dev = input_allocate_polled_device();
	if (!poll_dev)
		return -ENOMEM;
	pad->poll_dev = poll_dev;
	poll_dev->private = pad;
	poll_dev->poll = rg35xx_gamepad_poll;
	poll_dev->poll_interval = poll_interval;

	input = poll_dev->input;
	input->name = "RG35XX Gamepad";
	input->phys = DRV_NAME "/input0";
	input->dev.parent = dev;
	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	for (i = 0; i < nbuttons; i++) {
		struct rg35xx_button *b = &pad->buttons[i];
		enum of_gpio_flags flags;
		int gpio;

		gpio = of_get_gpio_flags(np, i, &flags);
		if (gpio < 0) {
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev, "invalid gpio %d\n", i);
			goto err_free;
		}
		of_property_read_u32_index(np, "linux,codes", i, &b->code);
		if (b->code >= KEY_CNT) {
			dev_err(dev, "invalid code %u\n", b->code);
			error = -EINVAL;
			goto err_free;
		}
		b->gpio = gpio;
		b->active_low = flags & OF_GPIO_ACTIVE_LOW;

		error = devm_gpio_request_one(dev, gpio, GPIOF_IN, DRV_NAME);
		if (error) {
			dev_err(dev, "unable to claim gpio %u: %d\n", gpio, error);
			goto err_free;
		}

		if (!rg35xx_is_axis(b->code))
			input_set_capability(input, EV_KEY, b->code);
	}
	rg35xx_drop_unused(dev, pad);

	/* input_set_abs_params() does not set EV_ABS in this kernel */
	__set_bit(EV_ABS, input->evbit);
	input_set_capability(input, EV_KEY, BTN_Z);
	input_set_abs_params(input, ABS_X, -32768, 32767, 16, 128);
	input_set_abs_params(input, ABS_Y, -32768, 32767, 16, 128);
	input_set_abs_params(input, ABS_Z, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_RX, -32768, 32767, 16, 128);
	input_set_abs_params(input, ABS_RY, -32768, 32767, 16, 128);
	input_set_abs_params(input, ABS_RZ, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_HAT0X, -1, 1, 0, 0);
	input_set_abs_params(input, ABS_HAT0Y, -1, 1, 0, 0);

	error = input_register_polled_device(poll_dev);
	if (error) {
		dev_err(dev, "unable to register polled device: %d\n", error);
		goto err_free;
	}

	platform_set_drvdata(pdev, pad);
	return 0;

err_free:
	input_free_polled_device(poll_dev);
	return error;
}

static int rg35xx_gamepad_remove(struct platform_device *pdev)
{
	struct rg35xx_gamepad *pad = platform_get_drvdata(pdev);

	input_unregister_polled_device(pad->poll_dev);
	input_free_polled_device(pad->poll_dev);
	return 0;
}

static const struct of_device_id rg35xx_gamepad_of_match[] = {
	{ .compatible = "anbernic,rg35xx-gamepad" },
	{ }
};
MODULE_DEVICE_TABLE(of, rg35xx_gamepad_of_match);

static struct platform_driver rg35xx_gamepad_driver = {
	.probe	= rg35xx_gamepad_probe,
	.remove	= rg35xx_gamepad_remove,
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = rg35xx_gamepad_of_match,
	},
};
module_platform_driver(rg35xx_gamepad_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Anbernic RG35XX gamepad driver");
