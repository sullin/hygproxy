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

#include <string.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "bt.h"

static esp_gatt_if_t bt_app_gattc_if;
static int bt_conn_id = -1;
static char bt_resp[32];

#define BT_TIMEOUT_TICKS	((30*1000)/portTICK_PERIOD_MS)
#define BT_CLOSE_TICKS		((10*1000)/portTICK_PERIOD_MS)
#define GAP_BLE_SCAN_DURATION_S	11
#define BT_SCAN_TIMEOUT_TICKS	(GAP_BLE_SCAN_DURATION_S*1000) / portTICK_PERIOD_MS

static enum state {
	STATE_UNINIT,
	STATE_REG,
	STATE_IDLE,
	STATE_OPEN,
	STATE_ACTIVE,
	STATE_CLOSE,

	STATE_SCAN_SET_PARAM,
	STATE_SCAN_START,
	STATE_SCAN_RESULT,
} state = STATE_REG;

static EventGroupHandle_t bt_event_group;
const int BT_RESULT_BIT = BIT0;
const int BT_FINISHED_BIT = BIT1;
const int BT_SCAN_FINISHED_BIT = BIT2;

#define HYG_NOTIFY_HANDLE	0x0e

SemaphoreHandle_t bt_mutex = NULL;
struct bt_scan_result *bt_scan_res;
int bt_scan_res_len;
int bt_scan_res_cnt;

void res_store(struct ble_scan_result_evt_param *res, uint8_t *adv_name, uint8_t adv_name_len) {
	if (bt_scan_res == NULL) return;
	if (adv_name_len > BT_SCAN_MAX_NAME-1)
		adv_name_len = BT_SCAN_MAX_NAME-1;


	int i;
	for (i=0; i<bt_scan_res_cnt; i++) {
		if (memcmp(bt_scan_res[i].addr, res->bda, sizeof(esp_bd_addr_t)) != 0)
			continue;
		if (bt_scan_res[bt_scan_res_cnt].name[0] != '\0') return;

		memcpy(bt_scan_res[bt_scan_res_cnt].name, adv_name, adv_name_len);
		bt_scan_res[bt_scan_res_cnt].name[adv_name_len] = '\0';
		return;
	}

	if (bt_scan_res_cnt >= bt_scan_res_len) return;

	memcpy(bt_scan_res[bt_scan_res_cnt].addr, res->bda, sizeof(esp_bd_addr_t));
	memcpy(bt_scan_res[bt_scan_res_cnt].name, adv_name, adv_name_len);
	bt_scan_res[bt_scan_res_cnt].name[adv_name_len] = '\0';
	bt_scan_res_cnt++;
}

void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	ESP_LOGV("BT", "gap CB %d", (int) event);

	switch (event) {
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
		if (state == STATE_SCAN_SET_PARAM) {
			if (param->scan_param_cmpl.status != ESP_BT_STATUS_SUCCESS) {
				state = STATE_IDLE;
				xEventGroupSetBits(bt_event_group, BT_SCAN_FINISHED_BIT);
				ESP_LOGV("BT", "scan set_param failed, error status = %x", param->scan_param_cmpl.status);
				break;
			}
			state = STATE_SCAN_START;
			esp_ble_gap_start_scanning(GAP_BLE_SCAN_DURATION_S);
		}
		break;
	}
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		if (state == STATE_SCAN_START) {
			if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
				state = STATE_IDLE;
				xEventGroupSetBits(bt_event_group, BT_SCAN_FINISHED_BIT);
				ESP_LOGV("BT", "scan start failed, error status = %x", param->scan_start_cmpl.status);
				break;
			}
			ESP_LOGV("BT", "scan start success");
			state = STATE_SCAN_RESULT;
		}
		break;
	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
		if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
			ESP_LOGV("BT", "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
			break;
		}
		ESP_LOGV("BT", "scan stop success");
		state = STATE_IDLE;
		xEventGroupSetBits(bt_event_group, BT_SCAN_FINISHED_BIT);
		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		switch (param->scan_rst.search_evt) {
		case ESP_GAP_SEARCH_INQ_RES_EVT: {
			uint8_t adv_name_len = 0;
			uint8_t *adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
												ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

			ESP_LOG_BUFFER_HEX_LEVEL("BT", param->scan_rst.bda, 6, ESP_LOG_DEBUG);
			ESP_LOG_BUFFER_CHAR_LEVEL("BT", adv_name, adv_name_len, ESP_LOG_DEBUG);

			if (state == STATE_SCAN_RESULT)
				res_store(&param->scan_rst, adv_name, adv_name_len);
			break;
		}
		case ESP_GAP_SEARCH_INQ_CMPL_EVT:
			ESP_LOGV("BT", "scan finished");
			state = STATE_IDLE;
			xEventGroupSetBits(bt_event_group, BT_SCAN_FINISHED_BIT);
			break;
		default:
			break;
		}
		break;
	}

	default:
		break;
	}
}

void gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param) {
	int ret;
	ESP_LOGV("BT", "gattc CB %d", (int) event);

	switch (event) {
	case ESP_GATTC_REG_EVT:
		if (param->reg.status == ESP_GATT_OK) {
			bt_app_gattc_if = gattc_if;
			state = STATE_IDLE;
		}
		break;

	case ESP_GATTC_CONNECT_EVT:
		ESP_LOGV("BT", "Connected %d", param->connect.conn_id);
		break;

	case ESP_GATTC_DISCONNECT_EVT:
		ESP_LOGV("BT", "Disconnected %d", param->disconnect.conn_id);
		break;

	case ESP_GATTC_OPEN_EVT: {
		if (state != STATE_OPEN || bt_conn_id != -1) {
			esp_ble_gattc_close(bt_app_gattc_if, param->open.conn_id);
			return;
		}
		if (param->open.status != ESP_GATT_OK) {
			ESP_LOGV("BT", "Failed opening connection");
			state = STATE_IDLE;
			xEventGroupSetBits(bt_event_group, BT_FINISHED_BIT);
			break;
		}
		ESP_LOGV("BT", "Opened connection %d", param->open.conn_id);
		bt_conn_id = param->open.conn_id;

		uint8_t val[2] = { 0x01, 0x00 };
		ret = esp_ble_gattc_write_char(bt_app_gattc_if, param->open.conn_id, 0x10,
				sizeof(val), val, ESP_GATT_WRITE_TYPE_NO_RSP,
				ESP_GATT_AUTH_REQ_NONE);
		if (ret != ESP_OK) {
			ESP_LOGV("BT", "Failed requesting write");
			state = STATE_CLOSE;
			esp_ble_gattc_close(bt_app_gattc_if, param->open.conn_id);
			xEventGroupSetBits(bt_event_group, BT_FINISHED_BIT);
			break;
		}
		ESP_LOGV("BT", "Requested writing char");
		state = STATE_ACTIVE;
		break;
	}

	case ESP_GATTC_CLOSE_EVT:
		if (bt_conn_id == param->close.conn_id) {
			ESP_LOGV("BT", "Closed connection");
			state = STATE_IDLE;
			bt_conn_id = -1;
			xEventGroupSetBits(bt_event_group, BT_FINISHED_BIT);
		}
		break;

	case ESP_GATTC_WRITE_CHAR_EVT:
		if (state != STATE_ACTIVE || bt_conn_id != param->write.conn_id) {
			esp_ble_gattc_close(bt_app_gattc_if, param->write.conn_id);
			return;
		}
		if (param->write.status != ESP_GATT_OK) {
			ESP_LOGV("BT", "Failed writing char");
			state = STATE_CLOSE;
			esp_ble_gattc_close(bt_app_gattc_if, param->write.conn_id);
			xEventGroupSetBits(bt_event_group, BT_FINISHED_BIT);
			break;
		}
		ESP_LOGV("BT", "Wrote char");
		break;

	case ESP_GATTC_NOTIFY_EVT: {
		if (state != STATE_ACTIVE ||
				param->notify.handle != HYG_NOTIFY_HANDLE ||
				bt_conn_id != param->notify.conn_id) {
			esp_ble_gattc_close(bt_app_gattc_if, param->notify.conn_id);
			return;
		}
		ESP_LOGV("BT", "Notify: %d bytes", param->notify.value_len);

		int len = param->notify.value_len;
		if (len >= sizeof(bt_resp)-1) break;
		memcpy(bt_resp, param->notify.value, len);
		bt_resp[len] = '\0';

		ESP_LOGV("BT", "Result: [%s]", bt_resp);
		state = STATE_CLOSE;
		esp_ble_gattc_close(bt_app_gattc_if, param->notify.conn_id);
		xEventGroupSetBits(bt_event_group, BT_RESULT_BIT);
		break;
	}

	default:
		break;
	}
}

