// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KVMCombo.cpp
 *  @brief   Plugin for visualization of KVM events.
 */

// KernelShark
#include "plugins/kvm_combo.h"
#include "VirtComboPlotTools.hpp"
#include "KsPlugins.hpp"

/**
 * @brief Plugin's draw function.
 *
 * @param argv_c: A C pointer to be converted to KsCppArgV (C++ struct).
 * @param sdHost: Data stream identifier of the Host.
 * @param pidHost: Process Id of the virtual CPU process in the Host.
 * @param draw_action: Draw action identifier.
 */
__hidden void draw_kvm_combos(kshark_cpp_argv *argv_c,
			      int sdHost, int pidHost,
			      int draw_action)
{
	plugin_kvm_context *plugin_ctx = __get_context(sdHost);
	if (!plugin_ctx)
		return;

	drawVirtCombos(argv_c,
		       sdHost,
		       pidHost,
		       plugin_ctx->vm_entry_id,
		       plugin_ctx->vm_exit_id,
		       draw_action);
}
