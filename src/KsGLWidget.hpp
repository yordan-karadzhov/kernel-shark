/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

 /**
 *  @file    KsGLWidget.hpp
 *  @brief   OpenGL widget for plotting trace graphs.
 */

#ifndef _KS_GLWIDGET_H
#define _KS_GLWIDGET_H

// Qt
#include <QRubberBand>
#include <QOpenGLWidget>

// KernelShark
#include "KsUtils.hpp"
#include "KsWidgetsLib.hpp"
#include "KsPlotTools.hpp"
#include "KsModels.hpp"
#include "KsDualMarker.hpp"

/** Structure describing all graphs to be plotted for a given Data stream. */
struct KsPerStreamPlots {
	/** CPUs to be plotted. */
	QVector<int>			_cpuList;

	/** "Y" coordinates of the bases of all CPU plots for this stream. */
	QVector<KsPlot::Graph *>	_cpuGraphs;

	/** Tasks to be plotted. */
	QVector<int>			_taskList;

	/** "Y" coordinates of the bases of all CPU plots for this stream. */
	QVector<KsPlot::Graph *>	_taskGraphs;
};

/** Structure describing a plot. */
struct KsPlotEntry {
	/** The Data stream identifier of the plot. */
	int	_streamId;

	/** Plotting action identifier (Task or CPU plot). */
	int	_type;

	/** Identifier of the plot (can be PID or CPU number). */
	int	_id;

	/** Graph pointer. */
	KsPlot::Graph	*_graph;

	/** "Y" coordinates of the bases of the plot. */
	int base() const {return _graph->base();}
};

KsPlotEntry &operator <<(KsPlotEntry &plot, QVector<int> &v);

void operator >>(const KsPlotEntry &plot, QVector<int> &v);

/** Vector of KsPlotEntry used to describe a Combo plot. */
typedef QVector<KsPlotEntry>	KsComboPlot;

/**
 * The KsGLWidget class provides a widget for rendering OpenGL graphics used
 * to plot trace graphs.
 */
class KsGLWidget : public QOpenGLWidget
{
	Q_OBJECT
public:
	explicit KsGLWidget(QWidget *parent = NULL);

	~KsGLWidget();

	void render();

	void reset();

	/** Reprocess all graphs. */
	void updateGeom() {resizeGL(width(), height());}

	void loadData(KsDataStore *data, bool resetPlots);

	void loadColors();

	/**
	 * Provide the widget with a pointer to the Dual Marker state machine
	 * object.
	 */
	void setMarkerSM(KsDualMarkerSM *m) {_mState = m;}

	/** Get the KsGraphModel object. */
	KsGraphModel *model() {return &_model;}

	/** Get the number of CPU graphs for a given Data stream. */
	int cpuGraphCount(int sd) const
	{
		auto it = _streamPlots.find(sd);
		if (it != _streamPlots.end())
			return it.value()._cpuList.count();
		return 0;
	}

	/** Get the number of Task graphs for a given Data stream. */
	int taskGraphCount(int sd) const
	{
		auto it = _streamPlots.find(sd);
		if (it != _streamPlots.end())
			return it.value()._taskList.count();
		return 0;
	}

	/** Get the total number of graphs for a given Data stream. */
	int graphCount(int sd) const
	{
		auto it = _streamPlots.find(sd);
		if (it != _streamPlots.end())
			return it.value()._taskList.count() +
			       it.value()._cpuList.count();
		return 0;
	}

	/**
	 * Get the total number of graphs for all Data stream. The Combo plots
	 * are not counted.
	 */
	int totGraphCount() const
	{
		int count(0);
		for (auto const &s: _streamPlots)
			count += s._taskList.count() +
				 s._cpuList.count();
		return count;
	}

	/** Get the number of plots in Combos. */
	int comboGraphCount() const {
		int count(0);
		for (auto const &c: _comboPlots)
			count += c.count();
		return count;
	}

	/** Check if the widget is empty (not showing anything). */
	bool isEmpty() const
	{
		return !_data || !_data->size() ||
		       (!totGraphCount() && !comboGraphCount());
	}

	/** Get the height of the widget. */
	int height() const
	{
		return totGraphCount() * (KS_GRAPH_HEIGHT + _vSpacing) +
		       comboGraphCount() * KS_GRAPH_HEIGHT +
		       _comboPlots.count() * _vSpacing +
		       _vMargin * 2;
	}

	/** Get the device pixel ratio. */
	int dpr() const {return _dpr;}

