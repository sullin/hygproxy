/*
 * conf.h
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

#ifndef MAIN_CONF_H_
#define MAIN_CONF_H_

#include <stdint.h>


#define CONF_IFX_CLI_NAME_LEN	32
#define CONF_MAX_IFX_CLIENTS	8
#define CONF_MAX_IFX_HOSTLEN	32
#define CONF_MAX_IFX_DB			16
#define CONF_MAX_IFX_PFX		32

struct conf_influx_client {
	uint64_t addr;
	char name[CONF_IFX_CLI_NAME_LEN];
};

struct conf {
	struct conf_influx {
		struct conf_influx_client clients[CONF_MAX_IFX_CLIENTS];
		char host[CONF_MAX_IFX_HOSTLEN];
		char db[CONF_MAX_IFX_DB];
		char pfx[CONF_MAX_IFX_PFX];
		uint16_t interval_s;
	} influx;
};

extern struct conf conf;

void conf_init();
void conf_store();

#endif /* MAIN_CONF_H_ */
