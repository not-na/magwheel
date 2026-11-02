/*
 * Copyright (c) 2026 not-na
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

int main(void)
{
	printk("MagWheel %s\n", APP_VERSION_STRING);

	printk("You've reached main()\n");
	// TODO

	return 0;
}

