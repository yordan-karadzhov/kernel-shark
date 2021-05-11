/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KVMComboDialog.hpp
 *  @brief   Plugin for visualization of KVM events.
 */

#ifndef _KS_COMBO_DIALOG_H
#define _KS_COMBO_DIALOG_H

#include "KsMainWindow.hpp"
#include "KsWidgetsLib.hpp"

/**
 * The KsVCPUCheckBoxWidget class provides a widget for selecting CPU plots to
 * show.
 */
struct KsVCPUCheckBoxWidget : public KsWidgetsLib::KsCheckBoxTreeWidget
{
	explicit KsVCPUCheckBoxWidget(QWidget *parent = nullptr);

	void update(int GuestId,
		    struct kshark_host_guest_map *gMap, int gMapCount);
};

/**
 * The KsComboPlotDialog class provides a widget for selecting Combo plots to
 * show.
 */
class KsComboPlotDialog : public QDialog
{
	Q_OBJECT
public:
	explicit KsComboPlotDialog(QWidget *parent = nullptr);

	~KsComboPlotDialog();

	void update();

	/** KernelShark GUI (main window) object. */
	KsMainWindow	*_gui_ptr;

signals:
	/** Signal emitted when the "Apply" button is pressed. */
	void apply(int sd, QVector<int>);

private:
	int				_guestMapCount;

	struct kshark_host_guest_map	*_guestMap;

	KsVCPUCheckBoxWidget		_vcpuTree;

	QVBoxLayout			_topLayout;

	QGridLayout			_streamMenuLayout;

	QHBoxLayout			_buttonLayout;

	QLabel				_hostLabel, _hostFileLabel, _guestLabel;

	QComboBox			_guestStreamComboBox;

	QPushButton			_applyButton, _cancelButton;

	QMetaObject::Connection		_applyButtonConnection;

	QMap<int, QVector<KsComboPlot>>	_plotMap;

	int	_currentGuestStream;

	int _findGuestPlots(int sdGuest);

	QVector<KsComboPlot> _streamCombos(int sdGuest);

	void _setCurrentPlots(int guestSd);

	void _applyPress();

private slots:

	void _guestStreamChanged(const QString&);
};

#endif
