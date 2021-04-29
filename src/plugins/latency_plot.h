/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    latency_plot.h
 *  @brief   Plugin for visualizing the latency between two trace events.
 */

#ifndef _KS_PLUGIN_LATENCY_PLOT_H
#define _KS_PLUGIN_LATENCY_PLOT_H

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Structure representing a plugin-specific context. */
struct plugin_latency_context {
	/** Trace event names. */
	char		*event_name[2];

	/** Trace event identifiers. */
	int		event_id[2];

	/** Event field names. */
	char		*field_name[2];

	/** True if the second pass is already done. */
	bool		second_pass_done;

	/**
	 * The maximum value of the latency between events A and B in this
	 * data sample.
	 */
	int64_t		max_latency;

	/** Container objects to store the trace event field's data. */
	struct kshark_data_container	*data[2];
};

KS_DECLARE_PLUGIN_CONTEXT_METHODS(struct plugin_latency_context)

void draw_latency(struct kshark_cpp_argv *argv_c,
		  int sd, int pid, int draw_action);

void *plugin_latency_add_menu(void *gui_ptr);

void plugin_set_event_fields(struct plugin_latency_context *plugin_ctx);

void plugin_mark_entry(const struct kshark_entry *e, char mark);

#ifdef __cplusplus
}
#endif

#endif
