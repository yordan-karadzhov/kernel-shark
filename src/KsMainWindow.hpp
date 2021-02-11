/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsMainWindow.hpp
 *  @brief   KernelShark GUI main window.
 */

#ifndef _KS_MAINWINDOW_H
#define _KS_MAINWINDOW_H

// Qt
#include <QMainWindow>
#include <QLocalServer>

// KernelShark
#include "KsTraceViewer.hpp"
#include "KsTraceGraph.hpp"
#include "KsWidgetsLib.hpp"
#include "KsPlugins.hpp"
#include "KsSession.hpp"
#include "KsUtils.hpp"

/**
 * The KsMainWindow class provides Main window for the KernelShark GUI.
 */
class KsMainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit KsMainWindow(QWidget *parent = nullptr);

	~KsMainWindow();

	void loadDataFile(const QString &fileName);

	void appendDataFile(const QString &fileName);

	void loadSession(const QString &fileName);

	QString lastSessionFile();

	/**
	 * @brief Register a list of plugins.
	 *
	 * @param plugins: Provide here the names of the plugin (as in the
 *			   CMake-generated header file) or the names of the
 *			   plugin's library files (.so including path).
 * 			   The names must be comma separated.
	 */
	void registerPlugins(const QString &plugins)
	{
		_plugins.registerPlugins(plugins);
	}

	/**
	 * @brief Unregister a list pf plugins.
	 *
	 * @param pluginNames: Provide here a comma separated list of plugin
	 *		       names (as in the CMake-generated header file).
	 */
	void unregisterPlugins(const QString &pluginNames)
	{
		_plugins.unregisterPlugins(pluginNames);
	}

	/**
	 * @brief Register a given plugin to given Data streams.
	 *
	 * @param pluginName: The name of the plugin to register.
	 * @param streamIds: Vector of Data stream identifiers.
	 */
	void registerPluginToStream(const QString &pluginName, QVector<int> streamIds)
	{
		_plugins.registerPluginToStream(pluginName, streamIds);
	}

	/**
	 * @brief Unregister a given plugin from given Data streams.
	 *
	 * @param pluginName: The name of the plugin to unregister.
	 * @param streamIds: Vector of Data stream identifiers.
	 */
	void unregisterPluginFromStream(const QString &pluginName, QVector<int> streamIds)
	{
		_plugins.unregisterPluginFromStream(pluginName, streamIds);
	}

	void setCPUPlots(int sd, QVector<int> cpus);

	void setTaskPlots(int sd, QVector<int> pids);

	void resizeEvent(QResizeEvent* event);

	/** Set the Full Screen mode. */
	void setFullScreenMode(bool f) {
		if ((!isFullScreen() && f) || (isFullScreen() && !f) )
			_changeScreenMode();
	}

	void addPluginMenu(QString place, pluginActionFunc action);

	/** Get the KsTraceGraph object. */
	KsTraceGraph *graphPtr() {return &_graph;}

	/** Get the KsTraceViewer object. */
	KsTraceViewer *viewPtr() {return &_view;}

	/** Get the KsWorkInProgress object. */
	KsWidgetsLib::KsWorkInProgress *wipPtr() {return &_workInProgress;}

	void markEntry(ssize_t row, DualMarkerState st);

	void markEntry(const kshark_entry *e, DualMarkerState st);

private:
	QSplitter	_splitter;

	/** GUI session. */
	KsSession	_session;

	/** Data Manager. */
	KsDataStore	_data;

	/** Widget for reading and searching in the trace data. */
	KsTraceViewer	_view;

	/** Widget for graphical visualization of the trace data. */
	KsTraceGraph	_graph;

	/** Dual Marker State Machine. */
	KsDualMarkerSM	_mState;

	/** Plugin manager. */
	KsPluginManager	_plugins;

	/** The process used to record trace data. */
	QProcess	_capture;

	/** Local Server used for comunucation with the Capture process. */
	QLocalServer	_captureLocalServer;

	// File menu.
	QAction		_openAction;

	QAction		_appendAction;

	QAction		_restoreSessionAction;

	QAction		_importSessionAction;

	QAction		_exportSessionAction;

	QAction		_quitAction;

	// Filter menu.
	QCheckBox	*_graphFilterSyncCBox;

	QCheckBox	*_listFilterSyncCBox;

	QAction		_showEventsAction;

	QAction		_showTasksAction;

	QAction		_showCPUsAction;

	QAction		_advanceFilterAction;

	QAction		_clearAllFilters;

	// Plots menu.
	QAction		_cpuSelectAction;

	QAction		_taskSelectAction;

	// Tools menu.
	QAction		_managePluginsAction;

	QAction		_addPluginsAction;

	QAction		_captureAction;

	QAction		_addOffcetAction;

	QWidgetAction	_colorAction;

	QWidget		_colSlider;

	QSlider		_colorPhaseSlider;

	QAction		_fullScreenModeAction;

	// Help menu.
	QAction		_aboutAction;

	QAction		_contentsAction;

	QAction		_bugReportAction;

	QShortcut	_deselectShortcut;

	QString		_lastDataFilePath, _lastConfFilePath, _lastPluginFilePath;

	QSettings	_settings;

	QMetaObject::Connection		_captureErrorConnection;

	// Status bar.
	KsWidgetsLib::KsWorkInProgress	_workInProgress;

	bool	_updateSessionSize;

	void _load(const QString& fileName, bool append);

	void _open();

	void _append();

	void _restoreSession();

	void _importSession();

	void _exportSession();

	void _listFilterSync(int state);

	void _graphFilterSync(int state);

	void _presetCBWidget(kshark_hash_id *showFilter,
			     kshark_hash_id *hideFilter,
			     KsWidgetsLib::KsCheckBoxWidget *cbw);

	void _applyFilter(int sd, QVector<int> all, QVector<int> show,
			  std::function<void(int, QVector<int>)> posFilter,
			  std::function<void(int, QVector<int>)> negFilter);

	void _showEvents();

	void _showTasks();

	void _showCPUs();

	void _advancedFiltering();

	void _clearFilters();

	void _cpuSelect();

	void _taskSelect();

	void _pluginSelect();

	void _pluginUpdate(int sd, QVector<int> pluginStates);

	void _pluginAdd();

	void _record();

	void _offset();

	void _setColorPhase(int);

	void _changeScreenMode();

	void _aboutInfo();

	void _contents();

	void _bugReport();

	void _captureStarted();

	void _captureError(QProcess::ProcessError error);

	void _captureErrorMessage(QProcess *capture);

	void _readSocket();

	void _splitterMoved(int pos, int index);

	void _createActions();

	void _createMenus();

	void _initCapture();

	void _updateSession();

	inline void _resizeEmpty() {resize(SCREEN_WIDTH * .5, FONT_HEIGHT * 3);}

	void _error(const QString &text,
		    const QString &errCode,
		    bool resize);

	void _deselectActive();

	void _deselectA();

	void _deselectB();

	void _rootWarning();

	void _updateFilterMenu();

	void _filterSyncCBoxUpdate(kshark_context *kshark_ctx);

	QString _getCacheDir();

private slots:
	void _captureFinished(int, QProcess::ExitStatus);
};

#endif // _KS_MAINWINDOW_H
