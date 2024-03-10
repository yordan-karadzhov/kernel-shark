// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

 /**
  *  @file    libkshark-configio.c
  *  @brief   Json Configuration I/O.
  */

// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-model.h"
#include "libkshark-plugin.h"
#include "libkshark-tepdata.h"

static struct json_object *kshark_json_config_alloc(const char *type)
{
	json_object *jobj, *jtype;

	jobj = json_object_new_object();
	jtype = json_object_new_string(type);

	if (!jobj || !jtype)
		goto fail;

	/* Set the type of this Json document. */
	json_object_object_add(jobj, "type", jtype);

	return jobj;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jobj);
	json_object_put(jtype);

	return NULL;
}

/**
 * @brief Allocate kshark_config_doc and set its format.
 *
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json and String formats are supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    free() to free the object.
 */
struct kshark_config_doc *
kshark_config_alloc(enum kshark_config_formats format)
{
	struct kshark_config_doc *doc;

	switch (format) {
	case KS_CONFIG_AUTO:
	case KS_CONFIG_JSON:
	case KS_CONFIG_STRING:
		doc = malloc(sizeof(*doc));
		if (!doc)
			goto fail;

		doc->format = format;
		doc->conf_doc = NULL;
		return doc;
	default:
		fprintf(stderr, "Document format %d not supported\n",
		format);
	}

	return NULL;

 fail:
	fprintf(stderr, "Failed to allocate memory for kshark_config_doc.\n");
	return NULL;
}

/**
 * @brief Create an empty Configuration document and set its format and type.
 *
 * @param type: String describing the type of the document,
 *		e.g. "kshark.config.record" or "kshark.config.filter".
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_config_new(const char *type, enum kshark_config_formats format)
{
	struct kshark_config_doc *doc = NULL;

	if (format == KS_CONFIG_AUTO)
		format = KS_CONFIG_JSON;

	switch (format) {
	case KS_CONFIG_JSON:
		doc = kshark_config_alloc(format);
		if (doc) {
			doc->conf_doc = kshark_json_config_alloc(type);
			if (!doc->conf_doc) {
				free(doc);
				doc = NULL;
			}
		}

		break;
	case KS_CONFIG_STRING:
		doc = kshark_config_alloc(format);
		break;
	default:
		fprintf(stderr, "Document format %d not supported\n",
			format);
		return NULL;
	}

	return doc;
}

/**
 * @brief Free the Configuration document.
 *
 * @param conf: Input location for the kshark_config_doc instance. It is safe
 *	        to pass a NULL value.
 */
void kshark_free_config_doc(struct kshark_config_doc *conf)
{
	if (!conf)
		return;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		json_object_put(conf->conf_doc);
		break;
	case KS_CONFIG_STRING:
		free(conf->conf_doc);
		break;
	}

	free(conf);
}

/**
 * @brief Use an existing Json document to create a new KernelShark
 *	  Configuration document.
 *
 * @param jobj: Input location for the json_object instance.
 *
 * @returns shark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *kshark_json_to_conf(struct json_object *jobj)
{
	struct kshark_config_doc *conf = kshark_config_alloc(KS_CONFIG_JSON);

	if (conf)
		conf->conf_doc = jobj;

	return conf;
}

/**
 * @brief Use an existing string to create a new KernelShark Configuration
 * document.
 *
 * @param val: Input location for the string.
 *
 * @returns shark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *kshark_string_to_conf(const char* val)
{
	struct kshark_config_doc *conf;
	char *str;

	conf = kshark_config_alloc(KS_CONFIG_STRING);
	if (conf) {
		if (asprintf(&str, "%s", val) > 0) {
			conf->conf_doc = str;
		} else {
			fprintf(stderr,
				"Failed to allocate string conf. doc.\n");
			free(conf);
			conf = NULL;
		}
	}

	return conf;
}

/**
 * @brief Add a field to a KernelShark Configuration document.
 *
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 * @param key: The name of the field.
 * @param val: Input location for the kshark_config_doc to be added. Currently
 *	       only Json and String formats are supported. Pass KS_CONFIG_AUTO
 *	       if you want "val" to have the same fornat as "conf". Upon
 *	       calling this function, the ownership of "val" transfers to
 *	       "conf".
 *
 * @returns True on success, otherwise False.
 */
bool kshark_config_doc_add(struct kshark_config_doc *conf,
			   const char *key,
			   struct kshark_config_doc *val)
{
	struct json_object *jobj = NULL;

	if (!conf || !val)
		return false;

	if (val->format == KS_CONFIG_AUTO)
		val->format = conf->format;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		switch (val->format) {
		case KS_CONFIG_JSON:
			json_object_object_add(conf->conf_doc, key,
					       val->conf_doc);
			break;

		case KS_CONFIG_STRING:
			jobj = json_object_new_string(val->conf_doc);
			if (!jobj)
				goto fail;

			json_object_object_add(conf->conf_doc, key, jobj);
			break;

		default:
			fprintf(stderr, "Value format %d not supported\n",
				val->format);
			return false;
		}

		free(val);
		break;
	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jobj);

	return false;
}

static bool get_jval(struct kshark_config_doc *conf,
		     const char *key, void **val)
{
	return json_object_object_get_ex(conf->conf_doc, key,
					 (json_object **) val);
}

/**
 * @brief Get the KernelShark Configuration document associate with a given
 *	  field name.
 *
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 * @param key: The name of the field.
 * @param val: Output location for the kshark_config_doc instance containing
 *	       the field. Currently only Json and String formats are supported.
 *
 * @returns True, if the key exists, otherwise False.
 */
bool kshark_config_doc_get(struct kshark_config_doc *conf,
			   const char *key,
			   struct kshark_config_doc *val)
{
	struct kshark_config_doc *tmp;

	if (!conf || !val)
		return false;

	if (val->format == KS_CONFIG_AUTO)
		val->format = conf->format;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		switch (val->format) {
		case KS_CONFIG_JSON:
			json_object_put(val->conf_doc);
			if (!get_jval(conf, key, &val->conf_doc))
				goto not_found;

			return true;
		case KS_CONFIG_STRING:
			tmp = kshark_config_alloc(KS_CONFIG_AUTO);
			if (!tmp)
				goto fail;

			if (!get_jval(conf, key, &tmp->conf_doc))
				goto not_found;

			val->conf_doc =
				(char *) json_object_get_string(tmp->conf_doc);
			free(tmp);

			return true;
		default:
			fprintf(stderr, "Value format %d not supported\n",
				val->format);
			return false;
		}

		break;
	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}

	return true;

 fail:
	fprintf(stderr, "Failed to get config. document <%s>.\n", key);

 not_found:
	return false;
}

