// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

 /**
 *  @file    KsGLWidget.cpp
 *  @brief   OpenGL widget for plotting trace graphs.
 */

// OpenGL
#include <GL/glut.h>
#include <GL/gl.h>

// KernelShark
#include "libkshark-plugin.h"
#include "KsGLWidget.hpp"
#include "KsUtils.hpp"
#include "KsPlugins.hpp"

/** A stream operator for converting vector of integers into KsPlotEntry. */
KsPlotEntry &operator <<(KsPlotEntry &plot, QVector<int> &v)
{
	plot._streamId = v.takeFirst();
	plot._type = v.takeFirst();
	plot._id = v.takeFirst();

	return plot;
}

/** A stream operator for converting KsPlotEntry into vector of integers. */
void operator >>(const KsPlotEntry &plot, QVector<int> &v)
{
	v.append(plot._streamId);
	v.append(plot._type);
	v.append(plot._id);
}

/** Create a default (empty) OpenGL widget. */
KsGLWidget::KsGLWidget(QWidget *parent)
: QOpenGLWidget(parent),
  _labelSize(100),
  _hMargin(15),
  _vMargin(25),
  _vSpacing(20),
  _mState(nullptr),
  _data(nullptr),
  _rubberBand(QRubberBand::Rectangle, this),
  _rubberBandOrigin(0, 0),
  _dpr(1)
{
	setMouseTracking(true);

	connect(&_model,	&QAbstractTableModel::modelReset,
		this,		qOverload<>(&KsGLWidget::update));
}

void KsGLWidget::_freeGraphs()
{
	for (auto &stream: _graphs) {
		for (auto &g: stream)
			delete g;
		stream.resize(0);
	}
}

void KsGLWidget::freePluginShapes()
{
	while (!_shapes.empty()) {
		auto s = _shapes.front();
		_shapes.pop_front();
		delete s;
	}
}

KsGLWidget::~KsGLWidget()
{
	_freeGraphs();
	freePluginShapes();
}

/** Reimplemented function used to set up all required OpenGL resources. */
void KsGLWidget::initializeGL()
{
	_dpr = QApplication::primaryScreen()->devicePixelRatio();
	ksplot_init_opengl(_dpr);

	ksplot_init_font(&_font, 15, TT_FONT_FILE);

	_labelSize = _getMaxLabelSize() + FONT_WIDTH * 2;
	updateGeom();
}

/**
 * Reimplemented function used to reprocess all graphs whene the widget has
 * been resized.
 */
void KsGLWidget::resizeGL(int w, int h)
{
	ksplot_resize_opengl(w, h);
	if(!_data)
		return;

	/*
	 * From the size of the widget, calculate the number of bins.
	 * One bin will correspond to one pixel.
	 */
	int nBins = width() - _bin0Offset() - _hMargin;
	if (nBins <= 0)
		return;

	/*
	 * Reload the data. The range of the histogram is the same
	 * but the number of bins changes.
	 */
	ksmodel_set_bining(_model.histo(),
			   nBins,
			   _model.histo()->min,
			   _model.histo()->max);

	_model.fill(_data);
}

/** Reimplemented function used to plot trace graphs. */
void KsGLWidget::paintGL()
{
	float size = 1.5 * _dpr;

	glClear(GL_COLOR_BUFFER_BIT);

	if (isEmpty())
		return;

	render();

	/* Draw the time axis. */
	_drawAxisX(size);

	for (auto const &stream: _graphs)
		for (auto const &g: stream)
			g->draw(size);

	for (auto const &s: _shapes) {
		if (!s)
			continue;

		if (s->_size < 0)
			s->_size = size + abs(s->_size + 1);

		s->draw();
	}

	/*
	 * Update and draw the markers. Make sure that the active marker
	 * is drawn on top.
	 */
	_mState->updateMarkers(*_data, this);
	_mState->passiveMarker().draw();
	_mState->activeMarker().draw();
}

/** Process and draw all graphs. */
void KsGLWidget::render()
{
	/* Process and draw all graphs by using the built-in logic. */
	_makeGraphs();

	/* Process and draw all plugin-specific shapes. */
	_makePluginShapes();
};

/** Reset (empty) the widget. */
void KsGLWidget::reset()
{
	_streamPlots.clear();
	_comboPlots.clear();
	_data = nullptr;
	_model.reset();
}

/** Reimplemented event handler used to receive mouse press events. */
void KsGLWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		_posMousePress = _posInRange(event->pos().x());
		_rangeBoundInit(_posMousePress);
	}
}

