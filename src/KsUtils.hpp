/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    KsUtils.hpp
 *  @brief   KernelShark Utils.
 */

#ifndef _KS_UTILS_H
#define _KS_UTILS_H

// C++ 11
#include <chrono>

// Qt
#include <QtWidgets>

// KernelShark
#include "libkshark.h"
#include "libkshark-model.h"
#include "KsCmakeDef.hpp"
#include "KsPlotTools.hpp"

/** Macro providing the height of the screen in pixels. */
#define SCREEN_HEIGHT  QGuiApplication::screens().at(0)->geometry().height()

/** Macro providing the width of the screen in pixels. */
#define SCREEN_WIDTH   QGuiApplication::screens().at(0)->geometry().width()

//! @cond Doxygen_Suppress

static auto fontHeight = []()
{
	QFont font;
	QFontMetrics fm(font);

	return fm.height();
};

static auto stringWidth = [](QString s)
{
	QFont font;
	QFontMetrics fm(font);
	return fm.horizontalAdvance(s);
};

//! @endcond

/** Macro providing the height of the font in pixels. */
#define FONT_HEIGHT		fontHeight()

/** Macro providing the width of the font in pixels. */
#define FONT_WIDTH 		(stringWidth("KernelShark") / 11)

/** Macro providing the width of a string in pixels. */
#define STRING_WIDTH(s)		stringWidth(s)

/** Macro providing the height of the KernelShark graphs in pixels. */
#define KS_GRAPH_HEIGHT		(FONT_HEIGHT * 2)

//! @cond Doxygen_Suppress

#define KS_JSON_CAST(doc) \
reinterpret_cast<json_object *>(doc)

#define KS_C_STR_CAST(doc) \
reinterpret_cast<const char *>(doc)

typedef std::chrono::high_resolution_clock::time_point  hd_time;

#define GET_TIME std::chrono::high_resolution_clock::now()

#define GET_DURATION(t0) \
std::chrono::duration_cast<std::chrono::duration<double>>( \
std::chrono::high_resolution_clock::now() - t0).count()

#define KS_SPLIT_SkipEmptyParts Qt::SkipEmptyParts

//! @endcond

namespace KsUtils {

QVector<int> getCPUList(int sd);

QVector<int> getPidList(int sd);

QVector<int> getEventIdList(int sd);

int getEventId(int sd, const QString &eventName);

QString getEventName(int sd, int eventId);

QStringList getEventFieldsList(int sd, int eventId);

kshark_event_field_format getEventFieldType(int sd, int eventId,
					    const QString &fieldName);

QVector<int> getStreamIdList(kshark_context *kshark_ctx);

QVector<int> getFilterIds(kshark_hash_id *filter);

/** @brief Geat the list of plugins provided by the package. */
inline QStringList getPluginList() {return QString(KS_BUILTIN_PLUGINS).split(";");}

void listFilterSync(bool state);

void graphFilterSync(bool state);

QCheckBox *addCheckBoxToMenu(QMenu *menu, QString name);

/** @brief Convert the timestamp of the trace record into a string showing
 *	   the time in seconds.
 *
 * @param ts: Input location for the timestamp.
 * @param prec: the number of digits after the decimal point in the return
 *		string.
 *
 * @returns String showing the time in seconds.
 */
inline QString Ts2String(int64_t ts, int prec)
{
	return QString::number(ts * 1e-9, 'f', prec);
}

bool matchCPUVisible(kshark_context *kshark_ctx,
		     kshark_entry *e, int sd, int *cpu);

bool isInstalled();

QString getFile(QWidget *parent,
		const QString &windowName,
		const QString &filter,
		QString &lastFilePath);

QStringList getFiles(QWidget *parent,
		     const QString &windowName,
		     const QString &filter,
		     QString &lastFilePath);

QString getSaveFile(QWidget *parent,
		    const QString &windowName,
		    const QString &filter,
		    const QString &extension,
		    QString &lastFilePath);

void setElidedText(QLabel* label, QString text,
		   enum Qt::TextElideMode mode,
		   int labelWidth);

QStringList splitArguments(QString cmd);

QVector<int> parseIdList(QString v_str);

QMap<int, QVector<int>> parseTaskList(QString v_str);

QStringList getTepEvtName(int sd, int eventId);

/** Get a string to be used as a standard name of a CPU graph. */
inline QString cpuPlotName(int cpu) {return QString("CPU %1").arg(cpu);}

QString taskPlotName(int sd, int pid);

/** Get the total number of Data streams. */
inline int getNStreams()
{
	kshark_context *kshark_ctx(nullptr);

	if (!kshark_instance(&kshark_ctx))
		return -1;
	return kshark_ctx->n_streams;
}

QString streamDescription(kshark_data_stream *stream);

}; // KsUtils

