/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsSession.hpp
 *  @brief   KernelShark Session.
 */

#ifndef _KS_SESSION_H
#define _KS_SESSION_H

// Qt
#include <QtWidgets>

// KernelShark
#include "KsDualMarker.hpp"
#include "KsTraceGraph.hpp"
#include "KsTraceViewer.hpp"

class KsMainWindow;

/**
 * The KsSession class provides instruments for importing/exporting the state
 * of the different components of the GUI from/to Json documents. These
 * instruments are used to save/load user session in the GUI.
 */
class KsSession
{
public:
	KsSession();

	virtual ~KsSession();

	/** Get the configuration document object. */
	kshark_config_doc *getConfDocPtr() const {return _config;}

	bool importFromFile(QString jfileName);

	void exportToFile(QString jfileName);

	void saveDataFile(QString fileName, QString dataSetName);

	QString getDataFile(kshark_context *kshark_ctx);

	void saveVisModel(kshark_trace_histo *histo);

	void loadVisModel(KsGraphModel *model);

	void saveGraphs(kshark_context *kshark_ctx,
			KsTraceGraph &graphs);

	void loadGraphs(kshark_context *kshark_ctx,
			KsTraceGraph &graphs);

	void saveDataStreams(kshark_context *kshark_ctx);

	void loadDataStreams(kshark_context *kshark_ctx,
			     KsDataStore *data);

	void saveMainWindowSize(const QMainWindow &window);

	void loadMainWindowSize(KsMainWindow *window);

	void saveSplitterSize(const QSplitter &splitter);

	void loadSplitterSize(QSplitter *splitter);

	void saveDualMarker(KsDualMarkerSM *dm);

	void loadDualMarker(KsDualMarkerSM *dmm, KsTraceGraph *graphs);

	void saveUserPlugins(const KsPluginManager &pm);

	void loadUserPlugins(kshark_context *kshark_ctx, KsPluginManager *pm);

	void saveTable(const KsTraceViewer &view);

	void loadTable(KsTraceViewer *view);

	void saveColorScheme();

	float getColorScheme();

private:
	kshark_config_doc *_config;

	json_object *_getMarkerJson();

	void _savePlots(int sd, KsGLWidget *glw, bool cpu);

	QVector<int> _getPlots(int sd, bool cpu);

	void _saveCPUPlots(int sd, KsGLWidget *glw);

	QVector<int> _getCPUPlots(int sd);

	void _saveTaskPlots(int sd, KsGLWidget *glw);

	QVector<int> _getTaskPlots(int sd);

	void _saveComboPlots(KsGLWidget *glw);

	QVector<int> _getComboPlots(int *nCombos);

	bool _getMarker(const char* name, size_t *pos);

	DualMarkerState _getMarkerState();
};

#endif
