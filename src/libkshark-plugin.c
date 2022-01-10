// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

 /**
  *  @file    libkshark-plugin.c
  *  @brief   KernelShark plugins.
  */

// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <errno.h>

// KernelShark
#include "libkshark-plugin.h"
#include "libkshark-tepdata.h"
#include "libkshark.h"

static struct kshark_event_proc_handler *
data_event_handler_alloc(int event_id,
			 kshark_plugin_event_handler_func evt_func)
{
	struct kshark_event_proc_handler *handler = malloc(sizeof(*handler));

	if (!handler) {
		fputs("failed to allocate memory for event handler\n", stderr);
		return NULL;
	}

	handler->next = NULL;
	handler->id = event_id;
	handler->event_func = evt_func;

	return handler;
}

static struct kshark_draw_handler *
data_draw_handler_alloc(kshark_plugin_draw_handler_func draw_func)
{
	struct kshark_draw_handler *handler = malloc(sizeof(*handler));

	if (!handler) {
		fputs("failed to allocate memory for draw handler\n", stderr);
		return NULL;
	}

	handler->next = NULL;
	handler->draw_func = draw_func;

	return handler;
}

/**
 * @brief Search the list of event handlers for a handle associated with a
 *	  given event type.
 *
 * @param handlers: Input location for the Event handler list.
 * @param event_id: Event Id to search for.
 */
struct kshark_event_proc_handler *
kshark_find_event_handler(struct kshark_event_proc_handler *handlers, int event_id)
{
	for (; handlers; handlers = handlers->next)
		if (handlers->id == event_id)
			return handlers;

	return NULL;
}

/**
 * @brief Add new event handler to an existing list of handlers.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param event_id: Event Id.
 * @param evt_func: Input location for an Event action provided by the plugin.
 *
 * @returns Zero on success, or a negative error code on failure.
 */
int kshark_register_event_handler(struct kshark_data_stream *stream,
				  int event_id,
				  kshark_plugin_event_handler_func evt_func)
{
	struct kshark_event_proc_handler *handler =
		data_event_handler_alloc(event_id, evt_func);

	if(!handler)
		return -ENOMEM;

	handler->next = stream->event_handlers;
	stream->event_handlers = handler;

	return 0;
}

/**
 * @brief Search the list for a specific plugin handle. If such a plugin handle
 *	  exists, unregister (remove and free) this handle from the list.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param event_id: Event Id of the plugin handler to be unregistered.
 * @param evt_func: Event action function to be unregistered.
 */
int kshark_unregister_event_handler(struct kshark_data_stream *stream,
				    int event_id,
				    kshark_plugin_event_handler_func evt_func)
{
	struct kshark_event_proc_handler **last;

	if (stream->stream_id < 0)
		return 0;

	for (last = &stream->event_handlers; *last; last = &(*last)->next) {
		if ((*last)->id == event_id &&
		    (*last)->event_func == evt_func) {
			struct kshark_event_proc_handler *this_handler;
			this_handler = *last;
			*last = this_handler->next;
			free(this_handler);

			return 0;
		}
	}

	return -EFAULT;
}

/**
 * @brief Free all Event handlers in a given list.
 *
 * @param handlers: Input location for the Event handler list.
 */
void kshark_free_event_handler_list(struct kshark_event_proc_handler *handlers)
{
	struct kshark_event_proc_handler *last;

	while (handlers) {
		last = handlers;
		handlers = handlers->next;
		free(last);
	}
}

/**
 * @brief Add new event handler to an existing list of handlers.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param draw_func: Input location for a Draw action provided by the plugin.
 *
 * @returns Zero on success, or a negative error code on failure.
 */
int kshark_register_draw_handler(struct kshark_data_stream *stream,
				 kshark_plugin_draw_handler_func draw_func)
{
	struct kshark_draw_handler *handler = data_draw_handler_alloc(draw_func);

	if(!handler)
		return -ENOMEM;

	handler->next = stream->draw_handlers;
	stream->draw_handlers = handler;

	return 0;
}

/**
 * @brief Search the list for a specific plugin handle. If such a plugin handle
 *	  exists, unregister (remove and free) this handle from the list.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param draw_func: Draw action function to be unregistered.
 */
