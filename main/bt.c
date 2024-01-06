/*
 * bt.c
 *
 * Bluetooth LE connectivity
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


//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "conf.h"
#include "bt.h"

static const char* TAG = "BT";
SemaphoreHandle_t bt_mutex = NULL;
#define BT_MUTEX_WAIT	(1000 / portTICK_PERIOD_MS)

struct result {
	float h;
	float t;
};
struct result bt_results[CONF_MAX_IFX_CLIENTS];

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_PASSIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static uint64_t bdaddr_to_uint64(esp_bd_addr_t adr) {
	uint64_t ret = 0;
	ret |= (uint64_t)(adr[0]) << 40;
	ret |= (uint64_t)(adr[1]) << 32;
	ret |= (uint64_t)(adr[2]) << 24;
	ret |= (uint64_t)(adr[3]) << 16;
	ret |= (uint64_t)(adr[4]) << 8;
	ret |= (uint64_t)(adr[5]) << 0;
	return ret;
}

static int bt_find_dev(esp_bd_addr_t adr) {
	uint64_t adr64 = bdaddr_to_uint64(adr);
	int i;
	for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
		const struct conf_influx_client *cli = &conf.influx.clients[i];
		if (cli->addr == 0 || cli->name[0] == '\0') continue;
		if (cli->addr == adr64) return i;
	}
	return -1;
}

void bt_results_clear() {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return;
	int i;
	for (i=0; i<CONF_MAX_IFX_CLIENTS; i++) {
		bt_results[i].t = NAN;
		bt_results[i].h = NAN;
	}
	xSemaphoreGive(bt_mutex);
}
float bt_result_get_t(int i) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return NAN;
	float ret = bt_results[i].t;
	xSemaphoreGive(bt_mutex);
	return ret;
}
float bt_result_get_clear_t(int i) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return NAN;
	float ret = bt_results[i].t;
	bt_results[i].t = NAN;
	xSemaphoreGive(bt_mutex);
	return ret;
}
float bt_result_get_h(int i) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return NAN;
	float ret = bt_results[i].h;
	xSemaphoreGive(bt_mutex);
	return ret;
}
float bt_result_get_clear_h(int i) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return NAN;
	float ret = bt_results[i].h;
	bt_results[i].h = NAN;
	xSemaphoreGive(bt_mutex);
	return ret;
}
void bt_result_set_t(int i, float t) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return;
	bt_results[i].t = t;
	xSemaphoreGive(bt_mutex);
}
void bt_result_set_h(int i, float h) {
	if (!xSemaphoreTake(bt_mutex, BT_MUTEX_WAIT)) return;
	bt_results[i].h = h;
	xSemaphoreGive(bt_mutex);
}



void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	ESP_LOGV(TAG, "gap CB %d", (int) event);

	switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
	    esp_ble_gap_start_scanning(0);	// 0=permanent
		break;
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		ESP_LOGV(TAG, "scan started");
		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		ESP_LOGV(TAG, "gap rst %d", (int) param->scan_rst.search_evt);
		switch (param->scan_rst.search_evt) {
		case ESP_GAP_SEARCH_INQ_RES_EVT: {
			ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->scan_rst.bda, 6, ESP_LOG_DEBUG);

			uint8_t srv_data_len = 0;
			uint8_t *srv_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
					ESP_BLE_AD_TYPE_SERVICE_DATA, &srv_data_len);
			ESP_LOG_BUFFER_HEX_LEVEL(TAG, srv_data, srv_data_len, ESP_LOG_DEBUG);

			if (srv_data_len < 6) break;
			if (srv_data[0] != 0x95 || srv_data[1] != 0xFE) break;
			uint8_t hdr = srv_data[2];
			uint8_t ofs = 13;
			if (hdr & 0x20) ofs = 14;
			if (srv_data_len < ofs+3) break;
			srv_data += ofs;
			srv_data_len -= ofs;

			if (srv_data[1] != 0x10) break;
			if (srv_data_len-3 != srv_data[2]) break;

			int dev = bt_find_dev(param->scan_rst.bda);
			if (dev < 0) break;
			ESP_LOGV(TAG, "DEV %s", conf.influx.clients[dev].name);

			if (srv_data[0] == 0x04 && srv_data[2]==0x02) { // temp
				int16_t t = (srv_data[4]<<8) | srv_data[3];
				ESP_LOGV(TAG, "T %d", t);
				bt_result_set_t(dev, t / 10.0f);
			}
			if (srv_data[0] == 0x06 && srv_data[2]==0x02) { // hum
				uint16_t h = (srv_data[4]<<8) | srv_data[3];
				ESP_LOGV(TAG, "H %d", h);
				bt_result_set_h(dev, h / 10.0f);
			}
			if (srv_data[0] == 0x0D && srv_data[2]==0x04) { // temp+hum
				int16_t t = (srv_data[4]<<8) | srv_data[3];
				ESP_LOGV(TAG, "T %d", t);
				bt_result_set_t(dev, t / 10.0f);
				uint16_t h = (srv_data[6]<<8) | srv_data[5];
				ESP_LOGV(TAG, "H %d", h);
				bt_result_set_h(dev, h / 10.0f);
			}
			break;
		}
		default:
			break;
		}
		break;
	}

	default:
		break;
	}
}

void bt_init() {
	bt_mutex = xSemaphoreCreateMutex();

	//esp_log_level_set(TAG, ESP_LOG_VERBOSE);
	bt_results_clear();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
	ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P9));

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
	ESP_ERROR_CHECK(esp_bluedroid_enable());
	ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&ble_scan_params));
}


