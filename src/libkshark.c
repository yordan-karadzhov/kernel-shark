// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

 /**
 *  @file    libkshark.c
 *  @brief   API for processing of FTRACE (trace-cmd) data.
 */

/** Use GNU C Library. */
#define _GNU_SOURCE 1

// C
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-tepdata.h"

static __thread struct trace_seq seq;

static struct kshark_context *kshark_context_handler = NULL;

static bool kshark_default_context(struct kshark_context **context)
{
	struct kshark_context *kshark_ctx;

	kshark_ctx = calloc(1, sizeof(*kshark_ctx));
	if (!kshark_ctx)
		return false;

	kshark_ctx->stream = calloc(KS_DEFAULT_NUM_STREAMS,
				    sizeof(*kshark_ctx->stream));

	kshark_ctx->event_handlers = NULL;
	kshark_ctx->collections = NULL;
	kshark_ctx->plugins = NULL;

	kshark_ctx->show_task_filter = tracecmd_filter_id_hash_alloc();
	kshark_ctx->hide_task_filter = tracecmd_filter_id_hash_alloc();

	kshark_ctx->show_event_filter = tracecmd_filter_id_hash_alloc();
	kshark_ctx->hide_event_filter = tracecmd_filter_id_hash_alloc();

	kshark_ctx->show_cpu_filter = tracecmd_filter_id_hash_alloc();
	kshark_ctx->hide_cpu_filter = tracecmd_filter_id_hash_alloc();

	kshark_ctx->filter_mask = 0x0;

	kshark_ctx->stream_info.array_size = KS_DEFAULT_NUM_STREAMS;
	kshark_ctx->stream_info.max_stream_id = -1;

	/* Will free kshark_context_handler. */
	kshark_free(NULL);

	/* Will do nothing if *context is NULL. */
	kshark_free(*context);

	*context = kshark_context_handler = kshark_ctx;

	return true;
}

static bool init_thread_seq(void)
{
	if (!seq.buffer)
		trace_seq_init(&seq);

	return seq.buffer != NULL;
}

/**
 * @brief Initialize a kshark session. This function must be called before
 *	  calling any other kshark function. If the session has been
 *	  initialized, this function can be used to obtain the session's
 *	  context.
 *
 * @param kshark_ctx: Optional input/output location for context pointer.
 *		      If it points to a context, that context will become
 *		      the new session. If it points to NULL, it will obtain
 *		      the current (or new) session. The result is only
 *		      valid on return of true.
 *
 * @returns True on success, or false on failure.
 */
bool kshark_instance(struct kshark_context **kshark_ctx)
{
	if (*kshark_ctx != NULL) {
		/* Will free kshark_context_handler */
		kshark_free(NULL);

		/* Use the context provided by the user. */
		kshark_context_handler = *kshark_ctx;
	} else {
		if (kshark_context_handler) {
			/*
			 * No context is provided by the user, but the context
			 * handler is already set. Use the context handler.
			 */
			*kshark_ctx = kshark_context_handler;
		} else {
			/* No kshark_context exists. Create a default one. */
			if (!kshark_default_context(kshark_ctx))
				return false;
		}
	}

	if (!init_thread_seq())
		return false;

	return true;
}

/**
 * @brief Open and prepare for reading a trace data file specified by "file".
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param file: The file to load.
 *
 * @returns The Id number of the data stream associated with this file on success.
 * 	    Otherwise a negative errno code.
 */
int kshark_open(struct kshark_context *kshark_ctx, const char *file)
{
	int sd, rt;

	sd = kshark_add_stream(kshark_ctx);
	if (sd < 0)
		return sd;

	rt = kshark_stream_open(kshark_ctx->stream[sd], file);
	if (rt < 0) {
		kshark_remove_stream(kshark_ctx, sd);
		return rt;
	}

	return sd;
}

static void kshark_stream_free(struct kshark_data_stream *stream)
{
	if (!stream)
		return;

	kshark_hash_id_free(stream->show_task_filter);
	kshark_hash_id_free(stream->hide_task_filter);

	kshark_hash_id_free(stream->show_event_filter);
	kshark_hash_id_free(stream->hide_event_filter);

	kshark_hash_id_free(stream->show_cpu_filter);
	kshark_hash_id_free(stream->hide_cpu_filter);

	kshark_hash_id_free(stream->tasks);

	free(stream->file);
	free(stream->name);
	free(stream->interface);
	free(stream);
}

static struct kshark_data_stream *kshark_stream_alloc()
{
	struct kshark_data_stream *stream;

	stream = calloc(1, sizeof(*stream));
	if (!stream)
		goto fail;

	stream->show_task_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);
	stream->hide_task_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);

	stream->show_event_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);
	stream->hide_event_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);

	stream->show_cpu_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);
	stream->hide_cpu_filter = kshark_hash_id_alloc(KS_FILTER_HASH_NBITS);

	stream->tasks = kshark_hash_id_alloc(KS_TASK_HASH_NBITS);

	if (!stream->show_task_filter ||
	    !stream->hide_task_filter ||
	    !stream->show_event_filter ||
	    !stream->hide_event_filter ||
	    !stream->tasks) {
		    goto fail;
	}

	kshark_set_data_format(stream->data_format, KS_INVALID_DATA);
	stream->name = strdup(KS_UNNAMED);

	return stream;

 fail:
	kshark_stream_free(stream);

	return NULL;
}
/**
 * The maximum number of Data streams that can be added simultaneously.
 * The limit is determined by the 16 bit integer used to store the stream Id
 * inside struct kshark_entry.
 */
#define KS_MAX_STREAM_ID	INT16_MAX

/**
 * Bit mask (0 - 15) used when converting indexes to pointers and vise-versa.
 */
#define INDEX_MASK		UINT16_MAX

/**
 * Bit mask (16 - 31/63) used when converting indexes to pointers and
 * vise-versa.
 */
#define INVALID_STREAM_MASK	(~((unsigned long) INDEX_MASK))

static int index_from_ptr(void *ptr)
{
	unsigned long index = (unsigned long) ptr;

	return (int) (index & INDEX_MASK);
}

static void *index_to_ptr(unsigned int index)
{
	unsigned long p;

	p = INVALID_STREAM_MASK | index;

	return (void *) p;
}

static bool kshark_is_valid_stream(void *ptr)
{
	unsigned long p = (unsigned long) ptr;
	bool v = !((p & ~INDEX_MASK) == INVALID_STREAM_MASK);

	return p && v;
}

/**
 * @brief Add new Data stream.
 *
 * @param kshark_ctx: Input location for context pointer.
 *
 * @returns Zero on success or a negative errno code on failure.
 */
int kshark_add_stream(struct kshark_context *kshark_ctx)
{
	struct kshark_data_stream *stream;
	int new_stream;

	if(kshark_ctx->stream_info.next_free_stream_id > KS_MAX_STREAM_ID)
		return -ENODEV;

	if (kshark_ctx->stream_info.next_free_stream_id ==
	    kshark_ctx->stream_info.array_size) {
		size_t new_size = 2 * kshark_ctx->stream_info.array_size;
		struct kshark_data_stream **streams_tmp;

		streams_tmp = realloc(kshark_ctx->stream,
				      new_size * sizeof(*kshark_ctx->stream));
		if (!streams_tmp)
			return -ENOMEM;

		kshark_ctx->stream = streams_tmp;
		kshark_ctx->stream_info.array_size = new_size;
	}

	stream = kshark_stream_alloc();
	if (!stream)
		return -ENOMEM;

	if (pthread_mutex_init(&stream->input_mutex, NULL) != 0) {
		kshark_stream_free(stream);
		return -EAGAIN;
	}

	if (kshark_ctx->stream_info.next_free_stream_id >
	    kshark_ctx->stream_info.max_stream_id) {
		new_stream = ++kshark_ctx->stream_info.max_stream_id;

		kshark_ctx->stream_info.next_free_stream_id = new_stream + 1;

		kshark_ctx->stream[new_stream] = stream;
		stream->stream_id = new_stream;
	} else {
		new_stream = kshark_ctx->stream_info.next_free_stream_id;

		kshark_ctx->stream_info.next_free_stream_id =
			index_from_ptr(kshark_ctx->stream[new_stream]);

		kshark_ctx->stream[new_stream] = stream;
		stream->stream_id = new_stream;
	}

	kshark_ctx->n_streams++;

	return stream->stream_id;
}