void bt_init() {
	bt_event_group = xEventGroupCreate();

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
	ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
	ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P9));

	esp_bluedroid_config_t cfg = {
		.ssp_en = 0, // legacy pairing
	};

	ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&cfg));
	ESP_ERROR_CHECK(esp_bluedroid_enable());
	ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
	ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc_cb));
	ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));

	bt_mutex = xSemaphoreCreateMutex();
}

const char* bt_request_hyg(esp_bd_addr_t addr) {
	xSemaphoreTake(bt_mutex, portMAX_DELAY);
	if (state != STATE_IDLE) {
		ESP_LOGV("BT", "Not idle");
		xSemaphoreGive(bt_mutex);
		return NULL;
	}
	xEventGroupClearBits(bt_event_group, BT_RESULT_BIT | BT_FINISHED_BIT);

	ESP_LOGV("BT", "Opening connection");
	state = STATE_OPEN;
	int ret = esp_ble_gattc_open(bt_app_gattc_if, addr, BLE_ADDR_TYPE_PUBLIC,
			true);
	if (ret != ESP_OK) {
		state = STATE_IDLE;
		ESP_LOGV("BT", "Failed requesting connection");
		xSemaphoreGive(bt_mutex);
		return NULL;
	}

	esp_ble_gattc_register_for_notify(bt_app_gattc_if, addr, HYG_NOTIFY_HANDLE);
	xEventGroupWaitBits(bt_event_group,
			BT_FINISHED_BIT, 0, 0, BT_TIMEOUT_TICKS);
	esp_ble_gattc_unregister_for_notify(bt_app_gattc_if, addr, HYG_NOTIFY_HANDLE);

	if (xEventGroupGetBits(bt_event_group) & BT_RESULT_BIT) {
		xSemaphoreGive(bt_mutex);
		return bt_resp;
	}
	if (bt_conn_id != -1)
		esp_ble_gattc_close(bt_app_gattc_if, bt_conn_id);

	if (state != STATE_IDLE) {
		ESP_LOGV("BT", "Not idle, waiting");
		xEventGroupWaitBits(bt_event_group,
				BT_FINISHED_BIT, 0, 0, BT_CLOSE_TICKS);
	}

	xSemaphoreGive(bt_mutex);
	return NULL;
}

int bt_scan(struct bt_scan_result *res, int len) {
	xSemaphoreTake(bt_mutex, portMAX_DELAY);
	if (state != STATE_IDLE) {
		ESP_LOGV("BTSCAN", "Not idle");
		xSemaphoreGive(bt_mutex);
		return -1;
	}

	xEventGroupClearBits(bt_event_group, BT_SCAN_FINISHED_BIT);

	bt_scan_res = res;
	bt_scan_res_len = len;
	bt_scan_res_cnt = 0;

	static esp_ble_scan_params_t ble_scan_params = {
		.scan_type				= BLE_SCAN_TYPE_ACTIVE,
		.own_addr_type			= BLE_ADDR_TYPE_PUBLIC,
		.scan_filter_policy		= BLE_SCAN_FILTER_ALLOW_ALL,
		.scan_interval			= 0x4000,
		.scan_window			= 0x4000,
		.scan_duplicate			= BLE_SCAN_DUPLICATE_DISABLE
	};

	ESP_LOGV("BTSCAN", "Requesting");
	state = STATE_SCAN_SET_PARAM;
	esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
	if (scan_ret != ESP_OK){
		ESP_LOGV("BT", "set scan params error, error code = %x", scan_ret);
		state = STATE_IDLE;
		xSemaphoreGive(bt_mutex);
		return -1;
	}

	ESP_LOGV("BTSCAN", "Waiting");
	xEventGroupWaitBits(bt_event_group,
			BT_SCAN_FINISHED_BIT, 0, 0, BT_SCAN_TIMEOUT_TICKS);
	ESP_LOGV("BTSCAN", "Finished");

	if (state != STATE_IDLE)
		esp_ble_gap_stop_scanning();

	bt_scan_res = NULL;
	xSemaphoreGive(bt_mutex);
	return bt_scan_res_cnt;
}
