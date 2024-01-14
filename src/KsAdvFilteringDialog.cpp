// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsAdvFilteringDialog.cpp
 *  @brief   GUI Dialog for Advanced filtering settings.
 */

// KernelShark
#include "libkshark.h"
#include "libkshark-tepdata.h"
#include "KsUtils.hpp"
#include "KsAdvFilteringDialog.hpp"

using namespace KsWidgetsLib;

/** Create dialog for Advanced Filtering. */
KsAdvFilteringDialog::KsAdvFilteringDialog(QWidget *parent)
: QDialog(parent),
  _condToolBar1(this),
  _condToolBar2(this),
  _condToolBar3(this),
  _descrLabel(this),
  _sysEvLabel("System/Event: ", &_condToolBar1),
  _opsLabel("Operator: ", this),
  _fieldLabel("Field: ", this),
  _systemComboBox(&_condToolBar1),
  _eventComboBox(&_condToolBar1),
  _opsComboBox(&_condToolBar2),
  _fieldComboBox(&_condToolBar3),
  _filterEdit(this),
  _helpButton("Show Help", this),
  _insertEvtButton("Insert", this),
  _insertOpButton("Insert", this),
  _insertFieldButton("Insert", this),
  _applyButton("Apply", this),
  _cancelButton("Cancel", this)
{
	struct kshark_context *kshark_ctx(NULL);
	int buttonWidth;

	if (!kshark_instance(&kshark_ctx))
		return;

	auto lamAddLine = [&] {
		QFrame* line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		_topLayout.addWidget(line);
	};

	setMinimumWidth(FONT_WIDTH * 80);

	buttonWidth = STRING_WIDTH("--Show Help--");

	_helpButton.setFixedWidth(buttonWidth);
	_helpButton.setDefault(false);
	_topLayout.addWidget(&_helpButton);

	connect(&_helpButton,	&QPushButton::pressed,
		this,		&KsAdvFilteringDialog::_help);

	_descrLabel.setText(_description());
	_topLayout.addWidget(&_descrLabel);

	/*
	 * For the moment do not show the syntax description. It will be shown
	 * only if the "Show Help" button is clicked.
	 */
	_descrLabel.hide();

	lamAddLine();

	_topLayout.addWidget(&_streamComboBox);
	_getFtraceStreams(kshark_ctx);

	_getFilters(kshark_ctx);

	if (_filters.count()) {
		_makeFilterTable();
		lamAddLine();
	}

	_condToolBar1.addWidget(&_sysEvLabel);
	_condToolBar1.addWidget(&_systemComboBox);
	_condToolBar1.addWidget(&_eventComboBox);

	connect(&_systemComboBox,	&QComboBox::currentIndexChanged,
		this,			&KsAdvFilteringDialog::_systemChanged);

	connect(&_eventComboBox,	&QComboBox::currentIndexChanged,
		this,			&KsAdvFilteringDialog::_eventChanged);

	_setSystemCombo(kshark_ctx);

	_condToolBar1.addSeparator();
	_condToolBar1.addWidget(&_insertEvtButton);
	_topLayout.addWidget(&_condToolBar1);

	_opsComboBox.addItems(_operators());

	_condToolBar2.addWidget(&_opsLabel);
	_condToolBar2.addWidget(&_opsComboBox);

	_condToolBar2.addSeparator();
	_condToolBar2.addWidget(&_insertOpButton);
	_topLayout.addWidget(&_condToolBar2);

	_condToolBar3.addWidget(&_fieldLabel);
	_condToolBar3.addWidget(&_fieldComboBox);

	_condToolBar3.addSeparator();
	_condToolBar3.addWidget(&_insertFieldButton);
	_topLayout.addWidget(&_condToolBar3);

	lamAddLine();

	_filterEdit.setMinimumWidth(50 * FONT_WIDTH);
	_topLayout.addWidget(&_filterEdit);
	this->setLayout(&_topLayout);

	buttonWidth = STRING_WIDTH("--Cancel--");
	_applyButton.setFixedWidth(buttonWidth);
	_applyButton.setDefault(true);
	_cancelButton.setFixedWidth(buttonWidth);
	_buttonLayout.addWidget(&_applyButton);
	_buttonLayout.addWidget(&_cancelButton);
	_buttonLayout.setAlignment(Qt::AlignLeft);
	_topLayout.addLayout(&_buttonLayout);

	connect(&_insertEvtButton,	&QPushButton::pressed,
		this,			&KsAdvFilteringDialog::_insertEvt);

	connect(&_insertOpButton,	&QPushButton::pressed,
		this,			&KsAdvFilteringDialog::_insertOperator);

	connect(&_insertFieldButton,	&QPushButton::pressed,
		this,			&KsAdvFilteringDialog::_insertField);

	_applyButtonConnection =
		connect(&_applyButton,	&QPushButton::pressed,
			this,		&KsAdvFilteringDialog::_applyPress);

	connect(&_applyButton,		&QPushButton::pressed,
		this,			&QWidget::close);

	connect(&_cancelButton,		&QPushButton::pressed,
		this,			&QWidget::close);
}

