// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsTraceGraph.cpp
 *  @brief   KernelShark Trace Graph widget.
 */

// KernelShark
#include "KsUtils.hpp"
#include "KsDualMarker.hpp"
#include "KsTraceGraph.hpp"
#include "KsQuickContextMenu.hpp"

/** Create a default (empty) Trace graph widget. */
KsTraceGraph::KsTraceGraph(QWidget *parent)
: KsWidgetsLib::KsDataWidget(parent),
  _pointerBar(this),
  _navigationBar(this),
  _zoomInButton("+", this),
  _quickZoomInButton("++", this),
  _zoomOutButton("-", this),
  _quickZoomOutButton("- -", this),
  _scrollLeftButton("<", this),
  _scrollRightButton(">", this),
  _labelP1("Pointer: ", this),
  _labelP2("", this),
  _labelI1("", this),
  _labelI2("", this),
  _labelI3("", this),
  _labelI4("", this),
  _labelI5("", this),
  _scrollArea(this),
  _glWindow(&_scrollArea),
  _mState(nullptr),
  _data(nullptr),
  _keyPressed(false)
{
	auto lamMakeNavButton = [&](QPushButton *b) {
		b->setMaximumWidth(FONT_WIDTH * 5);

		connect(b,	&QPushButton::released,
			this,	&KsTraceGraph::_stopUpdating);
		_navigationBar.addWidget(b);
	};

	_pointerBar.setMaximumHeight(FONT_HEIGHT * 1.75);
	_pointerBar.setOrientation(Qt::Horizontal);

	_navigationBar.setMaximumHeight(FONT_HEIGHT * 1.75);
	_navigationBar.setMinimumWidth(FONT_WIDTH * 90);
	_navigationBar.setOrientation(Qt::Horizontal);

	_pointerBar.addWidget(&_labelP1);
	_labelP2.setFrameStyle(QFrame::Panel | QFrame::Sunken);
	_labelP2.setStyleSheet("QLabel {background-color : white; color: black}");
	_labelP2.setTextInteractionFlags(Qt::TextSelectableByMouse);
	_labelP2.setFixedWidth(FONT_WIDTH * 16);
	_pointerBar.addWidget(&_labelP2);
	_pointerBar.addSeparator();

	_labelI1.setStyleSheet("QLabel {color : blue;}");
	_labelI2.setStyleSheet("QLabel {color : green;}");
	_labelI3.setStyleSheet("QLabel {color : red;}");
	_labelI4.setStyleSheet("QLabel {color : blue;}");
	_labelI5.setStyleSheet("QLabel {color : green;}");

	_pointerBar.addWidget(&_labelI1);
	_pointerBar.addSeparator();
	_pointerBar.addWidget(&_labelI2);
	_pointerBar.addSeparator();
	_pointerBar.addWidget(&_labelI3);
	_pointerBar.addSeparator();
	_pointerBar.addWidget(&_labelI4);
	_pointerBar.addSeparator();
	_pointerBar.addWidget(&_labelI5);

	_glWindow.installEventFilter(this);

	connect(&_glWindow,	&KsGLWidget::select,
		this,		&KsTraceGraph::markEntry);

	connect(&_glWindow,	&KsGLWidget::found,
		this,		&KsTraceGraph::_setPointerInfo);

	connect(&_glWindow,	&KsGLWidget::notFound,
		this,		&KsTraceGraph::_resetPointer);

	connect(&_glWindow,	&KsGLWidget::zoomIn,
		this,		&KsTraceGraph::_zoomIn);

	connect(&_glWindow,	&KsGLWidget::zoomOut,
		this,		&KsTraceGraph::_zoomOut);

	connect(&_glWindow,	&KsGLWidget::scrollLeft,
		this,		&KsTraceGraph::_scrollLeft);

	connect(&_glWindow,	&KsGLWidget::scrollRight,
		this,		&KsTraceGraph::_scrollRight);

	connect(&_glWindow,	&KsGLWidget::stopUpdating,
		this,		&KsTraceGraph::_stopUpdating);

	_glWindow.setContextMenuPolicy(Qt::CustomContextMenu);
	connect(&_glWindow,	&QWidget::customContextMenuRequested,
		this,		&KsTraceGraph::_onCustomContextMenu);

	_scrollArea.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_scrollArea.setWidget(&_glWindow);

	lamMakeNavButton(&_scrollLeftButton);
	connect(&_scrollLeftButton,	&QPushButton::pressed,
		this,			&KsTraceGraph::_scrollLeft);

	lamMakeNavButton(&_zoomInButton);
	connect(&_zoomInButton,		&QPushButton::pressed,
		this,			&KsTraceGraph::_zoomIn);

	lamMakeNavButton(&_zoomOutButton);
	connect(&_zoomOutButton,	&QPushButton::pressed,
		this,			&KsTraceGraph::_zoomOut);

	lamMakeNavButton(&_scrollRightButton);
	connect(&_scrollRightButton,	&QPushButton::pressed,
		this,			&KsTraceGraph::_scrollRight);

	_navigationBar.addSeparator();

	lamMakeNavButton(&_quickZoomInButton);
	connect(&_quickZoomInButton,	&QPushButton::pressed,
		this,			&KsTraceGraph::_quickZoomIn);

	lamMakeNavButton(&_quickZoomOutButton);
	connect(&_quickZoomOutButton,	&QPushButton::pressed,
		this,			&KsTraceGraph::_quickZoomOut);

	_layout.addWidget(&_pointerBar);
	_layout.addWidget(&_navigationBar);
	_layout.addWidget(&_scrollArea);
	this->setLayout(&_layout);
	updateGeom();
}

