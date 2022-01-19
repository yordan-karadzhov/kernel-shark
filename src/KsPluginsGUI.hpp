/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright 2022 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

/**
  *  @file    KsPluginsGUI.hpp
  *  @brief   KernelShark C++ plugin declarations.
  */

#ifndef _KS_PLUGINS_GUI_H
#define _KS_PLUGINS_GUI_H

// KernelShark
#include "libkshark.h"

void markEntryA(void *ks_ptr, const kshark_entry *e);

void markEntryB(void *ks_ptr, const kshark_entry *e);

#endif