kshark_data_stream *
KsAdvFilteringDialog::_getCurrentStream(kshark_context *kshark_ctx)
{
	int sd = _streamComboBox.currentData().toInt();

	return kshark_get_data_stream(kshark_ctx, sd);
}

void KsAdvFilteringDialog::_setSystemCombo(kshark_context *kshark_ctx)
{
	kshark_data_stream *stream;
	QVector<int> eventIds;
	QStringList sysList;
	int i(0);

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return;

	eventIds = KsUtils::getEventIdList(stream->stream_id);

	auto lamGetSysName = [&stream] (int eventId) {
		QStringList name = KsUtils::getTepEvtName(stream->stream_id,
							  eventId);
		return name[0];
	};

	while (i < stream->n_events) {
		QString sysName = lamGetSysName(eventIds[i]);
		sysList << sysName;
		while (sysName == lamGetSysName(eventIds[i])) {
			if (++i == stream->n_events)
				break;
		}
	}

	std::sort(sysList.begin(), sysList.end());
	_systemComboBox.addItems(sysList);

	i = _systemComboBox.findText("ftrace");
	if (i >= 0)
		_systemComboBox.setCurrentIndex(i);
}

QString KsAdvFilteringDialog::_description()
{
	QString descrText = "Usage:\n";
	descrText += " <sys/event>[,<sys/event>] : [!][(]<field><op><val>[)]";
	descrText += "[&&/|| [(]<field><op><val>[)]]\n\n";
	descrText += "Examples:\n\n";
	descrText += "   sched/sched_switch : next_prio < 100 && (prev_prio > 100";
	descrText += "&& prev_pid != 0)\n\n";
	descrText += "   irq.* : irq != 38\n\n";
	descrText += "   .* : common_pid == 1234\n";

	return descrText;
}

QStringList KsAdvFilteringDialog::_operators()
{
	QStringList OpsList;
	OpsList << ":" << "," << "==" << "!=" << ">" << "<" << ">=" << "<=";
	OpsList << "=~" << "!~" << "!" << "(" << ")" << "+" << "-";
	OpsList << "*" << "/" << "<<" << ">>" << "&&" << "||" << "&";

	return OpsList;
}

void KsAdvFilteringDialog::_getFtraceStreams(kshark_context *kshark_ctx)
{
	kshark_data_stream *stream;
	QVector<int> streamIds;

	_streamComboBox.clear();
	streamIds = KsUtils::getStreamIdList(kshark_ctx);
	for (auto const &sd: streamIds) {
		stream = kshark_ctx->stream[sd];
		if (kshark_is_tep(stream))
			_streamComboBox.addItem(KsUtils::streamDescription(stream), sd);
	}

	if (!_streamComboBox.count())
		_streamComboBox.addItem("No FTRACE data loaded", -1);
}

void KsAdvFilteringDialog::_getFilters(kshark_context *kshark_ctx)
{
	kshark_data_stream *stream;
	QVector<int> eventIds;
	QStringList eventName;
	char *filterStr;

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return;

	eventIds = KsUtils::getEventIdList(stream->stream_id);
	for (int i = 0; i < stream->n_events; ++i) {
		eventName = KsUtils::getTepEvtName(stream->stream_id, eventIds[i]);
		filterStr = kshark_tep_filter_make_string(stream, eventIds[i]);
		if (!filterStr)
			continue;

		_filters.insert(eventIds[i],
				QString("%1:%2/%3:%4").arg(QString::number(stream->stream_id),
							   eventName[0],
							   eventName[1],
							   filterStr));

		free(filterStr);
	}
}

void KsAdvFilteringDialog::_makeFilterTable()
{
	QMapIterator<int, QString> f(_filters);
	QTableWidgetItem *i1, *i2, *i3;
	QStringList headers;
	int count(0);

	_table = new KsCheckBoxTable(this);
	_table->setSelectionMode(QAbstractItemView::SingleSelection);
	headers << "Delete" << "Stream" << "Event" << " Id" << "Filter";
	_table->init(headers, _filters.count());

	for (auto it = _filters.cbegin(), end = _filters.cend(); it != end; ++it) {
		QStringList thisFilter = it.value().split(":");

		i1 = new QTableWidgetItem(thisFilter[0]);
		_table->setItem(count, 1, i1);

		i1 = new QTableWidgetItem(thisFilter[1]);
		_table->setItem(count, 2, i1);

		i2 = new QTableWidgetItem(tr("%1").arg(it.key()));
		_table->setItem(count, 3, i2);

		i3 = new QTableWidgetItem(thisFilter[2]);
		_table->setItem(count, 4, i3);

		++count;
	}

	_table->setVisible(false);
	_table->resizeColumnsToContents();
	_table->setVisible(true);

	_topLayout.addWidget(_table);
}