/**
 * @brief Load and show trace data.
 *
 * @param data: Input location for the KsDataStore object.
 *	  	KsDataStore::loadDataFile() must be called first.
 * @param resetPlots: If true, all existing graphs are closed
 *		      and a default configuration of graphs is displayed
 *		      (all CPU plots). If false, the current set of graphs
 *		      is preserved.
 */
void KsTraceGraph::loadData(KsDataStore *data, bool resetPlots)
{
	_data = data;
	_glWindow.loadData(data, resetPlots);
	updateGeom();
}

/** Connect the KsGLWidget widget and the State machine of the Dual marker. */
void KsTraceGraph::setMarkerSM(KsDualMarkerSM *m)
{
	_mState = m;
	_navigationBar.addSeparator();
	_mState->placeInToolBar(&_navigationBar);
	_glWindow.setMarkerSM(m);
}

/** Reset (empty) the widget. */
void KsTraceGraph::reset()
{
	/* Reset (empty) the OpenGL widget. */
	_glWindow.reset();

	_labelP2.setText("");
	for (auto l1: {&_labelI1, &_labelI2, &_labelI3, &_labelI4, &_labelI5})
		l1->setText("");

	_selfUpdate();
}

void KsTraceGraph::_selfUpdate()
{
	_markerReDraw();
	_glWindow.model()->update();
	updateGeom();
}

void KsTraceGraph::_zoomIn()
{
	KsWidgetsLib::KsDataWork action = KsWidgetsLib::KsDataWork::ZoomIn;

	startOfWork(action);
	_updateGraphs(action);
	endOfWork(action);
}

void KsTraceGraph::_zoomOut()
{
	KsWidgetsLib::KsDataWork action = KsWidgetsLib::KsDataWork::ZoomOut;

	startOfWork(action);
	_updateGraphs(action);
	endOfWork(action);
}

void KsTraceGraph::_quickZoomIn()
{
	if (_glWindow.isEmpty())
		return;

	startOfWork(KsWidgetsLib::KsDataWork::QuickZoomIn);

	/* Bin size will be 100 ns. */
	_glWindow.model()->quickZoomIn(100);
	if (_mState->activeMarker()._isSet &&
	    _mState->activeMarker().isVisible()) {
		/*
		 * Use the position of the active marker as
		 * a focus point of the zoom.
		 */
		uint64_t ts = _mState->activeMarker()._ts;
		_glWindow.model()->jumpTo(ts);
		_glWindow.render();
	}

	endOfWork(KsWidgetsLib::KsDataWork::QuickZoomIn);
}

void KsTraceGraph::_quickZoomOut()
{
	if (_glWindow.isEmpty())
		return;

	startOfWork(KsWidgetsLib::KsDataWork::QuickZoomOut);
	_glWindow.model()->quickZoomOut();
	_glWindow.render();
	endOfWork(KsWidgetsLib::KsDataWork::QuickZoomOut);
}

