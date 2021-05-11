// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

// Boost
#define BOOST_TEST_MODULE KernelSharkTests
#include <boost/test/unit_test.hpp>

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"
#include "KsUtils.hpp"
#include "KsModels.hpp"


using namespace KsUtils;

#define N_RECORDS_TEST1		1530

BOOST_AUTO_TEST_CASE(KsUtils_datatest)
{
	kshark_context *kshark_ctx{nullptr};
	kshark_entry **data{nullptr};
	std::string file(KS_TEST_DIR);
	ssize_t n_rows;
	int sd, ss_id;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	file += "/trace_test1.dat";
	sd = kshark_open(kshark_ctx, file.c_str());
	BOOST_CHECK_EQUAL(sd, 0);

	n_rows = kshark_load_entries(kshark_ctx, sd, &data);
	BOOST_CHECK_EQUAL(n_rows, N_RECORDS_TEST1);

	auto cpus = getCPUList(sd);
	BOOST_CHECK_EQUAL(cpus.size(), 8);
	for (int i = 0; i < cpus.size(); ++i)
		BOOST_CHECK_EQUAL(cpus[i], i);

	auto pids = getPidList(sd);
	BOOST_CHECK_EQUAL(pids.size(), 46);
	BOOST_CHECK_EQUAL(pids[0], 0);
	for (int i = 1; i < pids.size(); ++i)
		BOOST_CHECK(pids[i] > pids[i - 1]);

	auto evts = getEventIdList(sd);
	BOOST_CHECK_EQUAL(evts.size(), 40);
	BOOST_CHECK_EQUAL(evts[34], 323);

	ss_id = getEventId(sd, "sched/sched_switch");
	BOOST_CHECK_EQUAL(ss_id, 323);

	QString name = getEventName(sd, 323);
	BOOST_CHECK(name == QString("sched/sched_switch"));
	name = getEventName(sd, 999);
	BOOST_CHECK(name == QString("Unknown"));

	auto fields = getEventFieldsList(sd, ss_id);
	BOOST_CHECK_EQUAL(fields.size(), 11);
	BOOST_CHECK(fields[10] == QString("next_prio"));

	BOOST_CHECK_EQUAL(getEventFieldType(sd, ss_id, "next_prio"),
			  KS_INTEGER_FIELD);

	BOOST_CHECK_EQUAL(getEventFieldType(sd, ss_id, "next_comm"),
			  KS_INVALID_FIELD);

	for (ssize_t r = 0; r < n_rows; ++r)
		free(data[r]);
	free(data);

	kshark_close(kshark_ctx, sd);
	kshark_free(kshark_ctx);
}

BOOST_AUTO_TEST_CASE(KsUtils_setFilterSync)
{
	struct kshark_context *kshark_ctx{nullptr};

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	kshark_ctx->filter_mask = KS_TEXT_VIEW_FILTER_MASK |
				  KS_GRAPH_VIEW_FILTER_MASK |
				  KS_EVENT_VIEW_FILTER_MASK;

	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask, 0x7);

	listFilterSync(false);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_TEXT_VIEW_FILTER_MASK, 0);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_GRAPH_VIEW_FILTER_MASK,
			  KS_GRAPH_VIEW_FILTER_MASK);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_EVENT_VIEW_FILTER_MASK,
			  KS_EVENT_VIEW_FILTER_MASK);
	listFilterSync(true);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask, 0x7);

	graphFilterSync(false);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_TEXT_VIEW_FILTER_MASK,
			  KS_TEXT_VIEW_FILTER_MASK);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_GRAPH_VIEW_FILTER_MASK, 0);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask & KS_EVENT_VIEW_FILTER_MASK, 0);
	graphFilterSync(true);
	BOOST_CHECK_EQUAL(kshark_ctx->filter_mask, 0x7);

	kshark_free(kshark_ctx);
}

BOOST_AUTO_TEST_CASE(KsUtils_parseIds)
{
	QVector<int> ids_test = parseIdList("1,33,4-6,3,55-57");
	QVector<int> ids = {1, 33, 4, 5, 6, 3, 55, 56, 57};
	BOOST_CHECK(ids == ids_test);
}

