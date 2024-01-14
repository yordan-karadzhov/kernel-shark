// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    libkshark-tepdata.c
 *  @brief   Interface for processing of FTRACE (trace-cmd) data.
 */


// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// trace-cmd
#include <trace-cmd.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"
#include "libkshark-tepdata.h"

static __thread struct trace_seq seq;

static bool init_thread_seq(void)
{
	if (!seq.buffer)
		trace_seq_init(&seq);

	return seq.buffer != NULL;
}

/** Structure for handling all unique attributes of the FTRACE data. */
struct tepdata_handle {
	/** Page event used to parse the page. */
	struct tep_handle	*tep; /* MUST BE FIRST ENTRY */

	/** Input handle for the trace data file. */
	struct tracecmd_input	*input;

	/**
	 * Filter allowing sophisticated filtering based on the content of
	 * the event.
	 */
	struct tep_event_filter	*advanced_event_filter;

	/** The unique Id of the sched_switch_event event. */
	int sched_switch_event_id;

	/** Pointer to the sched_switch_next_field format descriptor. */
	struct tep_format_field	*sched_switch_next_field;

	/** Pointer to the sched_switch_comm_field format descriptor. */
	struct tep_format_field	*sched_switch_comm_field;
};

static inline int get_tepdate_handle(struct kshark_data_stream *stream,
				     struct tepdata_handle **handle)
{
	struct kshark_generic_stream_interface *interface;

	interface = stream->interface;
	if (!interface)
		return -EFAULT;

	*handle = interface->handle;

	return 0;
}

/** Get the Page event object used to parse the page. */
struct tep_handle *kshark_get_tep(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return NULL;

	return tep_handle->tep;
}

/** Get the input handle for the trace data file */
struct tracecmd_input *kshark_get_tep_input(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return NULL;

	return tep_handle->input;
}

static inline struct tep_event_filter *
get_adv_filter(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return NULL;

	return tep_handle->advanced_event_filter;
}

static int get_sched_switch_id(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return ret;

	return tep_handle->sched_switch_event_id;
}

static struct tep_format_field *get_sched_next(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return NULL;

	return tep_handle->sched_switch_next_field;
}

static struct tep_format_field *get_sched_comm(struct kshark_data_stream *stream)
{
	struct tepdata_handle *tep_handle;
	int ret;

	ret = get_tepdate_handle(stream, &tep_handle);
	if (ret < 0)
		return NULL;

	return tep_handle->sched_switch_comm_field;
}

static void set_entry_values(struct kshark_data_stream *stream,
			     struct tep_record *record,
			     struct kshark_entry *entry)
{
	struct tep_handle *tep = kshark_get_tep(stream);

	if (!tep)
		return;

	/* Offset of the record */
	entry->offset = record->offset;

	/* CPU Id of the record */
	entry->cpu = record->cpu;

	/* Time stamp of the record */
	entry->ts = record->ts;

	/* Event Id of the record */
	entry->event_id = tep_data_type(tep, record);

	/*
	 * Is visible mask. This default value means that the entry
	 * is visible everywhere.
	 */
	entry->visible = 0xFF;

	/* Process Id of the record */
	entry->pid = tep_data_pid(tep, record);
}

/** Prior time offset of the "missed_events" entry. */
#define ME_ENTRY_TIME_SHIFT	10

static void missed_events_action(struct kshark_data_stream *stream,
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

	entry->pid = tep_data_pid(kshark_get_tep(stream), record);
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

static int get_next_pid(struct kshark_data_stream *stream,
			struct tep_record *record)
{
	unsigned long long val;
	int ret;

	ret = tep_read_number_field(get_sched_next(stream),
				    record->data, &val);

	return ret ? : val;
}

static void register_command(struct kshark_data_stream *stream,
			     struct tep_record *record,
			     int pid)
{
	struct tep_format_field *comm_field = get_sched_comm(stream);
	const char *comm = record->data + comm_field->offset;
	/*
	 * TODO: The retrieve of the name of the command above needs to be
	 * implemented as a wrapper function in libtracevent.
	 */

	if (!tep_is_pid_registered(kshark_get_tep(stream), pid))
			tep_register_comm(kshark_get_tep(stream), comm, pid);
}

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
				tracecmd_free_record(temp_rec->rec);
			free(temp_rec);
		}
	}
	free(rec_list);
}

static ssize_t get_records(struct kshark_context *kshark_ctx,
			   struct kshark_data_stream *stream,
			   struct rec_list ***rec_list,
			   enum rec_type type)
{
	struct tep_event_filter *adv_filter = NULL;
	struct tracecmd_input *input;
	struct rec_list **temp_next;
	struct rec_list **cpu_list;
	struct rec_list *temp_rec;
	struct tep_record *rec;
	ssize_t count, total = 0;
	int pid, next_pid, cpu;

	input = kshark_get_tep_input(stream);
	if (!input)
		return -EFAULT;

	cpu_list = calloc(stream->n_cpus, sizeof(*cpu_list));
	if (!cpu_list)
		return -ENOMEM;

	if (type == REC_ENTRY)
		adv_filter = get_adv_filter(stream);

