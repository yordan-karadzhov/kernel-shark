// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsQuickContextMenu.cpp
 *  @brief   Quick Context Menus for KernelShark.
 */

#include "KsQuickContextMenu.hpp"
#include "KsTraceGraph.hpp"

/**
 * @brief Create KsQuickMarkerMenu.
 *
 * @param dm: The State machine of the Dual marker.
 * @param parent: The parent of this widget.
 */
KsQuickMarkerMenu::KsQuickMarkerMenu(KsDualMarkerSM *dm, QWidget *parent)
: QMenu("Context Menu", parent),
  _dm(dm),
  _deselectAction(this)
{
	if (dm->activeMarker()._isSet) {
		addSection("Marker menu");
		_deselectAction.setText("Deselect");
		_deselectAction.setShortcut(tr("Ctrl+D"));
		_deselectAction.setStatusTip(tr("Deselect marker"));

		connect(&_deselectAction,	&QAction::triggered,
			this,			&KsQuickMarkerMenu::deselect);

		addAction(&_deselectAction);
	}
}

/**
 * @brief Create KsQuickContextMenu.
 *
 * @param dm: The State machine of the Dual marker.
 * @param data: Input location for the KsDataStore object.
 * @param row: The index of the entry used to initialize the menu.
 * @param parent: The parent of this widget.
 */
KsQuickContextMenu::KsQuickContextMenu(KsDualMarkerSM *dm,
				       KsDataStore *data, size_t row,
				       QWidget *parent)
: KsQuickMarkerMenu(dm, parent),
  _data(data),
  _row(row),
  _rawTime(this),
  _rawEvent(this),
  _graphSyncCBox(nullptr),
  _listSyncCBox(nullptr),
  _hideTaskAction(this),
  _showTaskAction(this),
  _hideEventAction(this),
  _showEventAction(this),
  _hideCPUAction(this),
  _showCPUAction(this),
  _addCPUPlotAction(this),
  _addTaskPlotAction(this),
  _removeCPUPlotAction(this),
  _removeTaskPlotAction(this),
  _clearAllFilters(this)
{
	typedef void (KsQuickContextMenu::*mfp)();
	QString time, taskName, parentName, descr;
	kshark_entry *entry = _data->rows()[_row];
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	QStringList eventFields;
	int pid, cpu, sd, ret;
	KsTraceGraph *graphs;
	int64_t fieldVal;

	if (!parent || !_data)
		return;

	if (!kshark_instance(&kshark_ctx))
		return;

	taskName = kshark_get_task(entry);
	pid = kshark_get_pid(entry);
	cpu = entry->cpu;
	sd = entry->stream_id;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return;

	QString evtData("\t"), val;
	eventFields =  KsUtils::getEventFieldsList(entry->stream_id,
						   entry->event_id);

	for (auto const &field: eventFields) {
		std::string buff = field.toStdString();
		ret = kshark_read_event_field_int(entry, buff.c_str(), &fieldVal);
		if (ret < 0)
			continue;

		evtData += field + ":  " + val.setNum(fieldVal) + "\n\t";
	}

	addSection("Raw event");
	time = QString("\ttime:  %1 [ns]").arg(entry->ts);

	_rawTime.setDefaultWidget(new QLabel(time));
	addAction(&_rawTime);
	_rawEvent.setDefaultWidget(new QLabel(evtData));
	addAction(&_rawEvent);

	auto lamAddAction = [this, &descr] (QAction *action, mfp mf) {
		action->setText(descr);

		connect(action,	&QAction::triggered,
			this,	mf);

		addAction(action);
	};

	parentName = parent->metaObject()->className();

	addSection("Pointer filter menu");

	descr = "Show task [";
	descr += taskName;
	descr += "-";
	descr += QString("%1").arg(pid);
	descr += "] only";
	lamAddAction(&_showTaskAction, &KsQuickContextMenu::_showTask);

	descr = "Hide task [";
	descr += taskName;
	descr += "-";
	descr += QString("%1").arg(pid);
	descr += "]";
	lamAddAction(&_hideTaskAction, &KsQuickContextMenu::_hideTask);

	descr = "Show event [";
	descr += kshark_get_event_name(entry);
	descr += "] only";
	lamAddAction(&_showEventAction, &KsQuickContextMenu::_showEvent);

	descr = "Hide event [";
	descr += kshark_get_event_name(entry);
	descr += "]";
	lamAddAction(&_hideEventAction, &KsQuickContextMenu::_hideEvent);

	if (parentName == "KsTraceViewer") {
		descr = QString("Show CPU [%1] only").arg(cpu);
		lamAddAction(&_showCPUAction, &KsQuickContextMenu::_showCPU);
	}

	descr = QString("Hide CPU [%1]").arg(entry->cpu);
	lamAddAction(&_hideCPUAction, &KsQuickContextMenu::_hideCPU);

	descr = "Clear all filters";
	lamAddAction(&_clearAllFilters, &KsQuickContextMenu::_clearFilters);

	addSection("Pointer plot menu");

	if (parentName == "KsTraceViewer") {
		descr = "Add [";
		descr += taskName;
		descr += "-";
		descr += QString("%1").arg(pid);
		descr += "] plot";
		lamAddAction(&_addTaskPlotAction,
			     &KsQuickContextMenu::_addTaskPlot);
	}

	if (parentName == "KsTraceGraph" &&
	    (graphs = dynamic_cast<KsTraceGraph *>(parent))) {
		if (graphs->glPtr()->_streamPlots[sd]._taskList.contains(pid)) {
			descr = "Remove [";
			descr += taskName;
			descr += "-";
			descr += QString("%1").arg(pid);
			descr += "] plot";
			lamAddAction(&_removeTaskPlotAction,
				     &KsQuickContextMenu::_removeTaskPlot);
		} else {
			descr = "Add [";
			descr += taskName;
			descr += "-";
			descr += QString("%1").arg(pid);
			descr += "] plot";
			lamAddAction(&_addTaskPlotAction,
				     &KsQuickContextMenu::_addTaskPlot);
		}

		if (graphs->glPtr()->_streamPlots[sd]._cpuList.contains(cpu)) {
			descr = "Remove [CPU ";
			descr += QString("%1").arg(cpu);
			descr += "] plot";
			lamAddAction(&_removeCPUPlotAction,
				     &KsQuickContextMenu::_removeCPUPlot);
		} else {
			descr = "Add [CPU ";
			descr += QString("%1").arg(cpu);
			descr += "] plot";
			lamAddAction(&_addCPUPlotAction,
				     &KsQuickContextMenu::_addCPUPlot);
		}
	}
}

