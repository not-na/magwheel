/*
 * Copyright (c) 2026 not-na
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
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
#ifdef CONFIG_MAGWHEEL_SPEEDSCALING
	int last_delta[CONFIG_MAGWHEEL_SPEEDSCALING_SMOOTHING] = {0};
	uint64_t last_sample_time[CONFIG_MAGWHEEL_SPEEDSCALING_SMOOTHING] = {0};
#endif

	LOG_INF("Booted!");
	k_msleep(1000);

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

#ifdef CONFIG_MAGWHEEL_SPEEDSCALING
		uint64_t sample_time = k_cycle_get_64();
#endif

		ret = sensor_channel_get(encoder, SENSOR_CHAN_ROTATION, &val);
		if (ret != 0) {
			LOG_ERR("Failed to get sensor channel rotation");
			continue;
		}

		int rot = (val.val1 * 1000000 + val.val2) / (360 * 1000000 / CONFIG_MAGWHEEL_STEPS_PER_REV);

		LOG_INF_RATELIMIT_RATE(1000, "Rotation %d", rot);

		if (last_position == 0) {
			last_position = rot;
		}

		// How much backwards rotation would it take to get to the new position?
		int delta_rev = 0;
		if (rot > last_position) {
			delta_rev = rot - last_position - CONFIG_MAGWHEEL_STEPS_PER_REV;
		} else if (rot < last_position) {
			delta_rev = CONFIG_MAGWHEEL_STEPS_PER_REV - last_position + rot;
		}

		// How much forwards rotation would it take to get to the new position?
		int delta_fwd = rot - last_position;

		// Use the most likely candidate, whichever is smaller
		int delta = abs(delta_rev) > abs(delta_fwd) ? delta_fwd : delta_rev;

		if (abs(delta) <= CONFIG_MAGWHEEL_DEADZONE_DEFAULT) {
			continue;
		}

#ifdef CONFIG_MAGWHEEL_SPEEDSCALING
		// Calculate the speed since the oldest sample in the smoothing window
		uint64_t time_diff = sample_time - last_sample_time[CONFIG_MAGWHEEL_SPEEDSCALING_SMOOTHING - 1];
		int oldest_delta = 0;
		for (int i = 0; i < CONFIG_MAGWHEEL_SPEEDSCALING_SMOOTHING; i++) {
			oldest_delta += last_delta[i];
		}
		float speed = fabsf((float)oldest_delta / (k_cyc_to_ms_near32(time_diff)/1000.0f)); // steps per second

		// Update the delta and sample time arrays
		for (int i = CONFIG_MAGWHEEL_SPEEDSCALING_SMOOTHING - 1; i > 0; i--) {
			last_delta[i] = last_delta[i - 1];
			last_sample_time[i] = last_sample_time[i - 1];
		}
		last_delta[0] = delta;
		last_sample_time[0] = sample_time;

		// Apply speed scaling
		if (speed > CONFIG_MAGWHEEL_SPEEDSCALING_THRESHOLD_HIGH) {
			delta *= CONFIG_MAGWHEEL_SPEEDSCALING_FACTOR;
		} else if (speed > CONFIG_MAGWHEEL_SPEEDSCALING_THRESHOLD_LOW) {
			// Interpolate between 1 and the scaling factor based on speed
			float t = ((speed-CONFIG_MAGWHEEL_SPEEDSCALING_THRESHOLD_LOW) / (CONFIG_MAGWHEEL_SPEEDSCALING_THRESHOLD_HIGH - CONFIG_MAGWHEEL_SPEEDSCALING_THRESHOLD_LOW));
			t = t*t; // Quad easing
			float scale = t
						* (CONFIG_MAGWHEEL_SPEEDSCALING_FACTOR - 1) + 1;
			delta *= scale;
			LOG_INF_RATELIMIT_RATE(1000, "Speed %d steps/s, scale %d (/10)", (int)speed, (int)(scale*10));
		}

		LOG_INF_RATELIMIT_RATE(1000, "Delta %d, speed %d steps/s", delta, (int)speed);
#else
		LOG_INF_RATELIMIT_RATE(1000, "Delta %d", delta);
#endif

		hid_device_send_wheel_report(delta);

		// Only update last pos if we sent a report
		last_position = rot;

		k_msleep(1);
	}

	return 0;
}