/**
 * @brief Create an empty Record Configuration document. The type description
 *	  is set to "kshark.config.record".
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_record_config_new(enum kshark_config_formats format)
{
	return kshark_config_new("kshark.config.record", format);
}

/**
 * @brief Create an empty Data Stream Configuration document. The type
 * 	  description is set to "kshark.config.stream".
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_stream_config_new(enum kshark_config_formats format)
{
	return kshark_config_new("kshark.config.stream", format);
}

/**
 * @brief Create an empty Filter Configuration document. The type description
 *	  is set to "kshark.config.filter".
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_filter_config_new(enum kshark_config_formats format)
{
	return kshark_config_new("kshark.config.filter", format);
}

/**
 * @brief Create an empty Session Configuration document. The type description
 *	  is set to "kshark.config.filter".
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_session_config_new(enum kshark_config_formats format)
{
	return kshark_config_new("kshark.config.session", format);
}

/**
 * @brief Create an empty Text Configuration document. The Text Configuration
 *	  documents do not use type descriptions.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use free()
 *	    to free the object.
 */
struct kshark_config_doc *kshark_string_config_alloc(void)
{
	return kshark_config_alloc(KS_CONFIG_STRING);
}

static void json_del_if_exist(struct json_object *jobj, const char *key)
{
	struct json_object *temp;
	if (json_object_object_get_ex(jobj, key, &temp))
	    json_object_object_del(jobj, key);
}

static bool kshark_json_type_check(struct json_object *jobj, const char *type)
{
	struct json_object *jtype;
	const char *type_str;

	if (!json_object_object_get_ex(jobj, "type", &jtype))
		return false;

	type_str = json_object_get_string(jtype);
	if (strcmp(type_str, type) != 0)
		return false;

	return true;
}

/**
 * @brief Check the type of a Configuration document and compare with an
 *	  expected value.
 *
 * @param conf: Input location for the kshark_config_doc instance.
 * @param type: Input location for the expected value of the Configuration
 *		document type, e.g. "kshark.config.record" or
 *		"kshark.config.filter".
 *
 * @returns True, if the document has the expected type, otherwise False.
 */
bool kshark_type_check(struct kshark_config_doc *conf, const char *type)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_json_type_check(conf->conf_doc, type);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_trace_file_to_json(const char *file, const char *name,
				      struct json_object *jobj)
{
	struct json_object *jfile_name, *jbuffer_name, *jtime;
	char abs_path[FILENAME_MAX];
	struct stat st;

	if (!file || !jobj)
		return false;

	if (stat(file, &st) != 0) {
		fprintf(stderr, "Unable to find file %s\n", file);
		return false;
	}

	if (!realpath(file, abs_path)) {
		fprintf(stderr, "Unable to get absolute pathname for %s\n",
			file);
		return false;
	}

	jfile_name = json_object_new_string(abs_path);
	jbuffer_name = json_object_new_string(name);
	jtime = json_object_new_int64(st.st_mtime);

	if (!jfile_name || !jtime)
		goto fail;

	json_object_object_add(jobj, "file", jfile_name);
	json_object_object_add(jobj, "name", jbuffer_name);
	json_object_object_add(jobj, "time", jtime);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jobj);
	json_object_put(jfile_name);
	json_object_put(jtime);

	return false;
}

/**
 * @brief Record the name of a trace data file and its timestamp into a
 *	  Configuration document.
 *
 * @param file: The name of the file.
 * @param name: The name of the data buffer.
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_trace_file(const char *file, const char *name,
			 enum kshark_config_formats format)
{
	/*  Create a new Configuration document. */
	struct kshark_config_doc *conf =
		kshark_config_new("kshark.config.data", format);

	if (!conf)
		return NULL;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		kshark_trace_file_to_json(file, name, conf->conf_doc);
		return conf;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		kshark_free_config_doc(conf);
		return NULL;
	}
}

static bool kshark_trace_file_from_json(const char **file, const char **name,
					const char *type,
					struct json_object *jobj)
{
	struct json_object *jfile_name, *jbuffer_name, *jtime;
	const char *file_str, *name_str;
	struct stat st;
	char *header;
	int64_t time;
	bool type_OK = true;

	if (!jobj)
		return false;

	if (type) {
		/* Make sure that the condition document has a correct type. */
		type_OK = false;
		if (asprintf(&header, "kshark.config.%s", type) >= 0)
			type_OK = kshark_json_type_check(jobj, header);
	}

	if (!type_OK ||
	    !json_object_object_get_ex(jobj, "file", &jfile_name) ||
	    !json_object_object_get_ex(jobj, "name", &jbuffer_name) ||
	    !json_object_object_get_ex(jobj, "time", &jtime)) {
		fprintf(stderr,
			"Failed to retrieve data file from json_object.\n");
		return false;
	}

	file_str = json_object_get_string(jfile_name);
	name_str = json_object_get_string(jbuffer_name);
	time = json_object_get_int64(jtime);

	if (stat(file_str, &st) != 0) {
		fprintf(stderr, "Unable to find file %s\n", file_str);
		return false;
	}

	if (st.st_mtime != time) {
		fprintf(stderr, "Timestamp mismatch! (%" PRIu64 "!=%li)\nFile %s\n",
				time, st.st_mtime, file_str);
		return false;
	}

	*file = file_str;
	*name = name_str;

	return true;
}

/* Quiet warnings over documenting simple structures */
//! @cond Doxygen_Suppress

#define TOP_BUFF_NAME	"top buffer"

//! @endcond

/**
 * @brief Read the name of a trace data file and its timestamp from a
 *	  Configuration document and check if such a file exists.
 *	  If the file exists, open it.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns The Id number of the data stream associated with the loaded file on
 *	    success, otherwise -1. "conf" has the ownership over the returned
 *	    string.
 */
int kshark_import_trace_file(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc *conf)
{
	const char *file = NULL, *name = NULL;
	int sd = -1;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		if (kshark_trace_file_from_json(&file, &name, "data",
						conf->conf_doc)) {
			if (strcmp(name, KS_UNNAMED) == 0 ||
			    strcmp(name, TOP_BUFF_NAME) == 0) {
				sd = kshark_open(kshark_ctx, file);
			} else {
				int sd_top;

				sd_top = kshark_tep_find_top_stream(kshark_ctx,
								    file);
				if (sd_top < 0) {
					/*
					 * The "top" steam (buffer) has to be
					 * initialized first.
					 */
					sd_top = kshark_open(kshark_ctx, file);
				}

				if (sd_top >= 0)
					sd = kshark_tep_open_buffer(kshark_ctx,
								    sd_top,
								    name);

				if (sd >= 0)
				kshark_tep_handle_plugins(kshark_ctx, sd);
			}
		}

		break;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		break;
	}

	return sd;
}

static bool kshark_plugin_to_json(struct kshark_plugin_list *plugin,
				  struct json_object *jobj)
{
	struct json_object *jname = json_object_new_string(plugin->name);

	if (!kshark_trace_file_to_json(plugin->file, plugin->name, jobj) ||
	    !jname) {
		json_object_put(jname);
		return false;
	}

	json_object_object_add(jobj, "name", jname);
	return true;
}