int KsGLWidget::_getLastTask(struct kshark_trace_histo *histo,
			     int bin, int sd, int cpu)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_entry_collection *col;
	int pid;

	if (!kshark_instance(&kshark_ctx))
		return KS_EMPTY_BIN;

	col = kshark_find_data_collection(kshark_ctx->collections,
					  KsUtils::matchCPUVisible,
					  sd, &cpu, 1);

	for (int b = bin; b >= 0; --b) {
		pid = ksmodel_get_pid_back(histo, b, sd, cpu,
					   false, col, nullptr);
		if (pid >= 0)
			return pid;
	}

	return ksmodel_get_pid_back(histo, LOWER_OVERFLOW_BIN,
					   sd,
					   cpu,
					   false,
					   col,
					   nullptr);
}

int KsGLWidget::_getLastCPU(struct kshark_trace_histo *histo,
			    int bin, int sd, int pid)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_entry_collection *col;
	int cpu;

	if (!kshark_instance(&kshark_ctx))
		return KS_EMPTY_BIN;

	col = kshark_find_data_collection(kshark_ctx->collections,
					  kshark_match_pid,
					  sd, &pid, 1);

	for (int b = bin; b >= 0; --b) {
		cpu = ksmodel_get_cpu_back(histo, b, sd, pid,
					   false, col, nullptr);
		if (cpu >= 0)
			return cpu;
	}

	return ksmodel_get_cpu_back(histo, LOWER_OVERFLOW_BIN,
					   sd,
					   pid,
					   false,
					   col,
					   nullptr);

}

/** Reimplemented event handler used to receive mouse move events. */
void KsGLWidget::mouseMoveEvent(QMouseEvent *event)
{
	int bin, sd, cpu, pid;
	size_t row;
	bool ret;

	if (isEmpty())
		return;

	if (_rubberBand.isVisible())
		_rangeBoundStretched(_posInRange(event->pos().x()));

	bin = event->pos().x() - _bin0Offset();
	getPlotInfo(event->pos(), &sd, &cpu, &pid);

	ret = _find(bin, sd, cpu, pid, 5, false, &row);
	if (ret) {
		emit found(row);
	} else {
		if (cpu >= 0) {
			pid = _getLastTask(_model.histo(), bin, sd, cpu);
		}

		if (pid > 0) {
			cpu = _getLastCPU(_model.histo(), bin, sd, pid);
		}

		emit notFound(ksmodel_bin_ts(_model.histo(), bin), sd, cpu, pid);
	}
}

/** Reimplemented event handler used to receive mouse release events. */
void KsGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (isEmpty())
		return;

	if (event->button() == Qt::LeftButton) {
		size_t posMouseRel = _posInRange(event->pos().x());
		int min, max;
		if (_posMousePress < posMouseRel) {
			min = _posMousePress - _bin0Offset();
			max = posMouseRel - _bin0Offset();
		} else {
			max = _posMousePress - _bin0Offset();
			min = posMouseRel - _bin0Offset();
		}

		_rangeChanged(min, max);
	}
}

/** Reimplemented event handler used to receive mouse double click events. */
void KsGLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	KsPlot::PlotObject *pluginClicked(nullptr);
	double distance, distanceMin = FONT_HEIGHT;

	for (auto const &s: _shapes) {
		distance = s->distance(event->pos().x(), event->pos().y());
		if (distance < distanceMin) {
			distanceMin = distance;
			pluginClicked = s;
		}
	}

	if (pluginClicked)
		pluginClicked->doubleClick();

	else if (event->button() == Qt::LeftButton)
		_findAndSelect(event);
}

/** Reimplemented event handler used to receive mouse wheel events. */
void KsGLWidget::wheelEvent(QWheelEvent * event)
{
	int zoomFocus;

	if (QApplication::keyboardModifiers() != Qt::ControlModifier ||
	    isEmpty())
		return;

	if (_mState->activeMarker()._isSet &&
	    _mState->activeMarker().isVisible()) {
		/*
		 * Use the position of the marker as a focus point for the
		 * zoom.
		 */
		zoomFocus = _mState->activeMarker()._bin;
	} else {
		/*
		 * Use the position of the mouse as a focus point for the
		 * zoom.
		 */
		zoomFocus = event->position().x() - _bin0Offset();
	}

	if (event->angleDelta().y() > 0) {
		_model.zoomIn(.05, zoomFocus);
	} else {
		_model.zoomOut(.05, zoomFocus);
	}

	_mState->updateMarkers(*_data, this);
}

