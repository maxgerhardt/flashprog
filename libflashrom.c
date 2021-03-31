/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2012, 2016 secunet Security Networks AG
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
/**
 * @mainpage
 *
 * Have a look at the Modules section for a function reference.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "flash.h"
#include "fmap.h"
#include "programmer.h"
#include "layout.h"
#include "hwaccess.h"
#include "ich_descriptors.h"
#include "libflashrom.h"
#include "writeprotect.h"

/**
 * @defgroup flashrom-general General
 * @{
 */

/** Pointer to log callback function. */
static flashrom_log_callback *global_log_callback = NULL;

/**
 * @brief Initialize libflashrom.
 *
 * @param perform_selfcheck If not zero, perform a self check.
 * @return 0 on success
 */
int flashrom_init(const int perform_selfcheck)
{
	if (perform_selfcheck && selfcheck())
		return 1;
	myusec_calibrate_delay();
	return 0;
}

/**
 * @brief Shut down libflashrom.
 * @return 0 on success
 */
int flashrom_shutdown(void)
{
	return 0; /* TODO: nothing to do? */
}

/* TODO: flashrom_set_loglevel()? do we need it?
         For now, let the user decide in their callback. */

/**
 * @brief Set the log callback function.
 *
 * Set a callback function which will be invoked whenever libflashrom wants
 * to output messages. This allows frontends to do whatever they see fit with
 * such messages, e.g. write them to syslog, or to file, or print them in a
 * GUI window, etc.
 *
 * @param log_callback Pointer to the new log callback function.
 */
void flashrom_set_log_callback(flashrom_log_callback *const log_callback)
{
	global_log_callback = log_callback;
}
/** @private */
int print(const enum flashrom_log_level level, const char *const fmt, ...)
{
	if (global_log_callback) {
		int ret;
		va_list args;
		va_start(args, fmt);
		ret = global_log_callback(level, fmt, args);
		va_end(args);
		return ret;
	}
	return 0;
}

/** @} */ /* end flashrom-general */



/**
 * @defgroup flashrom-query Querying
 * @{
 */

/* TBD */

/** @} */ /* end flashrom-query */



/**
 * @defgroup flashrom-prog Programmers
 * @{
 */

/**
 * @brief Initialize the specified programmer.
 *
 * Currently, only one programmer may be initialized at a time.
 *
 * @param[out] flashprog Points to a pointer of type struct flashrom_programmer
 *                       that will be set if programmer initialization succeeds.
 *                       *flashprog has to be shutdown by the caller with @ref
 *                       flashrom_programmer_shutdown.
 * @param[in] prog_name Name of the programmer to initialize.
 * @param[in] prog_param Pointer to programmer specific parameters.
 * @return 0 on success
 */
int flashrom_programmer_init(struct flashrom_programmer **const flashprog,
			     const char *const prog_name, const char *const prog_param)
{
	unsigned prog;

	for (prog = 0; prog < PROGRAMMER_INVALID; prog++) {
		if (strcmp(prog_name, programmer_table[prog]->name) == 0)
			break;
	}
	if (prog >= PROGRAMMER_INVALID) {
		msg_ginfo("Error: Unknown programmer \"%s\". Valid choices are:\n", prog_name);
		list_programmers_linebreak(0, 80, 0);
		return 1;
	}
	return programmer_init(prog, prog_param);
}

/**
 * @brief Shut down the initialized programmer.
 *
 * @param flashprog The programmer to shut down.
 * @return 0 on success
 */
int flashrom_programmer_shutdown(struct flashrom_programmer *const flashprog)
{
	return programmer_shutdown();
}

/* TODO: flashrom_programmer_capabilities()? */

/** @} */ /* end flashrom-prog */



/**
 * @defgroup flashrom-flash Flash chips
 * @{
 */

/**
 * @brief Probe for a flash chip.
 *
 * Probes for a flash chip and returns a flash context, that can be used
 * later with flash chip and @ref flashrom-ops "image operations", if
 * exactly one matching chip is found.
 *
 * @param[out] flashctx Points to a pointer of type struct flashrom_flashctx
 *                      that will be set if exactly one chip is found. *flashctx
 *                      has to be freed by the caller with @ref flashrom_flash_release.
 * @param[in] flashprog The flash programmer used to access the chip.
 * @param[in] chip_name Name of a chip to probe for, or NULL to probe for
 *                      all known chips.
 * @return 0 on success,
 *         3 if multiple chips were found,
 *         2 if no chip was found,
 *         or 1 on any other error.
 */
