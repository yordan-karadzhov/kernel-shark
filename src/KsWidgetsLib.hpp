/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

 /**
  *  @file    KsWidgetsLib.hpp
  *  @brief   Defines small widgets and dialogues used by the KernelShark GUI.
  */

#ifndef _KS_WIDGETS_LIB_H
#define _KS_WIDGETS_LIB_H

// Qt
#include <QtWidgets>

// KernelShark
#include "libkshark.h"
#include "KsUtils.hpp"

namespace KsWidgetsLib
{

/**
 * The KsProgressBar class provides a visualization of the progress of a
 * running job.
 */
class KsProgressBar : public QWidget
{
	Q_OBJECT

	QStatusBar	_sb;

	QProgressBar	_pb;

public:
	KsProgressBar(QString message, QWidget *parent = nullptr);

	virtual ~KsProgressBar();

	void setValue(int i);

	void workInProgress();

private:
	bool	_notDone;
};

/** Defines the progress bar's maximum value. */
#define KS_PROGRESS_BAR_MAX 200

/** The height of the KsProgressBar widget. */
#define KS_PROGBAR_HEIGHT (FONT_HEIGHT * 5)

/** The width of the KsProgressBar widget. */
#define KS_PROGBAR_WIDTH  (FONT_WIDTH * 50)

/** Data Work identifiers. */
enum class KsDataWork
{
	AnyWork,
	EditPlotList,
	ZoomIn,
	QuickZoomIn,
	ZoomOut,
	QuickZoomOut,
	ScrollLeft,
	ScrollRight,
	JumpTo,
	GraphUpdateGeom,
	UpdatePlugins,
	ResizeGL,
	RenderGL,
};

/** Defines hash function needed by the QSet tempate container class. */
inline uint qHash(KsDataWork key, uint seed)
{
	/*
	 * Cast the enum class to uint and use the definition of qHash outside
	 * the current scope.
	 */
	return ::qHash(static_cast<uint>(key), seed);
}

/**
 * The KsWorkInProgress class provides a widget showing the
 * "work in progress" notification.
 */
class KsWorkInProgress : public QWidget
{
public:
	explicit KsWorkInProgress(QWidget *parent = nullptr);

	void show(KsDataWork w);

	void hide(KsDataWork w);

	bool isBusy(KsDataWork w = KsDataWork::AnyWork) const;

	void addToStatusBar(QStatusBar *sb);

private:
	QLabel	_icon, _message;

	QSet<KsDataWork>	_works;
};

/**
 * The KsDataWidget class provides a base widget that provides the capability
 * to show the "work in progress" notification. The class must be inherited by
 * the widgets performing heavy data processing operations.
 */
class KsDataWidget : public QWidget
{
public:
	/**
	 * @brief Create KsDataWidget.
	 *
	 * @param parent: The parent of this widget.
	 */
	explicit KsDataWidget(QWidget *parent = nullptr)
	: QWidget(parent), _workInProgress(nullptr) {}

	/** Set the pointer to the KsWorkInProgress widget. */
	void setWipPtr(KsWorkInProgress *wip)
	{
		_workInProgress = wip;
	}

	/**
	 * Call this function when a given work is about to start in order to
	 * show the "work in progress" notification.
	 */
	void startOfWork(KsDataWork w)
	{
		if (_workInProgress)
			_workInProgress->show(w);
	}

	/**
	 * Call this function when a given work is done in order to hide the
	 * "work in progress" notification.
	 */
	void endOfWork(KsDataWork w)
	{
		if (_workInProgress)
			_workInProgress->hide(w);
	}

	/** Check if the GUI is busy processing data. */
	bool isBusy(KsDataWork w = KsDataWork::AnyWork) const
	{
		return _workInProgress ? _workInProgress->isBusy(w) : false;
	}

private:
	KsWorkInProgress	*_workInProgress;
};

/**
 * The KsMessageDialog class provides a widget showing a message and having
 * a "Close" button.
 */
class KsMessageDialog : public QDialog
{
	QVBoxLayout	_layout;

	QLabel		_text;

	QPushButton	_closeButton;

public:
	explicit KsMessageDialog(QWidget *parent) = delete;