void kshark_unregister_draw_handler(struct kshark_data_stream *stream,
				    kshark_plugin_draw_handler_func draw_func)
{
	struct kshark_draw_handler **last;

	if (stream->stream_id < 0)
		return;

	for (last = &stream->draw_handlers; *last; last = &(*last)->next) {
		if ((*last)->draw_func == draw_func) {
			struct kshark_draw_handler *this_handler;
			this_handler = *last;
			*last = this_handler->next;
			free(this_handler);

			return;
		}
	}
}

/**
 * @brief Free all DRaw handlers in a given list.
 *
 * @param handlers: Input location for the Draw handler list.
 */
void kshark_free_draw_handler_list(struct kshark_draw_handler *handlers)
{
	struct kshark_draw_handler *last;

	while (handlers) {
		last = handlers;
		handlers = handlers->next;
		free(last);
	}
}

/** Close and free this plugin. */
static void free_plugin(struct kshark_plugin_list *plugin)
{
	if (plugin->handle)
		dlclose(plugin->handle);

	if (plugin->process_interface) {
		free(plugin->process_interface->name);
		free(plugin->process_interface);
	}

	if (plugin->readout_interface) {
		free(plugin->readout_interface->name);
		free(plugin->readout_interface);
	}

	free(plugin->name);
	free(plugin->file);
	free(plugin);
}

/**
 * @brief Allocate memory for a new plugin. Add this plugin to the list of
 *	  plugins.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param name: The name of the plugin to register.
 * @param file: The plugin object file to load.
 *
 * @returns The plugin object on success, or NULL on failure.
 */
struct kshark_plugin_list *
kshark_register_plugin(struct kshark_context *kshark_ctx,
		       const char *name,
		       const char *file)
{
	kshark_plugin_load_func init_func, close_func;
	kshark_check_data_func check_func;
	kshark_format_func format_func;
	struct kshark_plugin_list *plugin;
	struct stat st;
	int ret;

	printf("loading plugin \"%s\" from %s\n", name, file);

	plugin = kshark_find_plugin(kshark_ctx->plugins, file);
	if(plugin) {
		fputs("the plugin is already loaded.\n", stderr);
		return NULL;
	}

	ret = stat(file, &st);
	if (ret < 0) {
		fprintf(stderr, "plugin %s not found.\n", file);
		return NULL;
	}

	plugin = calloc(1, sizeof(struct kshark_plugin_list));
	if (!plugin) {
		fputs("failed to allocate memory for plugin.\n", stderr);
		return NULL;
	}

	plugin->handle = dlopen(file, RTLD_NOW | RTLD_GLOBAL);
	if (!plugin->handle) {
		fprintf(stderr,
			"failed to open plugin file.\n%s\n",
			dlerror());
		goto fail;
	}

	plugin->file = strdup(file);
	plugin->name = strdup(name);
	if (!plugin->file|| !plugin->name)
		goto fail;

	plugin->ctrl_interface =
		dlsym(plugin->handle, KSHARK_MENU_PLUGIN_INITIALIZER_NAME);

	init_func = dlsym(plugin->handle,
			  KSHARK_PLOT_PLUGIN_INITIALIZER_NAME);

	close_func = dlsym(plugin->handle,
			   KSHARK_PLOT_PLUGIN_DEINITIALIZER_NAME);

	if (init_func && close_func) {
		plugin->process_interface =
			calloc(1, sizeof(*plugin->process_interface));

		if (!plugin->process_interface)
			goto fail;

		plugin->process_interface->name = strdup(plugin->name);
		if (!plugin->process_interface->name)
			goto fail;

		plugin->process_interface->init = init_func;
		plugin->process_interface->close = close_func;
	} else if (init_func || close_func) {
		fprintf(stderr,
			"incomplete draw interface found (will be ignored).\n%s\n",
			dlerror());
	}

	init_func = dlsym(plugin->handle,
			  KSHARK_INPUT_INITIALIZER_NAME);

	close_func = dlsym(plugin->handle,
			   KSHARK_INPUT_DEINITIALIZER_NAME);

	check_func = dlsym(plugin->handle,
			   KSHARK_INPUT_CHECK_NAME);

	format_func = dlsym(plugin->handle,
			    KSHARK_INPUT_FORMAT_NAME);

	if (init_func && close_func && check_func && format_func) {
		plugin->readout_interface =
			calloc(1, sizeof(*plugin->readout_interface));

		if (!plugin->readout_interface)
			goto fail;

		plugin->readout_interface->name = strdup(plugin->name);
		if (!plugin->readout_interface->name)
			goto fail;

		plugin->readout_interface->init = init_func;
		plugin->readout_interface->close = close_func;
		plugin->readout_interface->check_data = check_func;

		kshark_set_data_format(plugin->readout_interface->data_format,
				       format_func());

		kshark_register_input(kshark_ctx, plugin->readout_interface);
	} else if (init_func || close_func || check_func) {
		fprintf(stderr,
			"incomplete input interface found (will be ignored).\n%s\n",
			dlerror());
	}

	if (!plugin->process_interface &&
	    !plugin->readout_interface &&
	    !plugin->ctrl_interface) {
		fputs("no interfaces found in this plugin.\n", stderr);
		goto fail;
	}

	plugin->next = kshark_ctx->plugins;
	kshark_ctx->plugins = plugin;
	kshark_ctx->n_plugins++;

	return plugin;

 fail:
	fprintf(stderr, "cannot load plugin '%s'\n", file);

	if (plugin->handle)
		dlclose(plugin->handle);

	free_plugin(plugin);

	return NULL;
}

