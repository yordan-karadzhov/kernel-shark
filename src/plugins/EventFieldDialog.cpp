// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    EventFieldDialog.cpp
 *  @brief   Dialog class used by the EventFieldPlot plugin.
 */

// C++
#include <iostream>
#include <vector>

// KernelShark
#include "KsMainWindow.hpp"
#include "EventFieldDialog.hpp"

/** The name of the menu item used to start the dialog of the plugin. */
#define DIALOG_NAME "Plot Event Field"

/** Create plugin dialog widget. */
KsEFPDialog::KsEFPDialog(QWidget *parent)
: QDialog(parent),
  _selectLabel("Show", this),
  _applyButton("Apply", this),
  _resetButton("Reset", this),
  _cancelButton("Cancel", this)
{
	setWindowTitle(DIALOG_NAME);

	_topLayout.addWidget(&_efsWidget);

	_topLayout.addWidget(&_selectLabel);
	_setSelectCombo();
	_topLayout.addWidget(&_selectComboBox);

	_buttonLayout.addWidget(&_applyButton);
	_applyButton.setAutoDefault(false);

	_buttonLayout.addWidget(&_resetButton);
	_resetButton.setAutoDefault(false);

	_buttonLayout.addWidget(&_cancelButton);
	_cancelButton.setAutoDefault(false);

	_buttonLayout.setAlignment(Qt::AlignLeft);
	_topLayout.addLayout(&_buttonLayout);

	connect(&_applyButton,	&QPushButton::pressed,
		this,		&KsEFPDialog::_apply);

	connect(&_applyButton,	&QPushButton::pressed,
		this,		&QWidget::close);

	connect(&_resetButton,	&QPushButton::pressed,
		this,		&KsEFPDialog::_reset);

	connect(&_resetButton,	&QPushButton::pressed,
		this,		&QWidget::close);

	connect(&_cancelButton,	&QPushButton::pressed,
		this,		&QWidget::close);

	setLayout(&_topLayout);
}

void KsEFPDialog::_setSelectCombo()
{
	_selectComboBox.clear();
	_selectComboBox.addItem("max. value", 0);
	_selectComboBox.addItem("min. value", 1);
}

/** Select the plotting criteria. */
void KsEFPDialog::selectCondition(plugin_efp_context *plugin_ctx)
{
	/* In the combo box "max" is 0 and "min" is 1. */
	plugin_ctx->show_max = !_selectComboBox.currentData().toInt();
}

/** Update the dialog, using the current settings of the plugin. */
void KsEFPDialog::update()
{
	_efsWidget.setStreamCombo();
}

static KsEFPDialog *efp_dialog(nullptr);

static int plugin_get_stream_id()
{
	return efp_dialog->_efsWidget.streamId();
}

/** Use the Event name selected by the user to update the plugin's context. */
__hidden void plugin_set_event_name(plugin_efp_context *plugin_ctx)
{
	QString buff = efp_dialog->_efsWidget.eventName();
	char *event;

	if (asprintf(&event, "%s", buff.toStdString().c_str()) >= 0) {
		plugin_ctx->event_name = event;
		return;
	}

	plugin_ctx->event_name = NULL;
}

/** Use the Field name selected by the user to update the plugin's context. */
__hidden void plugin_set_field_name(plugin_efp_context *plugin_ctx)
{
	QString buff = efp_dialog->_efsWidget.fieldName();
	char *field;

	if (asprintf(&field, "%s", buff.toStdString().c_str()) >= 0) {
		plugin_ctx->field_name = field;
		return;
	}

	plugin_ctx->field_name = NULL;
}

/** Use the condition selected by the user to update the plugin's context. */
__hidden void plugin_set_select_condition(plugin_efp_context *plugin_ctx)
{
	efp_dialog->selectCondition(plugin_ctx);
}

void KsEFPDialog::_apply()
{
	auto work = KsWidgetsLib::KsDataWork::UpdatePlugins;

	/* The plugin needs to process the data and this may take time
	 * on large datasets. Show a "Work In Process" warning.
	 */
	_gui_ptr->wipPtr()->show(work);
	_gui_ptr->registerPluginToStream("event_field_plot",
					 {plugin_get_stream_id()});
	_gui_ptr->wipPtr()->hide(work);
}

void KsEFPDialog::_reset()
{
	auto work = KsWidgetsLib::KsDataWork::UpdatePlugins;
	kshark_context *kshark_ctx(nullptr);
	QVector<int> streamIds;

	if (!kshark_instance(&kshark_ctx))
		return;

	streamIds = KsUtils::getStreamIdList(kshark_ctx);

	/*
	 * The plugin needs to process the data and this may take time
	 * on large datasets. Show a "Work In Process" warning.
	 */
	_gui_ptr->wipPtr()->show(work);
	_gui_ptr->unregisterPluginFromStream("event_field_plot",
					     streamIds);
	_gui_ptr->wipPtr()->hide(work);
}

static void showDialog([[maybe_unused]] KsMainWindow *ks)
{
	efp_dialog->update();
	efp_dialog->show();
}

/** Add the dialog of the plugin to the KernelShark menus. */
__hidden void *plugin_efp_add_menu(void *ks_ptr)
{
	if (!efp_dialog) {
		efp_dialog = new KsEFPDialog();
		efp_dialog->_gui_ptr = static_cast<KsMainWindow *>(ks_ptr);
	}

	QString menu("Tools/");
	menu += DIALOG_NAME;
	efp_dialog->_gui_ptr->addPluginMenu(menu, showDialog);

	return efp_dialog;
}