void KsTraceGraph::_scrollLeft()
{
	KsWidgetsLib::KsDataWork action = KsWidgetsLib::KsDataWork::ScrollLeft;

	startOfWork(action);
	_updateGraphs(action);
	endOfWork(action);
}

void KsTraceGraph::_scrollRight()
{
	KsWidgetsLib::KsDataWork action = KsWidgetsLib::KsDataWork::ScrollRight;

	startOfWork(action);
	_updateGraphs(action);
	endOfWork(action);
}

void KsTraceGraph::_stopUpdating()
{
	/*
	 * The user is no longer pressing the action button. Reset the
	 * "Key Pressed" flag. This will stop the ongoing user action.
	 */
	_keyPressed = false;
}

QString KsTraceGraph::_t2str(uint64_t sec, uint64_t usec) {
	QString usecStr;
	QTextStream ts(&usecStr);

	ts.setFieldAlignment(QTextStream::AlignRight);
	ts.setFieldWidth(6);
	ts.setPadChar('0');

	ts << usec;

	return QString::number(sec) + "." + usecStr;
}

void KsTraceGraph::_resetPointer(int64_t ts, int sd, int cpu, int pid)
{
	kshark_entry entry;
	uint64_t sec, usec;

	entry.cpu = cpu;
	entry.pid = pid;
	entry.stream_id = sd;

	if (ts < 0)
		ts = 0;

	kshark_convert_nano(ts, &sec, &usec);
	_labelP2.setText(_t2str(sec, usec));

	if (pid > 0 && cpu >= 0) {
		struct kshark_context *kshark_ctx(NULL);

		if (!kshark_instance(&kshark_ctx))
			return;

		QString comm(kshark_get_task(&entry));
		comm.append("-");
		comm.append(QString("%1").arg(pid));
		_labelI1.setText(comm);
		_labelI2.setText(QString("CPU %1").arg(cpu));
	} else {
		_labelI1.setText("");
		_labelI2.setText("");
	}

	for (auto const &l: {&_labelI3, &_labelI4, &_labelI5}) {
		l->setText("");
	}
}

void KsTraceGraph::_setPointerInfo(size_t i)
{
	kshark_entry *e = _data->rows()[i];
	auto lanMakeString = [] (char *buffer) {
		QString str(buffer);
		free(buffer);
		return str;
	};

	QString event(lanMakeString(kshark_get_event_name(e)));
	QString aux(lanMakeString(kshark_get_aux_info(e)));
	QString info(lanMakeString(kshark_get_info(e)));
	QString comm(lanMakeString(kshark_get_task(e)));
	QString elidedText;
	int labelWidth;
	uint64_t sec, usec;
	char *pointer;

	kshark_convert_nano(e->ts, &sec, &usec);
	labelWidth = asprintf(&pointer, "%" PRIu64 ".%06" PRIu64 "", sec, usec);
	if (labelWidth <= 0)
		return;

	_labelP2.setText(pointer);
	free(pointer);

	comm.append("-");
	comm.append(QString("%1").arg(kshark_get_pid(e)));

	_labelI1.setText(comm);
	_labelI2.setText(QString("CPU %1").arg(e->cpu));
	_labelI3.setText(aux);
	_labelI4.setText(event);
	_labelI5.setText(info);
	QCoreApplication::processEvents();

	labelWidth =
		_pointerBar.geometry().right() - _labelI4.geometry().right();
	if (labelWidth > STRING_WIDTH(info) + FONT_WIDTH * 5)
		return;

	/*
	 * The Info string is too long and cannot be displayed on the toolbar.
	 * Try to fit the text in the available space.
	 */
	KsUtils::setElidedText(&_labelI5, info, Qt::ElideRight, labelWidth);
	_labelI5.setVisible(true);
	QCoreApplication::processEvents();
}

/**
 * @brief Use the active marker to select particular entry.
 *
 * @param row: The index of the entry to be selected by the marker.
 */