	for (cpu = 0; cpu < stream->n_cpus; ++cpu) {
		count = 0;
		cpu_list[cpu] = NULL;
		temp_next = &cpu_list[cpu];

		rec = tracecmd_read_cpu_first(kshark_get_tep_input(stream), cpu);
		while (rec) {
			*temp_next = temp_rec = calloc(1, sizeof(*temp_rec));
			if (!temp_rec)
				goto fail;

			temp_rec->next = NULL;

			switch (type) {
			case REC_RECORD:
				temp_rec->rec = rec;
				pid = tep_data_pid(kshark_get_tep(stream), rec);
				break;
			case REC_ENTRY: {
				struct kshark_entry *entry;

				if (rec->missed_events) {
					/*
					 * Insert a custom "missed_events" entry just
					 * befor this record.
					 */
					entry = &temp_rec->entry;
					missed_events_action(stream, rec, entry);

					/* Apply time calibration. */
					kshark_postprocess_entry(stream, rec, entry);

					entry->stream_id = stream->stream_id;

					temp_next = &temp_rec->next;
					++count;

					/* Now allocate a new rec_list node and comtinue. */
					*temp_next = temp_rec = calloc(1, sizeof(*temp_rec));
					if (!temp_rec)
						goto fail;
				}

				entry = &temp_rec->entry;
				set_entry_values(stream, rec, entry);

				if (entry->event_id == get_sched_switch_id(stream)) {
					next_pid = get_next_pid(stream, rec);
					if (next_pid >= 0)
						register_command(stream, rec, next_pid);
				}

				entry->stream_id = stream->stream_id;

				/*
				 * Post-process the content of the entry. This includes
				 * time calibration and event-specific plugin actions.
				 */
				kshark_postprocess_entry(stream, rec, entry);

				pid = entry->pid;

				/* Apply Id filtering. */
				kshark_apply_filters(kshark_ctx, stream, entry);

				/* Apply advanced event filtering. */
				if (adv_filter && adv_filter->filters &&
				    tep_filter_match(adv_filter, rec) != FILTER_MATCH)
					unset_event_filter_flag(kshark_ctx, entry);

				tracecmd_free_record(rec);
				break;
			} /* REC_ENTRY */
			}

			kshark_hash_id_add(stream->tasks, pid);

			temp_next = &temp_rec->next;

			++count;
			rec = tracecmd_read_data(kshark_get_tep_input(stream), cpu);
		}

		if (!count)
			kshark_hash_id_add(stream->idle_cpus, cpu);
		else
			total += count;
	}

	*rec_list = cpu_list;
	return total;

 fail:
	free_rec_list(cpu_list, stream->n_cpus, type);
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
		default:
			return -1;
		}
		if (!ts || rec_ts < ts) {
			ts = rec_ts;
			next_cpu = cpu;
		}
	}

	return next_cpu;
}

/**
 * @brief Load the content of the trace data file asociated with a given
 *	  Data stream into an array of kshark_entries. This function
 *	  provides an abstraction of the entries from the raw data
 *	  that is read, however the "latency" and the "info" fields can be
 *	  accessed only via the offset into the file. This makes the access
 *	  to these two fields much slower.
 *	  If one or more filters are set, the "visible" fields of each entry
 *	  is updated according to the criteria provided by the filters. The
 *	  field "filter_mask" of the session's context is used to control the
 *	  level of visibility/invisibility of the filtered entries.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param kshark_ctx: Input location for context pointer.
 * @param data_rows: Output location for the trace data. The user is
 *		     responsible for freeing the elements of the outputted
 *		     array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t tepdata_load_entries(struct kshark_data_stream *stream,
				struct kshark_context *kshark_ctx,
				struct kshark_entry ***data_rows)
{
	enum rec_type type = REC_ENTRY;
	struct kshark_entry **rows;
	struct rec_list **rec_list;
	ssize_t count, total = 0;

	total = get_records(kshark_ctx, stream, &rec_list, type);
	if (total < 0)
		goto fail;

	rows = calloc(total, sizeof(struct kshark_entry *));
	if (!rows)
		goto fail_free;

	for (count = 0; count < total; count++) {
		int next_cpu;

		next_cpu = pick_next_cpu(rec_list, stream->n_cpus, type);

		if (next_cpu >= 0) {
			rows[count] = &rec_list[next_cpu]->entry;
			rec_list[next_cpu] = rec_list[next_cpu]->next;
		}
	}

	/* There should be no entries left in rec_list. */
	free_rec_list(rec_list, stream->n_cpus, type);
	*data_rows = rows;

	return total;

 fail_free:
	free_rec_list(rec_list, stream->n_cpus, type);

 fail:
	fprintf(stderr, "Failed to allocate memory during data loading.\n");
	return -ENOMEM;
}

static ssize_t tepdata_load_matrix(struct kshark_data_stream *stream,
				   struct kshark_context *kshark_ctx,
				   int16_t **event_array,
				   int16_t **cpu_array,
				   int32_t **pid_array,
				   int64_t **offset_array,
				   int64_t **ts_array)
{
	enum rec_type type = REC_ENTRY;
	struct rec_list **rec_list;
	ssize_t count, total = 0;
	bool status;

	total = get_records(kshark_ctx, stream, &rec_list, type);
	if (total < 0)
		goto fail;

	status = kshark_data_matrix_alloc(total, event_array,
						 cpu_array,
						 pid_array,
						 offset_array,
						 ts_array);
	if (!status)
		goto fail_free;

	for (count = 0; count < total; count++) {
		int next_cpu;

		next_cpu = pick_next_cpu(rec_list, stream->n_cpus, type);
		if (next_cpu >= 0) {
			struct rec_list *rec = rec_list[next_cpu];
			struct kshark_entry *e = &rec->entry;

			if (offset_array)
				(*offset_array)[count] = e->offset;

			if (cpu_array)
				(*cpu_array)[count] = e->cpu;

			if (ts_array) {
				kshark_calib_entry(stream, e);
				(*ts_array)[count] = e->ts;
			}

			if (pid_array)
				(*pid_array)[count] = e->pid;

			if (event_array)
				(*event_array)[count] = e->event_id;

			rec_list[next_cpu] = rec_list[next_cpu]->next;
			free(rec);
		}
	}

	/* There should be no entries left in rec_list. */
	free_rec_list(rec_list, stream->n_cpus, type);
	return total;

 fail_free:
	free_rec_list(rec_list, stream->n_cpus, type);

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
 * @param sd: Data stream identifier.
 * @param data_rows: Output location for the trace data. Use tracecmd_free_record()
 *	 	     to free the elements of the outputted array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t kshark_load_tep_records(struct kshark_context *kshark_ctx, int sd,
				struct tep_record ***data_rows)
{
	struct kshark_data_stream *stream;
	enum rec_type type = REC_RECORD;
	struct rec_list **rec_list;
	struct rec_list *temp_rec;
	struct tep_record **rows;
	struct tep_record *rec;
	ssize_t count, total = 0;

	if (*data_rows)
		free(*data_rows);

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return -EBADF;

	total = get_records(kshark_ctx, stream, &rec_list, type);
	if (total < 0)
		goto fail;

	rows = calloc(total, sizeof(struct tep_record *));
	if (!rows)
		goto fail_free;

	for (count = 0; count < total; count++) {
		int next_cpu;

		next_cpu = pick_next_cpu(rec_list, stream->n_cpus, type);

		if (next_cpu >= 0) {
			rec = rec_list[next_cpu]->rec;
			rows[count] = rec;

			temp_rec = rec_list[next_cpu];
			rec_list[next_cpu] = rec_list[next_cpu]->next;
			free(temp_rec);
			/* The record is still referenced in rows */
		}
	}

	/* There should be no records left in rec_list. */
	free_rec_list(rec_list, stream->n_cpus, type);
	*data_rows = rows;
	return total;

 fail_free:
	free_rec_list(rec_list, stream->n_cpus, type);

 fail:
	fprintf(stderr, "Failed to allocate memory during data loading.\n");
	return -ENOMEM;
}

static int tepdata_get_event_id(struct kshark_data_stream *stream,
				const struct kshark_entry *entry)
{
	int event_id = KS_EMPTY_BIN;
	struct tep_record *record;

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
		pthread_mutex_lock(&stream->input_mutex);

		record = tracecmd_read_at(kshark_get_tep_input(stream),
					  entry->offset, NULL);

		if (record)
			event_id = tep_data_type(kshark_get_tep(stream), record);

		tracecmd_free_record(record);

		pthread_mutex_unlock(&stream->input_mutex);
	}

	return (event_id == -1)? -EFAULT : event_id;
}

