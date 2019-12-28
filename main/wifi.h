/*
 * wifi.h
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

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

void wifi_init();
void wifi_disconnect();
void wifi_connect(const char *ssid, const char *pass);



#endif /* MAIN_WIFI_H_ */