	KsMessageDialog(QString message, QWidget *parent = nullptr);
};

/** The height of the KsMessageDialog widget. */
#define KS_MSG_DIALOG_HEIGHT (FONT_HEIGHT * 8)

/** The width of the KsMessageDialog widget. */
#define KS_MSG_DIALOG_WIDTH  (SCREEN_WIDTH / 10)

bool fileExistsDialog(QString fileName);

/**
 * The KsTimeOffsetDialog class provides a dialog used to enter the value of
 * the time offset between two Data streams.
 */
class KsTimeOffsetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit KsTimeOffsetDialog(QWidget *parent = nullptr);

	static double getValueNanoSec(QString dataFile, bool *ok);

signals:
	/** Signal emitted when the "Apply" button is pressed. */
	void apply(int sd, double val);

private:
	QInputDialog	_input;

	QComboBox	_streamCombo;

private slots:
	void _setDefault(int index);
};

/**
 * The KsCheckBoxWidget class is the base class of all CheckBox widget used
 * by KernelShark.
 */
class KsCheckBoxWidget : public QWidget
{
	Q_OBJECT
public:
	KsCheckBoxWidget(int sd, const QString &name = "",
			 QWidget *parent = nullptr);

	/** Get the name of the widget. */
	QString name() const {return _name;}

	/** Get the state of the "all" checkbox. */
	bool all() const
	{
		if(_allCb.checkState() == Qt::Checked)
			return true;
		return false;
	}

	/** The "all" checkbox to be visible or not. */
	void setVisibleCbAll(bool v) {_allCbAction->setVisible(v);}

	void setDefault(bool);

	void set(QVector<int> v);

	/**
	 * Get a vector containing all Ids (can be PID CPU Ids etc.) managed
	 * by this widget.
	 */
	QVector<int> getIds() const {return _id;}

	QVector<int> getCheckedIds();

	QVector<int> getStates();

	/**
	 * Get the identifier of the Data stream for which the selection
	 * applies.
	 */
	int sd() const {return _sd;}

	/** The user provided an input. The widget has been modified. */
	bool _userInput;

protected:
	/**
	 * Reimplemented event handler used to update the geometry of the widget on
	 * resize events.
	 */
	void resizeEvent(QResizeEvent* event) override
	{
		KsUtils::setElidedText(&_streamLabel, _streamName,
				       Qt::ElideLeft, width());
		QApplication::processEvents();
	}

protected:
	QToolBar _tb;

	/** Identifier of the Data stream for which the selection applies. */
	int		_sd;

	/** The "all" checkbox. */
	QCheckBox	_allCb;

	/** A vector of Id numbers coupled to each checkbox. */
	QVector<int>	_id;

	/** A nested widget used to position the checkboxes. */
	QWidget		_cbWidget;

	/** The layout of the nested widget. */
	QVBoxLayout	_cbLayout;

	/** The top level layout of this widget. */
	QVBoxLayout	_topLayout;

private:
	QAction		*_allCbAction;

	/**
	 * The name of this Data stream. Typically this will be the name of
	 * the data file.
	 */
	QString		_streamName;
	/**
	 * A label to show the name of the Data stream for which the selection
	 * applies. */
	QLabel		_streamLabel;

	/** The name of this widget. */
	QString		_name;

	/** A label to show the name of the widget. */
	QLabel		_nameLabel;

	virtual void _setCheckState(int i, Qt::CheckState st) = 0;

	virtual Qt::CheckState _checkState(int i) const = 0;

	virtual void _verify() {};

	void _checkAll(bool);

	void _setStream(int8_t sd);
};

/**
 * The KsCheckBoxDialog class is the base class of all CheckBox dialogs
 * used by KernelShark.
 */
class KsCheckBoxDialog : public QDialog
{
	Q_OBJECT
public:
	KsCheckBoxDialog() = delete;

	KsCheckBoxDialog(QVector<KsCheckBoxWidget *> cbws,
			 QWidget *parent = nullptr);

	/**
	 * The "apply" signal will emit a vector containing the Ids of all
	 * checked checkbox.
	 */
	void applyIds(bool v = true) {_applyIds = v;}

