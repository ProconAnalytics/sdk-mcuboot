/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#include "boot_request_mem.h"
#include <zephyr/retention/retention.h>
#include "bootutil/bootutil_log.h"

static const struct device *bootloader_request_dev =
	DEVICE_DT_GET(DT_CHOSEN(nrf_bootloader_request));

BOOT_LOG_MODULE_DECLARE(bootloader_request);

int boot_request_mem_init(void)
{
	int rc;

	if (!device_is_ready(bootloader_request_dev)) {
		return -EIO;
	}

	/*
	 * The retention SRAM is NOLOAD and its power-on content is
	 * undefined (random on cold boot, or stale data after an
	 * incomplete power cycle).  retention_read() does not check the
	 * prefix/CRC, so garbage could be mistaken for a valid recovery
	 * request and trap the device in serial recovery mode.
	 *
	 * Validate the area (prefix + checksum, as configured in the
	 * devicetree node) and clear it when invalid, mirroring what the
	 * flash backend (boot_request_flash.c) does in this function.
	 * retention_is_valid() returns 1 when the data is valid, 0 when
	 * it is not, and a negative errno on error.
	 */
	rc = retention_is_valid(bootloader_request_dev);
	if (rc < 0) {
		/* No prefix/checksum configured or read failure. */
		BOOT_LOG_WRN("Unable to validate boot request area: %d", rc);
		return rc;
	}

	if (rc == 0) {
		BOOT_LOG_INF("Boot request area is invalid. Clear area.");
		return retention_clear(bootloader_request_dev);
	}

	return 0;
}

bool boot_request_mem_wrtie_prepare(void)
{
	return true;
}

int boot_request_mem_read(size_t entry, uint8_t *value)
{
	if ((value == NULL) || (entry >= retention_size(bootloader_request_dev) / sizeof(*value))) {
		return -EINVAL;
	}

	return retention_read(bootloader_request_dev, entry * sizeof(*value), (void *)value,
			      sizeof(*value));
}

int boot_request_mem_write(size_t entry, uint8_t *value)
{
	if ((value == NULL) || (entry >= retention_size(bootloader_request_dev) / sizeof(*value))) {
		return -EINVAL;
	}

	return retention_write(bootloader_request_dev, entry * sizeof(*value), (void *)value,
			       sizeof(*value));
}

int boot_request_mem_selective_erase(size_t *nv_indexes, size_t nv_count)
{
	if ((nv_indexes == NULL) && (nv_count != 0)) {
		return -EINVAL;
	}

	/* Selective erase is not supported if there is no backup area. */
	if (nv_count != 0) {
		return -ENOTSUP;
	}

	return retention_clear(bootloader_request_dev);
}