void KsAdvFilteringDialog::_help()
{
	if (_descrLabel.isVisible()) {
		_descrLabel.hide();
		QApplication::processEvents();

		_helpButton.setText("Show Help");
		resize(width(), _noHelpHeight);
	} else {
		_helpButton.setText("Hide Help");
		_noHelpHeight = height();
		_descrLabel.show();
	}
}

void KsAdvFilteringDialog::_systemChanged(int)
{
	QString sysName = _systemComboBox.currentText();
	kshark_context *kshark_ctx(NULL);
	kshark_data_stream *stream;
	QStringList evtsList, name;
	QVector<int> eventIds;
	int i;

	if (!kshark_instance(&kshark_ctx))
		return;

	_eventComboBox.clear();

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return;

	eventIds = KsUtils::getEventIdList(stream->stream_id);
	for (i = 0; i < stream->n_events; ++i) {
		name = KsUtils::getTepEvtName(stream->stream_id, eventIds[i]);
		if (sysName == name[0])
			evtsList << name[1];
	}

	std::sort(evtsList.begin(), evtsList.end());
	_eventComboBox.addItems(evtsList);

	i = _eventComboBox.findText("function");
	if (i >= 0)
		_eventComboBox.setCurrentIndex(i);
}

QStringList
KsAdvFilteringDialog::_getEventFields(int eventId)
{
	kshark_context *kshark_ctx(NULL);
	kshark_data_stream *stream;
	QStringList fieldList;

	if (!kshark_instance(&kshark_ctx))
		return {};

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return {};

	fieldList = KsUtils::getEventFieldsList(stream->stream_id, eventId);

	std::sort(fieldList.begin(), fieldList.end());
	return fieldList;
}

void KsAdvFilteringDialog::_eventChanged(int)
{
	QString sysName = _systemComboBox.currentText();
	QString evtName = _eventComboBox.currentText();
	QStringList fieldList, eventName;
	kshark_context *kshark_ctx(NULL);
	kshark_data_stream *stream;
	QVector<int> eventIds;

	_fieldComboBox.clear();
	if (!kshark_instance(&kshark_ctx))
		return;

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return;

	eventIds = KsUtils::getEventIdList(stream->stream_id);
	for (int i = 0; i < stream->n_events; ++i) {
		eventName = KsUtils::getTepEvtName(stream->stream_id, eventIds[i]);
		if (sysName == eventName[0] && evtName == eventName[1]) {
			fieldList = _getEventFields(eventIds[i]);
			_fieldComboBox.addItems(fieldList);

			return;
		}
	}
}

void KsAdvFilteringDialog::_insertEvt()
{
	QString text = _filterEdit.text();

	auto set_evt = [&] ()
	{
		text += _systemComboBox.currentText();
		text += "/";
		text += _eventComboBox.currentText();
	};

	if (text == "") {
		set_evt();
		text += ":";
	} else {
		QString evt = text;
		text = "";
		set_evt();
		text += ",";
		text += evt;
	}
	_filterEdit.setText(text);
}

void KsAdvFilteringDialog::_insertOperator()
{
	QString text = _filterEdit.text();

	text += _opsComboBox.currentText();
	_filterEdit.setText(text);
}

void KsAdvFilteringDialog::_insertField()
{
	QString text = _filterEdit.text();

	text += _fieldComboBox.currentText();
	_filterEdit.setText(text);
}

void KsAdvFilteringDialog::_applyPress()
{
	QMapIterator<int, QString> f(_filters);
	kshark_context *kshark_ctx(NULL);
	kshark_data_stream *stream;
	int i(0);

	if (!kshark_instance(&kshark_ctx))
		return;

	stream = _getCurrentStream(kshark_ctx);
	if (!stream || !kshark_is_tep(stream))
		return;

	while (f.hasNext()) {
		f.next();
		if (_table->_cb[i]->checkState() == Qt::Checked) {
			kshark_data_stream *filter_stream;
			int sd = f.value().split(":").at(0).toInt();

			filter_stream = kshark_get_data_stream(kshark_ctx, sd);
			kshark_tep_filter_remove_event(filter_stream, f.key());
		}

		++i;
	}

	auto job_done = [&]() {
		/*
		* Disconnect Apply button. This is done in order to protect
		* against multiple clicks.
		*/
		disconnect(_applyButtonConnection);
		emit dataReload();
	};

	QByteArray filter = _filterEdit.text().toLocal8Bit();
	if (filter.isEmpty()) {
		job_done();
		return;
	}

	kshark_tep_add_filter_str(stream, filter.constData());
	job_done();
}