	/**
	 * The "apply" signal will emit a vector containing the statuse of all
	 * checkbox.
	 */
	void applyStatus(bool v = true) {_applyIds = !v;}

signals:
	/** Signal emitted when the "Apply" button is pressed. */
	void apply(int sd, QVector<int>);

private:
	void _applyPress();

	virtual void _preApplyAction() {}

	virtual void _postApplyAction() {}

	bool _applyIds;

	QVBoxLayout			_topLayout;

	QHBoxLayout			_cbLayout, _buttonLayout;

	QVector<KsCheckBoxWidget *>	_checkBoxWidgets;

	QPushButton			_applyButton, _cancelButton;

	QMetaObject::Connection		_applyButtonConnection;
};

/**
 * The KsPluginsCheckBoxDialog provides dialog for selecting plugins.
 * used by KernelShark. The class is used to override _postApplyAction().
 */
class KsPluginsCheckBoxDialog : public KsCheckBoxDialog
{
public:
	KsPluginsCheckBoxDialog() = delete;

	/** Create KsPluginsCheckBoxDialog. */
	KsPluginsCheckBoxDialog(QVector<KsCheckBoxWidget *> cbws,
				KsDataStore *d, QWidget *parent = nullptr)
	: KsCheckBoxDialog(cbws, parent), _data(d) {}

private:
	virtual void _postApplyAction() override;

	KsDataStore	*_data;
};

/** The KsCheckBoxTable class provides a table of checkboxes. */
class KsCheckBoxTable : public QTableWidget
{
	Q_OBJECT
public:
	explicit KsCheckBoxTable(QWidget *parent = nullptr);

	void init(QStringList headers, int size);

	/** A vector of checkboxes. */
	QVector<QCheckBox*>	_cb;

signals:
	/** Signal emitted when a checkboxes changes state. */
	void changeState(int row);

protected:
	void keyPressEvent(QKeyEvent *event) override;

	void mousePressEvent(QMouseEvent *event) override;

private:
	void _doubleClicked(int row, int col);
};

/**
 * The KsCheckBoxTableWidget class provides a widget to hold a table of
 * checkboxes.
 */
class KsCheckBoxTableWidget : public KsCheckBoxWidget
{
	Q_OBJECT
public:
	KsCheckBoxTableWidget(int sd, const QString &name = "",
			      QWidget *parent = nullptr);

	/** Only one checkbox at the time can be checked. */
	void setSingleSelection()
	{
		_table.setSelectionMode(QAbstractItemView::SingleSelection);
		setVisibleCbAll(false);
	}

protected:
	void _adjustSize();

	void _initTable(QStringList headers, int size);

	/** The KsCheckBoxTable, shown by this widget. */
	KsCheckBoxTable		_table;

private:
	void _setCheckState(int i, Qt::CheckState st) override
	{
		_table._cb[i]->setCheckState(st);
	}

	Qt::CheckState _checkState(int i) const override
	{
		return _table._cb[i]->checkState();
	}

	void _update(bool);

	void _changeState(int row);
};

/** The KsCheckBoxTree class provides a tree of checkboxes. */
class KsCheckBoxTree : public QTreeWidget
{
	Q_OBJECT
public:
	explicit KsCheckBoxTree(QWidget *parent = nullptr);

signals:
	/**
	 * Signal emitted when a checkboxes of the tree changes its state
	 * and the state of all toplevel and child checkboxes has to be
	 * reprocesed.
	 */
	void verify();

protected:
	void keyPressEvent(QKeyEvent *event) override;

	void mousePressEvent(QMouseEvent *event) override;

private:
	void _doubleClicked(QTreeWidgetItem *item, int col);
};

/**
 * The KsCheckBoxTreeWidget class provides a widget to hold a tree of
 * checkboxes.
 */
class KsCheckBoxTreeWidget : public KsCheckBoxWidget
{
	Q_OBJECT
public:
	KsCheckBoxTreeWidget() = delete;

	KsCheckBoxTreeWidget(int sd, const QString &name = "",
			     QWidget *parent = nullptr);

