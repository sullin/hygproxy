/*
 * poller.c
 *
 * Hygrometer poller task
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
#include <string.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt_defs.h"
#include "bt.h"
#include "influx.h"
#include "conf.h"
#include "poller.h"

static xTaskHandle s_vcs_task_hdl = NULL;

#define POLL_INTERVAL_MIN_S	30

static void int64_to_bdaddr(esp_bd_addr_t adr, uint64_t i) {
	adr[0] = (i>>40) & 0xFF;
	adr[1] = (i>>32) & 0xFF;
	adr[2] = (i>>24) & 0xFF;
	adr[3] = (i>>16) & 0xFF;
	adr[4] = (i>>8) & 0xFF;
	adr[5] = (i>>0) & 0xFF;
}

static void poller_task(void *arg) {
	TickType_t xLastWakeTime;
	while(1) {
		if (conf.influx.interval_s < POLL_INTERVAL_MIN_S ||
				conf.influx.db[0] == '\0' ||
				conf.influx.host[0] == '\0') {
			vTaskDelay(1000/portTICK_PERIOD_MS);
			xLastWakeTime = xTaskGetTickCount();
			continue;
		}

		int i;
		for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
			const struct conf_influx_client *cli = &conf.influx.clients[i];
			if (cli->addr == 0 || cli->name[0] == '\0') continue;
			esp_bd_addr_t adr;
			int64_to_bdaddr(adr, cli->addr);
			ESP_LOGI("POLLER", "Request %d: %02x%02x%02x%02x%02x%02x", i,
					adr[0], adr[1], adr[2], adr[3], adr[4], adr[5]);
			const char *ret = bt_request_hyg(adr);
			if (ret == NULL) {
				ESP_LOGE("POLLER", " no result");
			} else {
				ESP_LOGI("POLLER", " result: %s", ret);

				float t, h;
				if (sscanf(ret, "T=%f H=%f", &t, &h) != 2) {
					ESP_LOGE("POLLER", " malformed result");
				} else {
					influx_report(adr, cli->name, t, h);
				}
			}
		}

		if (conf.influx.interval_s < POLL_INTERVAL_MIN_S) continue;
		uint32_t interval = conf.influx.interval_s * 1000 / portTICK_PERIOD_MS;
		vTaskDelayUntil( &xLastWakeTime, interval);
	}
}


void poller_init() {
	xTaskCreate(poller_task, "pollT", 4096, NULL, 5, &s_vcs_task_hdl);
}
