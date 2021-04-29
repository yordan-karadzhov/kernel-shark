// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    event_field_plot.c
 *  @brief   Plugin for visualizing a given data field of a trace event.
 */

// C
#include <stdio.h>
#include <assert.h>
#include <limits.h>

// KernelShark
#include "plugins/event_field_plot.h"

static void efp_free_context(struct plugin_efp_context *plugin_ctx)
{
	if (!plugin_ctx)
		return;

	free(plugin_ctx->event_name);
	free(plugin_ctx->field_name);
	kshark_free_data_container(plugin_ctx->data);
}

/** A general purpose macro is used to define plugin context. */
KS_DEFINE_PLUGIN_CONTEXT(struct plugin_efp_context, efp_free_context);

static bool plugin_efp_init_context(struct kshark_data_stream *stream,
				    struct plugin_efp_context *plugin_ctx)
{
	plugin_set_event_name(plugin_ctx);
	plugin_set_field_name(plugin_ctx);
	plugin_set_select_condition(plugin_ctx);

	plugin_ctx->field_max = INT64_MIN;
	plugin_ctx->field_min = INT64_MAX;

	plugin_ctx->event_id =
		kshark_find_event_id(stream, plugin_ctx->event_name);

	if (plugin_ctx->event_id < 0) {
		fprintf(stderr, "Event %s not found in stream %s:%s\n",
			plugin_ctx->event_name, stream->file, stream->name);
		return false;
	}

	plugin_ctx->data = kshark_init_data_container();
	if (!plugin_ctx->data)
		return false;

	return true;
}

static void plugin_get_field(struct kshark_data_stream *stream, void *rec,
			     struct kshark_entry *entry)
{
	struct plugin_efp_context *plugin_ctx;
	int64_t val;

	plugin_ctx = __get_context(stream->stream_id);
	if (!plugin_ctx)
		return;

	kshark_read_record_field_int(stream, rec,
				     plugin_ctx->field_name,
				     &val);

	kshark_data_container_append(plugin_ctx->data, entry, val);

	if (val > plugin_ctx->field_max)
		plugin_ctx->field_max = val;

	if (val < plugin_ctx->field_min)
		plugin_ctx->field_min = val;
}

/** Load this plugin. */
int KSHARK_PLOT_PLUGIN_INITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_efp_context *plugin_ctx = __init(stream->stream_id);

	if (!plugin_ctx || !plugin_efp_init_context(stream, plugin_ctx)) {
		__close(stream->stream_id);
		return 0;
	}

	kshark_register_event_handler(stream,
				      plugin_ctx->event_id,
				      plugin_get_field);

	kshark_register_draw_handler(stream, draw_event_field);

	return 1;
}

/** Unload this plugin. */
int KSHARK_PLOT_PLUGIN_DEINITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_efp_context *plugin_ctx = __get_context(stream->stream_id);
	int ret = 0;

	if (plugin_ctx) {
		kshark_unregister_event_handler(stream,
						plugin_ctx->event_id,
						plugin_get_field);

		kshark_unregister_draw_handler(stream, draw_event_field);
		ret = 1;
	}

	__close(stream->stream_id);

	return ret;
}

/** Initialize the control interface of the plugin. */
void *KSHARK_MENU_PLUGIN_INITIALIZER(void *gui_ptr)
{
	return plugin_efp_add_menu(gui_ptr);
}
