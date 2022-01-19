// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2022 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
  *  @file    KsPluginsGUI.cpp
  *  @brief   KernelShark C++ plugin declarations.
  */

// KernelShark
#include "KsPluginsGUI.hpp"
#include "KsMainWindow.hpp"
#include "KsDualMarker.hpp"

void markEntryA(void *ks_ptr, const kshark_entry *e)
{
	KsMainWindow *ks = static_cast<KsMainWindow *>(ks_ptr);
	ks->markEntry(e, DualMarkerState::A);
}

void markEntryB(void *ks_ptr, const kshark_entry *e)
{
	KsMainWindow *ks = static_cast<KsMainWindow *>(ks_ptr);
	ks->markEntry(e, DualMarkerState::B);
}