/**
 * @brief Unrgister a plugin.
 *
 * @param kshark_ctx: Input location for the session context pointer.
 * @param name: The name of the plugin to unregister.
 * @param file: The plugin object file to unregister.
 */
void kshark_unregister_plugin(struct kshark_context *kshark_ctx,
			      const char *name,
			      const char *file)
{
	struct kshark_plugin_list **last;

	for (last = &kshark_ctx->plugins; *last; last = &(*last)->next) {
		if (strcmp((*last)->process_interface->name, name) == 0 &&
		    strcmp((*last)->file, file) == 0) {
			struct kshark_plugin_list *this_plugin;

			this_plugin = *last;
			*last = this_plugin->next;
			free_plugin(this_plugin);

			kshark_ctx->n_plugins--;

			return;
		}
	}
}

/**
 * @brief Free all plugins in a given list.
 *
 * @param plugins: Input location for the plugins list.
 */
void kshark_free_plugin_list(struct kshark_plugin_list *plugins)
{
	struct kshark_data_stream stream;
	struct kshark_plugin_list *last;

	stream.stream_id = KS_PLUGIN_CONTEXT_FREE;
	while (plugins) {
		last = plugins;
		plugins = plugins->next;

		if (last->process_interface)
			last->process_interface->close(&stream);
		free_plugin(last);
	}
}

/**
 * @brief Register a data readout interface (input).
 *
 * @param kshark_ctx: Input location for the context pointer.
 * @param plugin: Input location for the data readout interface (input).
 */
struct kshark_dri_list *
kshark_register_input(struct kshark_context *kshark_ctx,
		      struct kshark_dri *plugin)
{
	struct kshark_dri_list *input;
	struct kshark_dri_list **last;
	const char *name_err, *format_err;

	if (strcmp(plugin->data_format, TEP_DATA_FORMAT_IDENTIFIER) == 0) {
		name_err = "built in";
		format_err = TEP_DATA_FORMAT_IDENTIFIER;
			goto conflict;
	}

	for (last = &kshark_ctx->inputs; *last; last = &(*last)->next)
		if (strcmp((*last)->interface->name, plugin->name) == 0 ||
		    strcmp((*last)->interface->data_format, plugin->data_format) == 0 ) {
			name_err = (*last)->interface->name;
			format_err = (*last)->interface->data_format;
			goto conflict;
		}

	input = calloc(1, sizeof(*input));
	if (!input) {
		fputs("failed to allocate memory for readout plugin.\n", stderr);
		return NULL;
	}

	input->interface = plugin;
	input->next = kshark_ctx->inputs;
	kshark_ctx->inputs = input;
	kshark_ctx->n_inputs++;
	return input;

 conflict:
	fprintf(stderr,
		"Failed to register readout plugin (name=\'%s\', data_format=\'%s\')\n",
		plugin->name, plugin->data_format);

