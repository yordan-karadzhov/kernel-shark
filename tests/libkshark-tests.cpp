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
#include "KsCmakeDef.hpp"

#define N_TEST_STREAMS	1000

BOOST_AUTO_TEST_CASE(add_remove_streams)
{
	struct kshark_context *kshark_ctx = NULL;
	int sd, free = 0, i;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));

	for (i = 0; i < N_TEST_STREAMS; ++i) {
		sd = kshark_add_stream(kshark_ctx);
		BOOST_CHECK_EQUAL(sd, free);

		kshark_add_stream(kshark_ctx);

		free = i / 2;
		kshark_remove_stream(kshark_ctx, free);
		sd = kshark_add_stream(kshark_ctx);
		BOOST_CHECK_EQUAL(sd, free);

		free = i / 2 + 1;
		kshark_remove_stream(kshark_ctx, free);
	}

	BOOST_CHECK_EQUAL(kshark_ctx->n_streams, N_TEST_STREAMS);

	while (sd > 0)
		sd = kshark_add_stream(kshark_ctx);

	BOOST_CHECK_EQUAL(kshark_ctx->n_streams, INT16_MAX + 1);
	BOOST_CHECK_EQUAL(kshark_ctx->stream_info.array_size, INT16_MAX + 1);
	BOOST_CHECK_EQUAL(sd, -ENODEV);

	kshark_free(kshark_ctx);
}

BOOST_AUTO_TEST_CASE(get_stream)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	int sd;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	sd = kshark_add_stream(kshark_ctx);
	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK_EQUAL(stream, nullptr);

	kshark_ctx->stream[sd]->interface = malloc(1);
	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK(stream != nullptr);

	kshark_free(kshark_ctx);
}

BOOST_AUTO_TEST_CASE(close_all)
{
	struct kshark_context *kshark_ctx(nullptr);
	int sd, i;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	for (i = 0; i < N_TEST_STREAMS; ++i) {
		sd = kshark_add_stream(kshark_ctx);
		BOOST_CHECK_EQUAL(sd, i);
	}

	kshark_close_all(kshark_ctx);
	BOOST_CHECK_EQUAL(kshark_ctx->n_streams, 0);
	BOOST_CHECK_EQUAL(kshark_ctx->stream_info.next_free_stream_id, 0);
	BOOST_CHECK_EQUAL(kshark_ctx->stream_info.max_stream_id, -1);
	for (i = 0; i < kshark_ctx->stream_info.array_size; ++i)
		BOOST_CHECK_EQUAL(kshark_ctx->stream[i], nullptr);
}

#define ARRAY_DEFAULT_SIZE	1000
BOOST_AUTO_TEST_CASE(doule_size_macro)
{
	int i, n = ARRAY_DEFAULT_SIZE;
	int *arr = (int *) malloc(n * sizeof(*arr));
	bool ok;

	for (i = 0; i < n; ++i)
		arr[i] = i;

	ok = KS_DOUBLE_SIZE(arr, n);
	BOOST_CHECK_EQUAL(ok, true);
	BOOST_CHECK_EQUAL(n, ARRAY_DEFAULT_SIZE * 2);

	for (i = 0; i < ARRAY_DEFAULT_SIZE; ++i)
		BOOST_CHECK_EQUAL(arr[i], i);
	for (; i < n; ++i)
		BOOST_CHECK_EQUAL(arr[i], 0);
}

#define N_VALUES	2 * KS_CONTAINER_DEFAULT_SIZE + 1
#define MAX_TS		100000
BOOST_AUTO_TEST_CASE(fill_data_container)
{
	struct kshark_data_container *data = kshark_init_data_container();
	struct kshark_entry entries[N_VALUES];
	int64_t i, ts_last(0);

	BOOST_CHECK_EQUAL(data->capacity, KS_CONTAINER_DEFAULT_SIZE);

	for (i = 0; i < N_VALUES; ++i) {
		entries[i].ts = rand() % MAX_TS;
		kshark_data_container_append(data, &entries[i], 10 - entries[i].ts);
	}

	BOOST_CHECK_EQUAL(data->size, N_VALUES);
	BOOST_CHECK_EQUAL(data->capacity, 4 * KS_CONTAINER_DEFAULT_SIZE);

	kshark_data_container_sort(data);
	BOOST_CHECK_EQUAL(data->capacity, N_VALUES);
	for (i = 0; i < N_VALUES; ++i) {
		BOOST_CHECK(data->data[i]->entry->ts >= ts_last);
		BOOST_CHECK_EQUAL(data->data[i]->entry->ts,
				  10 - data->data[i]->field);

		ts_last = data->data[i]->entry->ts;
	}

	i = kshark_find_entry_field_by_time(MAX_TS / 2, data->data,
					    0, N_VALUES - 1);

	BOOST_CHECK(data->data[i - 1]->entry->ts < MAX_TS / 2);
	BOOST_CHECK(data->data[i]->entry->ts >= MAX_TS / 2);

	kshark_free_data_container(data);
}