int flashrom_flash_probe(struct flashrom_flashctx **const flashctx,
			 const struct flashrom_programmer *const flashprog,
			 const char *const chip_name)
{
	int i, ret = 2;
	struct flashrom_flashctx second_flashctx = { 0, };

	chip_to_probe = chip_name; /* chip_to_probe is global in flashrom.c */

	*flashctx = malloc(sizeof(**flashctx));
	if (!*flashctx)
		return 1;
	memset(*flashctx, 0, sizeof(**flashctx));

	for (i = 0; i < registered_master_count; ++i) {
		int flash_idx = -1;
		if (!ret || (flash_idx = probe_flash(&registered_masters[i], 0, *flashctx, 0)) != -1) {
			ret = 0;
			/* We found one chip, now check that there is no second match. */
			if (probe_flash(&registered_masters[i], flash_idx + 1, &second_flashctx, 0) != -1) {
				ret = 3;
				break;
			}
		}
	}
	if (ret) {
		free(*flashctx);
		*flashctx = NULL;
	}
	return ret;
}

/**
 * @brief Returns the size of the specified flash chip in bytes.
 *
 * @param flashctx The queried flash context.
 * @return Size of flash chip in bytes.
 */
size_t flashrom_flash_getsize(const struct flashrom_flashctx *const flashctx)
{
	return flashctx->chip->total_size * 1024;
}

/**
 * @brief Free a flash context.
 *
 * @param flashctx Flash context to free.
 */
void flashrom_flash_release(struct flashrom_flashctx *const flashctx)
{
	free(flashctx);
}

/**
 * @brief Set a flag in the given flash context.
 *
 * @param flashctx Flash context to alter.
 * @param flag	   Flag that is to be set / cleared.
 * @param value	   Value to set.
 */
void flashrom_flag_set(struct flashrom_flashctx *const flashctx,
		       const enum flashrom_flag flag, const bool value)
{
	switch (flag) {
		case FLASHROM_FLAG_FORCE:		flashctx->flags.force = value; break;
		case FLASHROM_FLAG_FORCE_BOARDMISMATCH:	flashctx->flags.force_boardmismatch = value; break;
		case FLASHROM_FLAG_VERIFY_AFTER_WRITE:	flashctx->flags.verify_after_write = value; break;
		case FLASHROM_FLAG_VERIFY_WHOLE_CHIP:	flashctx->flags.verify_whole_chip = value; break;
	}
}

/**
 * @brief Return the current value of a flag in the given flash context.
 *
 * @param flashctx Flash context to read from.
 * @param flag	   Flag to be read.
 * @return Current value of the flag.
 */
bool flashrom_flag_get(const struct flashrom_flashctx *const flashctx, const enum flashrom_flag flag)
{
	switch (flag) {
		case FLASHROM_FLAG_FORCE:		return flashctx->flags.force;
		case FLASHROM_FLAG_FORCE_BOARDMISMATCH:	return flashctx->flags.force_boardmismatch;
		case FLASHROM_FLAG_VERIFY_AFTER_WRITE:	return flashctx->flags.verify_after_write;
		case FLASHROM_FLAG_VERIFY_WHOLE_CHIP:	return flashctx->flags.verify_whole_chip;
		default:				return false;
	}
}

/** @} */ /* end flashrom-flash */



/**
 * @defgroup flashrom-layout Layout handling
 * @{
 */

/**
 * @brief Read a layout from the Intel ICH descriptor in the flash.
 *
 * Optionally verify that the layout matches the one in the given
 * descriptor dump.
 *
 * @param[out] layout Points to a struct flashrom_layout pointer that
 *                    gets set if the descriptor is read and parsed
 *                    successfully.
 * @param[in] flashctx Flash context to read the descriptor from flash.
 * @param[in] dump     The descriptor dump to compare to or NULL.
 * @param[in] len      The length of the descriptor dump.
 *
 * @return 0 on success,
 *         6 if descriptor parsing isn't implemented for the host,
 *         5 if the descriptors don't match,
 *         4 if the descriptor dump couldn't be parsed,
 *         3 if the descriptor on flash couldn't be parsed,
 *         2 if the descriptor on flash couldn't be read,
 *         1 on any other error.
 */