/** Reimplemented event handler used to receive key press events. */
void KsGLWidget::keyPressEvent(QKeyEvent *event)
{
	if (event->isAutoRepeat())
		return;

	switch (event->key()) {
	case Qt::Key_Plus:
		emit zoomIn();
		return;

	case Qt::Key_Minus:
		emit zoomOut();
		return;

	case Qt::Key_Left:
		emit scrollLeft();
		return;

	case Qt::Key_Right:
		emit scrollRight();
		return;

	default:
		QOpenGLWidget::keyPressEvent(event);
		return;
	}
}

/** Reimplemented event handler used to receive key release events. */
void KsGLWidget::keyReleaseEvent(QKeyEvent *event)
{
	if (event->isAutoRepeat())
		return;

	if(event->key() == Qt::Key_Plus ||
	   event->key() == Qt::Key_Minus ||
	   event->key() == Qt::Key_Left ||
	   event->key() == Qt::Key_Right) {
		emit stopUpdating();
		return;
	}

	QOpenGLWidget::keyPressEvent(event);
	return;
}

/**
 * The maximum number of CPU plots to be shown by default when the GUI starts.
 */
#define KS_MAX_START_PLOTS 16

void KsGLWidget::_defaultPlots(kshark_context *kshark_ctx)
{
	struct kshark_data_stream *stream;
	QVector<int> streamIds, plotVec;
	uint64_t tMin, tMax;
	int nCPUs, nBins;

	_model.reset();
	_streamPlots.clear();

	/*
	 * Make default CPU and Task lists. All CPUs from all Data streams will
	 * be plotted. No tasks will be plotted.
	 */
	streamIds = KsUtils::getStreamIdList(kshark_ctx);
	for (auto const &sd: streamIds) {
		stream = kshark_ctx->stream[sd];
		nCPUs = stream->n_cpus;
		plotVec.clear();

		/* If the number of CPUs is too big show only the first 16. */
		if (nCPUs > KS_MAX_START_PLOTS / kshark_ctx->n_streams)
			nCPUs = KS_MAX_START_PLOTS / kshark_ctx->n_streams;

		for (int cpu{0}; cpu < stream->n_cpus && plotVec.count() < nCPUs; ++cpu) {
			/* Do not add plots for idle CPUs. */
			if (!kshark_hash_id_find(stream->idle_cpus, cpu))
				plotVec.append(cpu);
		}

		_streamPlots[sd]._cpuList = plotVec;
		_streamPlots[sd]._taskList = {};
	}

	/*
	 * From the size of the widget, calculate the number of bins.
	 * One bin will correspond to one pixel.
	 */
	nBins = width() - _bin0Offset() - _hMargin;
	if (nBins < 0)
		nBins = 0;

	/* Now load the entire set of trace data. */
	tMin = _data->rows()[0]->ts;
	tMax = _data->rows()[_data->size() - 1]->ts;
	ksmodel_set_bining(_model.histo(), nBins, tMin, tMax);
}

/**
 * @brief Load and show trace data.
 *
 * @param data: Input location for the KsDataStore object.
 *		KsDataStore::loadDataFile() must be called first.
 * @param resetPlots: If true, all existing graphs are closed
 *		      and a default configuration of graphs is displayed
 *		      (all CPU plots). If false, the current set of graphs
 *		      is preserved.
 */
void KsGLWidget::loadData(KsDataStore *data, bool resetPlots)
{
	kshark_context *kshark_ctx(nullptr);

	if (!kshark_instance(&kshark_ctx) || !kshark_ctx->n_streams)
		return;

	loadColors();

	_data = data;
	if (resetPlots)
		_defaultPlots(kshark_ctx);

	_model.fill(_data);
}

/**
 * Create a Hash table of Rainbow colors. The sorted Pid values are mapped to
 * the palette of Rainbow colors.
 */
void KsGLWidget::loadColors()
{
	_pidColors.clear();
	_pidColors = KsPlot::taskColorTable();
	_cpuColors.clear();
	_cpuColors = KsPlot::CPUColorTable();
	_streamColors.clear();
	_streamColors = KsPlot::streamColorTable();
}

/**
 * Position the graphical elements of the marker according to the current
 * position of the graphs inside the GL widget.
 */