struct test_context {
	int a;
	char b;
};

KS_DEFINE_PLUGIN_CONTEXT(struct test_context, free);

BOOST_AUTO_TEST_CASE(init_close_plugin)
{
	struct test_context *ctx;
	int i;

	for (i = 0; i < N_TEST_STREAMS; ++i) {
		ctx = __init(i);
		ctx->a = i * 10;
		ctx->b = 'z';
	}

	for (i = 0; i < N_TEST_STREAMS; ++i) {
		ctx = __get_context(i);
		BOOST_REQUIRE(ctx != NULL);
		BOOST_CHECK_EQUAL(ctx->a, i * 10);
		BOOST_CHECK_EQUAL(ctx->b, 'z');

		__close(i);
		BOOST_REQUIRE(__get_context(i) == NULL);
	}

	__close(-1);
}

#define PLUGIN_1_LIB	"/plugin-dummy_dpi.so"
#define PLUGIN_1_NAME	"dummy_dpi"

#define PLUGIN_2_LIB	"/plugin-dummy_dpi_ctrl.so"
#define PLUGIN_2_NAME	"dummy_dpi_ctrl"

#define INPUT_A_LIB	"/input-dummy_input.so"
#define INPUT_A_NAME	"dummy_input"

#define INPUT_B_LIB	"/input-dummy_input_ctrl.so"
#define INPUT_B_NAME	"dummy_input_ctrl"

std::string path(KS_TEST_DIR);

BOOST_AUTO_TEST_CASE(register_plugin)
{
	kshark_plugin_list *p1, *p2, *i1, *i2, *x1, *x2;
	kshark_generic_stream_interface *interface;
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	std::string plugin;
	int sd;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	BOOST_REQUIRE(kshark_ctx->plugins == nullptr);
	BOOST_REQUIRE(kshark_ctx->inputs == nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 0);

	plugin = path + PLUGIN_1_LIB;
	p1 = kshark_register_plugin(kshark_ctx, PLUGIN_1_NAME, plugin.c_str());
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 1);
	BOOST_CHECK(kshark_ctx->plugins != nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->plugins->next, nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->plugins, p1);
	BOOST_CHECK(p1 != nullptr);
	BOOST_CHECK(p1->process_interface != nullptr);
	BOOST_CHECK(p1->handle != nullptr);
	BOOST_CHECK_EQUAL(strcmp(p1->file, plugin.c_str()), 0);
	BOOST_CHECK_EQUAL(strcmp(p1->name, PLUGIN_1_NAME), 0);

	BOOST_CHECK_EQUAL(p1->ctrl_interface, nullptr);
	BOOST_CHECK_EQUAL(p1->readout_interface, nullptr);

	plugin = path + PLUGIN_2_LIB;
	p2 = kshark_register_plugin(kshark_ctx, PLUGIN_2_NAME, plugin.c_str());
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 2);
	BOOST_CHECK_EQUAL(kshark_ctx->plugins, p2);
	BOOST_CHECK_EQUAL(kshark_ctx->plugins->next, p1);
	BOOST_CHECK(p2 != nullptr);
	BOOST_CHECK(p2->process_interface != nullptr);
	BOOST_CHECK(p2->handle != nullptr);
	BOOST_CHECK_EQUAL(strcmp(p2->file, plugin.c_str()), 0);
	BOOST_CHECK_EQUAL(strcmp(p2->name, PLUGIN_2_NAME), 0);
	BOOST_CHECK(p2->ctrl_interface != nullptr);

	BOOST_CHECK_EQUAL(p2->readout_interface, nullptr);

	plugin = path + INPUT_A_LIB;
	i1 = kshark_register_plugin(kshark_ctx, INPUT_A_NAME, plugin.c_str());
	BOOST_CHECK(i1 != nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 3);
	BOOST_CHECK_EQUAL(kshark_ctx->n_inputs, 1);
	BOOST_CHECK(kshark_ctx->inputs != nullptr);
	BOOST_CHECK(i1->readout_interface != nullptr);
	BOOST_CHECK(i1->handle != nullptr);
	BOOST_CHECK_EQUAL(strcmp(i1->file, plugin.c_str()), 0);
	BOOST_CHECK_EQUAL(strcmp(i1->name, INPUT_A_NAME), 0);

	BOOST_CHECK_EQUAL(i1->ctrl_interface, nullptr);
	BOOST_CHECK_EQUAL(i1->process_interface, nullptr);

	plugin = path + INPUT_B_LIB;
	i2 = kshark_register_plugin(kshark_ctx, INPUT_B_NAME, plugin.c_str());
	BOOST_CHECK(i2 != nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 4);
	BOOST_CHECK_EQUAL(kshark_ctx->n_inputs, 2);
	BOOST_CHECK(i2->readout_interface != nullptr);
	BOOST_CHECK(i2->handle != nullptr);
	BOOST_CHECK(strcmp(i2->file, plugin.c_str()) == 0);
	BOOST_CHECK(strcmp(i2->name, INPUT_B_NAME) == 0);
	BOOST_CHECK(i2->ctrl_interface != nullptr);

	BOOST_CHECK_EQUAL(i2->process_interface, nullptr);

	x1 = kshark_find_plugin_by_name(kshark_ctx->plugins, PLUGIN_2_NAME);
	BOOST_CHECK_EQUAL(x1, p2);

	plugin = path + PLUGIN_2_LIB;
	x2 = kshark_find_plugin(kshark_ctx->plugins, plugin.c_str());

	BOOST_CHECK_EQUAL(x2, p2);

	sd = kshark_add_stream(kshark_ctx);
	interface =
		(kshark_generic_stream_interface *) malloc(sizeof(*interface));
	kshark_ctx->stream[sd]->interface = interface;
	BOOST_CHECK_EQUAL(sd, 0);

	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK(stream != nullptr);

	BOOST_CHECK_EQUAL(stream->plugins, nullptr);
	kshark_register_plugin_to_stream(stream,
					 p1->process_interface,
					 true);
	BOOST_CHECK_EQUAL(stream->n_plugins, 1);
	BOOST_CHECK_EQUAL(stream->plugins->interface, p1->process_interface);
	BOOST_CHECK_EQUAL(stream->plugins->next, nullptr);

	kshark_register_plugin_to_stream(stream,
					 p2->process_interface,
					 true);
	BOOST_CHECK_EQUAL(stream->n_plugins, 2);
	BOOST_CHECK_EQUAL(stream->plugins->interface, p2->process_interface);
	BOOST_CHECK_EQUAL(stream->plugins->next->interface, p1->process_interface);

	kshark_unregister_plugin_from_stream(stream, p1->process_interface);
	BOOST_CHECK_EQUAL(stream->n_plugins, 1);
	BOOST_CHECK_EQUAL(stream->plugins->interface, p2->process_interface);
	BOOST_CHECK_EQUAL(stream->plugins->next, nullptr);

	kshark_free(kshark_ctx);
}