int flashrom_layout_read_from_ifd(struct flashrom_layout **const layout, struct flashctx *const flashctx,
				  const void *const dump, const size_t len)
{
#ifndef __FLASHROM_LITTLE_ENDIAN__
	return 6;
#else
	struct ich_layout dump_layout;
	int ret = 1;

	void *const desc = malloc(0x1000);
	struct ich_layout *const chip_layout = malloc(sizeof(*chip_layout));
	if (!desc || !chip_layout) {
		msg_gerr("Out of memory!\n");
		goto _free_ret;
	}

	if (prepare_flash_access(flashctx, true, false, false, false))
		goto _free_ret;

	msg_cinfo("Reading ich descriptor... ");
	if (flashctx->chip->read(flashctx, desc, 0, 0x1000)) {
		msg_cerr("Read operation failed!\n");
		msg_cinfo("FAILED.\n");
		ret = 2;
		goto _finalize_ret;
	}
	msg_cinfo("done.\n");

	if (layout_from_ich_descriptors(chip_layout, desc, 0x1000)) {
		msg_cerr("Couldn't parse the descriptor!\n");
		ret = 3;
		goto _finalize_ret;
	}

	if (dump) {
		if (layout_from_ich_descriptors(&dump_layout, dump, len)) {
			msg_cerr("Couldn't parse the descriptor!\n");
			ret = 4;
			goto _finalize_ret;
		}

		if (chip_layout->base.num_entries != dump_layout.base.num_entries ||
		    memcmp(chip_layout->entries, dump_layout.entries, sizeof(dump_layout.entries))) {
			msg_cerr("Descriptors don't match!\n");
			ret = 5;
			goto _finalize_ret;
		}
	}

	*layout = (struct flashrom_layout *)chip_layout;
	ret = 0;

_finalize_ret:
	finalize_flash_access(flashctx);
_free_ret:
	if (ret)
		free(chip_layout);
	free(desc);
	return ret;
#endif
}

#ifdef __FLASHROM_LITTLE_ENDIAN__
static int flashrom_layout_parse_fmap(struct flashrom_layout **layout,
		struct flashctx *const flashctx, const struct fmap *const fmap)
{
	int i;
	struct flashrom_layout *l = get_global_layout();

	if (!fmap || !l)
		return 1;

	if (l->num_entries + fmap->nareas > MAX_ROMLAYOUT) {
		msg_gerr("Cannot add fmap entries to layout - Too many entries.\n");
		return 1;
	}

	for (i = 0; i < fmap->nareas; i++) {
		l->entries[l->num_entries].start = fmap->areas[i].offset;
		l->entries[l->num_entries].end = fmap->areas[i].offset + fmap->areas[i].size - 1;
		l->entries[l->num_entries].included = false;
		l->entries[l->num_entries].name =
			strndup((const char *)fmap->areas[i].name, FMAP_STRLEN);
		if (!l->entries[l->num_entries].name) {
			msg_gerr("Error adding layout entry: %s\n", strerror(errno));
			return 1;
		}
		msg_gdbg("fmap %08x - %08x named %s\n",
			l->entries[l->num_entries].start,
			l->entries[l->num_entries].end,
			l->entries[l->num_entries].name);
		l->num_entries++;
	}

	*layout = l;
	return 0;
}
#endif /* __FLASHROM_LITTLE_ENDIAN__ */

/**
 * @brief Read a layout by searching the flash chip for fmap.
 *
 * @param[out] layout Points to a struct flashrom_layout pointer that
 *                    gets set if the fmap is read and parsed successfully.
 * @param[in] flashctx Flash context
 * @param[in] offset Offset to begin searching for fmap.
 * @param[in] offset Length of address space to search.
 *
 * @return 0 on success,
 *         3 if fmap parsing isn't implemented for the host,
 *         2 if the fmap couldn't be read,
 *         1 on any other error.
 */
int flashrom_layout_read_fmap_from_rom(struct flashrom_layout **const layout,
		struct flashctx *const flashctx, off_t offset, size_t len)
{
#ifndef __FLASHROM_LITTLE_ENDIAN__
	return 3;
#else
	struct fmap *fmap = NULL;
	int ret = 0;

	msg_gdbg("Attempting to read fmap from ROM content.\n");
	if (fmap_read_from_rom(&fmap, flashctx, offset, len)) {
		msg_gerr("Failed to read fmap from ROM.\n");
		return 1;
	}