	/** Get the size of the horizontal margin space. */
	int hMargin()	const {return _hMargin;}

	/** Get the size of the vertical margin space. */
	int vMargin()	const {return _vMargin;}

	/** Get the size of the vertical spaceing between the graphs. */
	int vSpacing()	const {return _vSpacing;}

	void setMarkPoints(const KsDataStore &data, KsGraphMark *mark);

	bool find(const QPoint &point, int variance, bool joined,
		  size_t *index);

	bool getPlotInfo(const QPoint &point, int *sd, int *cpu, int *pid);

	/** CPUs and Tasks graphs (per data stream) to be plotted. */
	QMap<int, KsPerStreamPlots>	_streamPlots;

	/** Combo graphs to be plotted. */
	QVector<KsComboPlot>		_comboPlots;

	/** Set the pointer to the WorkInProgress widget. */
	void setWipPtr(KsWidgetsLib::KsWorkInProgress *wip)
	{
		_workInProgress = wip;
	}

	/** Free the list of plugin-defined shapes. */
	void freePluginShapes();

protected:
	void initializeGL() override;

	void resizeGL(int w, int h) override;

	void paintGL() override;

	void mousePressEvent(QMouseEvent *event) override;

	void mouseMoveEvent(QMouseEvent *event) override;

	void mouseReleaseEvent(QMouseEvent *event) override;

	void mouseDoubleClickEvent(QMouseEvent *event) override;

	void wheelEvent(QWheelEvent * event) override;

	void keyPressEvent(QKeyEvent *event) override;

	void keyReleaseEvent(QKeyEvent *event) override;

signals:
	/**
	 * This signal is emitted when the mouse moves over a visible
	 * KernelShark entry.
	 */
	void found(size_t pos);

	/**
	 * This signal is emitted when the mouse moves but there is no visible
	 * KernelShark entry under the cursor.
	 */
	void notFound(uint64_t ts, int sd, int cpu, int pid);

	/** This signal is emitted when the Plus key is pressed. */
	void zoomIn();

	/** This signal is emitted when the Minus key is pressed. */
	void zoomOut();

	/** This signal is emitted when the Left Arrow key is pressed. */
	void scrollLeft();

	/** This signal is emitted when the Right Arrow key is pressed. */
	void scrollRight();

	/**
	 * This signal is emitted when one of the 4 Action keys is release
	 * (after being pressed).
	 */
	void stopUpdating();

	/**
	 * This signal is emitted in the case of a double click over a visible
	 * KernelShark entry.
	 */
	void select(size_t pos);

	/**
	 * This signal is emitted when the KsTraceViewer widget needs to be
	 * updated.
	 */
	void updateView(size_t pos, bool mark);

private:
	QMap<int, QVector<KsPlot::Graph *>>	_graphs;

	KsPlot::PlotObjList	_shapes;

	KsPlot::ColorTable	_pidColors;

	KsPlot::ColorTable	_cpuColors;

	KsPlot::ColorTable	_streamColors;

	KsWidgetsLib::KsWorkInProgress	*_workInProgress;

	int	_labelSize, _hMargin, _vMargin;

	unsigned int	_vSpacing;

	KsGraphModel	 _model;

	KsDualMarkerSM	*_mState;

	KsDataStore	*_data;

	QRubberBand	_rubberBand;

	QPoint		_rubberBandOrigin;

	size_t		_posMousePress;

	bool		_keyPressed;

	int 		_dpr;

	ksplot_font	_font;

	void _freeGraphs();

	void _drawAxisX(float size);

	int _getMaxLabelSize();

	void _makeGraphs();

	KsPlot::Graph *_newCPUGraph(int sd, int cpu);

	KsPlot::Graph *_newTaskGraph(int sd, int pid);

	void _makePluginShapes();

	int _posInRange(int x);

	void _rangeBoundInit(int x);

	void _rangeBoundStretched(int x);

	void _rangeChanged(int binMin, int binMax);

	bool _findAndSelect(QMouseEvent *event);

	bool _find(int bin, int sd, int cpu, int pid,
		   int variance, bool joined, size_t *row);

	int _getNextCPU(int bin, int sd, int pid);

	int _getLastTask(struct kshark_trace_histo *histo,
			 int bin, int sd, int cpu);

	int _getLastCPU(struct kshark_trace_histo *histo,
			int bin, int sd, int pid);

	void _defaultPlots(kshark_context *kshark_ctx);

	void _deselect();

	int _bin0Offset() {return _labelSize + 2 * _hMargin;}
};

#endif