void KsGLWidget::setMarkPoints(const KsDataStore &data, KsGraphMark *mark)
{
	const kshark_entry *e = data.rows()[mark->_pos];
	int sd = e->stream_id;

	mark->_mark.setDPR(_dpr);
	mark->_mark.setX(mark->_bin + _bin0Offset());
	mark->_mark.setY(_vMargin * 3 / 2 + 2, height() - _vMargin / 4);

	mark->_mark.setCPUVisible(false);
	mark->_mark.setTaskVisible(false);
	mark->_mark.setComboVisible(false);

	for (int i = 0; i < _streamPlots[sd]._cpuList.count(); ++i) {
		if (_streamPlots[sd]._cpuList[i] == e->cpu) {
			mark->_mark.setCPUY(_streamPlots[sd]._cpuGraphs[i]->base());
			mark->_mark.setCPUVisible(true);
		}
	}

	for (int i = 0; i < _streamPlots[sd]._taskList.count(); ++i) {
		if (_streamPlots[sd]._taskList[i] == e->pid) {
			mark->_mark.setTaskY(_streamPlots[sd]._taskGraphs[i]->base());
			mark->_mark.setTaskVisible(true);
		}
	}

	for (auto const &c: _comboPlots)
		for (auto const &p: c) {
			if (p._streamId != e->stream_id)
				continue;

			if (p._type & KSHARK_CPU_DRAW &&
			    p._id == e->cpu) {
				mark->_mark.setComboY(p.base());
				mark->_mark.setComboVisible(true);
			} else if (p._type & KSHARK_TASK_DRAW &&
				   p._id == e->pid) {
				mark->_mark.setComboY(p.base());
				mark->_mark.setComboVisible(true);
			}
		}
}

void KsGLWidget::_drawAxisX(float size)
{
	int64_t model_min = model()->histo()->min;
	int64_t model_max = model()->histo()->max;
	uint64_t sec, usec, tsMid;
	char *tMin, *tMid, *tMax;
	int mid = (width() - _bin0Offset() - _hMargin) / 2;
	int y_1 = _vMargin * 5 / 4;
	int y_2 = _vMargin * 6 / 4;
	int count;

	KsPlot::Point a0(_bin0Offset(), y_1);
	KsPlot::Point a1(_bin0Offset(), y_2);
	KsPlot::Point b0(_bin0Offset() + mid, y_1);
	KsPlot::Point b1(_bin0Offset() + mid, y_2);
	KsPlot::Point c0(width() - _hMargin, y_1);
	KsPlot::Point c1(width() - _hMargin, y_2);

	a0._size = c0._size = _dpr;

	a0.draw();
	c0.draw();
	KsPlot::drawLine(a0, a1, {}, size);
	KsPlot::drawLine(b0, b1, {}, size);
	KsPlot::drawLine(c0, c1, {}, size);
	KsPlot::drawLine(a0, c0, {}, size);

	if (model_min < 0)
		model_min = 0;

	kshark_convert_nano(model_min, &sec, &usec);
	count = asprintf(&tMin,"%" PRIu64 ".%06" PRIu64 "", sec, usec);
	if (count <= 0)
		return;

	tsMid = (model_min + model_max) / 2;
	kshark_convert_nano(tsMid, &sec, &usec);
	count = asprintf(&tMid, "%" PRIu64 ".%06" PRIu64 "", sec, usec);
	if (count <= 0)
		return;

	kshark_convert_nano(model_max, &sec, &usec);
	count = asprintf(&tMax, "%" PRIu64 ".%06" PRIu64 "", sec, usec);
	if (count <= 0)
		return;

	ksplot_print_text(&_font, nullptr,
			  a0.x(),
			  a0.y() - _hMargin / 2,
			  tMin);

	ksplot_print_text(&_font, nullptr,
			  b0.x() - _font.char_width * count / 2,
			  b0.y() - _hMargin / 2,
			  tMid);

	ksplot_print_text(&_font, nullptr,
			  c0.x() - _font.char_width * count,
			  c0.y() - _hMargin / 2,
			  tMax);

	free(tMin);
	free(tMid);
	free(tMax);
}