void KsTraceGraph::markEntry(size_t row)
{
	int yPosVis(-1);

	_glWindow.model()->jumpTo(_data->rows()[row]->ts);
	_mState->activeMarker().set(*_data, _glWindow.model()->histo(),
				    row, _data->rows()[row]->stream_id);

	_mState->updateMarkers(*_data, &_glWindow);

	/*
	 * If a Combo graph has been found, this Combo graph will be visible.
	 * Else the Task graph will be shown. If no Combo and no Task graph
	 * has been found, make visible the corresponding CPU graph.
	 */
	if (_mState->activeMarker()._mark.comboIsVisible())
		yPosVis = _mState->activeMarker()._mark.comboY();
	else if (_mState->activeMarker()._mark.taskIsVisible())
		yPosVis = _mState->activeMarker()._mark.taskY();
	else if (_mState->activeMarker()._mark.cpuIsVisible())
		yPosVis = _mState->activeMarker()._mark.cpuY();

	if (yPosVis > 0)
		_scrollArea.ensureVisible(0, yPosVis);
}

void KsTraceGraph::_markerReDraw()
{
	size_t row;

	if (_mState->markerA()._isSet) {
		row = _mState->markerA()._pos;
		_mState->markerA().set(*_data, _glWindow.model()->histo(),
				       row, _data->rows()[row]->stream_id);
	}

	if (_mState->markerB()._isSet) {
		row = _mState->markerB()._pos;
		_mState->markerB().set(*_data, _glWindow.model()->histo(),
				       row, _data->rows()[row]->stream_id);
	}
}

/**
 * @brief Redreaw all CPU graphs.
 *
 * @param sd: Data stream identifier.
 * @param v: CPU ids to be plotted.
 */
void KsTraceGraph::cpuReDraw(int sd, QVector<int> v)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	if (_glWindow._streamPlots.contains(sd))
		_glWindow._streamPlots[sd]._cpuList = v;

	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/**
 * @brief Redreaw all Task graphs.
 *
 * @param sd: Data stream identifier.
 * @param v: Process ids of the tasks to be plotted.
 */
void KsTraceGraph::taskReDraw(int sd, QVector<int> v)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	if (_glWindow._streamPlots.contains(sd))
		_glWindow._streamPlots[sd]._taskList = v;

	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/**
 * @brief Redreaw all virtCombo graphs.
 *
 * @param nCombos: Numver of Combo plots.
 * @param v: Descriptor of the Combo to be plotted.
 */
void KsTraceGraph::comboReDraw(int nCombos, QVector<int> v)
{
	KsComboPlot combo;

	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);

	_glWindow._comboPlots.clear();

	for (int i = 0; i < nCombos; ++i) {
		combo.resize(v.takeFirst());
		for (auto &p: combo)
			p << v;

		_glWindow._comboPlots.append(combo);
	}

	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/** Add (and plot) a CPU graph to the existing list of CPU graphs. */
void KsTraceGraph::addCPUPlot(int sd, int cpu)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	QVector<int> &list = _glWindow._streamPlots[sd]._cpuList;
	if (list.contains(cpu))
		return;

	list.append(cpu);
	std::sort(list.begin(), list.end());

	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/** Add (and plot) a Task graph to the existing list of Task graphs. */
void KsTraceGraph::addTaskPlot(int sd, int pid)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	QVector<int> &list = _glWindow._streamPlots[sd]._taskList;
	if (list.contains(pid))
		return;

	list.append(pid);
	std::sort(list.begin(), list.end());

	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/** Remove a CPU graph from the existing list of CPU graphs. */
void KsTraceGraph::removeCPUPlot(int sd, int cpu)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	if (!_glWindow._streamPlots[sd]._cpuList.contains(cpu))
		return;

	_glWindow._streamPlots[sd]._cpuList.removeAll(cpu);
	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/** Remove a Task graph from the existing list of Task graphs. */
void KsTraceGraph::removeTaskPlot(int sd, int pid)
{
	startOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
	if (!_glWindow._streamPlots[sd]._taskList.contains(pid))
		return;

	_glWindow._streamPlots[sd]._taskList.removeAll(pid);
	_selfUpdate();
	endOfWork(KsWidgetsLib::KsDataWork::EditPlotList);
}