	msg_gdbg("Adding fmap layout to global layout.\n");
	if (flashrom_layout_parse_fmap(layout, flashctx, fmap)) {
		msg_gerr("Failed to add fmap regions to layout.\n");
		ret = 1;
	}

	free(fmap);
	return ret;
#endif
}

/**
 * @brief Read a layout by searching buffer for fmap.
 *
 * @param[out] layout Points to a struct flashrom_layout pointer that
 *                    gets set if the fmap is read and parsed successfully.
 * @param[in] flashctx Flash context
 * @param[in] buffer Buffer to search in
 * @param[in] size Size of buffer to search
 *
 * @return 0 on success,
 *         3 if fmap parsing isn't implemented for the host,
 *         2 if the fmap couldn't be read,
 *         1 on any other error.
 */
int flashrom_layout_read_fmap_from_buffer(struct flashrom_layout **const layout,
		struct flashctx *const flashctx, const uint8_t *const buf, size_t size)
{
#ifndef __FLASHROM_LITTLE_ENDIAN__
	return 3;
#else
	struct fmap *fmap = NULL;
	int ret = 1;

	if (!buf || !size)
		goto _ret;

	msg_gdbg("Attempting to read fmap from buffer.\n");
	if (fmap_read_from_buffer(&fmap, buf, size)) {
		msg_gerr("Failed to read fmap from buffer.\n");
		goto _ret;
	}

	msg_gdbg("Adding fmap layout to global layout.\n");
	if (flashrom_layout_parse_fmap(layout, flashctx, fmap)) {
		msg_gerr("Failed to add fmap regions to layout.\n");
		goto _free_ret;
	}

	ret = 0;
_free_ret:
	free(fmap);
_ret:
	return ret;
#endif
}

/**
 * @brief Set the active layout for a flash context.
 *
 * Note: This just sets a pointer. The caller must not release the layout
 *       as long as he uses it through the given flash context.
 *
 * @param flashctx Flash context whose layout will be set.
 * @param layout   Layout to bet set.
 */
void flashrom_layout_set(struct flashrom_flashctx *const flashctx, const struct flashrom_layout *const layout)
{
	flashctx->layout = layout;
}

/** @} */ /* end flashrom-layout */


/**
 * @defgroup flashrom-wp
 * @{
 */

/**
 * @brief Create a new empty WP configuration.
 *
 * @param[out] cfg Points to a pointer of type struct flashrom_wp_cfg that will
 *                 be set if creation succeeds. *cfg has to be freed by the
 *                 caller with @ref flashrom_wp_cfg_release.
 * @return  0 on success
 *         >0 on failure
 */
enum flashrom_wp_result flashrom_wp_cfg_new(struct flashrom_wp_cfg **cfg)
{
	*cfg = calloc(1, sizeof(**cfg));
	return *cfg ? 0 : FLASHROM_WP_ERR_OTHER;
}

/**
 * @brief Free a WP configuration.
 *
 * @param[out] cfg Pointer to the flashrom_wp_cfg to free.
 */
void flashrom_wp_cfg_release(struct flashrom_wp_cfg *cfg)
{
	free(cfg);
}

/**
 * @brief Set the protection mode for a WP configuration.
 *
 * @param[in]  mode The protection mode to set.
 * @param[out] cfg  Pointer to the flashrom_wp_cfg structure to modify.
 */
void flashrom_wp_set_mode(struct flashrom_wp_cfg *cfg, enum flashrom_wp_mode mode)
{
	cfg->mode = mode;
}

/**
 * @brief Get the protection mode from a WP configuration.
 *
 * @param[in] cfg The WP configuration to get the protection mode from.
 * @return        The configuration's protection mode.
 */
enum flashrom_wp_mode flashrom_wp_get_mode(const struct flashrom_wp_cfg *cfg)
{
	return cfg->mode;
}

/**
 * @brief Set the protection range for a WP configuration.
 *
 * @param[out] cfg   Pointer to the flashrom_wp_cfg structure to modify.
 * @param[in]  start The range's start address.
 * @param[in]  len   The range's length.
 */
void flashrom_wp_set_range(struct flashrom_wp_cfg *cfg, size_t start, size_t len)
{
	cfg->range.start = start;
	cfg->range.len = len;
}

