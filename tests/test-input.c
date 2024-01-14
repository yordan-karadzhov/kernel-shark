
// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"

static ssize_t load_entries(struct kshark_data_stream *stream,
			    struct kshark_context *kshark_ctx,
			    struct kshark_entry ***data_rows)
{
	struct kshark_entry **rows;
	ssize_t total = 200, i;

	rows = calloc(total, sizeof(struct kshark_entry *));
	for (i = 0; i < total; ++i) {
		rows[i] = calloc(1, sizeof(struct kshark_entry));
		rows[i]->ts = 1000000 + i * 10000;
		rows[i]->stream_id = stream->stream_id;
		rows[i]->event_id = i % 5;
		rows[i]->pid = 10 + i % 2;
		rows[i]->cpu = i % 2;
		rows[i]->visible = 0xff;
	}

	*data_rows = rows;
	return total;
}

static char *dump_entry(struct kshark_data_stream *stream,
			const struct kshark_entry *entry)
{
	char *entry_str;
	int ret;

	ret = asprintf(&entry_str, "e: time=%li evt=%i s_id=%i", entry->ts,
								 entry->event_id,
								 entry->stream_id);

	if (ret <= 0)
		return NULL;

	return entry_str;
}

static const char *format_name = "format_a";

const char *KSHARK_INPUT_FORMAT()
{
	return format_name;
}

bool KSHARK_INPUT_CHECK(const char *file, char **format)
{
	char *ext = strrchr(file, '.');

	if (ext && strcmp(ext, ".ta") == 0)
		return true;

	return false;
}

static int get_pid(struct kshark_data_stream *stream,
		   const struct kshark_entry *entry)
{
	return entry->pid;
}

static char *get_task(struct kshark_data_stream *stream,
		      const struct kshark_entry *entry)
{
	char *entry_str;
	int ret;

	ret = asprintf(&entry_str, "test_a/test");

	if (ret <= 0)
		return NULL;

	return entry_str;
}

static char *get_event_name(struct kshark_data_stream *stream,
			    const struct kshark_entry *entry)
{
	char *evt_str;
	int ret;

	ret = asprintf(&evt_str, "test_a/event-%i", entry->event_id);

	if (ret <= 0)
		return NULL;

	return evt_str;
}

int KSHARK_INPUT_INITIALIZER(struct kshark_data_stream *stream)
{
	struct kshark_generic_stream_interface *interface;

	stream->interface = interface = calloc(1, sizeof(*interface));
	if (!interface)
		return -ENOMEM;

	interface->type = KS_GENERIC_DATA_INTERFACE;

	stream->n_cpus = 2;
	stream->n_events = 5;
	stream->idle_pid = 0;

	kshark_hash_id_add(stream->tasks, 10);
	kshark_hash_id_add(stream->tasks, 11);

	interface->get_pid = get_pid;
	interface->get_task = get_task;
	interface->get_event_name = get_event_name;

	interface->dump_entry = dump_entry;
	interface->load_entries = load_entries;

	return 0;
}

void KSHARK_INPUT_DEINITIALIZER(struct kshark_data_stream *stream)
{}