/**
 * @brief Use an existing Trace data stream to open and prepare for reading
 *	  a trace data file specified by "file".
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param file: The file to load.
 *
 * @returns Zero on success or a negative error code in the case of an errno.
 */
int kshark_stream_open(struct kshark_data_stream *stream, const char *file)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!stream || !kshark_instance(&kshark_ctx))
		return -EFAULT;

	stream->file = strdup(file);
	if (!stream->file)
		return -ENOMEM;

	if (kshark_tep_check_data(file)) {
		kshark_set_data_format(stream->data_format,
				       TEP_DATA_FORMAT_IDENTIFIER);

		return kshark_tep_init_input(stream);
	}

	return -ENODATA;
}

/**
 * @brief Remove Data stream.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier.
 *
 * @returns Zero on success or a negative errno code on failure.
 */
int kshark_remove_stream(struct kshark_context *kshark_ctx, int sd)
{
	struct kshark_data_stream *stream;

	if (sd < 0 ||
	    sd > kshark_ctx->stream_info.max_stream_id ||
	    !kshark_is_valid_stream(kshark_ctx->stream[sd]))
		return -EFAULT;

	stream = kshark_ctx->stream[sd];

	pthread_mutex_destroy(&stream->input_mutex);

	kshark_stream_free(stream);
	kshark_ctx->stream[sd] =
		index_to_ptr(kshark_ctx->stream_info.next_free_stream_id);
	kshark_ctx->stream_info.next_free_stream_id = sd;
	kshark_ctx->n_streams--;

	return 0;
}

/**
 * @brief Get the Data stream object having given Id.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier.
 *
 * @returns Pointer to a Data stream object if the sream exists. Otherwise
 *	    NULL.
 */
struct kshark_data_stream *
kshark_get_data_stream(struct kshark_context *kshark_ctx, int sd)
{
	if (sd >= 0 && sd <= kshark_ctx->stream_info.max_stream_id)
		if (kshark_ctx->stream[sd] &&
		    kshark_is_valid_stream(kshark_ctx->stream[sd]) &&
		    kshark_ctx->stream[sd]->interface)
			return kshark_ctx->stream[sd];

	return NULL;
}

/**
 * @brief Get the Data stream object corresponding to a given entry
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns Pointer to a Data stream object on success. Otherwise NULL.
 */
struct kshark_data_stream *
kshark_get_stream_from_entry(const struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!kshark_instance(&kshark_ctx))
		return NULL;

	return kshark_get_data_stream(kshark_ctx, entry->stream_id);
}

/**
 * @brief Get an array containing the Ids of all opened Trace data streams.
 * 	  The User is responsible for freeing the array.
 *
 * @param kshark_ctx: Input location for context pointer.
 */
int *kshark_all_streams(struct kshark_context *kshark_ctx)
{
	int *ids, i, count = 0;

	ids = calloc(kshark_ctx->n_streams, (sizeof(*ids)));
	if (!ids)
		return NULL;

	for (i = 0; i <= kshark_ctx->stream_info.max_stream_id; ++i)
		if (kshark_ctx->stream[i] &&
		    kshark_is_valid_stream(kshark_ctx->stream[i]))
			ids[count++] = i;

	return ids;
}

static int kshark_stream_close(struct kshark_data_stream *stream)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!stream || !kshark_instance(&kshark_ctx))
		return -EFAULT;

	/*
	 * All filters are file specific. Make sure that all Process Ids and
	 * Event Ids from this file are not going to be used with another file.
	 */
	kshark_hash_id_clear(stream->show_task_filter);
	kshark_hash_id_clear(stream->hide_task_filter);
	kshark_hash_id_clear(stream->show_event_filter);
	kshark_hash_id_clear(stream->hide_event_filter);
	kshark_hash_id_clear(stream->show_cpu_filter);
	kshark_hash_id_clear(stream->hide_cpu_filter);

	if (kshark_is_tep(stream))
		return kshark_tep_close_interface(stream);

	return -ENODATA;
}

/**
 * @brief Close the trace data file and free the trace data handle.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param sd: Data stream identifier.
 *
 * @returns Zero on success or a negative errno code on failure.
 */
int kshark_close(struct kshark_context *kshark_ctx, int sd)
{
	struct kshark_data_stream *stream;
	int ret;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return -EFAULT;

	ret = kshark_stream_close(stream);
	kshark_remove_stream(kshark_ctx, stream->stream_id);

	return ret;
}

/**
 * @brief Close all currently open trace data file and free the trace data handle.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 */
void kshark_close_all(struct kshark_context *kshark_ctx)
{
	int i, *stream_ids, n_streams;

	stream_ids = kshark_all_streams(kshark_ctx);

	/*
	 * Get a copy of shark_ctx->n_streams befor you start closing. Be aware
	 * that kshark_close() will decrement shark_ctx->n_streams.
	 */
	n_streams = kshark_ctx->n_streams;
	for (i = 0; i < n_streams; ++i)
		kshark_close(kshark_ctx, stream_ids[i]);

	free(stream_ids);
}

/**
 * @brief Deinitialize kshark session. Should be called after closing all
 *	  open trace data files and before your application terminates.
 *
 * @param kshark_ctx: Optional input location for session context pointer.
 *		      If it points to a context of a session, that session
 *		      will be deinitialize. If it points to NULL, it will
 *		      deinitialize the current session.
 */
void kshark_free(struct kshark_context *kshark_ctx)
{
	if (kshark_ctx == NULL) {
		if (kshark_context_handler == NULL)
			return;

		kshark_ctx = kshark_context_handler;
		/* kshark_ctx_handler will be set to NULL below. */
	}

	kshark_close_all(kshark_ctx);

	free(kshark_ctx->stream);

	if (kshark_ctx->plugins)
		kshark_free_plugin_list(kshark_ctx->plugins);

	if (kshark_ctx == kshark_context_handler)
		kshark_context_handler = NULL;

	free(kshark_ctx);
}

/**
 * @brief Get the name of the command/task from its Process Id.
 *
 * @param sd: Data stream identifier.
 * @param pid: Process Id of the command/task.
 */
char *kshark_comm_from_pid(int sd, int pid)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_data_stream *stream;
	struct kshark_entry e;

	if (!kshark_instance(&kshark_ctx))
		return NULL;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_task) {
		e.visible = KS_PLUGIN_UNTOUCHED_MASK;
		e.pid = pid;

		return interface->get_task(stream, &e);
	}

	return NULL;
}

/**
 * @brief Get the name of the event from its Id.
 *
 * @param sd: Data stream identifier.
 * @param event_id: The unique Id of the event type.
 */
char *kshark_event_from_id(int sd, int event_id)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_data_stream *stream;
	struct kshark_entry e;

	if (!kshark_instance(&kshark_ctx))
		return NULL;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_event_name) {
		e.visible = KS_PLUGIN_UNTOUCHED_MASK;
		e.event_id = event_id;

		return interface->get_event_name(stream, &e);
	}

	return NULL;
}

