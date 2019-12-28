/*
 * conf.c
 *
 * Non-volatile configuration storage
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

#include <stdio.h>
#include <string.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "conf.h"

struct conf conf;


void conf_init() {
	nvs_handle_t hnd;
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &hnd);
	ESP_ERROR_CHECK( err );

	memset(&conf, 0, sizeof(conf));

	size_t len;
	int i;
	for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
		char tmp[32];

		len = sizeof(tmp);
		snprintf(tmp, sizeof(tmp), "ifx_%d_name", i);
		err = nvs_get_str(hnd, tmp, conf.influx.clients[i].name, &len);
		if (err != ESP_OK) continue;

		snprintf(tmp, sizeof(tmp), "ifx_%d_addr", i);
		err = nvs_get_u64(hnd, tmp, &conf.influx.clients[i].addr);
		if (err != ESP_OK) continue;
	}

	len = sizeof(conf.influx.host);
	nvs_get_str(hnd, "ifx_host", conf.influx.host, &len);
	len = sizeof(conf.influx.db);
	nvs_get_str(hnd, "ifx_db", conf.influx.db, &len);
	len = sizeof(conf.influx.pfx);
	nvs_get_str(hnd, "ifx_pfx", conf.influx.pfx, &len);

	nvs_get_u16(hnd, "ifx_intrvl", &conf.influx.interval_s);

	nvs_close(hnd);
}

void conf_store() {
	nvs_handle_t hnd;
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &hnd);
	ESP_ERROR_CHECK( err );

	int i;
	for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
		char tmp[32];

		snprintf(tmp, sizeof(tmp), "ifx_%d_name", i);
		if (conf.influx.clients[i].addr)
			nvs_set_str(hnd, tmp, conf.influx.clients[i].name);
		else
			nvs_erase_key(hnd, tmp);

		snprintf(tmp, sizeof(tmp), "ifx_%d_addr", i);
		if (conf.influx.clients[i].addr)
			nvs_set_u64(hnd, tmp, conf.influx.clients[i].addr);
		else
			nvs_erase_key(hnd, tmp);
	}

	nvs_set_str(hnd, "ifx_host", conf.influx.host);
	nvs_set_str(hnd, "ifx_db", conf.influx.db);
	nvs_set_str(hnd, "ifx_pfx", conf.influx.pfx);
	nvs_set_u16(hnd, "ifx_intrvl", conf.influx.interval_s);

	nvs_commit(hnd);
	nvs_close(hnd);
}
