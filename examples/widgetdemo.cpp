// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

// C
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>

// C++
#include <iostream>

// Qt
#include <QtWidgets>

// KernelShark
#include "KsUtils.hpp"
#include "KsWidgetsLib.hpp"

#define default_input_file (char*)"trace.dat"

static char *input_file = nullptr;

using namespace std;
using namespace KsWidgetsLib;

void usage(const char *prog)
{
	cout << "Usage: " << prog << endl
	     << "  -h	Display this help message\n"
	     << "  -v	Display version and exit\n"
	     << "  -i	input_file, default is " << default_input_file << endl
	     << "  -p	register plugin, use plugin name, absolute or relative path\n"
	     << "  -u	unregister plugin, use plugin name or absolute path\n";
}

struct TaskPrint : public QObject
{
	void print(int sd, QVector<int> pids)
	{
		for (auto const &pid: pids)
			cout << "task: "
			     << kshark_comm_from_pid(sd, pid)
			     << "  pid: " << pid << endl;
	}
};

int main(int argc, char **argv)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	QApplication a(argc, argv);
	KsPluginManager plugins;
	int c, i(0), sd(-1);
	KsDataStore data;
	size_t nRows(0);

	if (!kshark_instance(&kshark_ctx))
		return 1;

	while ((c = getopt(argc, argv, "hvi:p:u:")) != -1) {
		switch(c) {
		case 'v':
			printf("kshark-gui %s\n", KS_VERSION_STRING);
			return 0;

		case 'i':
			input_file = optarg;
			break;

		case 'p':
			plugins.registerPlugins(QString(optarg));
			break;

		case 'u':
			plugins.unregisterPlugins(QString(optarg));
			break;

		case 'h':
			usage(argv[0]);
			return 0;
		}
	}

	if (!input_file) {
			struct stat st;
			if (stat(default_input_file, &st) == 0)
				input_file = default_input_file;
	}

	if (input_file) {
		sd = data.loadDataFile(input_file, {});
		nRows = data.size();
	} else {
		cerr << "No input file is provided.\n";
	}

	cout << nRows << " entries loaded\n";

	if (!nRows)
		return 1;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return 1;

	cout << "\n\n";
	for (kshark_plugin_list *pl = kshark_ctx->plugins; pl; pl = pl->next)
			cout << pl->file << endl;
	sleep(1);

	QStringList pluginsList;
	QVector<int> streamIds, enabledPlugins, failedPlugins;

	pluginsList = plugins.getStreamPluginList(sd);
	enabledPlugins = plugins.getActivePlugins(sd);
	failedPlugins = plugins.getPluginsByStatus(sd, KSHARK_PLUGIN_FAILED);

	KsCheckBoxWidget *pluginCBD
		= new KsPluginCheckBoxWidget(sd, pluginsList);
	pluginCBD->set(enabledPlugins);

	KsCheckBoxDialog *dialog1 = new KsCheckBoxDialog({pluginCBD});
	dialog1->applyStatus();
	QObject::connect(dialog1,	&KsCheckBoxDialog::apply,
			 &plugins,	&KsPluginManager::updatePlugins);

	dialog1->show();
	a.exec();

	cout << "\n\nYou selected\n";
	enabledPlugins = plugins.getActivePlugins(sd);
	for (auto const &p: pluginsList)
		qInfo() << p << (bool) enabledPlugins[i++];

	sleep(1);

	KsCheckBoxWidget *tasks_cbd =
		new KsTasksCheckBoxWidget(stream, true);

	tasks_cbd->setDefault(false);

	TaskPrint p;
	KsCheckBoxDialog *dialog2 = new KsCheckBoxDialog({tasks_cbd});
	QObject::connect(dialog2,	&KsCheckBoxDialog::apply,
			 &p,		&TaskPrint::print);

	cout << "\n\nYou selected\n";
	dialog2->show();
	a.exec();

	return 0;
}
