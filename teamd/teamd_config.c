/*
 *   teamd_config.c - Teamd configuration frontend
 *   Copyright (C) 2013 Jiri Pirko <jiri@resnulli.us>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <jansson.h>

#include "teamd.h"
#include "teamd_json.h"

#define TEAMD_IMPLICIT_CONFIG "{\"device\": \"team0\", \"runner\": {\"name\": \"roundrobin\"}}"

int teamd_config_load(struct teamd_context *ctx)
{
	json_error_t jerror;
	size_t jflags = JSON_REJECT_DUPLICATES;

	if (!ctx->config_text && !ctx->config_file) {
		ctx->config_text = strdup(TEAMD_IMPLICIT_CONFIG);
		if (!ctx->config_text)
			return -ENOMEM;
		teamd_log_warn("No config passed, using implicit one.");
	}
	if (ctx->config_text) {
		if (ctx->config_file)
			teamd_log_warn("Command line config string is present, ignoring given config file.");
		ctx->config_json = json_loads(ctx->config_text, jflags,
					      &jerror);
	} else if (ctx->config_file) {
		ctx->config_json = json_load_file(ctx->config_file, jflags,
						  &jerror);
	}
	if (!ctx->config_json) {
		teamd_log_err("Failed to parse config: %s on line %d, column %d",
			      jerror.text, jerror.line, jerror.column);
		return -EIO;
	}

	return 0;
}

void teamd_config_free(struct teamd_context *ctx)
{
	json_decref(ctx->config_json);
}

int teamd_config_dump(struct teamd_context *ctx, char **p_config_dump)
{
	char *dump;

	dump = json_dumps(ctx->config_json, TEAMD_JSON_DUMPS_FLAGS);
	if (!dump)
		return -ENOMEM;
	*p_config_dump = dump;
	return 0;
}

static int get_port_obj(json_t **pport_obj, json_t *config_json,
			const char *port_name)
{
	int err;
	json_t *ports_obj;
	json_t *port_obj;

	err = json_unpack(config_json, "{s:o}", "ports", &ports_obj);
	if (err) {
		ports_obj = json_object();
		if (!ports_obj)
			return -ENOMEM;
		err = json_object_set(config_json, "ports", ports_obj);
		if (err) {
			json_decref(ports_obj);
			return -ENOMEM;
		}
	}
	err = json_unpack(ports_obj, "{s:o}", port_name, &port_obj);
	if (err) {
		port_obj = json_object();
		if (!port_obj)
			return -ENOMEM;
		err = json_object_set(ports_obj, port_name, port_obj);
		if (err) {
			json_decref(port_obj);
			return -ENOMEM;
		}
	}
	if (pport_obj)
		*pport_obj = port_obj;
	return 0;
}

int teamd_config_actual_dump(struct teamd_context *ctx, char **p_config_dump)
{
	json_t *actual_json;
	struct teamd_port *tdport;
	json_t *ports_obj;
	void *iter;
	char *dump;
	int err;

	actual_json = json_deep_copy(ctx->config_json);
	if (!actual_json)
		return -ENOMEM;

	/*
	 * Create json objects for all present ports
	 */
	teamd_for_each_tdport(tdport, ctx) {
		err = get_port_obj(NULL, actual_json, tdport->ifname);
		if (err)
			goto errout;
	}

	/*
	 * Get rid of json object of ports which are not present
	 */
	err = json_unpack(actual_json, "{s:o}", "ports", &ports_obj);
	if (err) {
		err = -EINVAL;
		goto errout;
	}
	iter = json_object_iter(ports_obj);
	while (iter) {
		const char *port_name = json_object_iter_key(iter);

		iter = json_object_iter_next(ports_obj, iter);
		if (!teamd_get_port_by_ifname(ctx, (char *) port_name))
			json_object_del(ports_obj, port_name);
	}

	dump = json_dumps(actual_json, TEAMD_JSON_DUMPS_FLAGS);
	json_decref(actual_json);
	if (!dump)
		return -ENOMEM;
	*p_config_dump = dump;
	return 0;

errout:
	json_decref(actual_json);
	return err;
}

