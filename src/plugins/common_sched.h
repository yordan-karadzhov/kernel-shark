/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 *               2021 Intel Inc, Hongzhan Chen <hongzhan.chen@intel.com>
 */

/**
 *  @file    common_sched.h
 *  @brief   Common definitions for sched plugins.
 */

#ifndef _KS_PLUGIN_COMMON_SCHED_H
#define _KS_PLUGIN_COMMON_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

/** The type of the numerical data field used by the 'tep' APIs. */
typedef unsigned long long tep_num_field_t;

/** The type of the data field stored in the kshark_data_container object. */
typedef int64_t ks_num_field_t;

/** prev_state offset in data field */
#define PREV_STATE_SHIFT	((int) ((sizeof(ks_num_field_t) - 1) * 8))

/** Bit mask used when converting data to prev_state */
#define PREV_STATE_MASK		(((ks_num_field_t) 1 << 8) - 1)

/** Bit mask used when converting data to PID */
#define PID_MASK		(((ks_num_field_t) 1 << PREV_STATE_SHIFT) - 1)

/**
 * @brief Set the PID value for the data field stored in the
 *	  kshark_data_container object.
 *
 * @param field: Input pointer to data field.
 * @param pid:   Input pid to set in data field.
 */
static inline void plugin_sched_set_pid(ks_num_field_t *field,
				 tep_num_field_t pid)
{
	*field &= ~PID_MASK;
	*field |= pid & PID_MASK;
}

/**
 * @brief Retrieve the PID value from the data field stored in the
 *	  kshark_data_container object.
 *
 * @param field: Input location for the data field.
 */
static inline int plugin_sched_get_pid(ks_num_field_t field)
{
	return field & PID_MASK;
}

#ifdef __cplusplus
}
#endif

#endif