	fprintf(stderr,
		"Conflict with registered readout  (name=\'%s\', data_format=\'%s\')\n",
		name_err, format_err);

	return NULL;
}

/**
 * @brief Unrgister a data readout interface (input).
 *
 * @param kshark_ctx: Input location for the context pointer.
 * @param name: The data readout's name.
 */
void kshark_unregister_input(struct kshark_context *kshark_ctx,
			     const char *name)
{
	struct kshark_dri_list **last;

	for (last = &kshark_ctx->inputs; *last; last = &(*last)->next) {
		if (strcmp((*last)->interface->name, name) == 0) {
			struct kshark_dri_list *this_input;
			this_input = *last;
			*last = this_input->next;

			free(this_input);
			kshark_ctx->n_inputs--;

			return;
		}
	}
}

/**
 * @brief Free a list of plugin interfaces.
 *
 * @param plugins: Input location for the plugins list.
 */
void
kshark_free_dpi_list(struct kshark_dpi_list *plugins)
{
	struct kshark_dpi_list *last;

	while (plugins) {
		last = plugins;
		plugins = plugins->next;
		free(last);
	}
}

/**
 * @brief Find a plugin by its library file.
 *
 * @param plugins: A list of plugins to search in.
 * @param lib: The plugin object file to load.
 *
 * @returns The plugin object on success, or NULL on failure.
 */
struct kshark_plugin_list *
kshark_find_plugin(struct kshark_plugin_list *plugins, const char *lib)
{
	for (; plugins; plugins = plugins->next)
		if (strcmp(plugins->file, lib) == 0)
			return plugins;

	return NULL;
}

/**
 * @brief Find a plugin by its name.
 *
 * @param plugins: A list of plugins to search in.
 * @param name: The plugin object file to load.
 *
 * @returns The plugin object on success, or NULL on failure.
 */
struct kshark_plugin_list *
kshark_find_plugin_by_name(struct kshark_plugin_list *plugins,
			   const char *name)
{
	for (; plugins; plugins = plugins->next)
		if (strcmp(plugins->name, name) == 0)
			return plugins;

	return NULL;
}

/**
 * @brief Register plugin to a given data stream without initializing it.
 *	  In order to initialize this plugin use kshark_handle_dpi() or
 *	  kshark_handle_all_dpis().
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param plugin: Input location for the data processing interface.
 * @param active: If false, the plugin will be registered but disabled.
 *		  Otherwise the plugin will be active.
 *
 * @returns The plugin object on success, or NULL on failure.
 */
struct kshark_dpi_list *
kshark_register_plugin_to_stream(struct kshark_data_stream *stream,
				 struct kshark_dpi *plugin,
				 bool active)
{
	struct kshark_dpi_list *plugin_list = stream->plugins;

	/* Check if the plugin is already registered to this stream. */
	while (plugin_list) {
		if (strcmp(plugin_list->interface->name, plugin->name) == 0 &&
		    plugin_list->interface->init == plugin->init &&
		    plugin_list->interface->close == plugin->close) {
			/*
			 * The plugin has been registered already. Check if it
			 * is initialized and if this is the case, close the
			 * existing initialization. This way we guarantee a
			 * clean new initialization from kshark_handle_dpi()
			 * or kshark_handle_all_dpis().
			 */
			if (plugin_list->status & KSHARK_PLUGIN_LOADED)
				kshark_handle_dpi(stream, plugin_list,
						  KSHARK_PLUGIN_CLOSE);

			plugin_list->status =
				active ? KSHARK_PLUGIN_ENABLED : 0;

			return plugin_list;
		}

		plugin_list = plugin_list->next;
	}

	plugin_list = calloc(1, sizeof(*plugin_list));
	plugin_list->interface = plugin;

	if (active)
		plugin_list->status = KSHARK_PLUGIN_ENABLED;

	plugin_list->next = stream->plugins;
	stream->plugins = plugin_list;
	stream->n_plugins++;

	return plugin_list;
}

/**
 * @brief Unregister plugin to a given data stream.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param plugin: Input location for the data processing interface.
 */
void kshark_unregister_plugin_from_stream(struct kshark_data_stream *stream,
				          struct kshark_dpi *plugin)
{
	struct kshark_dpi_list **last;