/**
 * @brief Record the name of a plugin's obj file and its timestamp into a
 *	  Configuration document.
 *
 * @param plugin: The plugin to be expected.
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_plugin_file(struct kshark_plugin_list *plugin,
			  enum kshark_config_formats format)
{
	/*  Create a new Configuration document. */
	struct kshark_config_doc *conf =
		kshark_config_new("kshark.config.library", format);

	if (!conf)
		return NULL;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		kshark_plugin_to_json(plugin, conf->conf_doc);
		return conf;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		kshark_free_config_doc(conf);
		return NULL;
	}
}

static bool kshark_all_plugins_to_json(struct kshark_context *kshark_ctx,
				       struct json_object *jobj)
{
	struct kshark_plugin_list *plugin = kshark_ctx->plugins;
	struct json_object *jfile, *jlist;

	jlist = json_object_new_array();
	if (!jlist)
		return false;

	while (plugin) {
		jfile = json_object_new_object();
		if (!kshark_trace_file_to_json(plugin->file, plugin->name, jfile))
			goto fail;

		json_object_array_add(jlist, jfile);
		plugin = plugin->next;
	}

	json_object_object_add(jobj, "obj. files", jlist);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jobj);
	json_object_put(jlist);
	return false;
}

/**
 * @brief Record the current list of registered plugins into a
 *	  Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_all_plugins(struct kshark_context *kshark_ctx,
			  enum kshark_config_formats format)
{
	struct kshark_config_doc *conf =
		kshark_config_new("kshark.config.plugins", format);

	if (!conf)
		return NULL;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		kshark_all_plugins_to_json(kshark_ctx, conf->conf_doc);
		return conf;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		kshark_free_config_doc(conf);
		return NULL;
	}
}

static bool kshark_plugin_from_json(struct kshark_context *kshark_ctx,
				    struct json_object *jobj)
{
	const char *file, *name;

	if (!kshark_trace_file_from_json(&file, &name, NULL, jobj)) {
		fprintf(stderr, "Failed to import plugin!\n");
		return false;
	}

	return !!(long) kshark_register_plugin(kshark_ctx, name, file);
}

static bool kshark_all_plugins_from_json(struct kshark_context *kshark_ctx,
					 struct json_object *jobj)
{
	struct json_object *jlist = NULL, *jfile = NULL;
	int i, n_plugins;

	if (!kshark_ctx || !jobj)
		return false;

	if (!kshark_json_type_check(jobj, "kshark.config.plugins") ||
	    !json_object_object_get_ex(jobj, "obj. files", &jlist) ||
	    json_object_get_type(jlist) != json_type_array)
		goto fail;

	n_plugins = json_object_array_length(jlist);
	for (i = 0; i < n_plugins; ++i) {
		jfile = json_object_array_get_idx(jlist, i);
		if (!jfile)
			goto fail;

		kshark_plugin_from_json(kshark_ctx, jfile);
	}

	return true;

 fail:
	json_object_put(jfile);
	json_object_put(jlist);
	return false;
}

/**
 * @brief Load the list of registered plugins from a Configuration
 *	  document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if plugins have been loaded. If the configuration
 *	    document contains no data or in a case of an error, the function
 *	    returns False.
 */
