// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2020 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

// Boost
#define BOOST_TEST_MODULE KernelSharkTests
#include <boost/test/unit_test.hpp>

// KernelShark
#include "libkshark.h"

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