static char* missed_events_dump(struct kshark_data_stream *stream,
				      const struct kshark_entry *entry,
				      bool get_info)
{
	char *buffer;
	int size = 0;

	if (get_info)
		size = asprintf(&buffer, "missed_events=%i",
				(int) entry->offset);
	else
		size = asprintf(&buffer, "missed_events");

	if (size > 0)
		return buffer;

	return NULL;
}

static char *tepdata_get_event_name(struct kshark_data_stream *stream,
				    const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface;
	struct tep_event *event;
	char *buffer;

	interface = stream->interface;
	if (!interface)
		return NULL;

	int event_id = interface->get_event_id(stream, entry);
	if (event_id == -EFAULT)
		return NULL;

	if (event_id < 0) {
		switch (event_id) {
		case KS_EVENT_OVERFLOW:
			return missed_events_dump(stream, entry, false);
		default:
			return NULL;
		}
	}

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&stream->input_mutex);

	event = tep_find_event(kshark_get_tep(stream), event_id);

	pthread_mutex_unlock(&stream->input_mutex);

	if (!event ||
            asprintf(&buffer, "%s/%s", event->system, event->name) <= 0)
		return NULL;

	return buffer;
}

static int tepdata_get_pid(struct kshark_data_stream *stream,
			   const struct kshark_entry *entry)
{
	struct tep_record *record;
	int pid = KS_EMPTY_BIN;

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
		pthread_mutex_lock(&stream->input_mutex);

		record = tracecmd_read_at(kshark_get_tep_input(stream),
					  entry->offset, NULL);

		if (record)
			pid = tep_data_pid(kshark_get_tep(stream), record);

		tracecmd_free_record(record);

		pthread_mutex_unlock(&stream->input_mutex);
	}

	return pid;
}

static char *tepdata_get_task(struct kshark_data_stream *stream,
			      const struct kshark_entry *entry)
{
	struct kshark_generic_stream_interface *interface = stream->interface;
	const char *task;
	int pid;

	if (!interface)
		return NULL;

	pid = interface->get_pid(stream, entry);
	task = tep_data_comm_from_pid(kshark_get_tep(stream), pid);

	return task ? strdup(task) : NULL;
}

static char *tepdata_get_latency(struct kshark_data_stream *stream,
				 const struct kshark_entry *entry)
{
	struct tep_record *record;
	char *buffer;

	/* Check if this is a "Missed event" (event_id < 0). */
	if (!init_thread_seq() || entry->event_id < 0)
		return NULL;

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&stream->input_mutex);

	record = tracecmd_read_at(kshark_get_tep_input(stream), entry->offset, NULL);

	if (!record) {
		pthread_mutex_unlock(&stream->input_mutex);
		return NULL;
	}

	trace_seq_reset(&seq);
	tep_print_event(kshark_get_tep(stream), &seq, record,
			"%s", TEP_PRINT_LATENCY);

	tracecmd_free_record(record);

	pthread_mutex_unlock(&stream->input_mutex);

	if (asprintf(&buffer, "%s", seq.buffer)  <= 0)
		return NULL;

	return buffer;
}

static char *get_info_str(struct kshark_data_stream *stream,
			  struct tep_record *record,
			  struct tep_event *event)
{
	char *buffer;

	if (!init_thread_seq() || !record || !event)
		return NULL;

	trace_seq_reset(&seq);
	tep_print_event(kshark_get_tep(stream), &seq, record,
			"%s", TEP_PRINT_INFO);

	if (!seq.len)
		return NULL;
	/*
	 * The event info string contains a trailing newline.
	 * Remove this newline.
	 */
	if (seq.buffer[seq.len - 1] == '\n')
		seq.buffer[seq.len - 1] = '\0';

	if (asprintf(&buffer, "%s", seq.buffer)  <= 0)
		return NULL;

	return buffer;
}

static char *tepdata_get_info(struct kshark_data_stream *stream,
			      const struct kshark_entry *entry)
{
	struct tep_record *record;
	struct tep_event *event;
	char *info = NULL;
	int event_id;

	if (entry->event_id < 0) {
		switch (entry->event_id) {
		case KS_EVENT_OVERFLOW:
			return missed_events_dump(stream, entry, true);
		default:
			return NULL;
		}
	}

	/*
	 * Currently the data reading operations are not thread-safe.
	 * Use a mutex to protect the access.
	 */
	pthread_mutex_lock(&stream->input_mutex);

	record = tracecmd_read_at(kshark_get_tep_input(stream), entry->offset, NULL);
	if (!record) {
		pthread_mutex_unlock(&stream->input_mutex);
		return NULL;
	}

	event_id = tep_data_type(kshark_get_tep(stream), record);
	event = tep_find_event(kshark_get_tep(stream), event_id);

	if (event)
		info = get_info_str(stream, record, event);

	tracecmd_free_record(record);

	pthread_mutex_unlock(&stream->input_mutex);

	return info;
}