bool kshark_import_all_plugins(struct kshark_context *kshark_ctx,
			       struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_all_plugins_from_json(kshark_ctx,
						    conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static void kshark_stream_plugins_to_json(struct kshark_data_stream *stream,
				          struct json_object *jobj)
{
	struct kshark_dpi_list *plugin = stream->plugins;
	struct json_object *jlist, *jplg;
	bool active;

	jlist = json_object_new_array();
	while (plugin) {
		jplg = json_object_new_array();
		json_object_array_add(jplg,
				      json_object_new_string(plugin->interface->name));

		active = plugin->status & KSHARK_PLUGIN_ENABLED;
		json_object_array_add(jplg, json_object_new_boolean(active));

		json_object_array_add(jlist, jplg);

		plugin = plugin->next;
	}

	json_object_object_add(jobj, "registered", jlist);
}

/**
 * @brief Record the current list of plugins registered for a given Data
 *	  stream into a Configuration document.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param format: Input location for the Configuration format identifier.
 *		  Currently only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_stream_plugins(struct kshark_data_stream *stream,
			     enum kshark_config_formats format)
{
	struct kshark_config_doc *conf =
		kshark_config_new("kshark.config.plugins", format);

	if (!conf)
		return NULL;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		kshark_stream_plugins_to_json(stream, conf->conf_doc);
		return conf;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		kshark_free_config_doc(conf);
		return NULL;
	}
}

static bool kshark_stream_plugins_from_json(struct kshark_context *kshark_ctx,
					    struct kshark_data_stream *stream,
					    struct json_object *jobj)
{
	struct json_object *jlist, *jplg, *jname, *jstatus;
	struct kshark_plugin_list *plugin;
	struct kshark_dpi_list *dpi_list;
	struct kshark_dpi *dpi;
	int i, n_plugins;
	bool active;

	jlist = jplg = jname = jstatus = NULL;

	if (!kshark_ctx || !stream || !jobj)
		return false;

	if (!kshark_json_type_check(jobj, "kshark.config.plugins") ||
	    !json_object_object_get_ex(jobj, "registered", &jlist) ||
	    json_object_get_type(jlist) != json_type_array)
		goto fail;

	n_plugins = json_object_array_length(jlist);
	for (i = 0; i < n_plugins; ++i) {
		jplg = json_object_array_get_idx(jlist, i);
		if (!jplg ||
		    json_object_get_type(jplg) != json_type_array ||
		    json_object_array_length(jplg) != 2)
			goto fail;

		jname = json_object_array_get_idx(jplg, 0);
		jstatus = json_object_array_get_idx(jplg, 1);
		if (!jname || !jstatus)
			goto fail;

		plugin = kshark_find_plugin_by_name(kshark_ctx->plugins,
						    json_object_get_string(jname));

		if (plugin) {
			active = json_object_get_boolean(jstatus);
			dpi = plugin->process_interface;
			dpi_list = kshark_register_plugin_to_stream(stream, dpi,
								    active);

			kshark_handle_dpi(stream, dpi_list, KSHARK_PLUGIN_INIT);
		}
	}

	return true;

 fail:
	json_object_put(jplg);
	json_object_put(jlist);
	return false;
}

/**
 * @brief Load the list of registered plugins for a given Data
 *	  stream from a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param stream: Input location for a Trace data stream pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if plugins have been loaded. If the configuration
 *	    document contains no data or in a case of an error, the function
 *	    returns False.
 */
bool kshark_import_stream_plugins(struct kshark_context *kshark_ctx,
				  struct kshark_data_stream *stream,
				  struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_stream_plugins_from_json(kshark_ctx, stream,
						       conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_model_to_json(struct kshark_trace_histo *histo,
				 struct json_object *jobj)
{
	struct json_object *jrange, *jmin, *jmax, *jn_bins;
	if (!histo || !jobj)
		return false;

	jrange = json_object_new_array();

	jmin = json_object_new_int64(histo->min);
	jmax = json_object_new_int64(histo->max);
	jn_bins = json_object_new_int(histo->n_bins);

	if (!jrange || !jmin || !jmax || !jn_bins)
		goto fail;

	json_object_array_put_idx(jrange, 0, jmin);
	json_object_array_put_idx(jrange, 1, jmax);

	json_object_object_add(jobj, "range", jrange);
	json_object_object_add(jobj, "bins", jn_bins);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jobj);
	json_object_put(jrange);
	json_object_put(jmin);
	json_object_put(jmax);
	json_object_put(jn_bins);

	return false;
}

/**
 * @brief Record the current configuration of the Vis. model into a
 *	  Configuration document.
 *
 * @param histo: Input location for the Vis. model descriptor.
 * @param format: Input location for the kshark_config_doc instance. Currently
 *		  only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    free() to free the object.
 */
struct kshark_config_doc *
kshark_export_model(struct kshark_trace_histo *histo,
		    enum kshark_config_formats format)
{
	/*  Create a new Configuration document. */
	struct kshark_config_doc *conf =
		kshark_config_new("kshark.config.model", format);

	if (!conf)
		return NULL;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		kshark_model_to_json(histo, conf->conf_doc);
		return conf;

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		kshark_free_config_doc(conf);
		return NULL;
	}
}

static bool kshark_model_from_json(struct kshark_trace_histo *histo,
				   struct json_object *jobj)
{
	struct json_object *jrange, *jmin, *jmax, *jn_bins;
	uint64_t min, max;
	int n_bins;

	if (!histo || !jobj)
		return false;

	if (!kshark_json_type_check(jobj, "kshark.config.model") ||
	    !json_object_object_get_ex(jobj, "range", &jrange) ||
	    !json_object_object_get_ex(jobj, "bins", &jn_bins) ||
	    json_object_get_type(jrange) != json_type_array ||
	    json_object_array_length(jrange) != 2)
		goto fail;

	jmin = json_object_array_get_idx(jrange, 0);
	jmax = json_object_array_get_idx(jrange, 1);
	if (!jmin || !jmax)
		goto fail;

	min = json_object_get_int64(jmin);
	max = json_object_get_int64(jmax);
	n_bins = json_object_get_int(jn_bins);
	ksmodel_set_bining(histo, n_bins, min, max);

	if (histo->data && histo->data_size)
		ksmodel_fill(histo, histo->data, histo->data_size);

	return true;

 fail:
	fprintf(stderr, "Failed to load event filter from json_object.\n");
	return false;
}

/**
 * @brief Load the configuration of the Vis. model from a Configuration
 *	  document.
 *
 * @param histo: Input location for the Vis. model descriptor.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if the model has been loaded. If the model configuration
 *	    document contains no data or in a case of an error, the function
 *	    returns False.
 */
bool kshark_import_model(struct kshark_trace_histo *histo,
			 struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_model_from_json(histo, conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_event_filter_to_json(struct kshark_data_stream *stream,
					enum kshark_filter_type filter_type,
					const char *filter_name,
					struct json_object *jobj)
{
	json_object *jfilter_data, *jname;
	struct kshark_hash_id *filter;
	char *name_str;
	int *ids;
	size_t i;

	filter = kshark_get_filter(stream, filter_type);
	if (!filter)
		return false;

	jname = NULL;

	/*
	 * If this Json document already contains a description of the filter,
	 * delete this description.
	 */
	json_del_if_exist(jobj, filter_name);

	/* Get the array of Ids to be fitered. */
	ids = kshark_hash_ids(filter);
	if (!ids)
		return true;

	/* Create a Json array and fill the Id values into it. */
	jfilter_data = json_object_new_array();
	if (!jfilter_data)
		goto fail;

	for (i = 0; i < filter->count; ++i) {
		name_str = kshark_event_from_id(stream->stream_id,
						ids[i]);
		if (name_str) {
			jname = json_object_new_string(name_str);
			if (!jname)
				goto fail;

			json_object_array_add(jfilter_data, jname);
		}
	}

	free(ids);

	/* Add the array of Ids to the filter config document. */
	json_object_object_add(jobj, filter_name, jfilter_data);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jfilter_data);
	json_object_put(jname);
	free(ids);

	return false;
}

/**
 * @brief Record the current configuration of an Event Id filter into a
 *	  Configuration document.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param filter_type: Identifier of the filter.
 * @param filter_name: The name of the filter to show up in the Json document.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_event_filter(struct kshark_data_stream *stream,
				enum kshark_filter_type filter_type,
				const char *filter_name,
				struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_event_filter_to_json(stream, filter_type,
						   filter_name,
						   conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static int kshark_event_filter_from_json(struct kshark_data_stream *stream,
					 enum kshark_filter_type filter_type,
					 const char *filter_name,
					 struct json_object *jobj)
{
	int i, length, event_id, count = 0;
	struct kshark_hash_id *filter;
	json_object *jfilter, *jevent;
	const char *name_str;

	filter = kshark_get_filter(stream, filter_type);
	if (!filter)
		return 0;

	/*
	 * Use the name of the filter to find the array of events associated
	 * with this filter. Notice that the filter config document may
	 * contain no data for this particular filter.
	 */
	if (!json_object_object_get_ex(jobj, filter_name, &jfilter))
		return 0;

	if (!kshark_json_type_check(jobj, "kshark.config.filter") ||
	    json_object_get_type(jfilter) != json_type_array)
		goto fail;

	/* Set the filter. */
	length = json_object_array_length(jfilter);
	for (i = 0; i < length; ++i) {
		jevent = json_object_array_get_idx(jfilter, i);
		name_str = json_object_get_string(jevent);
		event_id = kshark_find_event_id(stream, name_str);
		if (event_id < 0)
			continue;

		kshark_hash_id_add(filter, event_id);
		count++;
	}

	if (count != length)
		count = -count;

	return count;

 fail:
	fprintf(stderr, "Failed to load event filter from json_object.\n");
	kshark_hash_id_clear(filter);
	return 0;
}

/**
 * @brief Load from Configuration document the configuration of an Event Id filter.
 *
 * @param stream: Input location for a Trace data stream pointer.
 * @param filter_type: Identifier of the filter.
 * @param filter_name: The name of the filter as showing up in the Config.
 *	               document.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns The total number of events added to the filter. If not all events
 *         listed in the input configuration have been added successfully,
 *         the returned number is negative.
 */
int kshark_import_event_filter(struct kshark_data_stream *stream,
			       enum kshark_filter_type filter_type,
			       const char *filter_name,
			       struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_event_filter_from_json(stream, filter_type,
						     filter_name,
						     conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return 0;
	}
}

static bool kshark_filter_array_to_json(struct kshark_hash_id *filter,
					const char *filter_name,
					struct json_object *jobj)
{
	json_object *jfilter_data, *jpid = NULL;
	int *ids;
	size_t i;

	/*
	 * If this Json document already contains a description of the filter,
	 * delete this description.
	 */
	json_del_if_exist(jobj, filter_name);

	/* Get the array of Ids to be filtered. */
	ids = kshark_hash_ids(filter);
	if (!ids)
		return true;

	/* Create a Json array and fill the Id values into it. */
	jfilter_data = json_object_new_array();
	if (!jfilter_data)
		goto fail;

	for (i = 0; i < filter->count; ++i) {
		jpid = json_object_new_int(ids[i]);
		if (!jpid)
			goto fail;

		json_object_array_add(jfilter_data, jpid);
	}

	free(ids);

	/* Add the array of Ids to the filter config document. */
	json_object_object_add(jobj, filter_name, jfilter_data);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jfilter_data);
	json_object_put(jpid);
	free(ids);

	return false;
}

/**
 * @brief Record the current configuration of a simple Id filter into a
 *	  Configuration document.
 *
 * @param filter: Input location for an Id filter.
 * @param filter_name: The name of the filter to show up in the Json document.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_filter_array(struct kshark_hash_id *filter,
				const char *filter_name,
				struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_filter_array_to_json(filter, filter_name,
						   conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_filter_array_from_json(struct kshark_hash_id *filter,
					  const char *filter_name,
					  struct json_object *jobj)
{
	json_object *jfilter, *jpid;
	int i, length;

	/*
	 * Use the name of the filter to find the array of events associated
	 * with this filter. Notice that the filter config document may
	 * contain no data for this particular filter.
	 */
	if (!json_object_object_get_ex(jobj, filter_name, &jfilter))
		return false;

	if (!kshark_json_type_check(jobj, "kshark.config.filter") ||
	    json_object_get_type(jfilter) != json_type_array)
		goto fail;

	/* Set the filter. */
	length = json_object_array_length(jfilter);
	for (i = 0; i < length; ++i) {
		jpid = json_object_array_get_idx(jfilter, i);
		if (!jpid)
			goto fail;

		kshark_hash_id_add(filter, json_object_get_int(jpid));
	}

	return true;

 fail:
	fprintf(stderr, "Failed to load task filter from json_object.\n");
	return false;
}

