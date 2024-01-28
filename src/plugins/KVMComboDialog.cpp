// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    KVMComboDialog.cpp
 *  @brief   Dialog class used by the KVMCombo plugin.
 */

// KernelShark
#include "libkshark.h"
#include "libkshark-tepdata.h"
#include "plugins/kvm_combo.h"
#include "KsMainWindow.hpp"
#include "KVMComboDialog.hpp"

using namespace KsWidgetsLib;

static KsComboPlotDialog *combo_dialog(nullptr);

static QMetaObject::Connection combo_dialogConnection;

/** The name of the menu item used to start the dialog of the plugin. */
#define DIALOG_NAME	"KVM Combo plots"

static void showDialog(KsMainWindow *ks)
{
	kshark_context *kshark_ctx(nullptr);

	if (!kshark_instance(&kshark_ctx))
		return;

	if (kshark_ctx->n_streams < 2) {
		QString err("Data from one Host and at least one Guest is required.");
		QMessageBox msgBox;
		msgBox.critical(nullptr, "Error", err);

		return;
	}

	combo_dialog->update();

	if (!combo_dialogConnection) {
		combo_dialogConnection =
			QObject::connect(combo_dialog,	&KsComboPlotDialog::apply,
					 ks->graphPtr(),&KsTraceGraph::comboReDraw);
	}

	combo_dialog->show();
}

/** Add the dialog of the plugin to the KernelShark menus. */
__hidden void *plugin_kvm_add_menu(void *ks_ptr)
{
	KsMainWindow *ks = static_cast<KsMainWindow *>(ks_ptr);
	QString menu("Plots/");
	menu += DIALOG_NAME;
	ks->addPluginMenu(menu, showDialog);

	if (!combo_dialog)
		combo_dialog = new KsComboPlotDialog();

	combo_dialog->_gui_ptr = ks;

	return combo_dialog;
}

/**
 * @brief Create KsCPUCheckBoxWidget.
 *
 * @param parent: The parent of this widget.
 */
KsVCPUCheckBoxWidget::KsVCPUCheckBoxWidget(QWidget *parent)
: KsCheckBoxTreeWidget(0, "vCPUs", parent)
{
	int height(FONT_HEIGHT * 1.5);
	QString style;

	style = QString("QTreeView::item { height: %1 ;}").arg(height);
	_tree.setStyleSheet(style);

	_initTree();
}

/**
 * Update the widget according to the mapping between host processes and guest
 * virtual CPUs.
 */
void KsVCPUCheckBoxWidget::update(int guestId,
				  kshark_host_guest_map *gMap, int gMapCount)
{
	KsPlot::ColorTable colTable;
	QColor color;
	int j;

	for (j = 0; j < gMapCount; j++)
		if (gMap[j].guest_id == guestId)
			break;
	if (j == gMapCount)
		return;

	_tree.clear();
	_id.resize(gMap[j].vcpu_count);
	_cb.resize(gMap[j].vcpu_count);
	colTable = KsPlot::CPUColorTable();

	for (int i = 0; i < gMap[j].vcpu_count; ++i) {
		QString strCPU = QLatin1String("vCPU ") + QString::number(i);
		strCPU += (QLatin1String("\t<") + QLatin1String(gMap[j].guest_name) + QLatin1Char('>'));

		QTreeWidgetItem *cpuItem = new QTreeWidgetItem;
		cpuItem->setText(0, "  ");
		cpuItem->setText(1, strCPU);
		cpuItem->setCheckState(0, Qt::Checked);
		color << colTable[i];
		cpuItem->setBackground(0, color);
		_tree.addTopLevelItem(cpuItem);
		_id[i] = i;
		_cb[i] = cpuItem;
	}

	_adjustSize();
	setDefault(false);
}

//! @cond Doxygen_Suppress

#define LABEL_WIDTH	(FONT_WIDTH * 50)

//! @endcond

/** Create default KsComboPlotDialog. */
KsComboPlotDialog::KsComboPlotDialog(QWidget *parent)
: QDialog(parent),
  _vcpuTree(this),
  _hostLabel("Host:", this),
  _hostFileLabel("", this),
  _guestLabel("Guest:", this),
  _guestStreamComboBox(this),
  _applyButton("Apply", this),
  _cancelButton("Cancel", this),
  _currentGuestStream(0)
{
	kshark_context *kshark_ctx(nullptr);
	int buttonWidth;

	auto lamAddLine = [&] {
		QFrame* line = new QFrame();

		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		_topLayout.addWidget(line);
	};

	setWindowTitle(DIALOG_NAME);

	if (!kshark_instance(&kshark_ctx))
		return;

	_guestStreamComboBox.setMaximumWidth(LABEL_WIDTH);

	_streamMenuLayout.addWidget(&_hostLabel, 0, 0);
	_streamMenuLayout.addWidget(&_hostFileLabel, 0, 1);
	_streamMenuLayout.addWidget(&_guestLabel, 1, 0);
	_streamMenuLayout.addWidget(&_guestStreamComboBox, 1, 1);

	_topLayout.addLayout(&_streamMenuLayout);

	lamAddLine();

	_topLayout.addWidget(&_vcpuTree);

	lamAddLine();

	buttonWidth = STRING_WIDTH("--Cancel--");
	_applyButton.setFixedWidth(buttonWidth);
	_cancelButton.setFixedWidth(buttonWidth);

	_buttonLayout.addWidget(&_applyButton);
	_applyButton.setAutoDefault(false);

	_buttonLayout.addWidget(&_cancelButton);
	_cancelButton.setAutoDefault(false);

	_buttonLayout.setAlignment(Qt::AlignLeft);
	_topLayout.addLayout(&_buttonLayout);

	connect(&_applyButton,	&QPushButton::pressed,
		this,		&QWidget::close);

	connect(&_cancelButton,	&QPushButton::pressed,
		this,		&QWidget::close);

	/*
	 * Using the old Signal-Slot syntax because QComboBox::currentIndexChanged
	 * has overloads.
	 */
	connect(&_guestStreamComboBox,	&QComboBox::currentIndexChanged,
		this,			&KsComboPlotDialog::_guestStreamChanged);

	setLayout(&_topLayout);

	_guestMapCount = 0;
	_guestMap = nullptr;
}