int teamd_config_port_update(struct teamd_context *ctx, const char *port_name,
			     const char *json_port_cfg_str)
{
	int err;
	json_t *port_obj;
	json_t *port_new_obj;
	json_error_t jerror;

	port_new_obj = json_loads(json_port_cfg_str, JSON_REJECT_DUPLICATES,
				  &jerror);
	if (!port_new_obj) {
		teamd_log_err("%s: Failed to parse port config string: "
			      "%s on line %d, column %d", port_name,
			      jerror.text, jerror.line, jerror.column);
		return -EIO;
	}
	err = get_port_obj(&port_obj, ctx->config_json, port_name);
	if (err) {
		teamd_log_err("%s: Failed to obtain port config object",
			      port_name);
		goto new_port_decref;
	}

	/* replace existing object content */
	json_object_clear(port_obj);
	err = json_object_update(port_obj, port_new_obj);
	if (err)
		teamd_log_err("%s: Failed to update existing config "
			      "port object", port_name);
new_port_decref:
	json_decref(port_new_obj);
	return err;
}

static int __json_path_lite(json_t **p_json_obj, json_t *json_root,
			    const char *fmt, va_list ap)
{
	json_t *json_obj = json_root;
	char *ptr;
	char *end;
	char path[128];
	size_t pathlen;
	int ret;

	ret = vsnprintf(path, sizeof(path), fmt, ap);
	if (ret < 0 || ret >= sizeof(path))
		return -EINVAL;

	pathlen = strlen(path);
	ptr = path;

	if (*ptr != '$')
		return -EINVAL;
	ptr++;
	while (ptr - path < pathlen) {
		if (*ptr == '.') {
			ptr++;
			end = strchr(ptr, '.');
			if (end)
				*end = '\0';
			json_obj = json_object_get(json_obj, ptr);
			if (end)
				*end = '.';
			else
				end = ptr + strlen(ptr);
			ptr = end;
		} else if (*ptr == '[') {
			int i;

			ptr++;
			end = strchr(ptr, ']');
			if (!end)
				return -EINVAL;
			*end = '\0';
			for (i = 0; i < strlen(ptr); i++)
				if (!isdigit(ptr[i]))
					return -EINVAL;
			json_obj = json_array_get(json_obj, atoi(ptr));
			ptr = end + 1;
		} else {
			return -EINVAL;
		}
		if (!json_obj)
			return -ENOENT;
	}
	*p_json_obj = json_obj;
	return 0;
}

static int teamd_config_object_get(struct teamd_context *ctx,
				   json_t **p_json_obj,
				   const char *fmt, va_list ap)
{
	int err;

	err = __json_path_lite(p_json_obj, ctx->config_json, fmt, ap);
	if (err) {
		if (err == -EINVAL)
			teamd_log_err("Failed to get value from config: Wrong path format");
		return err;
	}
	return 0;
}

int teamd_config_string_get(struct teamd_context *ctx, const char **p_str_val,
			    const char *fmt, ...)
{
	va_list ap;
	json_t *json_obj = json_obj;
	int err;

	va_start(ap, fmt);
	err = teamd_config_object_get(ctx, &json_obj, fmt, ap);
	va_end(ap);
	if (err)
		return err;

	if (!json_is_string(json_obj)) {
		teamd_log_err("Failed to get string from non-string object");
		return -ENOENT;
	}
	*p_str_val = json_string_value(json_obj);
	return 0;
}

int teamd_config_int_get(struct teamd_context *ctx, int *p_int_val,
			 const char *fmt, ...)
{
	va_list ap;
	json_t *json_obj = json_obj;
	int err;

	va_start(ap, fmt);
	err = teamd_config_object_get(ctx, &json_obj, fmt, ap);
	va_end(ap);
	if (err)
		return err;

	if (!json_is_integer(json_obj)) {
		teamd_log_err("Failed to get integer from non-integer object");
		return -ENOENT;
	}
	*p_int_val = json_integer_value(json_obj);
	return 0;
}

int teamd_config_bool_get(struct teamd_context *ctx, bool *p_bool_val,
			  const char *fmt, ...)
{
	va_list ap;
	json_t *json_obj = json_obj;
	int err;

	va_start(ap, fmt);
	err = teamd_config_object_get(ctx, &json_obj, fmt, ap);
	va_end(ap);
	if (err)
		return err;

	if (!json_is_boolean(json_obj)) {
		teamd_log_err("Failed to get boolean from non-boolean object");
		return -ENOENT;
	}
	*p_bool_val = json_is_true(json_obj) ? true : false;
	return 0;
}