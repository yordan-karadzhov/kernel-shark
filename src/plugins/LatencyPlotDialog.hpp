/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    LatencyPlotDialog.hpp
 *  @brief   Dialog class used by the LatencyPlot plugin.
 */

#ifndef _KS_EFP_DIALOG_H
#define _KS_EFP_DIALOG_H

// KernelShark
#include "plugins/latency_plot.h"
#include "KsWidgetsLib.hpp"

class KsMainWindow;

/**
 * The LatencyPlotDialog class provides a widget for selecting Trace event field to
 * be visualized.
 */

class LatencyPlotDialog : public QDialog
{
	Q_OBJECT
public:
	explicit LatencyPlotDialog(QWidget *parent = nullptr);

	void update();

	/** Widget for selecting Treace event A. */
	KsWidgetsLib::KsEventFieldSelectWidget	_efsWidgetA;

	/** Widget for selecting Treace event B. */
	KsWidgetsLib::KsEventFieldSelectWidget	_efsWidgetB;

	/** KernelShark GUI (main window) object. */
	KsMainWindow	*_gui_ptr;

private:
	QVBoxLayout	_topLayout;

	QGridLayout	_fieldSelectLayout;

	QHBoxLayout	_buttonLayout;

	QLabel		_evtALabel, _evtBLabel;

	QPushButton	_applyButton, _resetButton, _cancelButton;

	void _apply();

	void _reset();
};

#endif