static int *tepdata_get_event_ids(struct kshark_data_stream *stream)
{
	struct tep_event **events;
	int i, *evt_ids;

	events = tep_list_events(kshark_get_tep(stream), TEP_EVENT_SORT_SYSTEM);
	if (!events)
		return NULL;

	evt_ids = calloc(stream->n_events, sizeof(*evt_ids));
	if (!evt_ids)
		return NULL;

	for (i = 0; i < stream->n_events ; ++i)
		evt_ids[i] = events[i]->id;

	return evt_ids;
}

static int tepdata_get_field_names(struct kshark_data_stream *stream,
				   const struct kshark_entry *entry,
				   char ***fields_str)
{
	struct tep_format_field *field, **fields;
	struct tep_event *event;
	int i= 0, nr_fields;
	char **buffer;

	*fields_str = NULL;
	event = tep_find_event(kshark_get_tep(stream), entry->event_id);
	if (!event)
		return 0;

	nr_fields = event->format.nr_fields + event->format.nr_common;
	buffer = calloc(nr_fields, sizeof(**fields_str));
	if (!buffer)
		return -ENOMEM;

	/* Add all common fields. */
	fields = tep_event_common_fields(event);
	if (!fields)
		goto fail;

	for (field = *fields; field; field = field->next)
		if (asprintf(&buffer[i++], "%s", field->name) <= 0)
			goto fail;

	free(fields);

	/* Add all unique fields. */
	fields = tep_event_fields(event);
	if (!fields)
		goto fail;

	for (field = *fields; field; field = field->next)
		if (asprintf(&buffer[i++], "%s", field->name) <= 0)
			goto fail;

	free(fields);

	*fields_str = buffer;
	return nr_fields;

 fail:
	for (i = 0; i < nr_fields; ++i)
		free(buffer[i]);

	return -EFAULT;
}

/**
 * Custom entry info function type. To be user for dumping info for custom
 * KernelShark entryes.
 */
typedef char *(tepdata_custom_info_func)(struct kshark_data_stream *,
					const struct kshark_entry *,
					bool);

static char* tepdata_dump_custom_entry(struct kshark_data_stream *stream,
				       const struct kshark_entry *entry,
				       tepdata_custom_info_func info_func)
{
	char *entry_str;
	int size = 0;

	size = asprintf(&entry_str, "%" PRIu64 "; %s-%i; CPU %i; ; %s; %s; 0x%x",
			entry->ts,
			tep_data_comm_from_pid(kshark_get_tep(stream), entry->pid),
			entry->pid,
			entry->cpu,
			info_func(stream, entry, false),
			info_func(stream, entry, true),
			entry->visible);

	if (size > 0)
		return entry_str;

	return NULL;
}

/**
 * @brief Dump into a string the content of one entry. The function allocates
 *	  a null terminated string and returns a pointer to this string.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param entry: A Kernel Shark entry to be printed.
 *
 * @returns The returned string contains a semicolon-separated list of data
 *	    fields. The user has to free the returned string.
 */
static char *tepdata_dump_entry(struct kshark_data_stream *stream,
				const struct kshark_entry *entry)
{
	char *entry_str, *task, *latency, *event, *info;
	struct kshark_generic_stream_interface *interface;
	struct kshark_context *kshark_ctx = NULL;
	int n = 0;

	if (!kshark_instance(&kshark_ctx) || !init_thread_seq())
		return NULL;

	interface = stream->interface;
	if (!interface)
		return NULL;

	if (entry->event_id >= 0) {
		if (kshark_get_tep(stream)) {
			task = interface->get_task(stream, entry);
			latency = interface->aux_info(stream, entry);
			event = interface->get_event_name(stream, entry);
			info = interface->get_info(stream, entry);
			n = asprintf(&entry_str,
				     "%i; %" PRIu64 "; %s-%i; CPU %i; %s; %s; %s; 0x%x",
				     entry->stream_id,
				     entry->ts,
				     task,
				     interface->get_pid(stream, entry),
				     entry->cpu,
				     latency,
				     event,
				     info,
				     entry->visible);

			free(task);
			free(latency);
			free(event);
			free(info);
		} else {
			n = asprintf(&entry_str,
				     "%i; %" PRIu64 "; [UNKNOWN TASK]-%i; CPU %i; ; [UNKNOWN EVENT]; [NO INFO]; 0x%x",
				     entry->stream_id,
				     entry->ts,
				     interface->get_pid(stream, entry),
				     entry->cpu,
				     entry->visible);
		}

		if (n < 1)
			return NULL;
	} else {
		switch (entry->event_id) {
		case KS_EVENT_OVERFLOW:
			entry_str = tepdata_dump_custom_entry(stream, entry,
							     missed_events_dump);
		default:
			return NULL;
		}
	}

	return entry_str;
}

static int tepdata_find_event_id(struct kshark_data_stream *stream,
				 const char *event_name)
{
	struct tep_event *event;
	char *buffer, *system, *name;

	if (asprintf(&buffer, "%s", event_name) < 1)
		return -1;

	system = strtok(buffer, "/");
	name = strtok(NULL, "");
	if (!system || !name)
		return -1;

	event = tep_find_event_by_name(kshark_get_tep(stream), system, name);

	free(buffer);

	if (!event)
		return -1;

	return event->id;
}

static struct tep_format_field *
get_evt_field(struct kshark_data_stream *stream,
	      int event_id, const char *field_name)
{
	struct tep_event *event = tep_find_event(kshark_get_tep(stream),
						 event_id);
	if (!event)
		return NULL;

	return tep_find_any_field(event, field_name);
}

