/*
 * bt.h
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

#ifndef MAIN_BT_H_
#define MAIN_BT_H_

#include "esp_bt_defs.h"

void bt_init();
const char* bt_request_hyg(esp_bd_addr_t addr);

#define BT_SCAN_MAX_NAME	10

struct bt_scan_result {
	esp_bd_addr_t addr;
	char name[BT_SCAN_MAX_NAME];
};

int bt_scan(struct bt_scan_result *res, int len);

#endif /* MAIN_BT_H_ */