/**
 * @brief Load from Configuration document the configuration of a simple
 *	  Id filter.
 *
 * @param filter: Input location for an Id filter.
 * @param filter_name: The name of the filter as showing up in the Config.
 *	               document.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for this particular filter or in a case
 *	    of an error, the function returns False.
 */
bool kshark_import_filter_array(struct kshark_hash_id *filter,
				const char *filter_name,
				struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_filter_array_from_json(filter, filter_name,
						     conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_adv_filters_to_json(struct kshark_data_stream *stream,
				       struct json_object *jobj)
{
	json_object *jfilter_data, *jevent, *jname, *jfilter;
	char *filter_str;
	int *events, i;

	jevent = jname = jfilter = NULL;

	/*
	 * If this Json document already contains a description of the model,
	 * delete this description.
	 */
	json_del_if_exist(jobj, KS_ADV_EVENT_FILTER_NAME);

	if (!kshark_tep_filter_is_set(stream))
		return true;

	/* Create a Json array and fill the Id values into it. */
	jfilter_data = json_object_new_array();
	if (!jfilter_data)
		goto fail;

	events = kshark_get_all_event_ids(stream);
	if (!events)
		return false;

	for (i = 0; i < stream->n_events; ++i) {
		filter_str = kshark_tep_filter_make_string(stream, events[i]);
		if (!filter_str)
			continue;

		jevent = json_object_new_object();
		jname = json_object_new_string(kshark_event_from_id(stream->stream_id,
								    events[i]));

		jfilter = json_object_new_string(filter_str);
		if (!jevent || !jname || !jfilter)
			goto fail;

		json_object_object_add(jevent, "name", jname);
		json_object_object_add(jevent, "condition", jfilter);

		json_object_array_add(jfilter_data, jevent);
	}

	/* Add the array of advanced filters to the filter config document. */
	json_object_object_add(jobj, KS_ADV_EVENT_FILTER_NAME, jfilter_data);

	return true;

 fail:
	fprintf(stderr, "Failed to allocate memory for json_object.\n");
	json_object_put(jfilter_data);
	json_object_put(jevent);
	json_object_put(jname);
	json_object_put(jfilter);

	return false;
}

/**
 * @brief Record the current configuration of the advanced filter into a
 *	  Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Adv. Filter
 *		Configuration document will be created.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_adv_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc **conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);

	if (!stream)
		return false;

	if (!kshark_is_tep(stream)) {
		/* Nothing to export. */
		return true;
	}

	if (!*conf)
		*conf = kshark_filter_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	switch ((*conf)->format) {
	case KS_CONFIG_JSON:
		return kshark_adv_filters_to_json(stream,
						  (*conf)->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			(*conf)->format);
		return false;
	}
}

static bool kshark_adv_filters_from_json(struct kshark_data_stream *stream,
					 struct json_object *jobj)
{
	json_object *jfilter, *jname, *jcond;
	int i, length, n, ret = 0;
	char *filter_str = NULL;

	/*
	 * Use the name of the filter to find the array of events associated
	 * with this filter. Notice that the filter config document may
	 * contain no data for this particular filter.
	 */
	if (!json_object_object_get_ex(jobj, KS_ADV_EVENT_FILTER_NAME,
				       &jfilter))
		return false;

	if (!kshark_json_type_check(jobj, "kshark.config.filter") ||
	    json_object_get_type(jfilter) != json_type_array)
		goto fail;

	/* Set the filter. */
	length = json_object_array_length(jfilter);
	for (i = 0; i < length; ++i) {
		jfilter = json_object_array_get_idx(jfilter, i);

		if (!json_object_object_get_ex(jfilter, "name", &jname) ||
		    !json_object_object_get_ex(jfilter, "condition", &jcond))
			goto fail;

		n = asprintf(&filter_str, "%s:%s",
			     json_object_get_string(jname),
			     json_object_get_string(jcond));

		if (n <= 0) {
			filter_str = NULL;
			goto fail;
		}

		ret = kshark_tep_add_filter_str(stream, filter_str);
		if (ret < 0)
			goto fail;
	}

	return true;

 fail:
	fprintf(stderr, "Failed to laod Advanced filters.\n");

	free(filter_str);
	return false;
}

/**
 * @brief Load from Configuration document the configuration of the advanced
 *	  filter.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for the Adv. filter or in a case of
 *	    an error, the function returns False.
 */
