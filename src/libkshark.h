/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

 /**
 *  @file    libkshark.h
 *  @brief   API for processing of tracing data.
 */

#ifndef _LIB_KSHARK_H
#define _LIB_KSHARK_H

// C
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

// Json-C
#include <json-c/json.h>

#ifdef __cplusplus
extern "C" {
#endif

// KernelShark
#include "libkshark-plugin.h"

/**
 * Kernel Shark entry contains all information from one trace record needed
 * in order to  visualize the time-series of trace records. The part of the
 * data which is not directly required for the visualization (latency, record
 * info etc.) is available on-demand via the offset into the trace file.
 */
struct kshark_entry {
	/** Pointer to the next (in time) kshark_entry on the same CPU core. */
	struct kshark_entry *next; /* MUST BE FIRST ENTRY */

	/**
	 * A bit mask controlling the visibility of the entry. A value of OxFF
	 * would mean that the entry is visible everywhere. Use
	 * kshark_filter_masks to check the level of visibility/invisibility
	 * of the entry.
	 */
	uint16_t	visible;

	/** Data stream identifier. */
	int16_t		stream_id;

	/** Unique Id of the trace event type. */
	int16_t		event_id;

	/** The CPU core of the record. */
	int16_t		cpu;

	/** The PID of the task the record was generated. */
	int32_t		pid;

	/** The offset into the trace file, used to find the record. */
	int64_t		offset;

	/**
	 * The time of the record in nano seconds. The value is taken from
	 * the timestamps within the trace data file, which are architecture
	 * dependent. The time usually is the timestamp from when the system
	 * started.
	 */
	int64_t		ts;
};

/** Size of the hash table of PIDs in terms of bits being used by the key. */
#define KS_TASK_HASH_NBITS	16

/** Size of the hash table of Ids in terms of bits being used by the key. */
#define KS_FILTER_HASH_NBITS	8

/** A bucket for the hash table of integer Id numbers (kshark_hash_id). */
struct kshark_hash_id_item {
	/** Pointer to the Id in this bucket. */
	struct kshark_hash_id_item	*next;

	/** The Id value. */
	int				id;
};

/**
 * Hash table of integer Id numbers. To be used for fast filter of trace
 * entries.
 */
struct kshark_hash_id {
	/** Array of buckets. */
	struct kshark_hash_id_item	**hash;

	/** The number of Ids in the table. */
	size_t	count;

	/**
	 * The number of bits used by the hashing function.
	 * Note that the number of buckets in the table if given by
	 * 1 << n_bits.
	 */
	size_t	n_bits;
};

bool kshark_hash_id_find(struct kshark_hash_id *hash, int id);

int kshark_hash_id_add(struct kshark_hash_id *hash, int id);

void kshark_hash_id_clear(struct kshark_hash_id *hash);

struct kshark_hash_id *kshark_hash_id_alloc(size_t n_bits);

void kshark_hash_id_free(struct kshark_hash_id *hash);

int *kshark_hash_ids(struct kshark_hash_id *hash);

/* Quiet warnings over documenting simple structures */
//! @cond Doxygen_Suppress

static const char top_name[] = { 0x1b, 0x00 }; // Non printable character

//! @endcond

/**
 * Non printable character used for the name in the case when the name has to
 * be ignored.
 */
#define KS_UNNAMED	(char *) &top_name

/**
 * Timestamp calibration function type. To be user for system clock
 * calibration.
 */
typedef void (*time_calib_func) (int64_t *, int64_t *);

struct kshark_data_stream;

/** A function type to be used by the method interface of the data stream. */
typedef char *(*stream_get_str_func) (struct kshark_data_stream *,
				      const struct kshark_entry *);

/** A function type to be used by the method interface of the data stream. */
typedef int (*stream_get_int_func) (struct kshark_data_stream *,
				    const struct kshark_entry *);

/** A function type to be used by the method interface of the data stream. */
typedef int (*stream_find_id_func) (struct kshark_data_stream *,
				    const char *);

/** A function type to be used by the method interface of the data stream. */
typedef int *(*stream_get_ids_func) (struct kshark_data_stream *);

/** A function type to be used by the method interface of the data stream. */
typedef int (*stream_get_names_func) (struct kshark_data_stream *,
				      const struct kshark_entry *,
				      char ***);

/** Event field format identifier. */
typedef enum kshark_event_field_format {
	/** A field of unknown type. */
	KS_INVALID_FIELD,

	/** Integer number */
	KS_INTEGER_FIELD,

	/** Floating-point number */
	KS_FLOAT_FIELD
} kshark_event_field_format;

/** A function type to be used by the method interface of the data stream. */
typedef kshark_event_field_format
(*stream_event_field_type) (struct kshark_data_stream *,
			    const struct kshark_entry *,
			    const char *);

/** A function type to be used by the method interface of the data stream. */
typedef int (*stream_read_event_field) (struct kshark_data_stream *,
					const struct kshark_entry *,
					const char *,
					int64_t *);

/** A function type to be used by the method interface of the data stream. */
typedef int (*stream_read_record_field) (struct kshark_data_stream *,
					 void *,
					 const char *,
					 int64_t *);

struct kshark_context;

/** A function type to be used by the method interface of the data stream. */
typedef ssize_t (*load_entries_func) (struct kshark_data_stream *,
				      struct kshark_context *,
				      struct kshark_entry ***);

/** A function type to be used by the method interface of the data stream. */
typedef ssize_t (*load_matrix_func) (struct kshark_data_stream *,
				     struct kshark_context *,
				     int16_t **event_array,
				     int16_t **cpu_array,
				     int32_t **pid_array,
				     int64_t **offset_array,
				     int64_t **ts_array);

/** Data interface identifier. */
typedef enum kshark_data_interface_id {
	/** An interface with unknown type. */
	KS_INVALID_INTERFACE,

	/** Generic interface suitable for Ftrace data. */
	KS_GENERIC_DATA_INTERFACE,
} kshark_data_interface_id;

/**
 * Structure representing the interface of methods used to operate over
 * the data from a given stream.
 */
struct kshark_generic_stream_interface {
	/** Interface version identifier. */
	kshark_data_interface_id	type; /* MUST BE FIRST ENTRY. */

	/** Method used to retrieve the Process Id of the entry. */
	stream_get_int_func	get_pid;

	/** Method used to retrieve the Event Id of the entry. */
	stream_get_int_func	get_event_id;

	/** Method used to retrieve the Event name of the entry. */
	stream_get_str_func	get_event_name;

	/** Method used to retrieve the Task name of the entry. */
	stream_get_str_func	get_task;

	/** Method used to retrieve the Info string of the entry. */
	stream_get_str_func	get_info;

	/**
	 * Method used to retrieve an unspecified auxiliary info of the trace
	 * record.
	 */
	stream_get_str_func	aux_info;

	/** Method used to retrieve Id of the Event from its name. */
	stream_find_id_func	find_event_id;

	/** Method used to retrieve the array of Ids of all Events. */
	stream_get_ids_func	get_all_event_ids;

	/** Method used to dump the entry's content to string. */
	stream_get_str_func	dump_entry;

	/**
	 * Method used to retrieve the array of all field names of a given
	 * event.
	 */
	stream_get_names_func	get_all_event_field_names;

	/** Method used to access the type of an event's data field. */
	stream_event_field_type		get_event_field_type;

	/** Method used to access the value of an event's data field. */
	stream_read_event_field		read_event_field_int64;

	/** Method used to access the value of an event's data field. */
	stream_read_record_field	read_record_field_int64;

	/** Method used to load the data in the form of entries. */
	load_entries_func	load_entries;

	/** Method used to load the data in matrix form. */
	load_matrix_func	load_matrix;

	/** Generic data handle. */
	void			*handle;
};

/** Data format identifier string indicating invalid data. */
#define KS_INVALID_DATA		"invalid data"

/** Structure representing a stream of trace data. */
struct kshark_data_stream {
	/** Data stream identifier. */
	int16_t			stream_id;

	/** The number of CPUs presented in this data stream. */
	int			n_cpus;

	/** Hash table of Idle CPUs. */
	struct kshark_hash_id	*idle_cpus;

	/**
	 * The number of distinct event types presented in this data stream.
	 */
	int			n_events;

	/** The Process Id of the Idle task. */
	int			idle_pid;

	/** Trace data file pathname. */
	char			*file;

	/** Stream name. */
	char			*name;

	/** Hash table of task PIDs. */
	struct kshark_hash_id	*tasks;

	/** A mutex, used to protect the access to the input file. */
	pthread_mutex_t		input_mutex;

	/** Hash of tasks to filter on. */
	struct kshark_hash_id	*show_task_filter;

	/** Hash of tasks to not display. */
	struct kshark_hash_id	*hide_task_filter;

	/** Hash of events to filter on. */
	struct kshark_hash_id	*show_event_filter;

	/** Hash of events to not display. */
	struct kshark_hash_id	*hide_event_filter;

	/** Hash of CPUs to filter on. */
	struct kshark_hash_id	*show_cpu_filter;

	/** Hash of CPUs to not display. */
	struct kshark_hash_id	*hide_cpu_filter;

	/**
	 * Flag showing if some entries are filtered out (marked as invisible).
	 */
	bool			filter_is_applied;

	/** The type of the data. */
	char			data_format[KS_DATA_FORMAT_SIZE];

	/** List of Plugin interfaces. */
	struct kshark_dpi_list	*plugins;

	/** The number of plugins registered for this stream.*/
	int			n_plugins;

	/** System clock calibration function. */
	time_calib_func		calib;

	/** An array of time calibration constants. */
	int64_t			*calib_array;

	/** The size of the array of time calibration constants. */
	size_t			calib_array_size;

	/** List of Plugin's Event handlers. */
	struct kshark_event_proc_handler	*event_handlers;

	/** List of Plugin's Draw handlers. */
	struct kshark_draw_handler		*draw_handlers;

	/**
	 * The interface of methods used to operate over the data from a given
	 * stream.
	 */
	void				*interface;
};

static inline char *kshark_set_data_format(char *dest_format,
					   const char *src_format)
{
	return strncpy(dest_format, src_format, KS_DATA_FORMAT_SIZE - 1);
}

/** Hard-coded default number of data streams available at initialization. */
#define KS_DEFAULT_NUM_STREAMS	256

/**
 * Structure representing the parameters of the stream descriptor array owned
 * by the kshark session.
 */
struct kshark_stream_array_descriptor {
	/** The identifier of the Data stream added. */
	int		max_stream_id;

	/** The the next free Data stream identifier (index). */
	int		next_free_stream_id;

	/** The capacity of the array of stream objects (pointers). */
	int		array_size;
};

/** Structure representing a kshark session. */
struct kshark_context {
	/** Array of data stream descriptors. */
	struct kshark_data_stream	**stream;

	/** The number of data streams. */
	int				n_streams;

	/** Parameters of the stream descriptor array. */
	struct kshark_stream_array_descriptor	stream_info;

	/**
	 * Bit mask, controlling the visibility of the entries after filtering.
	 * If given bit is set here, all entries which are filtered-out will
	 * have this bit unset in their "visible" fields.
	 */
	uint8_t				filter_mask;

	/** List of Data collections. */
	struct kshark_entry_collection *collections;

	/** List of data readout interfaces. */
	struct kshark_dri_list		*inputs;

	/** The number of readout interfaces. */
	int				n_inputs;

	/** List of Plugins. */
	struct kshark_plugin_list	*plugins;

	/** The number of plugins. */
	int				n_plugins;
};

bool kshark_instance(struct kshark_context **kshark_ctx);

int kshark_open(struct kshark_context *kshark_ctx, const char *file);

int kshark_stream_open(struct kshark_data_stream *stream, const char *file);

int kshark_add_stream(struct kshark_context *kshark_ctx);

int kshark_remove_stream(struct kshark_context *kshark_ctx, int sd);

struct kshark_data_stream *
kshark_get_data_stream(struct kshark_context *kshark_ctx, int sd);

struct kshark_data_stream *
kshark_get_stream_from_entry(const struct kshark_entry *entry);

int *kshark_all_streams(struct kshark_context *kshark_ctx);

ssize_t kshark_get_task_pids(struct kshark_context *kshark_ctx, int sd,
			     int **pids);

int kshark_close(struct kshark_context *kshark_ctx, int sd);

void kshark_close_all(struct kshark_context *kshark_ctx);

void kshark_free(struct kshark_context *kshark_ctx);

char *kshark_comm_from_pid(int sd, int pid);

char *kshark_event_from_id(int sd, int event_id);

void kshark_convert_nano(uint64_t time, uint64_t *sec, uint64_t *usec);

char* kshark_dump_entry(const struct kshark_entry *entry);

void kshark_print_entry(const struct kshark_entry *entry);

int kshark_get_pid(const struct kshark_entry *entry);

int kshark_get_event_id(const struct kshark_entry *entry);

int *kshark_get_all_event_ids(struct kshark_data_stream *stream);

int kshark_find_event_id(struct kshark_data_stream *stream,
			 const char *event_name);

char *kshark_get_event_name(const struct kshark_entry *entry);

char *kshark_get_task(const struct kshark_entry *entry);

char *kshark_get_info(const struct kshark_entry *entry);

char *kshark_get_aux_info(const struct kshark_entry *entry);

kshark_event_field_format
kshark_get_event_field_type(const struct kshark_entry *entry,
			    const char *field);

int kshark_get_all_event_field_names(const struct kshark_entry *entry,
				     char ***field);

int kshark_read_record_field_int(struct kshark_data_stream *stream, void *rec,
				 const char *field, int64_t *val);

int kshark_read_event_field_int(const struct kshark_entry *entry,
				const char* field, int64_t *val);

ssize_t kshark_load_entries(struct kshark_context *kshark_ctx, int sd,
			    struct kshark_entry ***data_rows);

ssize_t kshark_load_matrix(struct kshark_context *kshark_ctx, int sd,
			   int16_t **event_array,
			   int16_t **cpu_array,
			   int32_t **pid_array,
			   int64_t **offset_array,
			   int64_t **ts_array);

/** Bit masks used to control the visibility of the entry after filtering. */
enum kshark_filter_masks {
	/**
	 * Use this mask to check the visibility of the entry in the text
	 * view.
	 */
	KS_TEXT_VIEW_FILTER_MASK	= 1 << 0,

	/**
	 * Use this mask to check the visibility of the entry in the graph
	 * view.
	 */
	KS_GRAPH_VIEW_FILTER_MASK	= 1 << 1,

	/** Special mask used whene filtering events. */
	KS_EVENT_VIEW_FILTER_MASK	= 1 << 2,

	/* The next 4 bits are reserved for more KS_X_VIEW_FILTER_MASKs. */

	/**
	 * Use this mask to check if the content of the entry has been accessed
	 * by a plugin-defined function.
	 */
	KS_PLUGIN_UNTOUCHED_MASK	= 1 << 7
};

/** Filter type identifier. */
enum kshark_filter_type {
	/** Dummy filter identifier reserved for future use. */
	KS_NO_FILTER,

	/**
	 * Identifier of the filter, used to specified the events to be shown.
	 */
	KS_SHOW_EVENT_FILTER,

	/**
	 * Identifier of the filter, used to specified the events to be
	 * filtered-out.
	 */
	KS_HIDE_EVENT_FILTER,

	/**
	 * Identifier of the filter, used to specified the tasks to be shown.
	 */
	KS_SHOW_TASK_FILTER,

	/**
	 * Identifier of the filter, used to specified the tasks to be
	 * filtered-out.
	 */
	KS_HIDE_TASK_FILTER,

	/**
	 * Identifier of the filter, used to specified the CPUs to be shown.
	 */
	KS_SHOW_CPU_FILTER,

	/**
	 * Identifier of the filter, used to specified the CPUs to be
	 * filtered-out.
	 */
	KS_HIDE_CPU_FILTER,
};

struct kshark_hash_id *
kshark_get_filter(struct kshark_data_stream *stream,
		  enum kshark_filter_type filter_id);

void kshark_filter_add_id(struct kshark_context *kshark_ctx, int sd,
			  int filter_id, int id);

int *kshark_get_filter_ids(struct kshark_context *kshark_ctx, int sd,
			   int filter_id, int *n);

void kshark_filter_clear(struct kshark_context *kshark_ctx, int sd,
			 int filter_id);

bool kshark_this_filter_is_set(struct kshark_hash_id *filter);

bool kshark_filter_is_set(struct kshark_context *kshark_ctx, int sd);

static inline void unset_event_filter_flag(struct kshark_context *kshark_ctx,
					   struct kshark_entry *e)
{
	/*
	 * All entries, filtered-out by the event filters, will be treated
	 * differently, when visualized. Because of this, ignore the value
	 * of the GRAPH_VIEW flag provided by the user via
	 * stream->filter_mask. The value of the EVENT_VIEW flag in
	 * stream->filter_mask will be used instead.
	 */
	int event_mask = kshark_ctx->filter_mask & ~KS_GRAPH_VIEW_FILTER_MASK;

	e->visible &= ~event_mask;
}

void kshark_apply_filters(struct kshark_context *kshark_ctx,
			  struct kshark_data_stream *stream,
			  struct kshark_entry *entry);

void kshark_filter_stream_entries(struct kshark_context *kshark_ctx, int sd,
				  struct kshark_entry **data,
				  size_t n_entries);

void kshark_filter_all_entries(struct kshark_context *kshark_ctx,
			       struct kshark_entry **data, size_t n_entries);

void kshark_clear_all_filters(struct kshark_context *kshark_ctx,
			      struct kshark_entry **data,
			      size_t n_entries);

void kshark_plugin_actions(struct kshark_data_stream *stream,
			   void *record, struct kshark_entry *entry);

void kshark_calib_entry(struct kshark_data_stream *stream,
			struct kshark_entry *entry);

void kshark_postprocess_entry(struct kshark_data_stream *stream,
			      void *record, struct kshark_entry *entry);

/** Search failed identifiers. */
enum kshark_search_failed {
	/** All entries have greater timestamps. */
	BSEARCH_ALL_GREATER = -1,

	/** All entries have smaller timestamps. */
	BSEARCH_ALL_SMALLER = -2,
};

/** General purpose Binary search macro. */
#define BSEARCH(h, l, cond) 				\
	({						\
		while (h - l > 1) {			\
			mid = (l + h) / 2;		\
			if (cond)			\
				l = mid;		\
			else				\
				h = mid;		\
		}					\
	})

ssize_t kshark_find_entry_by_time(int64_t time,
				  struct kshark_entry **data_rows,
				  size_t l, size_t h);

bool kshark_match_pid(struct kshark_context *kshark_ctx,
		      struct kshark_entry *e, int sd, int *pid);

bool kshark_match_cpu(struct kshark_context *kshark_ctx,
		      struct kshark_entry *e, int sd, int *cpu);

bool kshark_match_event_id(struct kshark_context *kshark_ctx,
			   struct kshark_entry *e, int sd, int *event_id);

bool kshark_match_event_and_pid(struct kshark_context *kshark_ctx,
				struct kshark_entry *e,
				int sd, int *values);

bool kshark_match_event_and_cpu(struct kshark_context *kshark_ctx,
				struct kshark_entry *e,
				int sd, int *values);

/**
 * Empty bin identifier.
 * KS_EMPTY_BIN is used to reset entire arrays to empty with memset(), thus it
 * must be -1 for that to work.
 */
#define KS_EMPTY_BIN		-1

/** Filtered bin identifier. */
#define KS_FILTERED_BIN		-2

/** Overflow Event identifier. */
#define KS_EVENT_OVERFLOW	(-EOVERFLOW)

/** Matching condition function type. To be user for data requests */
typedef bool (matching_condition_func)(struct kshark_context*,
				       struct kshark_entry*,
				       int, int*);

/**
 * Data request structure, defining the properties of the required
 * kshark_entry.
 */
struct kshark_entry_request {
	/** Pointer to the next Data request. */
	struct kshark_entry_request *next;

	/**
	 * Array index specifying the position inside the array from where
	 * the search starts.
	 */
	size_t first;

	/** Number of array elements to search in. */
	size_t n;

	/** Matching condition function. */
	matching_condition_func *cond;

	/** Data stream identifier. */
	int sd;

	/**
	 * Matching condition value, used by the Matching condition function.
	 */
	int *values;

	/** If true, a visible entry is requested. */
	bool vis_only;

	/**
	 * If "vis_only" is true, use this mask to specify the level of
	 * visibility of the requested entry.
	 */
	uint8_t vis_mask;
};

struct kshark_entry_request *
kshark_entry_request_alloc(size_t first, size_t n,
			   matching_condition_func cond, int sd, int *values,
			   bool vis_only, int vis_mask);

void kshark_free_entry_request(struct kshark_entry_request *req);

const struct kshark_entry *
kshark_get_entry_front(const struct kshark_entry_request *req,
		       struct kshark_entry **data,
		       ssize_t *index);

const struct kshark_entry *
kshark_get_entry_back(const struct kshark_entry_request *req,
		      struct kshark_entry **data,
		      ssize_t *index);

/**
 * Data collections are used to optimize the search for an entry having an
 * abstract property, defined by a Matching condition function and an array of
 * values. When a collection is processed, the data which is relevant for the
 * collection is enclosed in "Data intervals", defined by pairs of "Resume" and
 * "Break" points. It is guaranteed that the data outside of the intervals
 * contains no entries satisfying the abstract matching condition. However, the
 * intervals may (will) contain data that do not satisfy the matching condition.
 * Once defined, the Data collection can be used when searching for an entry
 * having the same (ore related) abstract property. The collection allows to
 * ignore the irrelevant data, thus it eliminates the linear worst-case time
 * complexity of the search.
 */
struct kshark_entry_collection {
	/** Pointer to the next Data collection. */
	struct kshark_entry_collection *next;

	/** Matching condition function, used to define the collections. */
	matching_condition_func *cond;

	/** Data stream identifier. */
	int stream_id;

	/**
	 * Array of matching condition values, used by the Matching condition
	 * finction to define the collection.
	 */
	int *values;

	/** The suze of the array of matching condition values. */
	int n_val;

	/**
	 * Array of indexes defining the beginning of each individual data
	 * interval.
	 */
	size_t *resume_points;

	/**
	 * Array of indexes defining the end of each individual data interval.
	 */
	size_t *break_points;

	/** Number of data intervals in this collection. */
	size_t size;
};

struct kshark_entry_collection *
kshark_add_collection_to_list(struct kshark_context *kshark_ctx,
			      struct kshark_entry_collection **col_list,
			      struct kshark_entry **data,
			      size_t n_rows,
			      matching_condition_func cond,
			      int sd, int *values, size_t n_val,
			      size_t margin);

struct kshark_entry_collection *
kshark_register_data_collection(struct kshark_context *kshark_ctx,
				struct kshark_entry **data, size_t n_rows,
				matching_condition_func cond,
				int sd, int *values, size_t n_val,
				size_t margin);

void kshark_unregister_data_collection(struct kshark_entry_collection **col,
				       matching_condition_func cond,
				       int sd, int *values, size_t n_val);

struct kshark_entry_collection *
kshark_find_data_collection(struct kshark_entry_collection *col,
			    matching_condition_func cond,
			    int sd, int *values, size_t n_val);

void kshark_reset_data_collection(struct kshark_entry_collection *col);

void kshark_unregister_stream_collections(struct kshark_entry_collection **col,
					  int sd);

void kshark_free_collection_list(struct kshark_entry_collection *col);

const struct kshark_entry *
kshark_get_collection_entry_front(struct kshark_entry_request *req,
				  struct kshark_entry **data,
				  const struct kshark_entry_collection *col,
				  ssize_t *index);

const struct kshark_entry *
kshark_get_collection_entry_back(struct kshark_entry_request *req,
				 struct kshark_entry **data,
				 const struct kshark_entry_collection *col,
				 ssize_t *index);

/** Structure representing a KernelShark Configuration document. */
struct kshark_config_doc {
	/** Document format identifier. */
	int	format;

	/** Configuration document instance. */
	void	*conf_doc;
};

/** Configuration format identifiers. */
enum kshark_config_formats {
	/** Unformatted Configuration document identifier. */
	KS_CONFIG_AUTO = 0,

	/**
	 * String Configuration document identifier. The String format is
	 * meant to be used only by kshark_config_doc_add() and
	 * kshark_config_doc_get(), when adding/getting simple string fields.
	 */
	KS_CONFIG_STRING,

	/** Json Configuration document identifier. */
	KS_CONFIG_JSON,
};

/**
 * Field name for the Configuration document describing the Hide Event filter.
 */
#define KS_HIDE_EVENT_FILTER_NAME	"hide event filter"

/**
 * Field name for the Configuration document describing the Show Event filter.
 */
#define KS_SHOW_EVENT_FILTER_NAME	"show event filter"

/**
 * Field name for the Configuration document describing the Hide Task filter.
 */
#define KS_HIDE_TASK_FILTER_NAME	"hide task filter"

/**
 * Field name for the Configuration document describing the Show Task filter.
 */
#define KS_SHOW_TASK_FILTER_NAME	"show task filter"

/**
 * Field name for the Configuration document describing the Hide Task filter.
 */
#define KS_HIDE_CPU_FILTER_NAME		"hide cpu filter"

/**
 * Field name for the Configuration document describing the Show Task filter.
 */
#define KS_SHOW_CPU_FILTER_NAME		"show cpu filter"

/**
 * Field name for the Configuration document describing the Advanced event
 * filter.
 */
#define KS_ADV_EVENT_FILTER_NAME	"adv event filter"

/**
 * Field name for the Configuration document describing user-specified filter
 * mask.
 */
#define KS_USER_FILTER_MASK_NAME	"filter mask"
/**
 * Field name for the Configuration document describing the state of the Vis.
 * model.
 */
#define KS_HISTO_NAME			"vis. model"

/**
 * Field name for the Configuration document describing the currently loaded
 * trace data file.
 */
#define KS_DATA_SOURCE_NAME		"trace data"

/**
 * Field name for the Configuration document describing all currently loaded
 * data streams.
 */
#define KS_DSTREAMS_NAME		"data streams"

struct kshark_config_doc *
kshark_config_alloc(enum kshark_config_formats);

struct kshark_config_doc *
kshark_config_new(const char *type, enum kshark_config_formats);

void kshark_free_config_doc(struct kshark_config_doc *conf);

struct kshark_config_doc *
kshark_record_config_new(enum kshark_config_formats);

struct kshark_config_doc *
kshark_stream_config_new(enum kshark_config_formats);

struct kshark_config_doc *
kshark_filter_config_new(enum kshark_config_formats);

struct kshark_config_doc *
kshark_session_config_new(enum kshark_config_formats format);

struct kshark_config_doc *kshark_string_config_alloc(void);

bool kshark_type_check(struct kshark_config_doc *conf, const char *type);

bool kshark_config_doc_add(struct kshark_config_doc *conf,
			   const char *key,
			   struct kshark_config_doc *val);

bool kshark_config_doc_get(struct kshark_config_doc *conf,
			   const char *key,
			   struct kshark_config_doc *val);

struct kshark_trace_histo;

struct kshark_config_doc *
kshark_export_trace_file(const char *file, const char *name,
			 enum kshark_config_formats format);

int kshark_import_trace_file(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc *conf);

struct kshark_config_doc *
kshark_export_plugin_file(struct kshark_plugin_list *plugin,
			  enum kshark_config_formats format);

struct kshark_config_doc *
kshark_export_all_plugins(struct kshark_context *kshark_ctx,
			  enum kshark_config_formats format);

bool kshark_import_all_plugins(struct kshark_context *kshark_ctx,
			       struct kshark_config_doc *conf);

struct kshark_config_doc *
kshark_export_stream_plugins(struct kshark_data_stream *stream,
			     enum kshark_config_formats format);

bool kshark_import_stream_plugins(struct kshark_context *kshark_ctx,
				  struct kshark_data_stream *stream,
				  struct kshark_config_doc *conf);

struct kshark_config_doc *
kshark_export_model(struct kshark_trace_histo *histo,
		     enum kshark_config_formats format);

bool kshark_import_model(struct kshark_trace_histo *histo,
			 struct kshark_config_doc *conf);

bool kshark_export_adv_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc **conf);

