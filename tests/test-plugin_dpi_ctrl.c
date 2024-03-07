// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2021 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

// C
#include <stdio.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"

/** Load this plugin. */
int KSHARK_PLOT_PLUGIN_INITIALIZER(__attribute__ ((unused)) struct kshark_data_stream *stream)
{
	printf("--> plugin2\n");
	return 2;
}

/** Unload this plugin. */
int KSHARK_PLOT_PLUGIN_DEINITIALIZER(__attribute__ ((unused)) struct kshark_data_stream *stream)
{
	printf("<-- plugin2\n");
	return 2;
}

/** Initialize the control interface of the plugin. */
void *KSHARK_MENU_PLUGIN_INITIALIZER(__attribute__ ((unused)) void *ptr)
{
	return NULL;
}
