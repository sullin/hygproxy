/*
 * wifi.c
 *
 * WiFi support
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
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "io.h"
#include "wifi.h"

int wifi_disconnected = 0;

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGV("WIFI", "started\n");
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGV("WIFI", "disconnected\n");
		led_set(0);
		xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
		if (!wifi_disconnected) esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		char tmp[20];
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI("WIFI", "ip:%s",
				 esp_ip4addr_ntoa(&event->ip_info.ip, tmp, sizeof(tmp)));
		led_set(1);
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init() {
	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(
			esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
					&wifi_event_handler, NULL));
	ESP_ERROR_CHECK(
			esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
					&wifi_event_handler, NULL));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI("WIFI", "Initialized");
}

void wifi_disconnect() {
	wifi_disconnected = 1;
	esp_wifi_disconnect();
}

void wifi_connect(const char *ssid, const char *pass) {
	ESP_LOGI("WIFI", "Connecting to SSID:%s", ssid);

	esp_wifi_disconnect();

	wifi_disconnected = 0;

	wifi_config_t wifi_config = { 0 };
	strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (pass) {
		strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
	}

	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_connect() );
}

int wifi_wait_conn(int timeout_ms) {
	int bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
			pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
	return (bits & WIFI_CONNECTED_BIT) != 0;
}