#define N_RECORDS_TEST2		73945
BOOST_AUTO_TEST_CASE(KsUtils_KsDataStore)
{
	int64_t ts_last(0);
	KsDataStore data;
	int sd;

	BOOST_CHECK_EQUAL(data.size(), 0);
	BOOST_CHECK_EQUAL(data.rows(), nullptr);

	sd = data.loadDataFile(QString(KS_TEST_DIR) + "/trace_test1.dat", {});
	BOOST_CHECK_EQUAL(sd, 0);
	BOOST_CHECK_EQUAL(data.size(), N_RECORDS_TEST1);
	BOOST_CHECK(data.rows() != nullptr);

	sd = data.appendDataFile(QString(KS_TEST_DIR) + "/trace_test2.dat", {});
	BOOST_CHECK_EQUAL(sd, 1);
	BOOST_CHECK_EQUAL(data.size(), N_RECORDS_TEST1 + N_RECORDS_TEST2);

	kshark_entry **rows = data.rows();
	for (ssize_t i = 0; i < data.size(); ++i) {
		BOOST_CHECK(rows[i]->ts >= ts_last);
		ts_last = rows[i]->ts;
	}

	data.clear();
	BOOST_CHECK_EQUAL(data.size(), 0);
	BOOST_CHECK_EQUAL(data.rows(), nullptr);
}

BOOST_AUTO_TEST_CASE(KsUtils_getPluginList)
{
	QStringList plugins{"sched_events",
			    "event_field_plot",
			    "latency_plot",
			    "kvm_combo",
			    "missed_events"
	};

	BOOST_CHECK(getPluginList() == plugins);
}

#define PLUGIN_1_LIB	"/plugin-dummy_dpi.so"
#define PLUGIN_2_LIB	"/plugin-dummy_dpi_ctrl.so"
#define INPUT_A_LIB	"/input-dummy_input.so"

QString path(KS_TEST_DIR);

BOOST_AUTO_TEST_CASE(KsUtils_KsPluginManager)
{
	struct kshark_context *kshark_ctx = NULL;
	int sd, argc{0};
	QCoreApplication a(argc, nullptr);

	KsPluginManager pm;
	pm.registerPlugins(path + INPUT_A_LIB);

	kshark_instance(&kshark_ctx);
	BOOST_CHECK_EQUAL(kshark_ctx->n_inputs, 1);
	BOOST_CHECK(kshark_ctx->inputs != nullptr);

	sd = kshark_add_stream(kshark_ctx);
	BOOST_CHECK_EQUAL(sd, 0);
	kshark_ctx->stream[sd]->interface =
		malloc(1);

	sd = kshark_add_stream(kshark_ctx);
	BOOST_CHECK_EQUAL(sd, 1);
	kshark_ctx->stream[sd]->interface = malloc(1);

	pm.registerPluginToStream("sched_events",
				  getStreamIdList(kshark_ctx));

	QStringList list = pm.getStreamPluginList(sd);
	BOOST_CHECK_EQUAL(list.count(), 1);
	BOOST_CHECK(list[0] == "sched_events");

	QString testPlugins = path + PLUGIN_1_LIB + ",";
	testPlugins += path + PLUGIN_2_LIB;
	pm.registerPlugins(testPlugins);
	auto listTest = pm.getUserPlugins();
	BOOST_CHECK_EQUAL(listTest.count(), 3);
	list = pm.getStreamPluginList(sd);

	for (auto const &p: listTest)
		pm.registerPluginToStream(p->name, {sd});

	list = pm.getStreamPluginList(sd);
	BOOST_CHECK_EQUAL(list.count(), 3);

	BOOST_CHECK(list == QStringList({"dummy_dpi_ctrl",
					 "dummy_dpi",
					 "sched_events"}));

	auto active = pm.getActivePlugins(sd);
	BOOST_CHECK(pm.getActivePlugins(sd) == QVector<int>({1, 1, 1}));

	auto enabled = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_ENABLED);
	BOOST_CHECK(enabled == QVector<int>({0, 1, 2}));
	auto loaded = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_LOADED);
	BOOST_CHECK(loaded == QVector<int>({0, 1}));
	auto failed = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_FAILED);
	BOOST_CHECK(failed == QVector<int>({2}));

	active[1] = 0;
	pm.updatePlugins(sd, active);
	BOOST_CHECK(active == pm.getActivePlugins(sd));

	enabled = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_ENABLED);
	BOOST_CHECK(enabled == QVector<int>({0, 2}));
	loaded = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_LOADED);
	BOOST_CHECK(loaded == QVector<int>({0}));
	failed = pm.getPluginsByStatus(sd, KSHARK_PLUGIN_FAILED);
	BOOST_CHECK(failed == QVector<int>({2}));

	kshark_free(kshark_ctx);
	a.exit();
}