int KsGLWidget::_getMaxLabelSize()
{
	int size, max(0);

	for (auto it = _streamPlots.begin(); it != _streamPlots.end(); ++it) {
		int sd = it.key();
		for (auto const &pid: it.value()._taskList) {
			size = _font.char_width *
			       KsUtils::taskPlotName(sd, pid).size();
			max = (size > max) ? size : max;
		}

		for (auto const &cpu: it.value()._cpuList) {
			size = _font.char_width * KsUtils::cpuPlotName(cpu).size();
			max = (size > max) ? size : max;
		}
	}

	for (auto &c: _comboPlots)
		for (auto const &p: c) {
			if (p._type & KSHARK_TASK_DRAW) {
				size = _font.char_width *
					KsUtils::taskPlotName(p._streamId, p._id).size();

				max = (size > max) ? size : max;
			} else if (p._type & KSHARK_CPU_DRAW) {
				size = _font.char_width *
				       KsUtils::cpuPlotName(p._id).size();

				max = (size > max) ? size : max;
			}
		}

	return max;
}

void KsGLWidget::_makeGraphs()
{
	int base(_vMargin * 2 + KS_GRAPH_HEIGHT), sd;
	KsPlot::Graph *g;

	/* The very first thing to do is to clean up. */
	_freeGraphs();

	if (!_data || !_data->size())
		return;

	_labelSize = _getMaxLabelSize() + FONT_WIDTH * 2;

	auto lamAddGraph = [&](int sd, KsPlot::Graph *graph, int vSpace=0) {
		if (!graph)
			return graph;

		KsPlot::Color color = {255, 255, 255}; // White
		/*
		 * Calculate the base level of the CPU graph inside the widget.
		 * Remember that the "Y" coordinate is inverted.
		 */

		graph->setBase(base);

		/*
		 * If we have multiple Data streams use the color of the stream
		 * for the label of the graph.
		 */
		if (KsUtils::getNStreams() > 1)
			color = KsPlot::getColor(&_streamColors, sd);

		graph->setLabelAppearance(&_font,
					  color,
					  _labelSize,
					  _hMargin);

		_graphs[sd].append(graph);
		base += graph->height() + vSpace;

		return graph;
	};

	for (auto it = _streamPlots.begin(); it != _streamPlots.end(); ++it) {
		sd = it.key();
		/* Create CPU graphs according to the cpuList. */
		it.value()._cpuGraphs = {};
		for (auto const &cpu: it.value()._cpuList) {
			g = lamAddGraph(sd, _newCPUGraph(sd, cpu), _vSpacing);
			it.value()._cpuGraphs.append(g);
		}

		/* Create Task graphs according to the taskList. */
		it.value()._taskGraphs = {};
		for (auto const &pid: it.value()._taskList) {
			g = lamAddGraph(sd, _newTaskGraph(sd, pid), _vSpacing);
			it.value()._taskGraphs.append(g);
		}
	}

	for (auto &c: _comboPlots) {
		int n = c.count();
		for (int i = 0; i < n; ++i) {
			sd = c[i]._streamId;
			if (c[i]._type & KSHARK_TASK_DRAW) {
				c[i]._graph = lamAddGraph(sd, _newTaskGraph(sd, c[i]._id));
			} else if (c[i]._type & KSHARK_CPU_DRAW) {
				c[i]._graph = lamAddGraph(sd, _newCPUGraph(sd, c[i]._id));
			} else {
				c[i]._graph = nullptr;
			}

			if (c[i]._graph && i < n - 1)
				c[i]._graph->setDrawBase(false);
		}

		base += _vSpacing;
	}
}

void KsGLWidget::_makePluginShapes()
{
	kshark_context *kshark_ctx(nullptr);
	kshark_draw_handler *draw_handlers;
	struct kshark_data_stream *stream;
	KsCppArgV cppArgv;
	int sd;

	if (!kshark_instance(&kshark_ctx))
		return;

	/* The very first thing to do is to clean up. */
	freePluginShapes();

	cppArgv._histo = _model.histo();
	cppArgv._shapes = &_shapes;

	for (auto it = _streamPlots.constBegin(); it != _streamPlots.constEnd(); ++it) {
		sd = it.key();
		stream = kshark_get_data_stream(kshark_ctx, sd);
		if (!stream)
			continue;

		for (int g = 0; g < it.value()._cpuList.count(); ++g) {
			cppArgv._graph = it.value()._cpuGraphs[g];
			draw_handlers = stream->draw_handlers;
			while (draw_handlers) {
				draw_handlers->draw_func(cppArgv.toC(),
							sd,
							it.value()._cpuList[g],
							KSHARK_CPU_DRAW);

				draw_handlers = draw_handlers->next;
			}
		}

		for (int g = 0; g < it.value()._taskList.count(); ++g) {
			cppArgv._graph = it.value()._taskGraphs[g];
			draw_handlers = stream->draw_handlers;
			while (draw_handlers) {
				draw_handlers->draw_func(cppArgv.toC(),
							sd,
							it.value()._taskList[g],
							KSHARK_TASK_DRAW);

				draw_handlers = draw_handlers->next;
			}
		}
	}

	for (auto const &c: _comboPlots) {
		for (auto const &p: c) {
			stream = kshark_get_data_stream(kshark_ctx, p._streamId);
			draw_handlers = stream->draw_handlers;
			cppArgv._graph = p._graph;
			while (draw_handlers) {
				draw_handlers->draw_func(cppArgv.toC(),
							 p._streamId,
							 p._id,
							 p._type);

				draw_handlers = draw_handlers->next;
			}
		}
	}
}