/**
 * @brief Get the original process Id of the entry. Using this function make
 *	  sense only in cases when the original value can be overwritten by
 *	  plugins. If you know that no plugins are loaded use "entry->pid"
 *	  directly.
 *
 * @param entry: Input location for an entry.
 */
int kshark_get_pid(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_pid)
		return interface->get_pid(stream, entry);

	return -EFAULT;
}

/**
 * @brief Get the original event Id of the entry. Using this function make
 *	  sense only in cases when the original value can be overwritten by
 *	  plugins. If you know that no plugins are loaded use "entry->event_id"
 *	  directly.
 *
 * @param entry: Input location for an entry.
 */
int kshark_get_event_id(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_event_id)
		return interface->get_event_id(stream, entry);

	return -EFAULT;
}

/**
 * @brief Get an array of all event Ids for a given data stream.
 *
 * @param stream: Input location for a Trace data stream pointer.
 *
 * @returns An array of event Ids. The user is responsible for freeing the
 *	    outputted array.
 */
int *kshark_get_all_event_ids(struct kshark_data_stream *stream)
{
	struct kshark_generic_stream_interface *interface;

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_all_event_ids)
		return interface->get_all_event_ids(stream);

	return NULL;
}

/**
 * @brief Find the event Ids corresponding to a given event name.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param event_name: The name of the event.
 *
 * @returns Event Ids number.
 */
int kshark_find_event_id(struct kshark_data_stream *stream,
			 const char *event_name)
{
	struct kshark_generic_stream_interface *interface;

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->find_event_id)
		return interface->find_event_id(stream, event_name);

	return -EFAULT;
}

/**
 * @brief Find the event name corresponding to a given entry.
 *
 * @param entry: Input location for an entry.
 *
 * @returns The mane of the event on success, or NULL in case of failure.
 *	    The use is responsible for freeing the output string.
 */
char *kshark_get_event_name(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_event_name)
		return interface->get_event_name(stream, entry);

	return NULL;
}

/**
 * @brief Find the task name corresponding to a given entry.
 *
 * @param entry: Input location for an entry.
 *
 * @returns The mane of the task on success, or NULL in case of failure.
 *	    The use is responsible for freeing the output string.
 */
char *kshark_get_task(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_task)
		return interface->get_task(stream, entry);

	return NULL;
}

/**
 * @brief Get the basic information (text) about the entry.
 *
 * @param entry: Input location for an entry.
 *
 * @returns A the info text. The user is responsible for freeing the
 *	    outputted string.
 */
char *kshark_get_info(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_info)
		return interface->get_info(stream, entry);

	return NULL;
}

/**
 * @brief Get the auxiliary information about the entry. In the case of
 * 	  TEP (Ftrace) data, this function provides the latency info.
 *
 * @param entry: Input location for an entry.
 *
 * @returns A the auxiliary text info. The user is responsible for freeing the
 *	    outputted string.
 */
char *kshark_get_aux_info(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->aux_info)
		return interface->aux_info(stream, entry);

	return NULL;
}

/**
 * @brief Get an array of all data field names associated with a given entry.
 *
 * @param entry: Input location for an entry.
 * @param fields: Output location of the array of field names. The user is
 *		  responsible for freeing the elements of the outputted array.
 *
 * @returns Total number of event fields on success, or a negative errno in
 *	    the case of a failure.
 */
int kshark_get_all_event_field_names(const struct kshark_entry *entry,
				     char ***fields)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_all_event_field_names)
		return interface->get_all_event_field_names(stream,
							    entry, fields);

	return -EFAULT;
}

/**
 * @brief Get the value type of an event field corresponding to a given entry.
 *
 * @param entry: Input location for an entry.
 * @param field: The name of the data field.
 *
 * @returns The type of the data field on success, or KS_INVALID_FIELD in
 *	    the case of a failure.
 */
kshark_event_field_format
kshark_get_event_field_type(const struct kshark_entry *entry,
			    const char *field)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return KS_INVALID_FIELD;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->get_event_field_type)
		return interface->get_event_field_type(stream, entry, field);

	return KS_INVALID_FIELD;
}

/**
 * @brief Get the value of an event field in a given trace record.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param rec: Input location for a record.
 * @param field: The name of the data field.
 * @param val: Output location for the value of the field.
 *
 * @returns Zero on success or a negative errno in the case of a failure.
 */
int kshark_read_record_field_int(struct kshark_data_stream *stream, void *rec,
				 const char *field, int64_t *val)
{
	struct kshark_generic_stream_interface *interface;

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->read_record_field_int64)
		return interface->read_record_field_int64(stream, rec,
							  field, val);

	return -EFAULT;
}

/**
 * @brief Get the value of an event field corresponding to a given entry.
 *	  The value is retrieved via the offset in the file of the original
 *	  record.
 *
 * @param entry: Input location for an entry.
 * @param field: The name of the data field.
 * @param val: Output location for the value of the field.
 *
 * @returns Zero on success or a negative errno in the case of a failure.
 */
int kshark_read_event_field_int(const struct kshark_entry *entry,
				const char* field, int64_t *val)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->read_event_field_int64)
		return interface->read_event_field_int64(stream, entry, field, val);

	return -EFAULT;
}

/**
 * @brief Get a summary of the entry.
 *
 * @param entry: Input location for an entry.
 *
 * @returns A summary text info. The user is responsible for freeing the
 *	    outputted string.
 */
char *kshark_dump_entry(const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_stream_from_entry(entry);

	if (!stream)
		return NULL;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->dump_entry)
		return interface->dump_entry(stream, entry);

	return NULL;
}

/** @brief Print the entry. */
void kshark_print_entry(const struct kshark_entry *entry)
{
	char *entry_str = kshark_dump_entry(entry);

	if (!entry_str)
		puts("(nil)");

	puts(entry_str);
	free(entry_str);
}

/**
 * @brief Load the content of the trace data file asociated with a given
 *	  Data stream into an array of kshark_entries.
 *	  If one or more filters are set, the "visible" fields of each entry
 *	  is updated according to the criteria provided by the filters. The
 *	  field "filter_mask" of the session's context is used to control the
 *	  level of visibility/invisibility of the filtered entries.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier.
 * @param data_rows: Output location for the trace data. The user is
 *		     responsible for freeing the elements of the outputted
 *		     array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t kshark_load_entries(struct kshark_context *kshark_ctx, int sd,
			    struct kshark_entry ***data_rows)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->load_entries)
		return interface->load_entries(stream, kshark_ctx, data_rows);

	return -EFAULT;
}

/**
 * @brief Load the content of the trace data file asociated with a given
 *	  Data stream into a data matrix. The user is responsible
 *	  for freeing the outputted data.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier.
 * @param cpu_array: Output location for the CPU column (array) of the matrix.
 * @param event_array: Output location for the Event Id column (array) of the
 *		       matrix.
 * @param pid_array: Output location for the PID column (array) of the matrix.
 * @param offset_array: Output location for the offset column (array) of the
 *			matrix.
 * @param ts_array: Output location for the time stamp column (array) of the
 *		    matrix.
 */
ssize_t kshark_load_matrix(struct kshark_context *kshark_ctx, int sd,
			   int16_t **event_array,
			   int16_t **cpu_array,
			   int32_t **pid_array,
			   int64_t **offset_array,
			   int64_t **ts_array)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);

	if (!stream)
		return -EFAULT;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->load_matrix)
		return interface->load_matrix(stream, kshark_ctx, event_array,
								  cpu_array,
								  pid_array,
								  offset_array,
								  ts_array);

	return -EFAULT;
}