	/** Only one checkbox at the time can be checked. */
	void setSingleSelection()
	{
		_tree.setSelectionMode(QAbstractItemView::SingleSelection);
		setVisibleCbAll(false);
	}

protected:
	void _adjustSize();

	void _initTree();

	/** The KsCheckBoxTree, shown by this widget. */
	KsCheckBoxTree			_tree;

	/** A vector of Tree items (checkboxes). */
	QVector<QTreeWidgetItem*>	_cb;

private:
	void _setCheckState(int i, Qt::CheckState st) override
	{
		_cb[i]->setCheckState(0, st);
	}

	Qt::CheckState _checkState(int i) const override
	{
		return _cb[i]->checkState(0);
	}

	void _update(QTreeWidgetItem *item, int column);

	void _verify();
};

/**
 * The KsCPUCheckBoxWidget class provides a widget for selecting CPU plots to
 * show.
 */
struct KsCPUCheckBoxWidget : public KsCheckBoxTreeWidget
{
	KsCPUCheckBoxWidget() = delete;

	KsCPUCheckBoxWidget(kshark_data_stream *stream,
			    QWidget *parent = nullptr);

private:
	/** The "hide empty" checkbox. */
	QCheckBox	_hideEmpty;
};

/**
 * The KsTasksCheckBoxWidget class provides a widget for selecting Tasks
 * to show or hide.
 */
struct KsTasksCheckBoxWidget : public KsCheckBoxTableWidget
{
	KsTasksCheckBoxWidget() = delete;

	KsTasksCheckBoxWidget(kshark_data_stream *stream,
			      bool cond = true,
			      QWidget *parent = nullptr);

private:
	/**
	 * A positive condition means that you want to show Tasks and
	 * a negative condition means that you want to hide Tasks.
	 */
	bool		_cond;
};

/**
 * The KsEventsCheckBoxWidget class provides a widget for selecting Trace
 * events to show or hide.
 */
struct KsEventsCheckBoxWidget : public KsCheckBoxTreeWidget
{
	KsEventsCheckBoxWidget() = delete;

	KsEventsCheckBoxWidget(kshark_data_stream *stream,
			       QWidget *parent = nullptr);

	QStringList getCheckedEvents(bool option);

	void removeSystem(QString name);

private:
	void _makeItems(kshark_data_stream *stream, QVector<int> eventIds);

	void _makeTepEventItems(kshark_data_stream *stream, QVector<int> eventIds);
};

/**
 * The KsPluginCheckBoxWidget class provides a widget for selecting plugins.
 */
struct KsPluginCheckBoxWidget : public KsCheckBoxTableWidget
{
	KsPluginCheckBoxWidget() = delete;

	KsPluginCheckBoxWidget(int sd, QStringList pluginList,
			       QWidget *parent = nullptr);

	void setInfo(int row, QString info);

	void setActive(QVector<int> rows, bool a);
};

/**
 * The KsDStreamCheckBoxWidget class provides a widget for selecting Data streams.
 */
struct KsDStreamCheckBoxWidget : public KsCheckBoxTableWidget
{
	explicit KsDStreamCheckBoxWidget(QWidget *parent = nullptr);
};

/**
 * The KsEventFieldSelectWidget class provides a widget for selecting a data
 * field of the trace event.
 */
class KsEventFieldSelectWidget : public QWidget
{
	Q_OBJECT
public:
	explicit KsEventFieldSelectWidget(QWidget *parent = nullptr);

	/** Get the currently selected stream Id. */
	int streamId() const {return _streamComboBox.currentData().toInt();}

	/** Get the currently selected event name. */
	QString eventName() const {return _eventComboBox.currentText();}

	/** Get the currently selected field name. */
	QString fieldName() const {return _fieldComboBox.currentText();}

	void setStreamCombo();

private slots:
	void _streamChanged(int);

	void _eventChanged(int);

private:
	QVBoxLayout	_topLayout;

	QComboBox	_streamComboBox, _eventComboBox, _fieldComboBox;

	QLabel		_streamLabel, _eventLabel, _fieldLabel;
};

}; // KsWidgetsLib

#endif