#define PLUGIN_ERR_LIB	"/plugin-dummy_dpi_err.so"
#define PLUGIN_ERR_NAME	"dummy_dpi_err"

BOOST_AUTO_TEST_CASE(handle_plugin)
{
	kshark_dpi_list *dpi1, *dpi2, *dpi_err;
	kshark_plugin_list *p1, *p2, *p_err;
	kshark_context *kshark_ctx(nullptr);
	kshark_data_stream *stream;
	std::string plugin;
	int sd, ret;

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));
	BOOST_CHECK_EQUAL(kshark_ctx->plugins, nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 0);

	plugin = path + PLUGIN_1_LIB;
	p1 = kshark_register_plugin(kshark_ctx, PLUGIN_1_NAME, plugin.c_str());

	plugin = path + PLUGIN_2_LIB;
	p2 = kshark_register_plugin(kshark_ctx, PLUGIN_2_NAME, plugin.c_str());
	BOOST_CHECK(kshark_ctx->plugins != nullptr);
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 2);

	sd = kshark_add_stream(kshark_ctx);
	kshark_ctx->stream[sd]->interface = malloc(1);
	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK(stream != nullptr);

	dpi1 = kshark_register_plugin_to_stream(stream,
						p1->process_interface,
						true);
	BOOST_CHECK_EQUAL(dpi1->status, KSHARK_PLUGIN_ENABLED);

	dpi2 = kshark_register_plugin_to_stream(stream,
						p2->process_interface,
						false);
	BOOST_CHECK_EQUAL(dpi2->status, 0);

	ret = kshark_handle_dpi(stream, dpi1, KSHARK_PLUGIN_INIT);
	BOOST_CHECK_EQUAL(ret, 1);
	BOOST_CHECK_EQUAL(dpi1->status,
			  KSHARK_PLUGIN_LOADED | KSHARK_PLUGIN_ENABLED);

	ret = kshark_handle_dpi(stream, dpi2, KSHARK_PLUGIN_INIT);
	BOOST_CHECK_EQUAL(ret, 0);
	BOOST_CHECK_EQUAL(dpi2->status, 0);

	dpi2->status |= KSHARK_PLUGIN_ENABLED;
	ret = kshark_handle_dpi(stream, dpi2, KSHARK_PLUGIN_INIT);
	BOOST_CHECK_EQUAL(ret, 2);
	BOOST_CHECK_EQUAL(dpi1->status,
			  KSHARK_PLUGIN_LOADED | KSHARK_PLUGIN_ENABLED);

	ret = kshark_handle_all_dpis(stream, KSHARK_PLUGIN_UPDATE);
	BOOST_CHECK_EQUAL(ret, 0);
	BOOST_CHECK_EQUAL(dpi1->status,
			  KSHARK_PLUGIN_LOADED | KSHARK_PLUGIN_ENABLED);
	BOOST_CHECK_EQUAL(dpi2->status,
			  KSHARK_PLUGIN_LOADED | KSHARK_PLUGIN_ENABLED);

	plugin = path + PLUGIN_ERR_LIB;
	p_err = kshark_register_plugin(kshark_ctx, PLUGIN_ERR_NAME,
				       plugin.c_str());
	BOOST_CHECK_EQUAL(kshark_ctx->n_plugins, 3);
	dpi_err = kshark_register_plugin_to_stream(stream,
						   p_err->process_interface,
						   true);
	BOOST_CHECK_EQUAL(ret, 0);
	ret = kshark_handle_dpi(stream, dpi_err, KSHARK_PLUGIN_INIT);
	BOOST_CHECK_EQUAL(dpi_err->status,
			  KSHARK_PLUGIN_FAILED | KSHARK_PLUGIN_ENABLED);
	BOOST_CHECK_EQUAL(ret, 0);
	ret = kshark_handle_dpi(stream, dpi_err, KSHARK_PLUGIN_CLOSE);
	BOOST_CHECK_EQUAL(ret, 0);
	BOOST_CHECK_EQUAL(dpi_err->status, KSHARK_PLUGIN_ENABLED);

	ret = kshark_handle_all_dpis(stream, KSHARK_PLUGIN_CLOSE);
	BOOST_CHECK_EQUAL(ret, -3);

	kshark_free(kshark_ctx);
}