/** Update the content of all graphs. */
void KsTraceGraph::update(KsDataStore *data)
{
	kshark_context *kshark_ctx(nullptr);
	QVector<int> streamIds;

	if (!kshark_instance(&kshark_ctx))
		return;

	streamIds = KsUtils::getStreamIdList(kshark_ctx);
	for (auto const &sd: streamIds)
		for (auto &pid: _glWindow._streamPlots[sd]._taskList) {
			kshark_unregister_data_collection(&kshark_ctx->collections,
							  kshark_match_pid,
							  sd, &pid, 1);
		}

	_selfUpdate();

	streamIds = KsUtils::getStreamIdList(kshark_ctx);
	for (auto const &sd: streamIds)
		for (auto &pid: _glWindow._streamPlots[sd]._taskList) {
			kshark_register_data_collection(kshark_ctx,
							data->rows(),
							data->size(),
							kshark_match_pid,
							sd, &pid, 1,
							25);
		}
}

/** Update the geometry of the widget. */
void KsTraceGraph::updateGeom()
{
	int saWidth, saHeight, dwWidth, hMin;

	/* Set the size of the Scroll Area. */
	saWidth = width() - _layout.contentsMargins().left() -
			    _layout.contentsMargins().right();

	saHeight = height() - _pointerBar.height() -
			      _navigationBar.height() -
			      _layout.spacing() * 2 -
			      _layout.contentsMargins().top() -
			      _layout.contentsMargins().bottom();

	_scrollArea.resize(saWidth, saHeight);

	/*
	 * Calculate the width of the Draw Window, taking into account the size
	 * of the scroll bar.
	 */
	dwWidth = _scrollArea.width();
	if (_glWindow.height() > _scrollArea.height())
		dwWidth -=
			qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);

	_glWindow.resize(dwWidth, _glWindow.height());

	/* Set the minimum height of the Graph widget. */
	hMin = _glWindow.height() +
	       _pointerBar.height() +
	       _navigationBar.height() +
	       _layout.contentsMargins().top() +
	       _layout.contentsMargins().bottom();

	if (hMin > KS_GRAPH_HEIGHT * 8)
		hMin = KS_GRAPH_HEIGHT * 8;

	setMinimumHeight(hMin);

	/*
	 * Now use the height of the Draw Window to fix the maximum height
	 * of the Graph widget.
	 */
	setMaximumHeight(_glWindow.height() +
			 _pointerBar.height() +
			 _navigationBar.height() +
			 _layout.spacing() * 2 +
			 _layout.contentsMargins().top() +
			 _layout.contentsMargins().bottom() +
			 2);  /* Just a little bit of extra space. This will
			       * allow the scroll bar to disappear when the
			       * widget is extended to maximum.
			       */

	_glWindow.updateGeom();
}

/**
 * Reimplemented event handler used to update the geometry of the widget on
 * resize events.
 */
void KsTraceGraph::resizeEvent(QResizeEvent* event)
{
	updateGeom();
}

/**
 * Reimplemented event handler (overriding a virtual function from QObject)
 * used to detect the position of the mouse with respect to the Draw window and
 * according to this position to grab / release the focus of the keyboard. The
 * function has nothing to do with the filtering of the trace events.
 */
bool KsTraceGraph::eventFilter(QObject* obj, QEvent* evt)
{
	/* Desable all mouse events for the OpenGL wiget when busy. */
	if (obj == &_glWindow && this->isBusy() &&
	    (evt->type() == QEvent::MouseButtonDblClick ||
	     evt->type() == QEvent::MouseButtonPress ||
	     evt->type() == QEvent::MouseButtonRelease ||
	     evt->type() == QEvent::MouseMove)
	)
		return true;

	if (obj == &_glWindow && evt->type() == QEvent::Enter)
		_glWindow.setFocus();

	if (obj == &_glWindow && evt->type() == QEvent::Leave)
		_glWindow.clearFocus();

	return QWidget::eventFilter(obj, evt);
}