/**
 * @brief  Get the type of a trace record field. For the moment only integer
 *	   fields are supported.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param entry: Input location for the Kernel Shark entry asociated with thes
 *		 record.
 * @param field: The name of the field.
 *
 * @returns KS_INTEGER_FIELD in case the field has an integer type. Otherwise
 *	    KS_INVALID_FIELD.
 */
kshark_event_field_format
tepdata_get_field_type(struct kshark_data_stream *stream,
		       const struct kshark_entry *entry,
		       const char *field)
{
	struct tep_format_field *evt_field;
	int mask = ~(TEP_FIELD_IS_SIGNED |
		     TEP_FIELD_IS_LONG |
		     TEP_FIELD_IS_FLAG);

	evt_field = get_evt_field(stream, entry->event_id, field);
	if (!evt_field)
		return KS_INVALID_FIELD;

	if (mask & evt_field->flags)
		return KS_INVALID_FIELD;

	return KS_INTEGER_FIELD;
}

/**
 * @brief  Get the value of a trace record field.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param rec: Input location for the trace record.
 * @param field: The name of the field.
 * @param val: Output location for the field value.
 *
 * @returns Returns 0 on success, otherwise a negative error code..
 */
int tepdata_read_record_field(struct kshark_data_stream *stream,
			      void *rec,
			      const char *field, int64_t *val)
{
	struct tep_format_field *evt_field;
	struct tep_record *record = rec;
	int event_id, ret;

	if (!record)
		return -EFAULT;

	event_id = tep_data_type(kshark_get_tep(stream), record);
	evt_field = get_evt_field(stream, event_id, field);
	if (!evt_field)
		return -EINVAL;

	ret = tep_read_number_field(evt_field, record->data,
				    (unsigned long long *) val);

	return ret;
}

/**
 * @brief  Get the value of a trace record field.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param entry: Input location for the Kernel Shark entry asociated with thes
 *		 record.
 * @param field: The name of the field.
 * @param val: Output location for the field value.
 *
 * @returns Returns 0 on success, otherwise a negative error code.
 */
int tepdata_read_event_field(struct kshark_data_stream *stream,
			     const struct kshark_entry *entry,
			     const char *field, int64_t *val)
{
	struct tep_format_field *evt_field;
	struct tep_record *record;
	int ret;

	evt_field = get_evt_field(stream, entry->event_id, field);
	if (!evt_field)
		return -EINVAL;

	record = tracecmd_read_at(kshark_get_tep_input(stream),
				  entry->offset, NULL);
	if (!record)
		return -EFAULT;

	ret = tep_read_number_field(evt_field, record->data,
				    (unsigned long long *) val);
	tracecmd_free_record(record);

	return ret;
}

/** Initialize all methods used by a stream of FTRACE data. */
static void kshark_tep_init_methods(struct kshark_generic_stream_interface *interface)
{
	if (!interface)
		return;

	interface->get_pid = tepdata_get_pid;
	interface->get_task = tepdata_get_task;
	interface->get_event_id = tepdata_get_event_id;
	interface->get_event_name = tepdata_get_event_name;
	interface->aux_info= tepdata_get_latency;
	interface->get_info = tepdata_get_info;
	interface->find_event_id = tepdata_find_event_id;
	interface->get_all_event_ids = tepdata_get_event_ids;
	interface->dump_entry = tepdata_dump_entry;
	interface->get_all_event_field_names = tepdata_get_field_names;
	interface->get_event_field_type = tepdata_get_field_type;
	interface->read_record_field_int64 = tepdata_read_record_field;
	interface->read_event_field_int64 = tepdata_read_event_field;
	interface->load_entries = tepdata_load_entries;
	interface->load_matrix = tepdata_load_matrix;
}

/** A list of built in default plugins for FTRACE (trace-cmd) data. */
const char *tep_plugin_names[] = {
	"sched_events",
	"missed_events",
	"kvm_combo",
};

/**
 * Register to the data stream all default plugins for FTRACE (trace-cmd) data.
 */
int kshark_tep_handle_plugins(struct kshark_context *kshark_ctx, int sd)
{
	struct kshark_plugin_list *plugin;
	struct kshark_data_stream *stream;
	int i, n_tep_plugins;

	n_tep_plugins = (sizeof(tep_plugin_names) / sizeof((tep_plugin_names)[0]));
	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return -EEXIST;

	for (i = 0; i < n_tep_plugins; ++i) {
		plugin = kshark_find_plugin_by_name(kshark_ctx->plugins,
						    tep_plugin_names[i]);

		if (plugin && plugin->process_interface) {
			kshark_register_plugin_to_stream(stream,
							 plugin->process_interface,
							 true);
		} else {
			fprintf(stderr, "Plugin \"%s\" not found.\n",
				tep_plugin_names[i]);
		}
	}

	return kshark_handle_all_dpis(stream, KSHARK_PLUGIN_INIT);
}

/** The Process Id of the Idle tasks is zero. */
#define LINUX_IDLE_TASK_PID	0

static int kshark_tep_stream_init(struct kshark_data_stream *stream,
				  struct tracecmd_input *input)
{
	struct kshark_generic_stream_interface *interface;
	struct tepdata_handle *tep_handle;
	struct tep_event *event;

	stream->interface = interface = calloc(1, sizeof(*interface));
	if (!interface)
		return -ENOMEM;

	interface->type = KS_GENERIC_DATA_INTERFACE;

	tep_handle = calloc(1, sizeof(*tep_handle));
	if (!tep_handle)
		goto fail;

	tep_handle->input = input;
	tep_handle->tep = tracecmd_get_tep(tep_handle->input);
	if (!tep_handle->tep)
		goto fail;

	tep_handle->sched_switch_event_id = -EINVAL;
	event = tep_find_event_by_name(tep_handle->tep,
				       "sched", "sched_switch");
	if (event) {
		tep_handle->sched_switch_event_id = event->id;

		tep_handle->sched_switch_next_field =
			tep_find_any_field(event, "next_pid");

		tep_handle->sched_switch_comm_field =
			tep_find_field(event, "next_comm");
	}

