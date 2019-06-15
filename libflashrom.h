/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2010 Google Inc.
 * Copyright (C) 2012 secunet Security Networks AG
 * (Written by Nico Huber <nico.huber@secunet.com> for secunet)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LIBFLASHROM_H__
#define __LIBFLASHROM_H__ 1

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

int flashrom_init(int perform_selfcheck);
int flashrom_shutdown(void);
/** @ingroup flashrom-general */
enum flashrom_log_level {
	FLASHROM_MSG_ERROR	= 0,
	FLASHROM_MSG_WARN	= 1,
	FLASHROM_MSG_INFO	= 2,
	FLASHROM_MSG_DEBUG	= 3,
	FLASHROM_MSG_DEBUG2	= 4,
	FLASHROM_MSG_SPEW	= 5,
};
/** @ingroup flashrom-general */
typedef int(flashrom_log_callback)(enum flashrom_log_level, const char *format, va_list);
void flashrom_set_log_callback(flashrom_log_callback *);

/** @ingroup flashrom-prog */
struct flashrom_programmer;
int flashrom_programmer_init(struct flashrom_programmer **, const char *prog_name, const char *prog_params);
int flashrom_programmer_shutdown(struct flashrom_programmer *);

struct flashrom_flashctx;
int flashrom_flash_probe(struct flashrom_flashctx **, const struct flashrom_programmer *, const char *chip_name);
size_t flashrom_flash_getsize(const struct flashrom_flashctx *);
int flashrom_flash_erase(struct flashrom_flashctx *);
void flashrom_flash_release(struct flashrom_flashctx *);

/** @ingroup flashrom-flash */
enum flashrom_flag {
	FLASHROM_FLAG_FORCE,
	FLASHROM_FLAG_FORCE_BOARDMISMATCH,
	FLASHROM_FLAG_VERIFY_AFTER_WRITE,
	FLASHROM_FLAG_VERIFY_WHOLE_CHIP,
};
void flashrom_flag_set(struct flashrom_flashctx *, enum flashrom_flag, bool value);
bool flashrom_flag_get(const struct flashrom_flashctx *, enum flashrom_flag);

int flashrom_image_read(struct flashrom_flashctx *, void *buffer, size_t buffer_len);
int flashrom_image_write(struct flashrom_flashctx *, void *buffer, size_t buffer_len, const void *refbuffer);
int flashrom_image_verify(struct flashrom_flashctx *, const void *buffer, size_t buffer_len);

struct flashrom_layout;
int flashrom_layout_read_from_ifd(struct flashrom_layout **, struct flashrom_flashctx *, const void *dump, size_t len);
int flashrom_layout_read_fmap_from_rom(struct flashrom_layout **,
		struct flashrom_flashctx *, size_t offset, size_t length);
int flashrom_layout_read_fmap_from_buffer(struct flashrom_layout **layout,
		struct flashrom_flashctx *, const uint8_t *buf, size_t len);
int flashrom_layout_add_region(struct flashrom_layout *, size_t start, size_t end, const char *name);
int flashrom_layout_include_region(struct flashrom_layout *, const char *name);
void flashrom_layout_release(struct flashrom_layout *);
void flashrom_layout_set(struct flashrom_flashctx *, const struct flashrom_layout *);

/** @ingroup flashrom-wp */
enum flashrom_wp_result {
	FLASHROM_WP_OK = 0,
	FLASHROM_WP_ERR_CHIP_UNSUPPORTED = 1,
	FLASHROM_WP_ERR_OTHER = 2,
	FLASHROM_WP_ERR_READ_FAILED = 3,
	FLASHROM_WP_ERR_WRITE_FAILED = 4,
	FLASHROM_WP_ERR_VERIFY_FAILED = 5,
	FLASHROM_WP_ERR_RANGE_UNSUPPORTED = 6,
	FLASHROM_WP_ERR_MODE_UNSUPPORTED = 7,
	FLASHROM_WP_ERR_RANGE_LIST_UNAVAILABLE = 8,
	FLASHROM_WP_ERR_UNSUPPORTED_STATE = 9,
};

enum flashrom_wp_mode {
	FLASHROM_WP_MODE_DISABLED,
	FLASHROM_WP_MODE_HARDWARE,
	FLASHROM_WP_MODE_POWER_CYCLE,
	FLASHROM_WP_MODE_PERMANENT
};
struct flashrom_wp_cfg;
struct flashrom_wp_ranges;

enum flashrom_wp_result flashrom_wp_cfg_new(struct flashrom_wp_cfg **);
void flashrom_wp_cfg_release(struct flashrom_wp_cfg *);
void flashrom_wp_set_mode(struct flashrom_wp_cfg *, enum flashrom_wp_mode);
enum flashrom_wp_mode flashrom_wp_get_mode(const struct flashrom_wp_cfg *);
void flashrom_wp_set_range(struct flashrom_wp_cfg *, size_t start, size_t len);
void flashrom_wp_get_range(size_t *start, size_t *len, const struct flashrom_wp_cfg *);

enum flashrom_wp_result flashrom_wp_read_cfg(struct flashrom_wp_cfg *, struct flashrom_flashctx *);
enum flashrom_wp_result flashrom_wp_write_cfg(struct flashrom_flashctx *, const struct flashrom_wp_cfg *);

enum flashrom_wp_result flashrom_wp_get_available_ranges(struct flashrom_wp_ranges **, struct flashrom_flashctx *);
size_t flashrom_wp_ranges_get_count(const struct flashrom_wp_ranges *);
enum flashrom_wp_result flashrom_wp_ranges_get_range(size_t *start, size_t *len, const struct flashrom_wp_ranges *, unsigned int index);
void flashrom_wp_ranges_release(struct flashrom_wp_ranges *);

#endif				/* !__LIBFLASHROM_H__ */