bool kshark_import_adv_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc *conf);

bool kshark_export_event_filter(struct kshark_data_stream *stream,
				enum kshark_filter_type filter_type,
				const char *filter_name,
				struct kshark_config_doc *conf);

int kshark_import_event_filter(struct kshark_data_stream *stream,
			       enum kshark_filter_type filter_type,
			       const char *filter_name,
			       struct kshark_config_doc *conf);

bool kshark_export_user_mask(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc **conf);

bool kshark_import_user_mask(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc *conf);

bool kshark_export_filter_array(struct kshark_hash_id *filter,
				const char *filter_name,
				struct kshark_config_doc *conf);

bool kshark_import_filter_array(struct kshark_hash_id *filter,
				const char *filter_name,
				struct kshark_config_doc *conf);

bool kshark_export_all_event_filters(struct kshark_context *kshark_ctx, int sd,
				     struct kshark_config_doc **conf);

bool kshark_export_all_task_filters(struct kshark_context *kshark_ctx, int sd,
				    struct kshark_config_doc **conf);

bool kshark_export_all_cpu_filters(struct kshark_context *kshark_ctx, int sd,
				   struct kshark_config_doc **conf);

struct kshark_config_doc *
kshark_export_all_filters(struct kshark_context *kshark_ctx, int sd,
			  enum kshark_config_formats format);