static struct kshark_task_list *
kshark_find_task(struct kshark_context *kshark_ctx, uint32_t key, int pid)
{
	struct kshark_task_list *list;

	for (list = kshark_ctx->tasks[key]; list; list = list->next) {
		if (list->pid == pid)
			return list;
	}

	return NULL;
}

static struct kshark_task_list *
kshark_add_task(struct kshark_context *kshark_ctx, int pid)
{
	struct kshark_task_list *list;
	uint32_t key;

	key = tracecmd_quick_hash(pid, KS_TASK_HASH_SHIFT);

	list = kshark_find_task(kshark_ctx, key, pid);
	if (list)
		return list;

	list = malloc(sizeof(*list));
	if (!list)
		return NULL;

	list->pid = pid;
	list->next = kshark_ctx->tasks[key];
	kshark_ctx->tasks[key] = list;

	return list;
}

/**
 * @brief Get an array containing the Process Ids of all tasks presented in
 *	  the loaded trace data file.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier.
 * @param pids: Output location for the Pids of the tasks. The user is
 *		responsible for freeing the elements of the outputted array.
 *
 * @returns The size of the outputted array of Pids in the case of success,
 *	    or a negative error code on failure.
 */
ssize_t kshark_get_task_pids(struct kshark_context *kshark_ctx, int sd,
			     int **pids)
{
	struct kshark_data_stream *stream;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return -EBADF;

	*pids = kshark_hash_ids(stream->tasks);
	return stream->tasks->count;
}

static bool filter_find(struct tracecmd_filter_id *filter, int pid,
			bool test)
{
	return !filter || !filter->count ||
		!!(unsigned long)tracecmd_filter_id_find(filter, pid) == test;
}

static bool kshark_show_task(struct kshark_context *kshark_ctx, int pid)
{
	return filter_find(kshark_ctx->show_task_filter, pid, true) &&
	       filter_find(kshark_ctx->hide_task_filter, pid, false);
}

static bool kshark_show_event(struct kshark_context *kshark_ctx, int pid)
{
	return filter_find(kshark_ctx->show_event_filter, pid, true) &&
	       filter_find(kshark_ctx->hide_event_filter, pid, false);
}

static bool kshark_show_cpu(struct kshark_context *kshark_ctx, int cpu)
{
	return filter_find(kshark_ctx->show_cpu_filter, cpu, true) &&
	       filter_find(kshark_ctx->hide_cpu_filter, cpu, false);
}

/**
 * @brief Add an Id value to the filster specified by "filter_id".
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param filter_id: Identifier of the filter.
 * @param id: Id value to be added to the filter.
 */
void kshark_filter_add_id(struct kshark_context *kshark_ctx,
			  int filter_id, int id)
{
	struct tracecmd_filter_id *filter;

	switch (filter_id) {
		case KS_SHOW_CPU_FILTER:
			filter = kshark_ctx->show_cpu_filter;
			break;
		case KS_HIDE_CPU_FILTER:
			filter = kshark_ctx->hide_cpu_filter;
			break;
		case KS_SHOW_EVENT_FILTER:
			filter = kshark_ctx->show_event_filter;
			break;
		case KS_HIDE_EVENT_FILTER:
			filter = kshark_ctx->hide_event_filter;
			break;
		case KS_SHOW_TASK_FILTER:
			filter = kshark_ctx->show_task_filter;
			break;
		case KS_HIDE_TASK_FILTER:
			filter = kshark_ctx->hide_task_filter;
			break;
		default:
			return;
	}

	tracecmd_filter_id_add(filter, id);
}

/**
 * @brief Clear (reset) the filster specified by "filter_id".
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param filter_id: Identifier of the filter.
 */
void kshark_filter_clear(struct kshark_context *kshark_ctx, int filter_id)
{
	struct tracecmd_filter_id *filter;

	switch (filter_id) {
		case KS_SHOW_CPU_FILTER:
			filter = kshark_ctx->show_cpu_filter;
			break;
		case KS_HIDE_CPU_FILTER:
			filter = kshark_ctx->hide_cpu_filter;
			break;
		case KS_SHOW_EVENT_FILTER:
			filter = kshark_ctx->show_event_filter;
			break;
		case KS_HIDE_EVENT_FILTER:
			filter = kshark_ctx->hide_event_filter;
			break;
		case KS_SHOW_TASK_FILTER:
			filter = kshark_ctx->show_task_filter;
			break;
		case KS_HIDE_TASK_FILTER:
			filter = kshark_ctx->hide_task_filter;
			break;
		default:
			return;
	}

	tracecmd_filter_id_clear(filter);
}

/**
 * @brief Check if a given Id filter is set.
 *
 * @param filter: Input location for the Id filster.
 *
 * @returns True if the Id filter is set, otherwise False.
 */
bool kshark_this_filter_is_set(struct tracecmd_filter_id *filter)
{
	return filter && filter->count;
}

/**
 * @brief Check if an Id filter is set.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 *
 * @returns True if at least one Id filter is set, otherwise False.
 */
bool kshark_filter_is_set(struct kshark_context *kshark_ctx)
{
	return kshark_this_filter_is_set(kshark_ctx->show_task_filter) ||
-              kshark_this_filter_is_set(kshark_ctx->hide_task_filter) ||
-              kshark_this_filter_is_set(kshark_ctx->show_cpu_filter) ||
-              kshark_this_filter_is_set(kshark_ctx->hide_cpu_filter) ||
-              kshark_this_filter_is_set(kshark_ctx->show_event_filter) ||
-              kshark_this_filter_is_set(kshark_ctx->hide_event_filter);
}

static void set_all_visible(uint16_t *v) {
	/*  Keep the original value of the PLUGIN_UNTOUCHED bit flag. */
	*v |= 0xFF & ~KS_PLUGIN_UNTOUCHED_MASK;
}

/**
 * @brief This function loops over the array of entries specified by "data"
 *	  and "n_entries" and sets the "visible" fields of each entry
 *	  according to the criteria provided by the filters of the session's
 *	  context. The field "filter_mask" of the session's context is used to
 *	  control the level of visibility/invisibility of the entries which
 *	  are filtered-out.
 *	  WARNING: Do not use this function if the advanced filter is set.
 *	  Applying the advanced filter requires access to prevent_record,
 *	  hence the data has to be reloaded using kshark_load_data_entries().
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param data: Input location for the trace data to be filtered.
 * @param n_entries: The size of the inputted data.
 */
void kshark_filter_entries(struct kshark_context *kshark_ctx,
			   struct kshark_entry **data,
			   size_t n_entries)
{
	int i;

	if (kshark_ctx->advanced_event_filter->filters) {
		/* The advanced filter is set. */
		fprintf(stderr,
			"Failed to filter!\n");
		fprintf(stderr,
			"Reset the Advanced filter or reload the data.\n");
		return;
	}

	if (!kshark_filter_is_set(kshark_ctx))
		return;

	/* Apply only the Id filters. */
	for (i = 0; i < n_entries; ++i) {
		/* Start with and entry which is visible everywhere. */
		set_all_visible(&data[i]->visible);

		/* Apply event filtering. */
		if (!kshark_show_event(kshark_ctx, data[i]->event_id))
			unset_event_filter_flag(kshark_ctx, data[i]);

		/* Apply CPU filtering. */
		if (!kshark_show_cpu(kshark_ctx, data[i]->cpu))
			data[i]->visible &= ~kshark_ctx->filter_mask;

		/* Apply task filtering. */
		if (!kshark_show_task(kshark_ctx, data[i]->pid))
			data[i]->visible &= ~kshark_ctx->filter_mask;
	}
}

