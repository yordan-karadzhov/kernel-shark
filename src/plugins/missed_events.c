// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    missed_events.c
 *  @brief   Plugin for visualization of missed events due to overflow of the
 *	     ring buffer.
 */

// C
#include <stdio.h>

// KernelShark
#include "libkshark.h"
#include "plugins/missed_events.h"

/** Load this plugin. */
int KSHARK_PLOT_PLUGIN_INITIALIZER(struct kshark_data_stream *stream)
{
	printf("--> missed_events init %i\n", stream->stream_id);
	kshark_register_draw_handler(stream, draw_missed_events);

	return 1;
}

/** Unload this plugin. */
int KSHARK_PLOT_PLUGIN_DEINITIALIZER(struct kshark_data_stream *stream)
{
	printf("<-- missed_events close %i\n", stream->stream_id);
	kshark_unregister_draw_handler(stream, draw_missed_events);

	return 1;
}
