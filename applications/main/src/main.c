/*
 * Copyright (c) 2026 not-na
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/sensor.h>


#include "hid.h"
#include "usb.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

static const struct device *led_strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
static const struct device *encoder = DEVICE_DT_GET_ONE(ams_as5600);

static int last_position = 0;

static struct led_rgb ok_led = {.r = 0x00, .g = 0xFF, .b = 0x00};


int main(void)
{
	int ret;

	LOG_INF("MagWheel starting up");

	hid_device_init();
	usb_device_init();

	if (!device_is_ready(encoder)) {
		LOG_ERR("AS5600 not ready");
		return -EIO;
	}

	if (!device_is_ready(led_strip)) {
		LOG_ERR("LED strip not ready");
		return -EIO;
	}

	led_strip_update_rgb(led_strip, &ok_led, STRIP_NUM_PIXELS);

	struct sensor_value val;
	int delta_acc = 0;

	while (true) {
		if (!get_hid_ready()) {
			LOG_INF("Waiting for HID to be ready...");
			continue;
		}

		ret = sensor_sample_fetch(encoder);
		if (ret != 0) {
			LOG_ERR("Failed to fetch sensor sample");
			continue;
		}

		ret = sensor_channel_get(encoder, SENSOR_CHAN_ROTATION, &val);
		if (ret != 0) {
			LOG_ERR("Failed to get sensor channel rotation");
			continue;
		}

		// Convert to 1/10° units => 3600 steps per revolution
		// TODO: make configurable
		int rot = (val.val1 * 1000000 + val.val2) / 100000;

		LOG_INF("Rotation: %d", rot);

		if (last_position == 0) {
			last_position = rot;
		}

		// Sequence math, reverse motion?
		int delta_rev = 0;
		if (rot > last_position) {
			delta_rev = rot - last_position - 3600;
		} else if (rot < last_position) {
			delta_rev = 3600 - last_position + rot;
		}

		// Normal, forward motion?
		int delta_fwd = rot - last_position;

		// Use the most likely candidate, whichever is smaller
		int delta = abs(delta_rev) > abs(delta_fwd) ? delta_fwd : delta_rev;

		if (delta <= CONFIG_MAGWHEEL_DEADZONE_DEFAULT) {
			continue;
		}

		hid_device_send_wheel_report(delta_acc);

		// Only update last pos if we sent a report
		last_position = rot;

		k_usleep(1000);
	}

	return 0;
}