KsPlot::Graph *KsGLWidget::_newCPUGraph(int sd, int cpu)
{
	QString name;
	/* The CPU graph needs to know only the colors of the tasks. */
	KsPlot::Graph *graph = new KsPlot::Graph(_model.histo(),
						 &_pidColors,
						 &_pidColors);

	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	kshark_entry_collection *col;

	if (!kshark_instance(&kshark_ctx))
		return nullptr;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return nullptr;

	graph->setIdleSuppressed(true, stream->idle_pid);
	graph->setHeight(KS_GRAPH_HEIGHT);
	graph->setLabelText(KsUtils::cpuPlotName(cpu).toStdString());

	col = kshark_find_data_collection(kshark_ctx->collections,
					  KsUtils::matchCPUVisible,
					  sd, &cpu, 1);

	graph->setDataCollectionPtr(col);
	graph->fillCPUGraph(sd, cpu);

	return graph;
}

KsPlot::Graph *KsGLWidget::_newTaskGraph(int sd, int pid)
{
	QString name;
	/*
	 * The Task graph needs to know the colors of the tasks and the colors
	 * of the CPUs.
	 */
	KsPlot::Graph *graph = new KsPlot::Graph(_model.histo(),
						 &_pidColors,
						 &_cpuColors);
	kshark_context *kshark_ctx(nullptr);
	kshark_entry_collection *col;
	kshark_data_stream *stream;

	if (!kshark_instance(&kshark_ctx))
		return nullptr;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return nullptr;

	graph->setHeight(KS_GRAPH_HEIGHT);
	graph->setLabelText(KsUtils::taskPlotName(sd, pid).toStdString());

	col = kshark_find_data_collection(kshark_ctx->collections,
					  kshark_match_pid, sd, &pid, 1);

	if (!col) {
		/*
		 * If a data collection for this task does not exist,
		 * register a new one.
		 */
		col = kshark_register_data_collection(kshark_ctx,
						      _data->rows(),
						      _data->size(),
						      kshark_match_pid,
						      sd, &pid, 1,
						      25);
	}

	/*
	 * Data collections are efficient only when used on graphs, having a
	 * lot of empty bins.
	 * TODO: Determine the optimal criteria to decide whether to use or
	 * not use data collection for this graph.
	 */
	if (_data->size() < 1e6 &&
	    col && col->size &&
	    _data->size() / col->size < 100) {
		/*
		 * No need to use collection in this case. Free the collection
		 * data, but keep the collection registered. This will prevent
		 * from recalculating the same collection next time when this
		 * task is ploted.
		 */
		kshark_reset_data_collection(col);
	}

	graph->setDataCollectionPtr(col);
	graph->fillTaskGraph(sd, pid);

	return graph;
}

/**
 * @brief Find the KernelShark entry under the the cursor.
 *
 * @param point: The position of the cursor.
 * @param variance: The variance of the position (range) in which an entry will
 *		    be searched.
 * @param joined: It True, search also in the associated CPU/Task graph.
 * @param index: Output location for the index of the entry under the cursor.
 * 		 If no entry has been found, the outputted value is zero.
 *
 * @returns True, if an entry has been found, otherwise False.
 */
bool KsGLWidget::find(const QPoint &point, int variance, bool joined,
		      size_t *index)
{
	int bin, sd, cpu, pid;

	/*
	 * Get the bin, pid and cpu numbers.
	 * Remember that one bin corresponds to one pixel.
	 */
	bin = point.x() - _bin0Offset();
	getPlotInfo(point, &sd, &cpu, &pid);

	return _find(bin, sd, cpu, pid, variance, joined, index);
}

