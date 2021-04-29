/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    EventFieldDialog.hpp
 *  @brief   Dialog class used by the EventFieldPlot plugin.
 */

#ifndef _KS_EFP_DIALOG_H
#define _KS_EFP_DIALOG_H

// KernelShark
#include "plugins/event_field_plot.h"
#include "KsWidgetsLib.hpp"

class KsMainWindow;

/**
 * The KsEFPDialog class provides a widget for selecting Trace event field to
 * be visualized.
 */

class KsEFPDialog : public QDialog
{
	Q_OBJECT
public:
	explicit KsEFPDialog(QWidget *parent = nullptr);

	void update();

	void selectCondition(plugin_efp_context *plugin_ctx);

	/** Widget for selecting Treace event. */
	KsWidgetsLib::KsEventFieldSelectWidget	_efsWidget;

	/** KernelShark GUI (main window) object. */
	KsMainWindow	*_gui_ptr;

private:
	QVBoxLayout	_topLayout;

	QHBoxLayout	_buttonLayout;

	QComboBox	_selectComboBox;

	QLabel		_selectLabel;

	QPushButton	_applyButton, _resetButton, _cancelButton;

	void _setSelectCombo();

	void _apply();

	void _reset();
};

#endif