/**
 * @brief This function loops over the array of entries specified by "data"
 *	  and "n_entries" and resets the "visible" fields of each entry to
 *	  the default value of "0xFF" (visible everywhere).
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param data: Input location for the trace data to be unfiltered.
 * @param n_entries: The size of the inputted data.
 */
void kshark_clear_all_filters(struct kshark_context *kshark_ctx,
			      struct kshark_entry **data,
			      size_t n_entries)
{
	int i;
	for (i = 0; i < n_entries; ++i)
		set_all_visible(&data[i]->visible);
}

static void kshark_set_entry_values(struct kshark_context *kshark_ctx,
				    struct tep_record *record,
				    struct kshark_entry *entry)
{
	/* Offset of the record */
	entry->offset = record->offset;

	/* CPU Id of the record */
	entry->cpu = record->cpu;

	/* Time stamp of the record */
	entry->ts = record->ts;

	/* Event Id of the record */
	entry->event_id = tep_data_type(kshark_ctx->pevent, record);

	/*
	 * Is visible mask. This default value means that the entry
	 * is visible everywhere.
	 */
	entry->visible = 0xFF;

	/* Process Id of the record */
	entry->pid = tep_data_pid(kshark_ctx->pevent, record);
}

/** Prior time offset of the "missed_events" entry. */
#define ME_ENTRY_TIME_SHIFT	10

static void missed_events_action(struct kshark_context *kshark_ctx,
				 struct tep_record *record,
				 struct kshark_entry *entry)
{
	/*
	 * Use the offset field of the entry to store the number of missed
	 * events.
	 */
	entry->offset = record->missed_events;

	entry->cpu = record->cpu;

	/*
	 * Position the "missed_events" entry a bit before (in time)
	 * the original record.
	 */
	entry->ts = record->ts - ME_ENTRY_TIME_SHIFT;

	/* All custom entries must have negative event Identifiers. */
	entry->event_id = KS_EVENT_OVERFLOW;

	entry->visible = 0xFF;

	entry->pid = tep_data_pid(kshark_ctx->pevent, record);
}

static const char* missed_events_dump(struct kshark_context *kshark_ctx,
				      const struct kshark_entry *entry,
				      bool get_info)
{
	int size = 0;
	static char *buffer;

	if (get_info)
		size = asprintf(&buffer, "missed_events=%i", (int) entry->offset);
	else
		size = asprintf(&buffer, "missed_events");
	if (size > 0)
		return buffer;

	return NULL;
}

/**
 * rec_list is used to pass the data to the load functions.
 * The rec_list will contain the list of entries from the source,
 * and will be a link list of per CPU entries.
 */
struct rec_list {
	union {
		/* Used by kshark_load_data_records */
		struct {
			/** next pointer, matches entry->next */
			struct rec_list		*next;
			/** pointer to the raw record data */
			struct tep_record	*rec;
		};
		/** entry - Used for kshark_load_data_entries() */
		struct kshark_entry		entry;
	};
};

/**
 * rec_type defines what type of rec_list is being used.
 */
enum rec_type {
	REC_RECORD,
	REC_ENTRY,
};

static void free_rec_list(struct rec_list **rec_list, int n_cpus,
			  enum rec_type type)
{
	struct rec_list *temp_rec;
	int cpu;

	for (cpu = 0; cpu < n_cpus; ++cpu) {
		while (rec_list[cpu]) {
			temp_rec = rec_list[cpu];
			rec_list[cpu] = temp_rec->next;
			if (type == REC_RECORD)
				free_record(temp_rec->rec);
			free(temp_rec);
		}
	}
	free(rec_list);
}

static ssize_t get_records(struct kshark_context *kshark_ctx,
			   struct rec_list ***rec_list, enum rec_type type)
{
	struct kshark_event_handler *evt_handler;
	struct tep_event_filter *adv_filter;
	struct kshark_task_list *task;
	struct tep_record *rec;
	struct rec_list **temp_next;
	struct rec_list **cpu_list;
	struct rec_list *temp_rec;
	size_t count, total = 0;
	int n_cpus;
	int pid;
	int cpu;

	n_cpus = tep_get_cpus(kshark_ctx->pevent);
	cpu_list = calloc(n_cpus, sizeof(*cpu_list));
	if (!cpu_list)
		return -ENOMEM;

	/* Just to shorten the name */
	if (type == REC_ENTRY)
		adv_filter = kshark_ctx->advanced_event_filter;

	for (cpu = 0; cpu < n_cpus; ++cpu) {
		count = 0;
		cpu_list[cpu] = NULL;
		temp_next = &cpu_list[cpu];

		rec = tracecmd_read_cpu_first(kshark_ctx->handle, cpu);
		while (rec) {
			*temp_next = temp_rec = calloc(1, sizeof(*temp_rec));
			if (!temp_rec)
				goto fail;

			temp_rec->next = NULL;

			switch (type) {
			case REC_RECORD:
				temp_rec->rec = rec;
				pid = tep_data_pid(kshark_ctx->pevent, rec);
				break;
			case REC_ENTRY: {
				struct kshark_entry *entry;
				int ret;

				if (rec->missed_events) {
					/*
					 * Insert a custom "missed_events" entry just
					 * befor this record.
					 */
					entry = &temp_rec->entry;
					missed_events_action(kshark_ctx, rec, entry);

					temp_next = &temp_rec->next;
					++count;

					/* Now allocate a new rec_list node and comtinue. */
					*temp_next = temp_rec = calloc(1, sizeof(*temp_rec));
				}

				entry = &temp_rec->entry;
				kshark_set_entry_values(kshark_ctx, rec, entry);

				/* Execute all plugin-provided actions (if any). */
				evt_handler = kshark_ctx->event_handlers;
				while ((evt_handler = kshark_find_event_handler(evt_handler,
										entry->event_id))) {
					evt_handler->event_func(kshark_ctx, rec, entry);
					evt_handler = evt_handler->next;
					entry->visible &= ~KS_PLUGIN_UNTOUCHED_MASK;
				}

				pid = entry->pid;
				/* Apply event filtering. */
				ret = FILTER_MATCH;
				if (adv_filter->filters)
					ret = tep_filter_match(adv_filter, rec);

				if (!kshark_show_event(kshark_ctx, entry->event_id) ||
				    ret != FILTER_MATCH) {
					unset_event_filter_flag(kshark_ctx, entry);
				}

				/* Apply CPU filtering. */
				if (!kshark_show_cpu(kshark_ctx, entry->pid)) {
					entry->visible &= ~kshark_ctx->filter_mask;
				}

				/* Apply task filtering. */
				if (!kshark_show_task(kshark_ctx, entry->pid)) {
					entry->visible &= ~kshark_ctx->filter_mask;
				}
				free_record(rec);
				break;
			} /* REC_ENTRY */
			}

			task = kshark_add_task(kshark_ctx, pid);
			if (!task) {
				free_record(rec);
				goto fail;
			}

			temp_next = &temp_rec->next;

			++count;
			rec = tracecmd_read_data(kshark_ctx->handle, cpu);
		}

		total += count;
	}

	*rec_list = cpu_list;
	return total;

 fail:
	free_rec_list(cpu_list, n_cpus, type);
	return -ENOMEM;
}

static int pick_next_cpu(struct rec_list **rec_list, int n_cpus,
			 enum rec_type type)
{
	uint64_t ts = 0;
	uint64_t rec_ts;
	int next_cpu = -1;
	int cpu;

	for (cpu = 0; cpu < n_cpus; ++cpu) {
		if (!rec_list[cpu])
			continue;

		switch (type) {
		case REC_RECORD:
			rec_ts = rec_list[cpu]->rec->ts;
			break;
		case REC_ENTRY:
			rec_ts = rec_list[cpu]->entry.ts;
			break;
		}
		if (!ts || rec_ts < ts) {
			ts = rec_ts;
			next_cpu = cpu;
		}
	}