#define FAKE_DATA_FILE_A	"test.ta"
#define FAKE_DATA_A_SIZE	200

#define FAKE_DATA_FILE_B	"test.tb"
#define FAKE_DATA_B_SIZE	100

BOOST_AUTO_TEST_CASE(readout_plugins)
{
	kshark_context *kshark_ctx(nullptr);
	kshark_entry **entries{nullptr};
	kshark_data_stream *stream;
	std::string plugin, data;
	int sd, i, n_entries;
	int64_t ts_last(0);

	BOOST_REQUIRE(kshark_instance(&kshark_ctx));

	plugin = path + INPUT_A_LIB;
	kshark_register_plugin(kshark_ctx, INPUT_A_NAME, plugin.c_str());
	plugin = path + INPUT_B_LIB;
	kshark_register_plugin(kshark_ctx, INPUT_B_NAME, plugin.c_str());

	data = FAKE_DATA_FILE_A;
	sd = kshark_open(kshark_ctx, data.c_str());
	BOOST_CHECK_EQUAL(sd, 0);

	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK(stream != nullptr);
	BOOST_CHECK(stream->interface != nullptr);
	BOOST_CHECK_EQUAL(strcmp(stream->data_format, "format_a"), 0);

	data = FAKE_DATA_FILE_B;
	sd = kshark_open(kshark_ctx, data.c_str());
	BOOST_CHECK_EQUAL(sd, 1);

	stream = kshark_get_data_stream(kshark_ctx, sd);
	BOOST_CHECK(stream != nullptr);
	BOOST_CHECK(stream->interface != nullptr);
	BOOST_CHECK_EQUAL(strcmp(stream->data_format, "format_b"), 0);

	n_entries = kshark_load_all_entries(kshark_ctx, &entries);
	BOOST_CHECK_EQUAL(n_entries, FAKE_DATA_A_SIZE + FAKE_DATA_B_SIZE);

	for (i = 0; i < n_entries; ++i) {
		BOOST_CHECK(ts_last <= entries[i]->ts);
		ts_last = entries[i]->ts;
	}

	for (i = 0; i < n_entries; ++i)
		free(entries[i]);
	free(entries);

	kshark_free(kshark_ctx);
}

BOOST_AUTO_TEST_CASE(check_font_found)
{
#ifdef TT_FONT_FILE
BOOST_REQUIRE(true);
#else
BOOST_REQUIRE(false);
#endif
}