	stream->n_cpus = tep_get_cpus(tep_handle->tep);
	stream->n_events = tep_get_events_count(tep_handle->tep);
	stream->idle_pid = LINUX_IDLE_TASK_PID;

	tep_handle->advanced_event_filter =
		tep_filter_alloc(tep_handle->tep);

	kshark_tep_init_methods(interface);

	interface->handle = tep_handle;

	return 0;

 fail:
	free(tep_handle);
	free(interface);
	stream->interface = NULL;
	return -EFAULT;
}

static inline char *set_tep_format(struct kshark_data_stream *stream)
{
	return kshark_set_data_format(stream->data_format,
				      TEP_DATA_FORMAT_IDENTIFIER);
}

static struct tracecmd_input *get_top_input(struct kshark_context *kshark_ctx,
					    int sd)
{
	struct kshark_data_stream *top_stream;

	top_stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!top_stream)
		return NULL;

	return kshark_get_tep_input(top_stream);
}

/**
 * @brief Get an array containing the names of all buffers in FTRACE data
 *	  file.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier of the top buffers in the FTRACE data
 *	  file.
 * @param n_buffers: Output location for the size of the outputted array,
 *	    or a negative error code on failure.
 *
 * @returns Array of strings on success, or NULL on failure. The user is
 *	    responsible for freeing the elements of the outputted array.
 */
char **kshark_tep_get_buffer_names(struct kshark_context *kshark_ctx, int sd,
				   int *n_buffers)
{
	struct tracecmd_input *top_input;
	char **buffer_names;
	int i, n;

	top_input = get_top_input(kshark_ctx, sd);
	if (!top_input) {
		*n_buffers = -EFAULT;
		return NULL;
	}

	n = tracecmd_buffer_instances(top_input);
	buffer_names = calloc(n, sizeof(char *));
	if (!buffer_names) {
		*n_buffers = -ENOMEM;
		return NULL;
	}

	for (i = 0; i < n; ++i) {
		buffer_names[i] =
			strdup(tracecmd_buffer_instance_name(top_input, i));
		if (!buffer_names[i])
			goto free_all;
	}

	*n_buffers = n;
	return buffer_names;

 free_all:
	for (i = 0; i < n; ++i)
		free(buffer_names[i]);
	free(buffer_names);

	*n_buffers = -ENOMEM;
	return NULL;
}

static void set_stream_fields(struct tracecmd_input *top_input, int i,
			      const char *file,
			      const char *name,
			      struct kshark_data_stream *buffer_stream,
			      struct tracecmd_input **buffer_input)
{
	*buffer_input = tracecmd_buffer_instance_handle(top_input, i);

	buffer_stream->name = strdup(name);
	buffer_stream->file = strdup(file);
	set_tep_format(buffer_stream);
}

/**
 * @brief Open a given buffers in FTRACE (trace-cmd) data file.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier of the top buffers in the FTRACE data
 *	  file.
 * @param buffer_name: The name of the buffer to open.
 *
 * @returns Data stream identifier of the buffer on success. Otherwise a
 *	    negative error code.
 */
int kshark_tep_open_buffer(struct kshark_context *kshark_ctx, int sd,
			   const char *buffer_name)
{
	struct kshark_data_stream *top_stream, *buffer_stream;
	struct tracecmd_input *top_input, *buffer_input;
	int i, sd_buffer, n_buffers, ret = -ENODATA;
	char **names;

	top_stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!top_stream)
		return -EFAULT;

	top_input = kshark_get_tep_input(top_stream);
	if (!top_input)
		return -EFAULT;

	names = kshark_tep_get_buffer_names(kshark_ctx, sd, &n_buffers);
	if (!names)
		return n_buffers;

	sd_buffer = kshark_add_stream(kshark_ctx);
	buffer_stream = kshark_get_data_stream(kshark_ctx, sd_buffer);
	if (!buffer_stream)
		return -EFAULT;

	for (i = 0; i < n_buffers; ++i) {
		if (strcmp(buffer_name, names[i]) == 0) {
			set_stream_fields(top_input, i,
					  top_stream->file,
					  buffer_name,
					  buffer_stream,
					  &buffer_input);

			if (!buffer_stream->name || !buffer_stream->file) {
				free(buffer_stream->name);
				free(buffer_stream->file);

				ret = -ENOMEM;
				break;
			}

			ret = kshark_tep_stream_init(buffer_stream,
						     buffer_input);
			break;
		}
	}

	for (i = 0; i < n_buffers; ++i)
		free(names[i]);
	free(names);

	return (ret < 0)? ret : buffer_stream->stream_id;
}

/**
 * @brief Initialize data streams for all buffers in a FTRACE (trace-cmd) data
 *	  file.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param sd: Data stream identifier of the top buffers in the FTRACE data
 *	  file.
 *
 * @returns The total number of data streams initialized on success. Otherwise
 *	    a negative error code.
 */
int kshark_tep_init_all_buffers(struct kshark_context *kshark_ctx,
				int sd)
{
	struct kshark_data_stream *top_stream, *buffer_stream;
	struct tracecmd_input *buffer_input;
	struct tracecmd_input *top_input;
	int i, n_buffers, sd_buffer, ret;

	top_stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!top_stream)
		return -EFAULT;

	top_input = kshark_get_tep_input(top_stream);
	if (!top_input)
		return -EFAULT;

	n_buffers = tracecmd_buffer_instances(top_input);
	for (i = 0; i < n_buffers; ++i) {
		sd_buffer = kshark_add_stream(kshark_ctx);
		if (sd_buffer < 0)
			return -EFAULT;

		buffer_stream = kshark_ctx->stream[sd_buffer];

		set_stream_fields(top_input, i,
				  top_stream->file,
				  tracecmd_buffer_instance_name(top_input, i),
				  buffer_stream,
				  &buffer_input);

		if (!buffer_stream->name || !buffer_stream->file) {
			free(buffer_stream->name);
			free(buffer_stream->file);
			ret = -ENOMEM;
			break;
		}

		ret = kshark_tep_stream_init(buffer_stream, buffer_input);
		if (ret != 0)
			return -EFAULT;
	}

	return n_buffers;
}

