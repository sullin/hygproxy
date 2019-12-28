/*
 * influx.c
 *
 * Functions to send measurements to Influx server
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

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "esp_bt_defs.h"
#include "conf.h"
#include "influx.h"

#define PORT			8089

char buf[128];

static int influx_escape(char *b, int len, const char *s) {
	while (*s != '\0') {
		char c = *s++;
		if (!isprint(c)) continue;
		if (c == ',' || c == ' ' || c == '=') {
			if (len == 0) return -1;
			*b++ = '\\';
			len--;
		}
		if (len == 0) return -1;
		*b++ = c;
		len--;
	}
	if (len == 0) return -1;
	*b = '\0';
	return 0;
}

static void influx_send(const char *buf) {
	ESP_LOGV("IFX", "Send: [%s]", buf);

	struct sockaddr_in dest_addr;
	int addr_family;
	int ip_protocol;

	dest_addr.sin_addr.s_addr = inet_addr(conf.influx.host);
	if (dest_addr.sin_addr.s_addr == INADDR_NONE) {
		ESP_LOGE("IFX", "Unable to parse address %s", conf.influx.host);
		return;
	}
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(PORT);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;

	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
	if (sock < 0) {
		ESP_LOGE("IFX", "Unable to create socket: errno %d", errno);
		return;
	}

	int err = sendto(sock, buf, strlen(buf), 0,
			(struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err < 0) {
		ESP_LOGE("IFX", "Unable to send data");
	}
	close(sock);
}

void influx_report(esp_bd_addr_t sensor, const char *name, float temp, float hyg) {
	char namebuf[32];
	if (influx_escape(namebuf, sizeof(namebuf), name)) return;

	const char* spacer = "";
	if (conf.influx.pfx[0] != '\0') {
		spacer = ",";
	}

	snprintf(buf, sizeof(buf), "%s,type=bt,id=%02x%02x%02x%02x%02x%02x,name=%s%s%s temperature=%.1f,humidity=%.1f",
			conf.influx.db,
			sensor[0], sensor[1], sensor[2], sensor[3],
			sensor[4], sensor[5],
			name, spacer, conf.influx.pfx, temp, hyg);

	influx_send(buf);
}