bool kshark_import_adv_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc *conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);

	if (!stream)
		return false;

	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_adv_filters_from_json(stream,
						    conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_user_mask_to_json(struct kshark_context *kshark_ctx,
				     struct json_object *jobj)
{
	json_object *jmask;
	uint8_t mask;

	mask = kshark_ctx->filter_mask;

	jmask = json_object_new_int((int) mask);
	if (!jmask)
		return false;

	/* Add the mask to the filter config document. */
	json_object_object_add(jobj, KS_USER_FILTER_MASK_NAME, jmask);
	return true;
}

/**
 * @brief Record the current value of the the user-specified filter mask into
 *	  a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Adv. Filter
 *		Configuration document will be created.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_user_mask(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc **conf)
{
	if (!*conf)
		*conf = kshark_filter_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	switch ((*conf)->format) {
	case KS_CONFIG_JSON:
		return kshark_user_mask_to_json(kshark_ctx, (*conf)->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			(*conf)->format);
		return false;
	}
}

static bool kshark_user_mask_from_json(struct kshark_context *kshark_ctx,
				       struct json_object *jobj)
{
	json_object *jmask;
	uint8_t mask;

	if (!kshark_json_type_check(jobj, "kshark.config.filter"))
		return false;
	/*
	 * Use the name of the filter to find the value of the filter maks.
	 * Notice that the filter config document may contain no data for
	 * the mask.
	 */
	if (!json_object_object_get_ex(jobj, KS_USER_FILTER_MASK_NAME,
				       &jmask))
		return false;

	mask = json_object_get_int(jmask);
	kshark_ctx->filter_mask = mask;

	return true;
}

/**
 * @brief Load from Configuration document the value of the user-specified
 *	  filter mask.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a mask has been loaded. If the filter configuration
 *	    document contains no data for the mask or in a case of an error,
 *	    the function returns False.
 */
bool kshark_import_user_mask(struct kshark_context *kshark_ctx,
			     struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_user_mask_from_json(kshark_ctx, conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_calib_array_from_json(struct kshark_context *kshark_ctx,
					 int sd, struct json_object *jobj)
{
	json_object *jcalib_argv, *jcalib;
	int64_t *calib_argv = NULL;
	int i, calib_length;

	if (!json_object_object_get_ex(jobj, "calib. array", &jcalib_argv) ||
	    json_object_get_type(jcalib_argv) != json_type_array)
		return false;

	calib_length = json_object_array_length(jcalib_argv);
	if (!calib_length)
		return false;

	calib_argv = calloc(calib_length, sizeof(*calib_argv));
	for (i = 0; i < calib_length; ++i) {
		jcalib = json_object_array_get_idx(jcalib_argv, i);
		calib_argv[i] = json_object_get_int64(jcalib);
	}

	kshark_ctx->stream[sd]->calib = kshark_offset_calib;
	kshark_ctx->stream[sd]->calib_array = calib_argv;
	kshark_ctx->stream[sd]->calib_array_size = calib_length;

	return true;
}

/**
 * @brief Load from Configuration document the value of the time calibration
 *	  constants into a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Configuration
 *		document will be created.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_import_calib_array(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_calib_array_from_json(kshark_ctx, sd, conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static bool kshark_calib_array_to_json(struct kshark_context *kshark_ctx,
				       int sd, struct json_object *jobj)
{
	json_object *jval = NULL, *jcalib = NULL;
	struct kshark_data_stream *stream;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream || !stream->calib_array_size)
		goto fail;

	jcalib = json_object_new_array();
	if (!jcalib)
		goto fail;

	for (size_t i = 0; i < stream->calib_array_size; ++i) {
		jval = json_object_new_int64(stream->calib_array[i]);
		if (!jval)
			goto fail;

		json_object_array_add(jcalib, jval);
	}

	/* Add the mask to the filter config document. */
	json_object_object_add(jobj, "calib. array", jcalib);

	return true;

 fail:
	json_object_put(jval);
	json_object_put(jcalib);

	return false;
}

/**
 * @brief Record the current values of the time calibration constants into
 *	  a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Configuration
 *		document will be created.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_calib_array(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc **conf)
{
	if (!*conf)
		*conf = kshark_stream_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	switch ((*conf)->format) {
	case KS_CONFIG_JSON:
		return kshark_calib_array_to_json(kshark_ctx, sd,
						  (*conf)->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			(*conf)->format);
		return false;
	}
}

/**
 * @brief Record the current configuration of "show task" and "hide task"
 *	  filters into a Json document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Filter
 *		Configuration document will be created.
 *
 * @returns True, if a filter has been recorded. If both filters contain
 *	    no Id values or in a case of an error, the function returns False.
 */
bool kshark_export_all_event_filters(struct kshark_context *kshark_ctx, int sd,
				     struct kshark_config_doc **conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret;

	if (!stream)
		return false;

	if (!*conf)
		*conf = kshark_filter_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	/* Save a filter only if it contains Id values. */
	ret = true;
	if (kshark_this_filter_is_set(stream->show_event_filter))
		ret &= kshark_export_event_filter(stream,
						  KS_SHOW_EVENT_FILTER,
						  KS_SHOW_EVENT_FILTER_NAME,
						  *conf);

	if (kshark_this_filter_is_set(stream->hide_event_filter))
		ret &= kshark_export_event_filter(stream,
						  KS_HIDE_EVENT_FILTER,
						  KS_HIDE_EVENT_FILTER_NAME,
						  *conf);

	return ret;
}

/**
 * @brief Record the current configuration of "show task" and "hide task"
 *	  filters into a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Filter
 *		Configuration document will be created.
 *
 * @returns True, if a filter has been recorded. If both filters contain
 *	    no Id values or in a case of an error, the function returns False.
 */
bool kshark_export_all_task_filters(struct kshark_context *kshark_ctx, int sd,
				    struct kshark_config_doc **conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret;

	if (!stream)
		return false;

	if (!*conf)
		*conf = kshark_filter_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	/* Save a filter only if it contains Id values. */
	ret = true;
	if (kshark_this_filter_is_set(stream->show_task_filter))
		ret &= kshark_export_filter_array(stream->show_task_filter,
						  KS_SHOW_TASK_FILTER_NAME,
						  *conf);

	if (kshark_this_filter_is_set(stream->hide_task_filter))
		ret &= kshark_export_filter_array(stream->hide_task_filter,
						  KS_HIDE_TASK_FILTER_NAME,
						  *conf);

	return ret;
}

/**
 * @brief Record the current configuration of "show cpu" and "hide cpu"
 *	  filters into a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Filter
 *		Configuration document will be created.
 *
 * @returns True, if a filter has been recorded. If both filters contain
 *	    no Id values or in a case of an error, the function returns False.
 */
bool kshark_export_all_cpu_filters(struct kshark_context *kshark_ctx, int sd,
				   struct kshark_config_doc **conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret;

	if (!stream)
		return false;

	if (!*conf)
		*conf = kshark_filter_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	/* Save a filter only if it contains Id values. */
	ret = true;
	if (kshark_this_filter_is_set(stream->show_cpu_filter))
		ret &= kshark_export_filter_array(stream->show_cpu_filter,
						  KS_SHOW_CPU_FILTER_NAME,
						  *conf);

	if (kshark_this_filter_is_set(stream->hide_cpu_filter))
		ret &= kshark_export_filter_array(stream->hide_cpu_filter,
						  KS_HIDE_CPU_FILTER_NAME,
						  *conf);

	return ret;
}

/**
 * @brief Load from a Configuration document the configuration of "show event"
 *	  and "hide event" filters.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for any event filter or in a case
 *	    of an error, the function returns False.
 */
bool kshark_import_all_event_filters(struct kshark_context *kshark_ctx, int sd,
				     struct kshark_config_doc *conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret = false;

	if (!stream)
		return false;

	ret |= kshark_import_event_filter(stream,
					  KS_HIDE_EVENT_FILTER,
					  KS_HIDE_EVENT_FILTER_NAME,
					  conf);

	ret |= kshark_import_event_filter(stream,
					  KS_SHOW_EVENT_FILTER,
					  KS_SHOW_EVENT_FILTER_NAME,
					  conf);

	return ret;
}

/**
 * @brief Load from Configuration document the configuration of "show task"
 *	  and "hide task" filters.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for any task filter or in a case of an
 *	    error, the function returns False.
 */
bool kshark_import_all_task_filters(struct kshark_context *kshark_ctx, int sd,
				    struct kshark_config_doc *conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret = false;

	if (!stream)
		return false;

	ret |= kshark_import_filter_array(stream->hide_task_filter,
					  KS_HIDE_TASK_FILTER_NAME,
					  conf);

	ret |= kshark_import_filter_array(stream->show_task_filter,
					  KS_SHOW_TASK_FILTER_NAME,
					  conf);

	return ret;
}

/**
 * @brief Load from Configuration document the configuration of "show cpu"
 *	  and "hide cpu" filters.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for any cpu filter or in a case of an
 *	    error, the function returns False.
 */
bool kshark_import_all_cpu_filters(struct kshark_context *kshark_ctx, int sd,
				    struct kshark_config_doc *conf)
{
	struct kshark_data_stream *stream =
		kshark_get_data_stream(kshark_ctx, sd);
	bool ret = false;

	if (!stream)
		return false;

	ret |= kshark_import_filter_array(stream->hide_cpu_filter,
					  KS_HIDE_CPU_FILTER_NAME,
					  conf);

	ret |= kshark_import_filter_array(stream->show_cpu_filter,
					  KS_SHOW_CPU_FILTER_NAME,
					  conf);

	return ret;
}

/**
 * @brief Create a Filter Configuration document containing the current
 *	  configuration of all filters.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param format: Input location for the kshark_config_doc instance. Currently
 *		  only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_all_filters(struct kshark_context *kshark_ctx, int sd,
			  enum kshark_config_formats format)
{
	/*  Create a new Configuration document. */
	struct kshark_config_doc *conf =
		kshark_filter_config_new(format);

	/* Save a filter only if it contains Id values. */
	if (!conf ||
	    !kshark_export_all_event_filters(kshark_ctx, sd, &conf) ||
	    !kshark_export_all_task_filters(kshark_ctx, sd, &conf) ||
	    !kshark_export_all_cpu_filters(kshark_ctx, sd, &conf) ||
	    !kshark_export_user_mask(kshark_ctx, &conf) ||
	    !kshark_export_adv_filters(kshark_ctx, sd, &conf)) {
		kshark_free_config_doc(conf);
		return NULL;
	}

	return conf;
}