int KsGLWidget::_getNextCPU(int bin, int sd, int pid)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_entry_collection *col;
	int cpu;

	if (!kshark_instance(&kshark_ctx))
		return KS_EMPTY_BIN;

	col = kshark_find_data_collection(kshark_ctx->collections,
					  kshark_match_pid,
					  sd, &pid, 1);
	if (!col)
		return KS_EMPTY_BIN;

	for (int i = bin; i < _model.histo()->n_bins; ++i) {
		cpu = ksmodel_get_cpu_front(_model.histo(), i, sd, pid,
					    false, col, nullptr);
		if (cpu >= 0)
			return cpu;
	}

	return KS_EMPTY_BIN;
}

bool KsGLWidget::_find(int bin, int sd, int cpu, int pid,
		       int variance, bool joined, size_t *row)
{
	int hSize = _model.histo()->n_bins;
	ssize_t found;

	if (bin < 0 || bin > hSize || (cpu < 0 && pid < 0)) {
		/*
		 * The click is outside of the range of the histogram.
		 * Do nothing.
		 */
		*row = 0;
		return false;
	}

	auto lamGetEntryByCPU = [&](int b) {
		/* Get the first data entry in this bin. */
		found = ksmodel_first_index_at_cpu(_model.histo(),
						   b, sd, cpu);
		if (found < 0) {
			/*
			 * The bin is empty or the entire connect of the bin
			 * has been filtered.
			 */
			return false;
		}

		*row = found;
		return true;
	};

	auto lamGetEntryByPid = [&](int b) {
		/* Get the first data entry in this bin. */
		found = ksmodel_first_index_at_pid(_model.histo(),
						   b, sd, pid);
		if (found < 0) {
			/*
			 * The bin is empty or the entire connect of the bin
			 * has been filtered.
			 */
			return false;
		}

		*row = found;
		return true;
	};

	auto lamFindEntryByCPU = [&](int b) {
		/*
		 * The click is over the CPU graphs. First try the exact
		 * match.
		 */
		if (lamGetEntryByCPU(bin))
			return true;

		/* Now look for a match, nearby the position of the click. */
		for (int i = 1; i < variance; ++i) {
			if (bin + i <= hSize && lamGetEntryByCPU(bin + i))
				return true;

			if (bin - i >= 0 && lamGetEntryByCPU(bin - i))
				return true;
		}

		*row = 0;
		return false;
	};

	auto lamFindEntryByPid = [&](int b) {
		/*
		 * The click is over the Task graphs. First try the exact
		 * match.
		 */
		if (lamGetEntryByPid(bin))
			return true;

		/* Now look for a match, nearby the position of the click. */
		for (int i = 1; i < variance; ++i) {
			if ((bin + i <= hSize) && lamGetEntryByPid(bin + i))
				return true;

			if ((bin - i >= 0) && lamGetEntryByPid(bin - i))
				return true;
		}

		*row = 0;
		return false;
	};

	if (cpu >= 0)
		return lamFindEntryByCPU(bin);

	if (pid >= 0) {
		bool ret = lamFindEntryByPid(bin);

		/*
		 * If no entry has been found and we have a joined search, look
		 * for an entry on the next CPU used by this task.
		 */
		if (!ret && joined) {
			cpu = _getNextCPU(sd, bin, pid);
			ret = lamFindEntryByCPU(bin);
		}

		return ret;
	}

	*row = 0;
	return false;
}

bool KsGLWidget::_findAndSelect(QMouseEvent *event)
{
	size_t row;
	bool found = find(event->pos(), 10, true, &row);

	if (found) {
		emit select(row);
		emit updateView(row, true);
	}

	return found;
}

void KsGLWidget::_rangeBoundInit(int x)
{
	/*
	 * Set the origin of the rubber band that shows the new range. Only
	 * the X coordinate of the origin matters. The Y coordinate will be
	 * set to zero.
	 */
	_rubberBandOrigin.rx() = x;
	_rubberBandOrigin.ry() = 0;

	_rubberBand.setGeometry(_rubberBandOrigin.x(),
				_rubberBandOrigin.y(),
				0, 0);

	/* Make the rubber band visible, although its size is zero. */
	_rubberBand.show();
}

