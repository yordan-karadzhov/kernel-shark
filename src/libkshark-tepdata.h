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

void kshark_tracecmd_plugin_list_free(char **list);

struct tep_handle;

struct tep_handle *kshark_get_tep(struct kshark_data_stream *stream);

struct tracecmd_input;

struct tracecmd_input *kshark_get_tep_input(struct kshark_data_stream *stream);

struct tep_record;

ssize_t kshark_load_tep_records(struct kshark_context *kshark_ctx, int sd,
				struct tep_record ***data_rows);

/**
 * Structure representing the mapping between the virtual CPUs and their
 * corresponding processes in the host.
 */
struct kshark_host_guest_map {
	/** ID of guest stream */
	int guest_id;

	/** ID of host stream */
	int host_id;

	/** Guest name */
	char *guest_name;

	/** Number of guest's CPUs in *cpu_pid array */
	int vcpu_count;

	/** Array of host task PIDs, index is the VCPU id */
	int *cpu_pid;
};

void kshark_tracecmd_free_hostguest_map(struct kshark_host_guest_map *map,
					int count);

int kshark_tracecmd_get_hostguest_mapping(struct kshark_host_guest_map **map);

char **kshark_tep_get_buffer_names(struct kshark_context *kshark_ctx, int sd,
				   int *n_buffers);

int kshark_tep_open_buffer(struct kshark_context *kshark_ctx, int sd,
			   const char *buffer_name);

int kshark_tep_init_all_buffers(struct kshark_context *kshark_ctx, int sd);

int kshark_tep_handle_plugins(struct kshark_context *kshark_ctx, int sd);

int kshark_tep_find_top_stream(struct kshark_context *kshark_ctx,
			       const char *file);

bool kshark_tep_is_top_stream(struct kshark_data_stream *stream);

struct tep_event;

struct tep_format_field;

bool define_wakeup_event(struct tep_handle *tep,
			 struct tep_event **wakeup_event);

#ifdef __cplusplus
}
#endif

#endif // _KSHARK_TEPDATA_H
