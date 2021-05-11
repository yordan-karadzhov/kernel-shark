// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    kvm_combo.c
 *  @brief   Plugin for visualization of KVM events.
 */

// C
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// KernelShark
#include "plugins/kvm_combo.h"
#include "libkshark-plugin.h"
#include "libkshark-tepdata.h"

/** A general purpose macro is used to define plugin context. */
KS_DEFINE_PLUGIN_CONTEXT(struct plugin_kvm_context, free);

static bool plugin_kvm_init_context(struct kshark_data_stream *stream,
				    struct plugin_kvm_context *plugin_ctx)
{
	plugin_ctx->vm_entry_id = kshark_find_event_id(stream, "kvm/kvm_entry");
	plugin_ctx->vm_exit_id = kshark_find_event_id(stream, "kvm/kvm_exit");
	if (plugin_ctx->vm_entry_id < 0 ||
	    plugin_ctx->vm_exit_id < 0)
		return false;

	return true;
}

/** Load this plugin. */
int KSHARK_PLOT_PLUGIN_INITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_kvm_context *plugin_ctx = __init(stream->stream_id);

	if (!plugin_ctx || !plugin_kvm_init_context(stream, plugin_ctx)) {
		__close(stream->stream_id);
		return 0;
	}

	kshark_register_draw_handler(stream, draw_kvm_combos);

	return 1;
}

/** Unload this plugin. */
int KSHARK_PLOT_PLUGIN_DEINITIALIZER(struct kshark_data_stream *stream)
{
	struct plugin_kvm_context *plugin_ctx = __get_context(stream->stream_id);
	int ret = 0;

	if (plugin_ctx) {
		kshark_unregister_draw_handler(stream, draw_kvm_combos);
		ret = 1;
	}

	__close(stream->stream_id);

	return ret;
}

/** Initialize the control interface of the plugin. */
void *KSHARK_MENU_PLUGIN_INITIALIZER(void *gui_ptr)
{
	return plugin_kvm_add_menu(gui_ptr);
}