/** Is this a stream corresponding to the "top" buffer in the file. */
bool kshark_tep_is_top_stream(struct kshark_data_stream *stream)
{
	return strcmp(stream->name, KS_UNNAMED) == 0;
}

/** Check is the file contains TEP tracing data. */
bool kshark_tep_check_data(const char *file_name)
{
	/*
	 * TODO: This is very naive. Implement more appropriate check. Ideally
	 * it should be part of the trace-cmd library.
	 */
	char *ext = strrchr(file_name, '.');
	if (ext && strcmp(ext, ".dat") == 0) {
		return true;
	}

	return false;
}

/** Initialize the FTRACE data input (from file). */
int kshark_tep_init_input(struct kshark_data_stream *stream)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tracecmd_input *input;

	if (!kshark_instance(&kshark_ctx) || !init_thread_seq())
		return -EEXIST;

	/*
	 * Turn off function trace indent and turn on show parent
	 * if possible.
	 */
	tep_plugin_add_option("ftrace:parent", "1");
	tep_plugin_add_option("ftrace:indent", "0");

	input = tracecmd_open_head(stream->file, 0);
	if (!input)
		return -EEXIST;

	/* Read the tracing data from the file. */
	if (tracecmd_init_data(input) < 0)
		goto fail;

	/* Initialize the stream asociated with the main buffer. */
	if (kshark_tep_stream_init(stream, input) < 0)
		goto fail;

	stream->name = strdup(KS_UNNAMED);

	return 0;

 fail:
	tracecmd_close(input);
	return -EFAULT;
}

/** Initialize using the locally available tracing events. */
int kshark_tep_init_local(struct kshark_data_stream *stream)
{
	struct kshark_generic_stream_interface *interface;
	struct tepdata_handle *tep_handle;

	stream->interface = interface = calloc(1, sizeof(*interface));
	if (!interface)
		return -ENOMEM;

	interface->type = KS_GENERIC_DATA_INTERFACE;

	tep_handle = calloc(1, sizeof(*tep_handle));
	if (!tep_handle)
		goto fail;

	tep_handle->tep = tracefs_local_events(tracefs_tracing_dir());
	if (!tep_handle->tep)
		goto fail;

	stream->n_events = tep_get_events_count(tep_handle->tep);
	stream->n_cpus =  tep_get_cpus(tep_handle->tep);
	set_tep_format(stream);
	if (asprintf(&stream->file, "Local system") <= 0)
		goto fail;

	interface->handle = tep_handle;
	kshark_tep_init_methods(interface);

	return 0;

 fail:
	free(tep_handle);
	free(interface);
	stream->interface = NULL;
	return -EFAULT;
}

/** Method used to close a stream of FTRACE data. */
int kshark_tep_close_interface(struct kshark_data_stream *stream)
{
	struct kshark_generic_stream_interface *interface = stream->interface;
	struct tepdata_handle *tep_handle;

	if (!interface)
		return -EFAULT;

	tep_handle = interface->handle;
	if (!tep_handle)
		return -EFAULT;

	if (seq.buffer) {
		trace_seq_destroy(&seq);
		seq.buffer = NULL;
	}

	if (tep_handle->advanced_event_filter) {
		tep_filter_reset(tep_handle->advanced_event_filter);
		tep_filter_free(tep_handle->advanced_event_filter);
		tep_handle->advanced_event_filter = NULL;
	}

	if (tep_handle->input)
		tracecmd_close(tep_handle->input);

	free(tep_handle);
	interface->handle = NULL;

	return 0;
}

/** Check if the filter any filter is set. */
bool kshark_tep_filter_is_set(struct kshark_data_stream *stream)
{
	struct tep_event_filter *adv_filter = get_adv_filter(stream);

	if (adv_filter && adv_filter->filters)
		return true;

	return false;
}

/**
 * @brief Add a filter based on the content of the event.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param filter_str: The definition of the filter.
 *
 * @returns 0 if the filter was successfully added or a negative error code.
 */
int kshark_tep_add_filter_str(struct kshark_data_stream *stream,
			       const char *filter_str)
{
	struct tep_event_filter *adv_filter = get_adv_filter(stream);
	int ret = tep_filter_add_filter_str(adv_filter, filter_str);

	if (ret < 0) {
		char error_str[200];
		int error_status =
			tep_strerror(kshark_get_tep(stream), ret, error_str,
				     sizeof(error_str));

		if (error_status == 0)
			fprintf(stderr, "filter failed due to: %s\n",
					error_str);
	}

	return ret;
}

/**
 * @brief Get a string showing the filter definition.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param event_id: The unique Id of the event type of the filter.
 *
 * @returns A string that displays the filter contents. This string must be
 *	    freed with free(str). NULL is returned if no filter is found or
 *	    allocation failed.
 */
char *kshark_tep_filter_make_string(struct kshark_data_stream *stream,
				    int event_id)
{
	struct tep_event_filter *adv_filter = get_adv_filter(stream);

	return tep_filter_make_string(adv_filter, event_id);
}

/**
 * @brief Remove a filter based on the content of the event.
 *
 * @param stream: Input location for the FTRACE data stream pointer.
 * @param event_id: The unique Id of the event type of the filter.
 *
 * @return 1: if an event was removed or 0 if the event was not found.
 */
int kshark_tep_filter_remove_event(struct kshark_data_stream *stream,
				   int event_id)
{
	struct tep_event_filter *adv_filter = get_adv_filter(stream);

	return tep_filter_remove_event(adv_filter, event_id);
}

/** Reset all filters based on the content of the event. */
void kshark_tep_filter_reset(struct kshark_data_stream *stream)
{
	return tep_filter_reset(get_adv_filter(stream));
}

/** Get an array of available tracer plugins. */
char **kshark_tracecmd_local_plugins()
{
	return tracefs_tracers(tracefs_tracing_dir());
}