bool kshark_import_all_event_filters(struct kshark_context *kshark_ctx, int sd,
				     struct kshark_config_doc *conf);

bool kshark_import_all_task_filters(struct kshark_context *kshark_ctx, int sd,
				    struct kshark_config_doc *conf);

bool kshark_import_all_cpu_filters(struct kshark_context *kshark_ctx, int sd,
				   struct kshark_config_doc *conf);

bool kshark_import_all_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc *conf);

struct kshark_config_doc *
kshark_export_dstream(struct kshark_context *kshark_ctx, int sd,
		      enum kshark_config_formats format);

int kshark_import_dstream(struct kshark_context *kshark_ctx,
			  struct kshark_config_doc *conf);

bool kshark_export_all_dstreams(struct kshark_context *kshark_ctx,
				struct kshark_config_doc **conf);

ssize_t kshark_import_all_dstreams(struct kshark_context *kshark_ctx,
				   struct kshark_config_doc *conf,
				   struct kshark_entry ***data_rows);


bool kshark_save_config_file(const char *file_name,
			     struct kshark_config_doc *conf);

struct kshark_config_doc *kshark_open_config_file(const char *file_name,
						  const char *type);

struct kshark_config_doc *kshark_json_to_conf(struct json_object *jobj);

void kshark_offset_calib(int64_t *ts, int64_t *atgv);