	return next_cpu;
}

/**
 * @brief Load the content of the trace data file into an array of
 *	  kshark_entries. This function provides an abstraction of the
 *	  entries from the raw data that is read, however the "latency"
 *	  and the "info" fields can be accessed only via the offset
 *	  into the file. This makes the access to these two fields much
 *	  slower.
 *	  If one or more filters are set, the "visible" fields of each entry
 *	  is updated according to the criteria provided by the filters. The
 *	  field "filter_mask" of the session's context is used to control the
 *	  level of visibility/invisibility of the filtered entries.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param data_rows: Output location for the trace data. The user is
 *		     responsible for freeing the elements of the outputted
 *		     array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t kshark_load_data_entries(struct kshark_context *kshark_ctx,
				 struct kshark_entry ***data_rows)
{
	struct kshark_entry **rows;
	struct rec_list **rec_list;
	enum rec_type type = REC_ENTRY;
	ssize_t count, total = 0;
	int n_cpus;

	if (*data_rows)
		free(*data_rows);

	total = get_records(kshark_ctx, &rec_list, type);
	if (total < 0)
		goto fail;

	n_cpus = tep_get_cpus(kshark_ctx->pevent);

	rows = calloc(total, sizeof(struct kshark_entry *));
	if (!rows)
		goto fail_free;

	for (count = 0; count < total; count++) {
		int next_cpu;

		next_cpu = pick_next_cpu(rec_list, n_cpus, type);

		if (next_cpu >= 0) {
			rows[count] = &rec_list[next_cpu]->entry;
			rec_list[next_cpu] = rec_list[next_cpu]->next;
		}
	}

	free_rec_list(rec_list, n_cpus, type);
	*data_rows = rows;
	return total;

 fail_free:
	free_rec_list(rec_list, n_cpus, type);

 fail:
	fprintf(stderr, "Failed to allocate memory during data loading.\n");
	return -ENOMEM;
}

/**
 * @brief Load the content of the trace data file into an array of
 *	  tep_records. Use this function only if you need fast access
 *	  to all fields of the record.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param data_rows: Output location for the trace data. Use free_record()
 *	 	     to free the elements of the outputted array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t kshark_load_data_records(struct kshark_context *kshark_ctx,
				 struct tep_record ***data_rows)
{
	struct tep_record **rows;
	struct tep_record *rec;
	struct rec_list **rec_list;
	struct rec_list *temp_rec;
	enum rec_type type = REC_RECORD;
	ssize_t count, total = 0;
	int n_cpus;

	total = get_records(kshark_ctx, &rec_list, type);
	if (total < 0)
		goto fail;

	n_cpus = tep_get_cpus(kshark_ctx->pevent);

	rows = calloc(total, sizeof(struct tep_record *));
	if (!rows)
		goto fail_free;

	for (count = 0; count < total; count++) {
		int next_cpu;

		next_cpu = pick_next_cpu(rec_list, n_cpus, type);

		if (next_cpu >= 0) {
			rec = rec_list[next_cpu]->rec;
			rows[count] = rec;

			temp_rec = rec_list[next_cpu];
			rec_list[next_cpu] = rec_list[next_cpu]->next;
			free(temp_rec);
			/* The record is still referenced in rows */
		}
	}

	/* There should be no records left in rec_list */
	free_rec_list(rec_list, n_cpus, type);
	*data_rows = rows;
	return total;

 fail_free:
	free_rec_list(rec_list, n_cpus, type);

 fail:
	fprintf(stderr, "Failed to allocate memory during data loading.\n");
	return -ENOMEM;
}

static inline void free_ptr(void *ptr)
{
	if (ptr)
		free(*(void **)ptr);
}

/**
 * @brief Allocate data arrays (matrix columns) to be used to load the tracing
 *	  data into a data matrix form.
 *
 * @param n_rows: Number matrix rows to be allocated. Must be equal to the
 *	 	  number of trace records.
 * @param cpu_array: Output location for the CPU Id column.
 * @param pid_array: Output location for the PID column.
 * @param event_array: Output location for the Event Id column.
 * @param offset_array: Output location for the record offset column.
 * @param ts_array: Output location for the timestamp column.
 *
 * @returns True on success. Else false.
 */
bool kshark_data_matrix_alloc(size_t n_rows, int16_t **event_array,
					     int16_t **cpu_array,
					     int32_t **pid_array,
					     int64_t **offset_array,
					     int64_t **ts_array)
{
	if (offset_array) {
		*offset_array = calloc(n_rows, sizeof(**offset_array));
		if (!*offset_array)
			return false;
	}

	if (cpu_array) {
		*cpu_array = calloc(n_rows, sizeof(**cpu_array));
		if (!*cpu_array)
			goto free_offset;
	}

	if (ts_array) {
		*ts_array = calloc(n_rows, sizeof(**ts_array));
		if (!*ts_array)
			goto free_cpu;
	}

	if (pid_array) {
		*pid_array = calloc(n_rows, sizeof(**pid_array));
		if (!*pid_array)
			goto free_ts;
	}

	if (event_array) {
		*event_array = calloc(n_rows, sizeof(**event_array));
		if (!*event_array)
			goto free_pid;
	}

	return true;

 free_pid:
	free_ptr(pid_array);
 free_ts:
	free_ptr(ts_array);
 free_cpu:
	free_ptr(cpu_array);
 free_offset:
	free_ptr(offset_array);

	fprintf(stderr, "Failed to allocate memory during data loading.\n");
	return false;
}

static const char *get_latency(struct tep_handle *pe,
			       struct tep_record *record)
{
	if (!record)
		return NULL;

	trace_seq_reset(&seq);
	tep_print_event(pe, &seq, record, "%s", TEP_PRINT_LATENCY);
	return seq.buffer;
}

static const char *get_info(struct tep_handle *pe,
				   struct tep_record *record,
				   struct tep_event *event)
{
	char *pos;

	if (!record || !event)
		return NULL;

	trace_seq_reset(&seq);
	tep_print_event(pe, &seq, record, "%s", TEP_PRINT_INFO);

	/*
	 * The event info string contains a trailing newline.
	 * Remove this newline.
	 */
	if ((pos = strchr(seq.buffer, '\n')) != NULL)
		*pos = '\0';

	return seq.buffer;
}

/**
 * @brief This function allows for an easy access to the original value of the
 *	  Process Id as recorded in the tep_record object. The record is read
 *	  from the file only in the case of an entry being touched by a plugin.
 *	  Be aware that using the kshark_get_X_easy functions can be
 *	  inefficient if you need an access to more than one of the data fields
 *	  of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns The original value of the Process Id as recorded in the
 *	    tep_record object on success, otherwise negative error code.
 */
int kshark_get_pid_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_record *data;
	int pid;

	if (!kshark_instance(&kshark_ctx))
		return -ENODEV;

	if (entry->visible & KS_PLUGIN_UNTOUCHED_MASK) {
		pid = entry->pid;
	} else {
		/*
		 * The entry has been touched by a plugin callback function.
		 * Because of this we do not trust the value of "entry->pid".
		 *
		 * Currently the data reading operations are not thread-safe.
		 * Use a mutex to protect the access.
		 */
		pthread_mutex_lock(&kshark_ctx->input_mutex);

		data = tracecmd_read_at(kshark_ctx->handle, entry->offset,
					NULL);
		pid = tep_data_pid(kshark_ctx->pevent, data);
		free_record(data);

		pthread_mutex_unlock(&kshark_ctx->input_mutex);
	}

	return pid;
}