KsComboPlotDialog::~KsComboPlotDialog()
{
	kshark_tracecmd_free_hostguest_map(_guestMap, _guestMapCount);
}

/** Update the Plugin dialog. */
void KsComboPlotDialog::update()
{
	kshark_context *kshark_ctx(nullptr);
	KsPlot::ColorTable colTable;
	QString streamName;
	QColor color;
	int ret, sd, i;

	if (!kshark_instance(&kshark_ctx))
		return;

	kshark_tracecmd_free_hostguest_map(_guestMap, _guestMapCount);
	_guestMap = nullptr;
	_guestMapCount = 0;
	ret = kshark_tracecmd_get_hostguest_mapping(&_guestMap);
	if (ret <= 0) {
		QString err("Cannot find host / guest tracing into the loaded streams");
		QMessageBox msgBox;
		msgBox.critical(nullptr, "Error", err);
		return;
	} else {
		_guestMapCount = ret;
	}

	streamName = KsUtils::streamDescription(kshark_ctx->stream[_guestMap[0].host_id]);
	KsUtils::setElidedText(&_hostFileLabel, streamName, Qt::ElideLeft, LABEL_WIDTH);

	_guestStreamComboBox.clear();
	colTable = KsPlot::streamColorTable();
	for (i = 0; i < _guestMapCount; i++) {
		sd = _guestMap[i].guest_id;
		if (sd >= kshark_ctx->n_streams)
			continue;

		streamName = KsUtils::streamDescription(kshark_ctx->stream[sd]);
		_guestStreamComboBox.addItem(streamName, sd);
		color << colTable[sd];
		_guestStreamComboBox.setItemData(i, QBrush(color),
						    Qt::BackgroundRole);
	}

	if (!_applyButtonConnection) {
		_applyButtonConnection =
			connect(&_applyButton,	&QPushButton::pressed,
				this,		&KsComboPlotDialog::_applyPress);
	}

	sd = _guestStreamComboBox.currentData().toInt();
	_setCurrentPlots(sd);
}

int KsComboPlotDialog::_findGuestPlots(int sdGuest)
{
	for (int i = 0; i < _guestMapCount; i++)
		if (_guestMap[i].guest_id == sdGuest)
			return i;
	return -1;
}

QVector<KsComboPlot> KsComboPlotDialog::_streamCombos(int sdGuest)
{
	QVector<int> cbVec = _vcpuTree.getCheckedIds();
	int j = _findGuestPlots(sdGuest);
	QVector <KsComboPlot> plots;
	KsComboPlot combo(2);

	if (j < 0)
		return {};

	for (auto const &i: cbVec) {
		if (i >= _guestMap[j].vcpu_count)
			continue;

		combo[0]._streamId = _guestMap[j].guest_id;
		combo[0]._id = i;
		combo[0]._type = KSHARK_CPU_DRAW |
				 KSHARK_GUEST_DRAW;

		combo[1]._streamId = _guestMap[j].host_id;
		combo[1]._id = _guestMap[j].cpu_pid[i];
		combo[1]._type = KSHARK_TASK_DRAW |
				 KSHARK_HOST_DRAW;

		plots.append(combo);
	}

	return plots;
}

void KsComboPlotDialog::_applyPress()
{
	int guestId = _guestStreamComboBox.currentData().toInt();
	QVector<int> allCombosVec;
	int nPlots(0);

	_plotMap[guestId] = _streamCombos(guestId);

	for (auto it = _plotMap.cbegin(), end = _plotMap.cend(); it != end; ++it) {
			for (auto const &combo: it.value()) {
			allCombosVec.append(2);
			combo[0] >> allCombosVec;
			combo[1] >> allCombosVec;
			++nPlots;
		}
	}

	emit apply(nPlots, allCombosVec);
}

void KsComboPlotDialog::_setCurrentPlots(int sdGuest)
{
	QVector<KsComboPlot> currentCombos =_plotMap[sdGuest];
	int i = _findGuestPlots(sdGuest);
	if (i < 0 || _guestMap[i].vcpu_count <= 0)
		return;

	QVector<int> vcpuCBs(_guestMap[i].vcpu_count, 0);
	for(auto const &p: currentCombos)
		vcpuCBs[p[0]._id] = 1;

	_vcpuTree.set(vcpuCBs);
}

void KsComboPlotDialog::_guestStreamChanged(int)
{
	QString sdStr = _guestStreamComboBox.currentText();
	if (sdStr.isEmpty())
		return;

	int newGuestId = _guestStreamComboBox.currentData().toInt();

	_plotMap[_currentGuestStream] = _streamCombos(_currentGuestStream);

	_vcpuTree.update(newGuestId, _guestMap, _guestMapCount);
	_setCurrentPlots(newGuestId);

	_currentGuestStream = newGuestId;
}
