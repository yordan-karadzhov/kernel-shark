/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    libkshark-tepdata.h
 *  @brief   Interface for processing of FTRACE (trace-cmd) data.
 */

#ifndef _KSHARK_TEPDATA_H
#define _KSHARK_TEPDATA_H

// KernelShark
#include "libkshark.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Unique identifier of the TEP tracing data format. */
#define TEP_DATA_FORMAT_IDENTIFIER	"tep data"

/** Check if this Data stream corresponds to a TEP tracing data. */
static inline bool kshark_is_tep(struct kshark_data_stream *stream)
{
	return strcmp(stream->data_format, TEP_DATA_FORMAT_IDENTIFIER) == 0;
}

bool kshark_tep_check_data(const char *file_name);

int kshark_tep_init_input(struct kshark_data_stream *stream);

int kshark_tep_init_local(struct kshark_data_stream *stream);

int kshark_tep_close_interface(struct kshark_data_stream *stream);

bool kshark_tep_filter_is_set(struct kshark_data_stream *stream);

int kshark_tep_add_filter_str(struct kshark_data_stream *stream,
			      const char *filter_str);

char *kshark_tep_filter_make_string(struct kshark_data_stream *stream,
				    int event_id);

int kshark_tep_filter_remove_event(struct kshark_data_stream *stream,
				   int event_id);

void kshark_tep_filter_reset(struct kshark_data_stream *stream);

char **kshark_tracecmd_local_plugins();

ssize_t kshark_load_tep_records(struct kshark_context *kshark_ctx, int sd,
				struct tep_record ***data_rows);

#ifdef __cplusplus
}
#endif

#endif // _KSHARK_TEPDATA_H