void KsQuickContextMenu::_hideTask()
{
	int pid = kshark_get_pid(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	QVector<int> vec;

	if (!kshark_instance(&kshark_ctx))
		return;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return;

	vec =_getFilterVector(stream->hide_task_filter, pid);
	_data->applyNegTaskFilter(sd, vec);
}

void KsQuickContextMenu::_showTask()
{
	int pid = kshark_get_pid(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;
	_data->applyPosTaskFilter(sd, QVector<int>(1, pid));
}

void KsQuickContextMenu::_hideEvent()
{
	int eventId = kshark_get_event_id(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	QVector<int> vec;

	if (!kshark_instance(&kshark_ctx))
		return;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return;

	vec =_getFilterVector(stream->hide_event_filter, eventId);
	_data->applyNegEventFilter(sd, vec);
}

void KsQuickContextMenu::_showEvent()
{
	int eventId = kshark_get_event_id(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;

	_data->applyPosEventFilter(sd, QVector<int>(1, eventId));
}

void KsQuickContextMenu::_showCPU()
{
	int cpu = _data->rows()[_row]->cpu;
	int sd = _data->rows()[_row]->stream_id;

	_data->applyPosCPUFilter(sd, QVector<int>(1, cpu));
}

void KsQuickContextMenu::_hideCPU()
{
	int sd = _data->rows()[_row]->stream_id;
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	QVector<int> vec;

	if (!kshark_instance(&kshark_ctx))
		return;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return;

	vec =_getFilterVector(stream->hide_cpu_filter,
			      _data->rows()[_row]->cpu);

	_data->applyNegCPUFilter(sd, vec);
}

QVector<int> KsQuickContextMenu::_getFilterVector(kshark_hash_id *filter, int newId)
{
	QVector<int> vec = KsUtils::getFilterIds(filter);
	if (!vec.contains(newId))
		vec.append(newId);

	return vec;
}

void KsQuickContextMenu::_addTaskPlot()
{
	int pid = kshark_get_pid(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;

	emit addTaskPlot(sd, pid);
}

void KsQuickContextMenu::_addCPUPlot()
{
	emit addCPUPlot(_data->rows()[_row]->stream_id, _data->rows()[_row]->cpu);
}

void KsQuickContextMenu::_removeTaskPlot()
{
	int pid = kshark_get_pid(_data->rows()[_row]);
	int sd = _data->rows()[_row]->stream_id;

	emit removeTaskPlot(sd, pid);
}

void KsQuickContextMenu::_removeCPUPlot()
{
	emit removeCPUPlot(_data->rows()[_row]->stream_id, _data->rows()[_row]->cpu);
}

/**
 * @brief Create KsRmPlotContextMenu.
 *
 * @param dm: The State machine of the Dual marker.
 * @param sd: Data stream identifier.
 * @param parent: The parent of this widget.
 */
KsRmPlotContextMenu::KsRmPlotContextMenu(KsDualMarkerSM *dm, int sd,
					 QWidget *parent)
: KsQuickMarkerMenu(dm, parent),
  _removePlotAction(this),
  _sd(sd)
{
	addSection("Plots");

	connect(&_removePlotAction,	&QAction::triggered,
		this,			&KsRmPlotContextMenu::removePlot);

	addAction(&_removePlotAction);
}

/**
 * @brief Create KsRmCPUPlotMenu.
 *
 * @param dm: The State machine of the Dual marker.
 * @param sd: Data stream identifier.
 * @param cpu : CPU Id.
 * @param parent: The parent of this widget.
 */
KsRmCPUPlotMenu::KsRmCPUPlotMenu(KsDualMarkerSM *dm, int sd, int cpu,
				 QWidget *parent)
: KsRmPlotContextMenu(dm, sd, parent)
{
	_removePlotAction.setText(QString("Remove [CPU %1]").arg(cpu));
}

/**
 * @brief Create KsRmTaskPlotMenu.
 *
 * @param dm: The State machine of the Dual marker.
 * @param sd: Data stream identifier.
 * @param pid: Process Id.
 * @param parent: The parent of this widget.
 */
KsRmTaskPlotMenu::KsRmTaskPlotMenu(KsDualMarkerSM *dm, int sd, int pid,
				   QWidget *parent)
: KsRmPlotContextMenu(dm, sd, parent)
{
	QString descr;

	descr = "Remove [ ";
	descr += kshark_comm_from_pid(sd, pid);
	descr += "-";
	descr += QString("%1").arg(pid);
	descr += "] plot";
	_removePlotAction.setText(descr);
}