void KsGLWidget::_rangeBoundStretched(int x)
{
	QPoint pos;

	pos.rx() = x;
	pos.ry() = this->height();

	/*
	 * Stretch the rubber band between the origin position and the current
	 * position of the mouse. Only the X coordinate matters. The Y
	 * coordinate will be the height of the widget.
	 */
	if (_rubberBandOrigin.x() < pos.x()) {
		_rubberBand.setGeometry(QRect(_rubberBandOrigin.x(),
					      _rubberBandOrigin.y(),
					      pos.x() - _rubberBandOrigin.x(),
					      pos.y() - _rubberBandOrigin.y()));
	} else {
		_rubberBand.setGeometry(QRect(pos.x(),
					      _rubberBandOrigin.y(),
					      _rubberBandOrigin.x() - pos.x(),
					      pos.y() - _rubberBandOrigin.y()));
	}
}

void KsGLWidget::_rangeChanged(int binMin, int binMax)
{
	size_t nBins = _model.histo()->n_bins;
	int binMark = _mState->activeMarker()._bin;
	uint64_t min, max;

	/* The rubber band is no longer needed. Make it invisible. */
	_rubberBand.hide();

	if ( (binMax - binMin) < 4) {
		/* Most likely this is an accidental click. Do nothing. */
		return;
	}

	/*
	 * Calculate the new range of the histogram. The number of bins will
	 * stay the same.
	 */

	min = ksmodel_bin_ts(_model.histo(), binMin);
	max = ksmodel_bin_ts(_model.histo(), binMax);
	if (max - min < nBins) {
		/*
		 * The range cannot be smaller than the number of bins.
		 * Do nothing.
		 */
		return;
	}

	/* Recalculate the model and update the markers. */
	ksmodel_set_bining(_model.histo(), nBins, min, max);
	_model.fill(_data);
	_mState->updateMarkers(*_data, this);

	/*
	 * If the Marker is inside the new range, make sure that it will
	 * be visible in the table. Note that for this check we use the
	 * bin number of the marker, retrieved before its update.
	 */
	if (_mState->activeMarker()._isSet &&
	    binMark < binMax && binMark > binMin) {
		emit updateView(_mState->activeMarker()._pos, true);
		return;
	}

	/*
	 * Find the first bin which contains unfiltered data and send a signal
	 * to the View widget to make this data visible.
	 */
	for (int bin = 0; bin < _model.histo()->n_bins; ++bin) {
		int64_t row = ksmodel_first_index_at_bin(_model.histo(), bin);
		if (row != KS_EMPTY_BIN &&
		    (_data->rows()[row]->visible & KS_TEXT_VIEW_FILTER_MASK)) {
			emit updateView(row, false);
			return;
		}
	}
}

int KsGLWidget::_posInRange(int x)
{
	int posX;
	if (x < _bin0Offset())
		posX = _bin0Offset();
	else if (x > (width() - _hMargin))
		posX = width() - _hMargin;
	else
		posX = x;

	return posX;
}

/**
 * @brief Get information about the graph plotted at given position (under the mouse).
 *
 * @param point: The position to be inspected.
 * @param sd: Output location for the Data stream Identifier of the graph.
 * @param cpu: Output location for the CPU Id of the graph, or -1 if this is
 *	       a Task graph.
 * @param pid: Output location for the Process Id of the graph, or -1 if this is
 *	       a CPU graph.
 */
bool KsGLWidget::getPlotInfo(const QPoint &point, int *sd, int *cpu, int *pid)
{
	int base, n;

	*sd = *cpu = *pid = -1;

	for (auto it = _streamPlots.constBegin(); it != _streamPlots.constEnd(); ++it) {
		n = it.value()._cpuList.count();
		for (int i = 0; i < n; ++i) {
			base = it.value()._cpuGraphs[i]->base();
			if (base - KS_GRAPH_HEIGHT < point.y() &&
			    point.y() < base) {
				*sd = it.key();
				*cpu = it.value()._cpuList[i];

				return true;
			}
		}

		n = it.value()._taskList.count();
		for (int i = 0; i < n; ++i) {
			base = it.value()._taskGraphs[i]->base();
			if (base - KS_GRAPH_HEIGHT < point.y() &&
			    point.y() < base) {
				*sd = it.key();
				*pid = it.value()._taskList[i];

				return true;
			}
		}
	}

	for (auto const &c: _comboPlots) {
		for (auto const &p: c) {
			base = p.base();
			if (base - KS_GRAPH_HEIGHT < point.y() && point.y() < base) {
				*sd = p._streamId;
				if (p._type & KSHARK_CPU_DRAW)
					*cpu = p._id;
				else if (p._type & KSHARK_TASK_DRAW)
					*pid = p._id;

				return true;
			}
		}
	}

	return false;
}