BOOST_AUTO_TEST_CASE(ViewModel)
{
	QStringList header{"#", "CPU", "Time Stamp", "Task", "PID", "Latency", "Event", "Info"};
	struct kshark_context *kshark_ctx(nullptr);
	KsViewModel model;
	KsDataStore data;

	data.loadDataFile(QString(KS_TEST_DIR) + "/trace_test1.dat", {});
	model.fill(&data);
	BOOST_CHECK_EQUAL(model.rowCount({}), N_RECORDS_TEST1);
	BOOST_CHECK_EQUAL(model.columnCount({}), 8);
	BOOST_CHECK_EQUAL(model.singleStream(), true);
	BOOST_CHECK(model.header() == header);

	data.appendDataFile(QString(KS_TEST_DIR) + "/trace_test2.dat", {});
	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	BOOST_CHECK(getStreamIdList(kshark_ctx) == QVector<int>({0, 1}));

	model.update(&data);
	header = QStringList{" >> "} + header;

	BOOST_CHECK_EQUAL(model.rowCount({}), N_RECORDS_TEST1 + N_RECORDS_TEST2);
	BOOST_CHECK_EQUAL(model.columnCount({}), 9);
	BOOST_CHECK_EQUAL(model.singleStream(), false);
	BOOST_CHECK(model.header() == header);

	BOOST_CHECK(model.getValueStr(0, 0) == "1");
	BOOST_CHECK(model.getValueStr(4, 1) == "trace-cmd");
	BOOST_CHECK(model.getValueStr(5, 2) == "29474");
	BOOST_CHECK(model.getValueStr(7, 2) == "sched/sched_switch");

	BOOST_CHECK(model.getValueStr(0, N_RECORDS_TEST1 + N_RECORDS_TEST2 - 1) == "0");
	BOOST_CHECK(model.getValueStr(4, N_RECORDS_TEST1 + N_RECORDS_TEST2 - 1) == "<idle>");

	model.reset();
	BOOST_CHECK_EQUAL(model.rowCount({}), 0);
}

BOOST_AUTO_TEST_CASE(GraphModel)
{
	struct kshark_context *kshark_ctx(nullptr);
	KsGraphModel model;
	KsDataStore data;

	data.loadDataFile(QString(KS_TEST_DIR) + "/trace_test1.dat", {});
	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	BOOST_CHECK(getStreamIdList(kshark_ctx) == QVector<int>({0}));

	model.fill(&data);
	BOOST_CHECK_EQUAL(model.rowCount({}), KS_DEFAULT_NBUNS);
	BOOST_CHECK(abs(model.histo()->min - data.rows()[0]->ts) < model.histo()->bin_size);
	BOOST_CHECK(abs(model.histo()->max - data.rows()[N_RECORDS_TEST1 - 1]->ts) < model.histo()->bin_size);

	model.reset();
	BOOST_CHECK_EQUAL(model.rowCount({}), 0);
}

BOOST_AUTO_TEST_CASE(KsUtils_parseTasks)
{
	QVector<int> pids{28121, 28137, 28141, 28199, 28201, 205666, 267481};
	kshark_context *kshark_ctx{nullptr};
	kshark_entry **data{nullptr};
	std::string file(KS_TEST_DIR);
	ssize_t n_rows;
	int sd;

	kshark_instance(&kshark_ctx);
	file += "/trace_test1.dat";
	sd = kshark_open(kshark_ctx, file.c_str());
	n_rows = kshark_load_entries(kshark_ctx, sd, &data);

	auto pids_test = parseTaskList("zoom,sleep");
	BOOST_CHECK(pids == pids_test[0]);

	for (ssize_t r = 0; r < n_rows; ++r)
		free(data[r]);
	free(data);

	kshark_close(kshark_ctx, sd);
	kshark_free(kshark_ctx);
}
