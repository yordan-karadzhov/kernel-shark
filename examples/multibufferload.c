#include <stdio.h>
#include <stdlib.h>

#include "libkshark.h"
#include "libkshark-tepdata.h"

const char *default_file = "trace.dat";

int main(int argc, char **argv)
{
	struct kshark_context *kshark_ctx;
	struct kshark_entry **data = NULL;
	ssize_t r, n_rows;
	int sd;

	/* Create a new kshark session. */
	kshark_ctx = NULL;
	if (!kshark_instance(&kshark_ctx))
		return 1;

	/* Open a trace data file produced by trace-cmd. */
	if (argc > 1)
		sd = kshark_open(kshark_ctx, argv[1]);
	else
		sd = kshark_open(kshark_ctx, default_file);

	if (sd < 0) {
		kshark_free(kshark_ctx);
		return 1;
	}

	/* Initialize data streams for all buffers in this file. */
	kshark_tep_init_all_buffers(kshark_ctx, sd);

	/* Load all buffers. */
	n_rows = kshark_load_all_entries(kshark_ctx, &data);

	/* Print to the screen the first 20 entries. */
	for (r = 0; r < 20; ++r)
		kshark_print_entry(data[r]);

	/* Free the memory. */
	for (r = 0; r < n_rows; ++r)
		free(data[r]);
	free(data);

	kshark_close_all(kshark_ctx);

	/* Close the session. */
	kshark_free(kshark_ctx);

	return 0;
}
