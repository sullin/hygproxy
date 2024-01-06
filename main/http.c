/*
 * http.c
 *
 * HTTP server for configuration
 *
 * Copyright 2019 Anti Sullin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "esp_http_server.h"
#include "esp_system.h"
#include "cJSON.h"
#include "bt.h"
#include "conf.h"
#include "http.h"

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");
extern const char conf_html_start[] asm("_binary_conf_html_start");
extern const char conf_html_end[]   asm("_binary_conf_html_end");
extern const char hdr_html_start[] asm("_binary_hdr_html_start");
extern const char hdr_html_end[]   asm("_binary_hdr_html_end");
extern const char main_css_start[] asm("_binary_main_css_start");
extern const char main_css_end[]   asm("_binary_main_css_end");
extern const char main_js_start[] asm("_binary_main_js_start");
extern const char main_js_end[]   asm("_binary_main_js_end");


#define SCRATCH_BUFSIZE (1024)
struct http_server_context {
	char scratch[SCRATCH_BUFSIZE];
} http_server_context;


static esp_err_t http_index_html_handler(httpd_req_t *req) {
	httpd_resp_send_chunk(req, hdr_html_start, hdr_html_end-hdr_html_start);
	httpd_resp_send_chunk(req, index_html_start, index_html_end-index_html_start);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t http_conf_html_handler(httpd_req_t *req) {
	httpd_resp_send_chunk(req, hdr_html_start, hdr_html_end-hdr_html_start);
	httpd_resp_send_chunk(req, conf_html_start, conf_html_end-conf_html_start);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t http_main_css_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/css");

	httpd_resp_send_chunk(req, main_css_start, main_css_end-main_css_start);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t http_main_js_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "application/javascript");

	httpd_resp_send_chunk(req, main_js_start, main_js_end-main_js_start);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t http_conf_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	cJSON *root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "ifx_host", conf.influx.host);
	cJSON_AddStringToObject(root, "ifx_db", conf.influx.db);
	cJSON_AddStringToObject(root, "ifx_pfx", conf.influx.pfx);
	cJSON_AddNumberToObject(root, "ifx_int", conf.influx.interval_s);

	cJSON *ifx_clients = cJSON_CreateArray();
	cJSON_AddItemToObject(root, "ifx_clients", ifx_clients);

	int i;
	for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
		if (conf.influx.clients[i].addr == 0) continue;
		cJSON *cli = cJSON_CreateObject();
		cJSON_AddItemToArray(ifx_clients, cli);

		cJSON_AddStringToObject(cli, "name", conf.influx.clients[i].name);

		char tmp[32];
		snprintf(tmp, sizeof(tmp), "%012llx", conf.influx.clients[i].addr);
		cJSON_AddStringToObject(cli, "addr", tmp);

		cJSON_AddNumberToObject(cli, "t", bt_result_get_t(i));
		cJSON_AddNumberToObject(cli, "h", bt_result_get_h(i));
	}

	const char *str = cJSON_Print(root);
	httpd_resp_sendstr(req, str);
	free((void *)str);
	cJSON_Delete(root);
	return ESP_OK;
}

static cJSON* http_post_json(httpd_req_t *req) {
	int total_len = req->content_len;
	int cur_len = 0;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
		return NULL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, http_server_context.scratch + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error");
			return NULL;
		}
		cur_len += received;
	}
	http_server_context.scratch[total_len] = '\0';

	cJSON *root = cJSON_Parse(http_server_context.scratch);
	if (root == NULL) {
		httpd_resp_send_err(req,  HTTPD_400_BAD_REQUEST, "Invalid JSON");
		return NULL;
	}

	return root;
}

static esp_err_t http_cjson_get_str(httpd_req_t *req, const cJSON *obj, const char *name, char *str, int maxlen) {
	const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, name);
	if (it == NULL) return ESP_OK;

	if (cJSON_IsString(it)) { // includes NULL check
		if (strlen(it->valuestring) < maxlen-1) {
			strcpy(str, it->valuestring);
			return ESP_OK;
		}
	}

	char tmp[32];
	snprintf(tmp, sizeof(tmp), "Invalid field %s", name);
	httpd_resp_send_err(req,  HTTPD_400_BAD_REQUEST, tmp);
	return ESP_FAIL;
}

static esp_err_t http_cjson_get_num(httpd_req_t *req, const cJSON *obj, const char *name, int *ret) {
	const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, name);
	if (it == NULL) return ESP_OK;

	if (cJSON_IsNumber(it)) { // includes NULL check
		*ret = it->valueint;
		return ESP_OK;
	}

	char tmp[32];
	snprintf(tmp, sizeof(tmp), "Invalid field %s", name);
	httpd_resp_send_err(req,  HTTPD_400_BAD_REQUEST, tmp);
	return ESP_FAIL;
}

static esp_err_t http_conf_put(httpd_req_t *req) {
	cJSON *root = http_post_json(req);
	if (root == NULL) return ESP_FAIL;


	esp_err_t err = http_cjson_get_str(req, root, "ifx_host", conf.influx.host, sizeof(conf.influx.host));
	if (err != ESP_OK) return err;

	err = http_cjson_get_str(req, root, "ifx_db", conf.influx.db, sizeof(conf.influx.db));
	if (err != ESP_OK) return err;

	err = http_cjson_get_str(req, root, "ifx_pfx", conf.influx.pfx, sizeof(conf.influx.pfx));
	if (err != ESP_OK) return err;

	int tmp = conf.influx.interval_s;
	err = http_cjson_get_num(req, root, "ifx_int", &tmp);
	if (err != ESP_OK) return err;
	if (tmp < 0 || tmp > 0xFFFF) return ESP_FAIL;
	conf.influx.interval_s = tmp;

	int i = 0;
	const cJSON *clients = cJSON_GetObjectItemCaseSensitive(root, "ifx_clients");
	if (clients != NULL) {
		if (!cJSON_IsArray(clients)) {
			httpd_resp_send_err(req,  HTTPD_400_BAD_REQUEST, "Invalid field clients");
			return ESP_FAIL;
		}

		const cJSON *cli;
		cJSON_ArrayForEach(cli, clients) {
			if (i >= CONF_MAX_IFX_CLIENTS) {
				httpd_resp_send_err(req,  HTTPD_400_BAD_REQUEST, "Too many clients");
				return ESP_FAIL;
			}

			err = http_cjson_get_str(req, cli, "name", conf.influx.clients[i].name, CONF_IFX_CLI_NAME_LEN);
			if (err != ESP_OK) return err;

			char tmp[32];
			err = http_cjson_get_str(req, cli, "addr", tmp, sizeof(tmp));
			if (err != ESP_OK) return err;

			sscanf(tmp, "%llx", &conf.influx.clients[i].addr);
			i++;
		}
	}

	for (; i<CONF_MAX_IFX_CLIENTS; i++) {
		conf.influx.clients[i].name[0] = '\0';
		conf.influx.clients[i].addr = 0;
	}

	bt_results_clear();
	conf_store();
	httpd_resp_sendstr(req, "OK");
	return ESP_OK;
}

static const httpd_uri_t http_uris[] = {
	{
		.uri = "/",
		.method = HTTP_GET,
		.handler = http_index_html_handler,
	}, {
		.uri = "/conf.html",
		.method = HTTP_GET,
		.handler = http_conf_html_handler,
	}, {
		.uri = "/main.css",
		.method = HTTP_GET,
		.handler = http_main_css_handler,
	}, {
		.uri = "/main.js",
		.method = HTTP_GET,
		.handler = http_main_js_handler,
	}, {
		.uri = "/api/conf.json",
		.method = HTTP_GET,
		.handler = http_conf_handler,
	}, {
		.uri = "/api/conf.json",
		.method = HTTP_PUT,
		.handler = http_conf_put,
	}, {}
};

void http_init() {
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	ESP_LOGI("HTTP", "Starting HTTP Server");
	ESP_ERROR_CHECK(httpd_start(&server, &config));

	const httpd_uri_t *u = http_uris;
	while (u->handler) {
		httpd_register_uri_handler(server, u++);
	}
}
