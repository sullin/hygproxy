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

void bt_results_clear();
float bt_result_get_clear_t(int i);
float bt_result_get_clear_h(int i);

float bt_result_get_t(int i);
float bt_result_get_h(int i);

#endif /* MAIN_BT_H_ */
