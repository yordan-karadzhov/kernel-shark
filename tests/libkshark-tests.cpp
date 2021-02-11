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

#define N_TEST_STREAMS	1000

BOOST_AUTO_TEST_CASE(add_remove_streams)
{
	struct kshark_context *kshark_ctx = NULL;
	int sd, free = 0, i;

	kshark_instance(&kshark_ctx);

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

	kshark_close_all(kshark_ctx);
	kshark_free(kshark_ctx);
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
		BOOST_REQUIRE(data->data[i]->entry->ts >= ts_last);
		BOOST_CHECK_EQUAL(data->data[i]->entry->ts,
				  10 - data->data[i]->field);

		ts_last = data->data[i]->entry->ts;
	}

	i = kshark_find_entry_field_by_time(MAX_TS / 2, data->data,
					    0, N_VALUES - 1);

	BOOST_REQUIRE(data->data[i - 1]->entry->ts < MAX_TS / 2);
	BOOST_REQUIRE(data->data[i]->entry->ts >= MAX_TS / 2);

	kshark_free_data_container(data);
}

struct test_context {
	int a;
	char b;
};

KS_DEFINE_PLUGIN_CONTEXT(struct test_context);

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