void kshark_set_clock_offset(struct kshark_context *kshark_ctx,
			     struct kshark_entry **entries, size_t size,
			     int sd, int64_t offset);

/** Structure representing a data set made of KernelShark entries. */
struct kshark_entry_data_set {
	/** Array of entries pointers. */
	struct kshark_entry **data;

	/** The size of the data set. */
	ssize_t n_rows;
};

struct kshark_entry **
kshark_merge_data_entries(struct kshark_entry_data_set *buffers,
			  int n_buffers);

ssize_t kshark_load_all_entries(struct kshark_context *kshark_ctx,
				struct kshark_entry ***data_rows);

ssize_t kshark_append_all_entries(struct kshark_context *kshark_ctx,
				  struct kshark_entry **prior_data,
				  ssize_t n_prior_rows,
				  int first_streams,
				  struct kshark_entry ***merged_data);

bool kshark_data_matrix_alloc(size_t n_rows, int16_t **event_array,
					     int16_t **cpu_array,
					     int32_t **pid_array,
					     int64_t **offset_array,
					     int64_t **ts_array);

/** Structure representing a data set made of data columns (arrays). */
struct kshark_matrix_data_set {
	/** Event Id column. */
	int16_t *event_array;

	/** CPU Id column. */
	int16_t *cpu_array;