/**
 * @brief This function allows for an easy access to the original value of the
 *	  Task name as recorded in the tep_record object. The record is read
 *	  from the file only in the case of an entry being touched by a plugin.
 *	  Be aware that using the kshark_get_X_easy functions can be
 *	  inefficient if you need an access to more than one of the data fields
 *	  of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns The original name of the task, retrieved from the Process Id
 *	    recorded in the tep_record object on success, otherwise NULL.
 */
const char *kshark_get_task_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	int pid = kshark_get_pid_easy(entry);

	if (pid < 0)
		return NULL;

	kshark_instance(&kshark_ctx);
	return tep_data_comm_from_pid(kshark_ctx->pevent, pid);
}

/**
 * @brief This function allows for an easy access to the latency information
 *	  recorded in the tep_record object. The record is read from the file
 *	  using the offset field of kshark_entry. Be aware that using the
 *	  kshark_get_X_easy functions can be inefficient if you need an access
 *	  to more than one of the data fields of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns On success the function returns a string showing the latency
 *	    information, coded into 5 fields:
 *	    interrupts disabled, need rescheduling, hard/soft interrupt,
 *	    preempt count and lock depth. On failure it returns NULL.
 */
const char *kshark_get_latency_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_record *data;
	const char *lat;

	if (!kshark_instance(&kshark_ctx))
		return NULL;

	if (entry->event_id < 0)
		return NULL;

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&kshark_ctx->input_mutex);

	data = tracecmd_read_at(kshark_ctx->handle, entry->offset, NULL);
	lat = get_latency(kshark_ctx->pevent, data);
	free_record(data);

	pthread_mutex_unlock(&kshark_ctx->input_mutex);

	return lat;
}

/**
 * @brief This function allows for an easy access to the original value of the
 *	  Event Id as recorded in the tep_record object. The record is read
 *	  from the file only in the case of an entry being touched by a plugin.
 *	  Be aware that using the kshark_get_X_easy functions can be
 *	  inefficient if you need an access to more than one of the data fields
 *	  of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns The original value of the Event Id as recorded in the
 *	    tep_record object on success, otherwise negative error code.
 */
int kshark_get_event_id_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_record *data;
	int event_id;

	if (!kshark_instance(&kshark_ctx))
		return -ENODEV;

	if (entry->visible & KS_PLUGIN_UNTOUCHED_MASK) {
		event_id = entry->event_id;
	} else {
		/*
		 * The entry has been touched by a plugin callback function.
		 * Because of this we do not trust the value of
		 * "entry->event_id".
		 *
		 * Currently the data reading operations are not thread-safe.
		 * Use a mutex to protect the access.
		 */
		pthread_mutex_lock(&kshark_ctx->input_mutex);

		data = tracecmd_read_at(kshark_ctx->handle, entry->offset,
					NULL);
		event_id = tep_data_type(kshark_ctx->pevent, data);
		free_record(data);

		pthread_mutex_unlock(&kshark_ctx->input_mutex);
	}

	return (event_id == -1)? -EFAULT : event_id;
}

/**
 * @brief This function allows for an easy access to the original name of the
 *	  trace event as recorded in the tep_record object. The record is read
 *	  from the file only in the case of an entry being touched by a plugin.
 *	  Be aware that using the kshark_get_X_easy functions can be
 *	  inefficient if you need an access to more than one of the data fields
 *	  of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns The mane of the trace event recorded in the tep_record object on
 *	    success, otherwise "[UNKNOWN EVENT]" or NULL.
 */
const char *kshark_get_event_name_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_event *event;

	int event_id = kshark_get_event_id_easy(entry);
	if (event_id == -EFAULT)
		return NULL;

	kshark_instance(&kshark_ctx);

	if (event_id < 0) {
		switch (event_id) {
		case KS_EVENT_OVERFLOW:
			return missed_events_dump(kshark_ctx, entry, false);
		default:
			return NULL;
		}
	}

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&kshark_ctx->input_mutex);
	event = tep_find_event(kshark_ctx->pevent, event_id);
	pthread_mutex_unlock(&kshark_ctx->input_mutex);

	if (event)
		return event->name;

	return "[UNKNOWN EVENT]";
}

/**
 * @brief This function allows for an easy access to the tep_record's info
 *	  streang. The record is read from the file using the offset field of
 *	  kshark_entry. Be aware that using the kshark_get_X_easy functions can
 *	  be inefficient if you need an access to more than one of the data
 *	  fields of the record.
 *
 * @param entry: Input location for the KernelShark entry.
 *
 * @returns A string showing the data output of the trace event on success,
 *	    otherwise NULL.
 */
const char *kshark_get_info_easy(struct kshark_entry *entry)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_event *event;
	struct tep_record *data;
	const char *info = NULL;
	int event_id;

	if (!kshark_instance(&kshark_ctx))
		return NULL;

	if (entry->event_id < 0) {
		switch (entry->event_id) {
		case KS_EVENT_OVERFLOW:
			return missed_events_dump(kshark_ctx, entry, true);
		default:
			return NULL;
		}
	}

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&kshark_ctx->input_mutex);

	data = tracecmd_read_at(kshark_ctx->handle, entry->offset, NULL);
	event_id = tep_data_type(kshark_ctx->pevent, data);
	event = tep_find_event(kshark_ctx->pevent, event_id);
	if (event)
		info = get_info(kshark_ctx->pevent, data, event);

	free_record(data);

	pthread_mutex_unlock(&kshark_ctx->input_mutex);

	return info;
}

/**
 * @brief Convert the timestamp of the trace record (nanosecond precision) into
 *	  seconds and microseconds.
 *
 * @param time: Input location for the timestamp.
 * @param sec: Output location for the value of the seconds.
 * @param usec: Output location for the value of the microseconds.
 */
void kshark_convert_nano(uint64_t time, uint64_t *sec, uint64_t *usec)
{
	uint64_t s;

	*sec = s = time / 1000000000ULL;
	*usec = (time - s * 1000000000ULL) / 1000;
}

/**
 * @brief Dump into a string the content a custom entry. The function allocates
 *	  a null terminated string and returns a pointer to this string.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param entry: A Kernel Shark entry to be printed.
 * @param info_func:
 *
 * @returns The returned string contains a semicolon-separated list of data
 *	    fields. The user has to free the returned string.
 */
char* kshark_dump_custom_entry(struct kshark_context *kshark_ctx,
			       const struct kshark_entry *entry,
			       kshark_custom_info_func info_func)
{
	const char *event_name, *task, *info;
	char *entry_str;
	int size = 0;

	task = tep_data_comm_from_pid(kshark_ctx->pevent, entry->pid);
	event_name = info_func(kshark_ctx, entry, false);
	info = info_func(kshark_ctx, entry, true);

	size = asprintf(&entry_str, "%" PRIu64 "; %s-%i; CPU %i; ; %s; %s",
			entry->ts,
			task,
			entry->pid,
			entry->cpu,
			event_name,
			info);

	if (size > 0)
		return entry_str;

	return NULL;
}


/**
 * @brief Binary search inside a time-sorted array of kshark_entries.
 *
 * @param time: The value of time to search for.
 * @param data: Input location for the trace data.
 * @param l: Array index specifying the lower edge of the range to search in.
 * @param h: Array index specifying the upper edge of the range to search in.
 *
 * @returns On success, the first kshark_entry inside the range, having a
	    timestamp equal or bigger than "time".
	    If all entries inside the range have timestamps greater than "time"
	    the function returns BSEARCH_ALL_GREATER (negative value).
	    If all entries inside the range have timestamps smaller than "time"
	    the function returns BSEARCH_ALL_SMALLER (negative value).
 */