/**
 * @brief Get the protection range from a WP configuration.
 *
 * @param[out] start Points to a size_t to write the range start to.
 * @param[out] len   Points to a size_t to write the range length to.
 * @param[in]  cfg   The WP configuration to get the range from.
 */
void flashrom_wp_get_range(size_t *start, size_t *len, const struct flashrom_wp_cfg *cfg)
{
	*start = cfg->range.start;
	*len = cfg->range.len;
}

/**
 * @brief Write a WP configuration to a flash chip.
 *
 * @param[in] flash The flash context used to access the chip.
 * @param[in] cfg   The WP configuration to write to the chip.
 * @return  0 on success
 *         >0 on failure
 */
enum flashrom_wp_result flashrom_wp_write_cfg(struct flashctx *flash, const struct flashrom_wp_cfg *cfg)
{
	/*
	 * TODO: Call custom implementation if the programmer is opaque, as
	 * direct WP operations require SPI access. In particular, linux_mtd
	 * has its own WP operations we should use instead.
	 */
	if (flash->mst->buses_supported & BUS_SPI)
		return wp_write_cfg(flash, cfg);

	return FLASHROM_WP_ERR_OTHER;
}

/**
 * @brief Read the current WP configuration from a flash chip.
 *
 * @param[out] cfg   Pointer to a struct flashrom_wp_cfg to store the chip's
 *                   configuration in.
 * @param[in]  flash The flash context used to access the chip.
 * @return  0 on success
 *         >0 on failure
 */
enum flashrom_wp_result flashrom_wp_read_cfg(struct flashrom_wp_cfg *cfg, struct flashctx *flash)
{
	/*
	 * TODO: Call custom implementation if the programmer is opaque, as
	 * direct WP operations require SPI access. In particular, linux_mtd
	 * has its own WP operations we should use instead.
	 */
	if (flash->mst->buses_supported & BUS_SPI)
		return wp_read_cfg(cfg, flash);

	return FLASHROM_WP_ERR_OTHER;
}

/**
 * @brief Get a list of protection ranges supported by the flash chip.
 *
 * @param[out] ranges Points to a pointer of type struct flashrom_wp_ranges
 *                    that will be set if available ranges are found. Finding
 *                    available ranges may not always be possible, even if the
 *                    chip's protection range can be read or modified. *ranges
 *                    must be freed using @ref flashrom_wp_ranges_free.
 * @param[in] flash   The flash context used to access the chip.
 * @return  0 on success
 *         >0 on failure
 */
enum flashrom_wp_result flashrom_wp_get_available_ranges(struct flashrom_wp_ranges **list, struct flashrom_flashctx *flash)
{
	/*
	 * TODO: Call custom implementation if the programmer is opaque, as
	 * direct WP operations require SPI access. We actually can't implement
	 * this in linux_mtd right now, but we should adopt a proper generic
	 * architechure to match the read and write functions anyway.
	 */
	if (flash->mst->buses_supported & BUS_SPI)
		return wp_get_available_ranges(list, flash);

	return FLASHROM_WP_ERR_OTHER;
}

/**
 * @brief Get a number of protection ranges in a range list.
 *
 * @param[in]  ranges The range list to get the count from.
 * @return Number of ranges in the list.
 */
size_t flashrom_wp_ranges_get_count(const struct flashrom_wp_ranges *list)
{
	return list->count;
}

/**
 * @brief Get a protection range from a range list.
 *
 * @param[out] start  Points to a size_t to write the range's start to.
 * @param[out] len    Points to a size_t to write the range's length to.
 * @param[in]  ranges The range list to get the range from.
 * @param[in]  index  Index of the range to get.
 * @return  0 on success
 *         >0 on failure
 */
enum flashrom_wp_result flashrom_wp_ranges_get_range(size_t *start, size_t *len, const struct flashrom_wp_ranges *list, unsigned int index)
{
	if (index >= list->count)
		return FLASHROM_WP_ERR_OTHER;

	*start = list->ranges[index].start;
	*len = list->ranges[index].len;

	return 0;
}

/**
 * @brief Free a WP range list.
 *
 * @param[out] cfg Pointer to the flashrom_wp_ranges to free.
 */
void flashrom_wp_ranges_release(struct flashrom_wp_ranges *list)
{
	if (!list)
		return;

	free(list->ranges);
	free(list);
}


/** @} */ /* end flashrom-wp */