void KsTraceGraph::_updateGraphs(KsWidgetsLib::KsDataWork action)
{
	double k;
	int bin;

	if (_glWindow.isEmpty())
		return;

	/*
	 * Set the "Key Pressed" flag. The flag will stay set as long as the user
	 * keeps the corresponding action button pressed.
	 */
	_keyPressed = true;

	/* Initialize the zooming factor with a small value. */
	k = .01;
	while (_keyPressed) {
		switch (action) {
		case KsWidgetsLib::KsDataWork::ZoomIn:
			if (_mState->activeMarker()._isSet &&
			    _mState->activeMarker().isVisible()) {
				/*
				 * Use the position of the active marker as
				 * a focus point of the zoom.
				 */
				bin = _mState->activeMarker()._bin;
				_glWindow.model()->zoomIn(k, bin);
			} else {
				/*
				 * The default focus point is the center of the
				 * range interval of the model.
				 */
				_glWindow.model()->zoomIn(k);
			}

			break;

		case KsWidgetsLib::KsDataWork::ZoomOut:
			if (_mState->activeMarker()._isSet &&
			    _mState->activeMarker().isVisible()) {
				/*
				 * Use the position of the active marker as
				 * a focus point of the zoom.
				 */
				bin = _mState->activeMarker()._bin;
				_glWindow.model()->zoomOut(k, bin);
			} else {
				/*
				 * The default focus point is the center of the
				 * range interval of the model.
				 */
				_glWindow.model()->zoomOut(k);
			}

			break;

		case KsWidgetsLib::KsDataWork::ScrollLeft:
			_glWindow.model()->shiftBackward(10);
			break;

		case KsWidgetsLib::KsDataWork::ScrollRight:
			_glWindow.model()->shiftForward(10);
			break;

		default:
			return;
		}

		/*
		 * As long as the action button is pressed, the zooming factor
		 * will grow smoothly until it reaches a maximum value. This
		 * will have a visible effect of an accelerating zoom.
		 */
		if (k < .25)
			k  *= 1.02;

		_mState->updateMarkers(*_data, &_glWindow);
		_glWindow.render();
		QCoreApplication::processEvents();
	}
}

void KsTraceGraph::_onCustomContextMenu(const QPoint &point)
{
	KsQuickMarkerMenu *menu(nullptr);
	int sd, cpu, pid;
	size_t row;
	bool found;

	found = _glWindow.find(point, 20, true, &row);
	if (found) {
		/* KernelShark entry has been found under the cursor. */
		KsQuickContextMenu *entryMenu;
		menu = entryMenu = new KsQuickContextMenu(_mState, _data, row,
							  this);

		connect(entryMenu,	&KsQuickContextMenu::addTaskPlot,
			this,		&KsTraceGraph::addTaskPlot);

		connect(entryMenu,	&KsQuickContextMenu::addCPUPlot,
			this,		&KsTraceGraph::addCPUPlot);

		connect(entryMenu,	&KsQuickContextMenu::removeTaskPlot,
			this,		&KsTraceGraph::removeTaskPlot);

		connect(entryMenu,	&KsQuickContextMenu::removeCPUPlot,
			this,		&KsTraceGraph::removeCPUPlot);
	} else {
		if (!_glWindow.getPlotInfo(point, &sd, &cpu, &pid))
			return;

		if (cpu >= 0) {
			/*
			 * This is a CPU plot, but we do not have an entry
			 * under the cursor.
			 */
			KsRmCPUPlotMenu *rmMenu;
			menu = rmMenu = new KsRmCPUPlotMenu(_mState, sd, cpu, this);

			auto lamRmPlot = [&sd, &cpu, this] () {
				removeCPUPlot(sd, cpu);
			};

			connect(rmMenu, &KsRmPlotContextMenu::removePlot,
				lamRmPlot);
		}

		if (pid >= 0) {
			/*
			 * This is a Task plot, but we do not have an entry
			 * under the cursor.
			 */
			KsRmTaskPlotMenu *rmMenu;
			menu = rmMenu = new KsRmTaskPlotMenu(_mState, sd, pid, this);

			auto lamRmPlot = [&sd, &pid, this] () {
				removeTaskPlot(sd, pid);
			};

			connect(rmMenu, &KsRmPlotContextMenu::removePlot,
				lamRmPlot);
		}
	}

	if (menu) {
		connect(menu,	&KsQuickMarkerMenu::deselect,
			this,	&KsTraceGraph::deselect);

		/*
		 * Note that this slot was connected to the
		 * customContextMenuRequested signal of the OpenGL widget.
		 * Because of this the coordinates of the point are given with
		 * respect to the frame of this widget.
		 */
		QPoint global = _glWindow.mapToGlobal(point);
		global.ry() -= menu->sizeHint().height() / 2;

		/*
		 * Shift the menu so that it is not positioned under the mouse.
		 * This will prevent from an accidental selection of the menu
		 * item under the mouse.
		 */
		global.rx() += FONT_WIDTH;

		menu->exec(global);
	}
}