ssize_t kshark_find_entry_by_time(uint64_t time,
				 struct kshark_entry **data,
				 size_t l, size_t h)
{
	size_t mid;

	if (data[l]->ts > time)
		return BSEARCH_ALL_GREATER;

	if (data[h]->ts < time)
		return BSEARCH_ALL_SMALLER;

	/*
	 * After executing the BSEARCH macro, "l" will be the index of the last
	 * entry having timestamp < time and "h" will be the index of the first
	 * entry having timestamp >= time.
	 */
	BSEARCH(h, l, data[mid]->ts < time);
	return h;
}

/**
 * @brief Binary search inside a time-sorted array of tep_records.
 *
 * @param time: The value of time to search for.
 * @param data: Input location for the trace data.
 * @param l: Array index specifying the lower edge of the range to search in.
 * @param h: Array index specifying the upper edge of the range to search in.
 *
 * @returns On success, the first tep_record inside the range, having a
	    timestamp equal or bigger than "time".
	    If all entries inside the range have timestamps greater than "time"
	    the function returns BSEARCH_ALL_GREATER (negative value).
	    If all entries inside the range have timestamps smaller than "time"
	    the function returns BSEARCH_ALL_SMALLER (negative value).
 */
ssize_t kshark_find_record_by_time(uint64_t time,
				   struct tep_record **data,
				   size_t l, size_t h)
{
	size_t mid;

	if (data[l]->ts > time)
		return BSEARCH_ALL_GREATER;

	if (data[h]->ts < time)
		return BSEARCH_ALL_SMALLER;

	/*
	 * After executing the BSEARCH macro, "l" will be the index of the last
	 * record having timestamp < time and "h" will be the index of the
	 * first record having timestamp >= time.
	 */
	BSEARCH(h, l, data[mid]->ts < time);
	return h;
}

/**
 * @brief Simple Pid matching function to be user for data requests.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param e: kshark_entry to be checked.
 * @param pid: Matching condition value.
 *
 * @returns True if the Pid of the entry matches the value of "pid".
 *	    Else false.
 */
bool kshark_match_pid(struct kshark_context *kshark_ctx,
		      struct kshark_entry *e, int pid)
{
	if (e->pid == pid)
		return true;

	return false;
}

/**
 * @brief Simple Cpu matching function to be user for data requests.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param e: kshark_entry to be checked.
 * @param cpu: Matching condition value.
 *
 * @returns True if the Cpu of the entry matches the value of "cpu".
 *	    Else false.
 */
bool kshark_match_cpu(struct kshark_context *kshark_ctx,
		      struct kshark_entry *e, int cpu)
{
	if (e->cpu == cpu)
		return true;

	return false;
}

/**
 * @brief Create Data request. The request defines the properties of the
 *	  requested kshark_entry.
 *
 * @param first: Array index specifying the position inside the array from
 *		 where the search starts.
 * @param n: Number of array elements to search in.
 * @param cond: Matching condition function.
 * @param val: Matching condition value, used by the Matching condition
 *	       function.
 * @param vis_only: If true, a visible entry is requested.
 * @param vis_mask: If "vis_only" is true, use this mask to specify the level
 *		    of visibility of the requested entry.
 *
 * @returns Pointer to kshark_entry_request on success, or NULL on failure.
 *	    The user is responsible for freeing the returned
 *	    kshark_entry_request.
 */
struct kshark_entry_request *
kshark_entry_request_alloc(size_t first, size_t n,
			   matching_condition_func cond, int val,
			   bool vis_only, int vis_mask)
{
	struct kshark_entry_request *req = malloc(sizeof(*req));

	if (!req) {
		fprintf(stderr,
			"Failed to allocate memory for entry request.\n");
		return NULL;
	}

	req->next = NULL;
	req->first = first;
	req->n = n;
	req->cond = cond;
	req->val = val;
	req->vis_only = vis_only;
	req->vis_mask = vis_mask;

	return req;
}

/**
 * @brief Free all Data requests in a given list.
 * @param req: Intput location for the Data request list.
 */
void kshark_free_entry_request(struct kshark_entry_request *req)
{
	struct kshark_entry_request *last;

	while (req) {
		last = req;
		req = req->next;
		free(last);
	}
}

/** Dummy entry, used to indicate the existence of filtered entries. */
const struct kshark_entry dummy_entry = {
	.next		= NULL,
	.visible	= 0x00,
	.cpu		= KS_FILTERED_BIN,
	.pid		= KS_FILTERED_BIN,
	.event_id	= -1,
	.offset		= 0,
	.ts		= 0
};

static const struct kshark_entry *
get_entry(const struct kshark_entry_request *req,
          struct kshark_entry **data,
          ssize_t *index, ssize_t start, ssize_t end, int inc)
{
	struct kshark_context *kshark_ctx = NULL;
	const struct kshark_entry *e = NULL;
	ssize_t i;

	if (index)
		*index = KS_EMPTY_BIN;

	if (!kshark_instance(&kshark_ctx))
		return e;

	/*
	 * We will do a sanity check in order to protect against infinite
	 * loops.
	 */
	assert((inc > 0 && start < end) || (inc < 0 && start > end));
	for (i = start; i != end; i += inc) {
		if (req->cond(kshark_ctx, data[i], req->val)) {
			/*
			 * Data satisfying the condition has been found.
			 */
			if (req->vis_only &&
			    !(data[i]->visible & req->vis_mask)) {
				/* This data entry has been filtered. */
				e = &dummy_entry;
			} else {
				e = data[i];
				break;
			}
		}
	}

	if (index) {
		if (e)
			*index = (e->cpu != KS_FILTERED_BIN)? i : KS_FILTERED_BIN;
		else
			*index = KS_EMPTY_BIN;
	}

	return e;
}

/**
 * @brief Search for an entry satisfying the requirements of a given Data
 *	  request. Start from the position provided by the request and go
 *	  searching in the direction of the increasing timestamps (front).
 *
 * @param req: Input location for Data request.
 * @param data: Input location for the trace data.
 * @param index: Optional output location for the index of the returned
 *		 entry inside the array.
 *
 * @returns Pointer to the first entry satisfying the matching conditionon
 *	    success, or NULL on failure.
 *	    In the special case when some entries, satisfying the Matching
 *	    condition function have been found, but all these entries have
 *	    been discarded because of the visibility criteria (filtered
 *	    entries), the function returns a pointer to a special
 *	    "Dummy entry".
 */
const struct kshark_entry *
kshark_get_entry_front(const struct kshark_entry_request *req,
                       struct kshark_entry **data,
                       ssize_t *index)
{
	ssize_t end = req->first + req->n;

	return get_entry(req, data, index, req->first, end, +1);
}

/**
 * @brief Search for an entry satisfying the requirements of a given Data
 *	  request. Start from the position provided by the request and go
 *	  searching in the direction of the decreasing timestamps (back).
 *
 * @param req: Input location for Data request.
 * @param data: Input location for the trace data.
 * @param index: Optional output location for the index of the returned
 *		 entry inside the array.
 *
 * @returns Pointer to the first entry satisfying the matching conditionon
 *	    success, or NULL on failure.
 *	    In the special case when some entries, satisfying the Matching
 *	    condition function have been found, but all these entries have
 *	    been discarded because of the visibility criteria (filtered
 *	    entries), the function returns a pointer to a special
 *	    "Dummy entry".
 */
const struct kshark_entry *
kshark_get_entry_back(const struct kshark_entry_request *req,
                      struct kshark_entry **data,
                      ssize_t *index)
{
	ssize_t end = req->first - req->n;

	if (end < 0)
		end = -1;

	return get_entry(req, data, index, req->first, end, -1);
}