	/** PID column. */
	int32_t *pid_array;

	/** Record offset column. */
	int64_t *offset_array;

	/** Timestamp column. */
	int64_t *ts_array;

	/** The size of the data set. */
	ssize_t n_rows;
};

struct kshark_matrix_data_set
kshark_merge_data_matrices(struct kshark_matrix_data_set *buffers,
			   int n_buffers);

/**
 * Structure used to store the data of a kshark_entry plus one additional
 * 64 bit integer data field.
 */
struct kshark_data_field_int64 {
	/** The entry object holding the basic data of the trace record. */
	struct kshark_entry	*entry;

	/** Additional 64 bit integer data field. */
	int64_t			field;
};

/** The capacity of the kshark_data_container object after initialization. */
#define KS_CONTAINER_DEFAULT_SIZE	1024

/** Structure used to store an array of entries and data fields. */
struct kshark_data_container {
	/** An array of kshark_data_field_int64 objects. */
	struct kshark_data_field_int64	**data;

	/** The total number of kshark_data_field_int64 objects stored. */
	ssize_t		size;

	/** The memory capacity of the container. */
	ssize_t		capacity;

	/** Is sorted in time. */
	bool		sorted;
};

struct kshark_data_container *kshark_init_data_container();

void kshark_free_data_container(struct kshark_data_container *container);

ssize_t kshark_data_container_append(struct kshark_data_container *container,
				     struct kshark_entry *entry, int64_t field);

void kshark_data_container_sort(struct kshark_data_container *container);

ssize_t kshark_find_entry_field_by_time(int64_t time,
					struct kshark_data_field_int64 **data,
					size_t l, size_t h);

#ifdef __cplusplus
}
#endif

#endif // _LIB_KSHARK_H