void kshark_tracecmd_plugin_list_free(char **list)
{
	tracefs_list_free(list);
}

/**
 * @brief Free an array, allocated by kshark_tracecmd_get_hostguest_mapping() API
 *
 *
 * @param map: Array, allocated by kshark_tracecmd_get_hostguest_mapping() API
 * @param count: Number of entries in the array
 *
 */
void kshark_tracecmd_free_hostguest_map(struct kshark_host_guest_map *map, int count)
{
	int i;

	if (!map)
		return;
	for (i = 0; i < count; i++) {
		free(map[i].guest_name);
		free(map[i].cpu_pid);
		memset(&map[i], 0, sizeof(*map));
	}
	free(map);
}

/**
 * @brief Get mapping of guest VCPU to host task, running that VCPU.
 *	  Array of mappings for each guest is allocated and returned
 *	  in map input parameter.
 *
 *
 * @param map: Returns allocated array of kshark_host_guest_map structures, each
 *	       one describing VCPUs mapping of one guest.
 *
 * @return The number of entries in the *map array, or a negative error code on
 *	   failure.
 */
int kshark_tracecmd_get_hostguest_mapping(struct kshark_host_guest_map **map)
{
	struct kshark_host_guest_map *gmap = NULL;
	struct tracecmd_input *peer_handle = NULL;
	struct kshark_data_stream *peer_stream;
	struct tracecmd_input *guest_handle = NULL;
	struct kshark_data_stream *guest_stream;
	struct kshark_context *kshark_ctx = NULL;
	unsigned long long trace_id;
	const char *name;
	int vcpu_count;
	const int *cpu_pid;
	int *stream_ids;
	int i, j, k;
	int count = 0;
	int ret;

	if (!map || !kshark_instance(&kshark_ctx))
		return -EFAULT;
	if (*map)
		return -EEXIST;

	stream_ids = kshark_all_streams(kshark_ctx);
	for (i = 0; i < kshark_ctx->n_streams; i++) {
		guest_stream = kshark_get_data_stream(kshark_ctx, stream_ids[i]);
		if (!guest_stream || !kshark_is_tep(guest_stream))
			continue;
		guest_handle = kshark_get_tep_input(guest_stream);
		if (!guest_handle)
			continue;
		trace_id = tracecmd_get_traceid(guest_handle);
		if (!trace_id)
			continue;
		for (j = 0; j < kshark_ctx->n_streams; j++) {
			if (stream_ids[i] == stream_ids[j])
				continue;
			peer_stream = kshark_get_data_stream(kshark_ctx, stream_ids[j]);
			if (!peer_stream || !kshark_is_tep(guest_stream))
				continue;
			peer_handle = kshark_get_tep_input(peer_stream);
			if (!peer_handle)
				continue;
			ret = tracecmd_get_guest_cpumap(peer_handle, trace_id,
							&name, &vcpu_count, &cpu_pid);
			if (!ret && vcpu_count) {
				gmap = realloc(*map,
					       (count + 1) * sizeof(struct kshark_host_guest_map));
				if (!gmap)
					goto mem_error;
				*map = gmap;
				memset(&gmap[count], 0, sizeof(struct kshark_host_guest_map));
				count++;
				gmap[count - 1].guest_id = stream_ids[i];
				gmap[count - 1].host_id = stream_ids[j];
				gmap[count - 1].guest_name = strdup(name);
				if (!gmap[count - 1].guest_name)
					goto mem_error;
				gmap[count - 1].vcpu_count = vcpu_count;
				gmap[count - 1].cpu_pid = malloc(sizeof(int) * vcpu_count);
				if (!gmap[count - 1].cpu_pid)
					goto mem_error;
				for (k = 0; k < vcpu_count; k++)
					gmap[count - 1].cpu_pid[k] = cpu_pid[k];
				break;
			}
		}
	}

	free(stream_ids);
	return count;

mem_error:
	free(stream_ids);
	if (*map) {
		kshark_tracecmd_free_hostguest_map(*map, count);
		*map = NULL;
	}

	return -ENOMEM;
}

/**
 * @brief Find the data stream corresponding the top buffer of a FTRACE
 *	  (trace-cmd) data file.
 *
 * @param kshark_ctx: Input location for context pointer.
 * @param file: The name of the file.
 *
 * @returns Data stream identifier of the top buffers in the FTRACE data
 *	    fileon success. Otherwise a negative error code.
 */
int kshark_tep_find_top_stream(struct kshark_context *kshark_ctx,
			       const char *file)
{
	struct kshark_data_stream *top_stream = NULL, *stream;
	int i, *stream_ids = kshark_all_streams(kshark_ctx);

	for (i = 0; i < kshark_ctx->n_streams; ++i) {
		stream = kshark_ctx->stream[stream_ids[i]];
		if (strcmp(stream->file, file) == 0 &&
		    kshark_tep_is_top_stream(stream))
			top_stream = stream;
	}

	free(stream_ids);

	if (!top_stream)
		return -EEXIST;

	return top_stream->stream_id;
}

static bool find_wakeup_event(struct tep_handle *tep, const char *wakeup_name,
			      struct tep_event **waking_event_ptr)
{
	struct tep_event *event;

	event = tep_find_event_by_name(tep, "sched", wakeup_name);

	if (event)
		*waking_event_ptr = event;

	return !!event;
}

/**
 * @brief Search the available trace events and retrieve a definition of
 *	  a waking_event.
 *
 * @param tep: Input location for the the Page event object.
 * @param waking_event_ptr: Output location for the the waking_event object.
 *
 * @returns True on success, otherwise False.
 */
bool define_wakeup_event(struct tep_handle *tep,
			 struct tep_event **waking_event_ptr)
{
	bool wakeup_found;

	wakeup_found = find_wakeup_event(tep, "sched_wakeup",
					 waking_event_ptr);

	wakeup_found |= find_wakeup_event(tep, "sched_wakeup_new",
					  waking_event_ptr);

	wakeup_found |= find_wakeup_event(tep, "sched_waking",
					  waking_event_ptr);

	return wakeup_found;
}