/**
 * @brief Load from a Configuration document the configuration of all filters.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True, if a filter has been loaded. If the filter configuration
 *	    document contains no data for any filter or in a case of an error,
 *	    the function returns False.
 */
bool kshark_import_all_filters(struct kshark_context *kshark_ctx, int sd,
			       struct kshark_config_doc *conf)
{
	bool ret;

	ret = kshark_import_all_task_filters(kshark_ctx, sd, conf);
	ret |= kshark_import_all_cpu_filters(kshark_ctx, sd, conf);
	ret |= kshark_import_all_event_filters(kshark_ctx, sd, conf);
	ret |= kshark_import_user_mask(kshark_ctx, conf);
	ret |= kshark_import_adv_filters(kshark_ctx, sd, conf);

	return ret;
}

/**
 * @brief Create a Data Stream Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param sd: Data stream identifier.
 * @param format: Input location for the kshark_config_doc instance. Currently
 *		  only Json format is supported.
 *
 * @returns kshark_config_doc instance on success, otherwise NULL. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *
kshark_export_dstream(struct kshark_context *kshark_ctx, int sd,
		      enum kshark_config_formats format)
{
	struct kshark_config_doc *file_conf, *filter_conf, *sd_conf, *plg_conf;
	struct kshark_config_doc *dstream_conf;
	struct kshark_data_stream *stream;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return NULL;

	/*  Create new Configuration documents. */
	dstream_conf = kshark_stream_config_new(format);
	sd_conf = kshark_config_alloc(KS_CONFIG_JSON);

	sd_conf->conf_doc = json_object_new_int(sd);

	filter_conf = kshark_export_all_filters(kshark_ctx, sd, format);

	if (kshark_is_tep(stream) && kshark_tep_is_top_stream(stream))
		file_conf = kshark_export_trace_file(stream->file,
						     TOP_BUFF_NAME,
						     format);
	else
		file_conf = kshark_export_trace_file(stream->file,
						     stream->name,
						     format);

	plg_conf = kshark_export_stream_plugins(stream, format);

	if (!dstream_conf ||
	    !sd_conf ||
	    !filter_conf ||
	    !file_conf ||
	    !plg_conf)
		goto fail;

	kshark_config_doc_add(dstream_conf, "stream id", sd_conf);
	kshark_config_doc_add(dstream_conf, "data", file_conf);
	kshark_config_doc_add(dstream_conf, "filters", filter_conf);
	kshark_config_doc_add(dstream_conf, "plugins", plg_conf);

	if (stream->calib_array_size)
		kshark_export_calib_array(kshark_ctx, sd, &dstream_conf);

	return dstream_conf;

 fail:
	kshark_free_config_doc(dstream_conf);
	kshark_free_config_doc(filter_conf);
	kshark_free_config_doc(file_conf);
	kshark_free_config_doc(plg_conf);
	kshark_free_config_doc(sd_conf);

	return NULL;
}

/**
 * @brief Load Data Stream from a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns The Stream Id on the loaded Data Stream on success, otherwise a
 *	    negative error code.
 */
int kshark_import_dstream(struct kshark_context *kshark_ctx,
			  struct kshark_config_doc *conf)
{
	struct kshark_config_doc *file_conf, *filter_conf, *plg_conf;
	struct kshark_data_stream *stream;
	bool ret = false;
	int sd = -EFAULT;

	if (!kshark_type_check(conf, "kshark.config.stream"))
		return sd;

	file_conf = kshark_config_alloc(KS_CONFIG_JSON);
	filter_conf = kshark_config_alloc(KS_CONFIG_JSON);
	plg_conf = kshark_config_alloc(KS_CONFIG_JSON);
	if (!file_conf || !filter_conf || !plg_conf) {
		fprintf(stderr,
			"Failed to allocate memory for Json document.\n");
		goto free;
	}

