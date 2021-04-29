// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    latency_plot.c
 *  @brief   Plugin for visualizing the latency between two trace events.
 */

// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdio.h>
#include <assert.h>

// KernelShark
#include "plugins/latency_plot.h"

static void latency_free_context(struct plugin_latency_context *plugin_ctx)
{
	if (!plugin_ctx)
		return;

	free(plugin_ctx->event_name[0]);
	free(plugin_ctx->field_name[0]);

	free(plugin_ctx->event_name[1]);
	free(plugin_ctx->field_name[1]);

	kshark_free_data_container(plugin_ctx->data[0]);
	kshark_free_data_container(plugin_ctx->data[1]);
}

/** A general purpose macro is used to define plugin context. */
KS_DEFINE_PLUGIN_CONTEXT(struct plugin_latency_context, latency_free_context);

static bool plugin_latency_init_context(struct kshark_data_stream *stream,
	struct plugin_latency_context *plugin_ctx)
{
	plugin_set_event_fields(plugin_ctx);

	plugin_ctx->event_id[0] =
		kshark_find_event_id(stream, plugin_ctx->event_name[0]);

	if (plugin_ctx->event_id[0] < 0) {
		fprintf(stderr, "Event %s not found in stream %s:%s\n",
			plugin_ctx->event_name[0], stream->file, stream->name);
		return false;
	}

	plugin_ctx->event_id[1] =
		kshark_find_event_id(stream, plugin_ctx->event_name[1]);
	if (plugin_ctx->event_id[1] < 0) {
		fprintf(stderr, "Event %s not found in stream %s:%s\n",
			plugin_ctx->event_name[1], stream->file, stream->name);
		return false;
	}

	plugin_ctx->second_pass_done = false;
	plugin_ctx->max_latency = INT64_MIN;

	plugin_ctx->data[0] = kshark_init_data_container();
	plugin_ctx->data[1] = kshark_init_data_container();
	if (!plugin_ctx->data[0] || !plugin_ctx->data[1])
		return false;

	return true;
}

static void plugin_get_field(struct kshark_data_stream *stream, void *rec,
			     struct kshark_entry *entry,
			     char *field_name,
			     struct kshark_data_container *data)
{
	int64_t val;

	kshark_read_record_field_int(stream, rec, field_name, &val);
	kshark_data_container_append(data, entry, val);
}

static void plugin_get_field_a(struct kshark_data_stream *stream, void *rec,
			       struct kshark_entry *entry)
{
	struct plugin_latency_context *plugin_ctx;

	plugin_ctx = __get_context(stream->stream_id);
	if (!plugin_ctx)
		return;

	plugin_get_field(stream, rec, entry,
			 plugin_ctx->field_name[0],
			 plugin_ctx->data[0]);
}

static void plugin_get_field_b(struct kshark_data_stream *stream, void *rec,
			       struct kshark_entry *entry)
{
	struct plugin_latency_context *plugin_ctx;

	plugin_ctx = __get_context(stream->stream_id);
	if (!plugin_ctx)
		return;

	plugin_get_field(stream, rec, entry,
			 plugin_ctx->field_name[1],
			 plugin_ctx->data[1]);
}

/** Load this plugin. */
int KSHARK_PLOT_PLUGIN_INITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_latency_context *plugin_ctx = __init(stream->stream_id);

	if (!plugin_ctx || !plugin_latency_init_context(stream, plugin_ctx)) {
		__close(stream->stream_id);
		return 0;
	}

	/* Register Event handler to be executed during data loading. */
	kshark_register_event_handler(stream,
				      plugin_ctx->event_id[0],
				      plugin_get_field_a);

	kshark_register_event_handler(stream,
				      plugin_ctx->event_id[1],
				      plugin_get_field_b);

	/* Register a drawing handler to plot on top of each Graph. */
	kshark_register_draw_handler(stream, draw_latency);

	return 1;
}

/** Unload this plugin. */
int KSHARK_PLOT_PLUGIN_DEINITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_latency_context *plugin_ctx = __get_context(stream->stream_id);
	int ret = 0;

	if (plugin_ctx) {
		kshark_unregister_event_handler(stream,
						plugin_ctx->event_id[0],
						plugin_get_field_a);

		kshark_unregister_event_handler(stream,
						plugin_ctx->event_id[1],
						plugin_get_field_b);

		kshark_unregister_draw_handler(stream, draw_latency);

		ret = 1;
	}

	__close(stream->stream_id);

	return ret;
}

/** Initialize the control interface of the plugin. */
void *KSHARK_MENU_PLUGIN_INITIALIZER(void *gui_ptr)
{
	return plugin_latency_add_menu(gui_ptr);
}