	for (last = &stream->plugins; *last; last = &(*last)->next) {
		if ((*last)->interface->init == plugin->init &&
		    (*last)->interface->close == plugin->close &&
		    strcmp((*last)->interface->name, plugin->name) == 0) {
			struct kshark_dpi_list *this_plugin;

			this_plugin = *last;
			*last = this_plugin->next;
			this_plugin->interface->close(stream);
			free(this_plugin);

			stream->n_plugins--;

			return;
		}
	}
}

static int plugin_init(struct kshark_data_stream *stream,
			struct kshark_dpi_list *plugin)
{
	int handler_count = plugin->interface->init(stream);

	if (handler_count > 0) {
		plugin->status &= ~KSHARK_PLUGIN_FAILED;
		plugin->status |= KSHARK_PLUGIN_LOADED;
	} else {
		if (strcmp(stream->name, KS_UNNAMED) == 0) {
			fprintf(stderr,
			"plugin \"%s\" failed to initialize on stream %s\n",
			plugin->interface->name,
			stream->file);
		} else {
		fprintf(stderr,
			"plugin \"%s\" failed to initialize on stream %s:%s\n",
			plugin->interface->name,
			stream->file,
			stream->name);
		}

		plugin->status |= KSHARK_PLUGIN_FAILED;
		plugin->status &= ~KSHARK_PLUGIN_LOADED;
	}

	return handler_count;
}

static int plugin_close(struct kshark_data_stream *stream,
			 struct kshark_dpi_list *plugin)
{
	int handler_count = plugin->interface->close(stream);

	plugin->status &= ~KSHARK_PLUGIN_LOADED;

	return handler_count;
}

/**
 * @brief Use this function to initialize/update/deinitialize a plugin for
 *	  a given Data stream.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param plugin: The plugin to be handled.
 * @param task_id: Action identifier specifying the action to be executed.
 *
 * @returns The number of successful added/removed plugin handlers on success,
 *	    or a negative error code on failure.
 */
int kshark_handle_dpi(struct kshark_data_stream *stream,
		      struct kshark_dpi_list *plugin,
		      enum kshark_plugin_actions task_id)
{
	int handler_count = 0;

	switch (task_id) {
	case KSHARK_PLUGIN_INIT:
		if (plugin->status & KSHARK_PLUGIN_ENABLED)
			handler_count += plugin_init(stream, plugin);

		break;

	case KSHARK_PLUGIN_UPDATE:
		if (plugin->status & KSHARK_PLUGIN_LOADED)
			handler_count -= plugin_close(stream, plugin);

		plugin->status &= ~KSHARK_PLUGIN_FAILED;

		if (plugin->status & KSHARK_PLUGIN_ENABLED)
			handler_count += plugin_init(stream, plugin);

		break;

	case KSHARK_PLUGIN_CLOSE:
		if (plugin->status & KSHARK_PLUGIN_LOADED)
			handler_count -= plugin_close(stream, plugin);

		plugin->status &= ~KSHARK_PLUGIN_FAILED;
		break;

	default:
		return -EINVAL;
	}

	return handler_count;
}

/**
 * @brief Use this function to initialize/update/deinitialize all registered
 *	  data processing plugins for a given Data stream.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param task_id: Action identifier specifying the action to be executed. Can
 *		   be KSHARK_PLUGIN_INIT, KSHARK_PLUGIN_UPDATE or
 *		   KSHARK_PLUGIN_CLOSE.
 *
 * @returns The number of successful added/removed plugin handlers on success,
 *	    or a negative error code on failure.
 */
int kshark_handle_all_dpis(struct kshark_data_stream *stream,
			   enum kshark_plugin_actions task_id)
{
	struct kshark_dpi_list *plugin;
	int handler_count = 0;

	for (plugin = stream->plugins; plugin; plugin = plugin->next)
		handler_count +=
			kshark_handle_dpi(stream, plugin, task_id);

	return handler_count;
}

/**
 * @brief Free all readout interfaces in a given list.
 *
 * @param inputs: Input location for the inputs list.
 */
void kshark_free_dri_list(struct kshark_dri_list *inputs)
{
	struct kshark_dri_list *last;

	while (inputs) {
		last = inputs;
		inputs = inputs->next;

		free(last);
	}
}