/** Identifier of the Dual Marker active state. */
enum class DualMarkerState {
	A,
	B
};

/**
 * The KsDataStore class provides the access to trace data for all KernelShark
 * widgets.
 */
class KsDataStore : public QObject
{
	Q_OBJECT
public:
	explicit KsDataStore(QWidget *parent = nullptr);

	~KsDataStore();

	int loadDataFile(const QString &file,
			 QVector<kshark_dpi *> plugins);

	int appendDataFile(const QString &file, int64_t shift);

	void clear();

	/** Get the trace data array. */
	kshark_entry **rows() const {return _rows;}

	/** Get a reference of the trace data array. */
	kshark_entry ***rows_r() {return &_rows;}

	/** Get the size of the data array. */
	ssize_t size() const {return _dataSize;}

	/** Set the size of the data (number of entries). */
	void setSize(ssize_t s) {_dataSize = s;}

	void reload();

	void update();

	void registerCPUCollections();

	void unregisterCPUCollections();

	void applyPosTaskFilter(int sd, QVector<int> vec);

	void applyNegTaskFilter(int sd, QVector<int> vec);

	void applyPosEventFilter(int sd, QVector<int> vec);

	void applyNegEventFilter(int sd, QVector<int> vec);

	void applyPosCPUFilter(int sd, QVector<int> vec);

	void applyNegCPUFilter(int sd, QVector<int> vec);

	void clearAllFilters();

	void setClockOffset(int sd, int64_t offset);

signals:
	/**
	 * This signal is emitted when the data has changed and the View
	 * widgets have to update.
	 */
	void updateWidgets(KsDataStore *);

private:
	/** Trace data array. */
	kshark_entry		**_rows;

	/** The size of the data array. */
	ssize_t			_dataSize;

	int _openDataFile(kshark_context *kshark_ctx, const QString &file);

	void _freeData();

	void _applyIdFilter(int filterId, QVector<int> vec, int sd);

	void _addPluginsToStream(kshark_context *kshark_ctx, int sd,
				 QVector<kshark_dpi *> plugins);
};

/** A Plugin Manager class. */
class KsPluginManager : public QObject
{
	Q_OBJECT
public:
	explicit KsPluginManager(QWidget *parent = nullptr);

	QStringList getStreamPluginList(int sd) const;

	QVector<int> getActivePlugins(int sd) const;

	QVector<int> getPluginsByStatus(int sd, int status) const;

	/** Get a list of all plugins added by the user. */
	const QVector<kshark_plugin_list *>
	getUserPlugins() const {return _userPlugins;}

	void registerPluginMenues();

	void updatePlugins(int sd, QVector<int> pluginStates);

	void addPlugins(const QStringList &fileNames, QVector<int> streams);

	void registerPlugins(const QString &pluginNames);

	void unregisterPlugins(const QString &pluginNames);

	void registerPluginToStream(const QString &pluginName,
				    QVector<int> streamId);

	void unregisterPluginFromStream(const QString &pluginName,
					QVector<int> streamId);

	void deletePluginDialogs();

	/** Append to the list of User plugin. */
	void addUserPluginToList(kshark_plugin_list *p) {_userPlugins.append(p);}

signals:
	/** This signal is emitted when a plugin is loaded or unloaded. */
	void dataReload();

private:
	QVector<kshark_plugin_list *>	_userPlugins;

	/** Plugin dialogs. */
	QVector<QWidget *>		_pluginDialogs;

	QVector<kshark_plugin_list *>
	_loadPluginList(const QStringList &plugins);

	void _registerCtrlInterface(kshark_plugin_list *plugin);

	std::string _pluginLibFromName(const QString &plugin);

	std::string _pluginNameFromLib(const QString &plugin);

	void _pluginToStream(const QString &pluginName,
			     QVector<int> streamId,
			     bool reg);
};

KsPlot::Color& operator <<(KsPlot::Color &thisColor, const QColor &c);

QColor& operator <<(QColor &thisColor, const KsPlot::Color &c);

#endif