	if (kshark_config_doc_get(conf, "data", file_conf) &&
	    kshark_config_doc_get(conf, "filters", filter_conf) &&
	    kshark_config_doc_get(conf, "plugins", plg_conf)) {
		sd = kshark_import_trace_file(kshark_ctx, file_conf);
		if (sd < 0) {
			fprintf(stderr,
				"Failed to import data file form Json document.\n");
			goto free;
		}

		stream = kshark_ctx->stream[sd];
		kshark_import_calib_array(kshark_ctx, sd, conf);
		ret = kshark_import_all_filters(kshark_ctx, sd,
						filter_conf);
		if (!ret) {
			fprintf(stderr,
				"Failed to import filters form Json document.\n");
			kshark_close(kshark_ctx, sd);
			sd = -EFAULT;
			goto free;
		}

		ret = kshark_import_stream_plugins(kshark_ctx, stream, plg_conf);

		if (!ret) {
			fprintf(stderr,
				"Failed to import stream plugins form Json document.\n");
			kshark_close(kshark_ctx, sd);
			sd = -EFAULT;
			goto free;
		}
	}

 free:
	/* Free only the kshark_config_doc objects. */
	free(file_conf);
	free(filter_conf);
	free(plg_conf);

	return sd;
}

static bool
kshark_export_all_dstreams_to_json(struct kshark_context *kshark_ctx,
				   struct json_object *jobj)
{
	int *stream_ids = kshark_all_streams(kshark_ctx);
	struct kshark_config_doc *dstream_conf;
	struct json_object *jall_streams;

	json_del_if_exist(jobj, KS_DSTREAMS_NAME);
	jall_streams = json_object_new_array();

	for (int i = 0; i < kshark_ctx->n_streams; ++i) {
		dstream_conf = kshark_export_dstream(kshark_ctx, stream_ids[i],
						     KS_CONFIG_JSON);
		if (!dstream_conf)
			goto fail;

		json_object_array_put_idx(jall_streams, i, dstream_conf->conf_doc);

		/* Free only the kshark_config_doc object. */
		free(dstream_conf);
	}

	free(stream_ids);

	json_object_object_add(jobj, KS_DSTREAMS_NAME, jall_streams);

	return true;

 fail:
	json_object_put(jall_streams);
	free(stream_ids);

	return false;
}

/**
 * @brief Record the current configuration for all Data Streams into a Json
 *	  document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported. If NULL, a new Configuration
 *		document will be created.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_export_all_dstreams(struct kshark_context *kshark_ctx,
				struct kshark_config_doc **conf)
{
	if (!*conf)
		*conf = kshark_session_config_new(KS_CONFIG_JSON);

	if (!*conf)
		return false;

	switch ((*conf)->format) {
	case KS_CONFIG_JSON:
		return kshark_export_all_dstreams_to_json(kshark_ctx,
							  (*conf)->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			(*conf)->format);
		return false;
	}
}

static ssize_t
kshark_import_all_dstreams_from_json(struct kshark_context *kshark_ctx,
				     struct json_object *jobj,
				     struct kshark_entry ***data_rows)
{
	struct kshark_config_doc dstream_conf;
	json_object *jall_streams, *jstream;
	int sd, i, length;

	if (!json_object_object_get_ex(jobj, KS_DSTREAMS_NAME, &jall_streams) ||
	    json_object_get_type(jall_streams) != json_type_array)
		return -EFAULT;

	length = json_object_array_length(jall_streams);
	if (!length)
		return -EFAULT;

	dstream_conf.format = KS_CONFIG_JSON;
	for (i = 0; i < length; ++i) {
		jstream = json_object_array_get_idx(jall_streams, i);
		dstream_conf.conf_doc = jstream;
		sd = kshark_import_dstream(kshark_ctx, &dstream_conf);

		if (sd < 0)
			return -EFAULT;
	}

	return kshark_load_all_entries(kshark_ctx, data_rows);
}

/**
 * @brief Load all Data Streams from a Configuration document.
 *
 * @param kshark_ctx: Input location for session context pointer.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 * @param data_rows: Output location for the trace data. The user is
 *		     responsible for freeing the elements of the outputted
 *		     array.
 *
 * @returns The size of the outputted data in the case of success, or a
 *	    negative error code on failure.
 */
ssize_t kshark_import_all_dstreams(struct kshark_context *kshark_ctx,
				   struct kshark_config_doc *conf,
				   struct kshark_entry ***data_rows)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_import_all_dstreams_from_json(kshark_ctx,
							    conf->conf_doc,
							    data_rows);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return -EFAULT;
	}
}

static bool kshark_save_json_file(const char *file_name,
				  struct json_object *jobj)
{
	int flags;

	/* Save the file in a human-readable form. */
	flags = JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY;
	if (json_object_to_file_ext(file_name, jobj, flags) == 0)
		return true;

	return false;
}

/**
 * @brief Save a Configuration document into a file.
 *
 * @param file_name: The name of the file.
 * @param conf: Input location for the kshark_config_doc instance. Currently
 *		only Json format is supported.
 *
 * @returns True on success, otherwise False.
 */
bool kshark_save_config_file(const char *file_name,
			     struct kshark_config_doc *conf)
{
	switch (conf->format) {
	case KS_CONFIG_JSON:
		return kshark_save_json_file(file_name, conf->conf_doc);

	default:
		fprintf(stderr, "Document format %d not supported\n",
			conf->format);
		return false;
	}
}

static struct json_object *kshark_open_json_file(const char *file_name,
						 const char *type)
{
	struct json_object *jobj, *var;
	const char *type_var;

	jobj = json_object_from_file(file_name);

	if (!jobj)
		return NULL;

	/* Get the type of the document. */
	if (!json_object_object_get_ex(jobj, "type", &var))
		goto fail;

	type_var = json_object_get_string(var);

	if (strcmp(type, type_var) != 0)
		goto fail;

	return jobj;

 fail:
	/* The document has a wrong type. */
	fprintf(stderr, "Failed to open Json file %s.\n", file_name);
	fprintf(stderr, "The document has a wrong type.\n");

	json_object_put(jobj);
	return NULL;
}

static const char *get_ext(const char *filename)
{
	const char *dot = strrchr(filename, '.');

	if(!dot)
		return "unknown";

	return dot + 1;
}

/**
 * @brief Open for read a Configuration file and check if it has the
 *	  expected type.
 *
 * @param file_name: The name of the file. Currently only Json files are
 *		     supported.
 * @param type: String describing the expected type of the document,
 *		e.g. "kshark.config.record" or "kshark.config.filter".
 *
 * @returns kshark_config_doc instance on success, or NULL on failure. Use
 *	    kshark_free_config_doc() to free the object.
 */
struct kshark_config_doc *kshark_open_config_file(const char *file_name,
						  const char *type)
{
	struct kshark_config_doc *conf = NULL;

	if (strcmp(get_ext(file_name), "json") == 0) {
		struct json_object *jobj =
			kshark_open_json_file(file_name, type);

		if (jobj) {
			conf = malloc(sizeof(*conf));
			conf->conf_doc = jobj;
			conf->format = KS_CONFIG_JSON;
		}
	}

	return conf;
}
