/*
 * cli.c
 *
 * Command line interface for configuring WiFi
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

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"
#include "driver/uart.h"
#include "wifi.h"
#include "cli.h"

static int cli_disconnect(int argc, char **argv) {
	wifi_disconnect();
	return 0;
}

void cli_register_disconnect(void)
{
	const esp_console_cmd_t disconnect_cmd = {
		.command = "disconnect",
		.help = "Disconnect from WiFi",
		.hint = NULL,
		.func = &cli_disconnect,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&disconnect_cmd) );
}

///////////////////////////////////////////////////////////////////////////////
static struct {
	struct arg_str *ssid;
	struct arg_str *password;
	struct arg_end *end;
} cli_connect_args;

static int cli_connect(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **) &cli_connect_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, cli_connect_args.end, argv[0]);
		return 1;
	}
	ESP_LOGI(__func__, "Connecting to '%s'", cli_connect_args.ssid->sval[0]);

	wifi_connect(cli_connect_args.ssid->sval[0],
			cli_connect_args.password->sval[0]);
	return 0;
}

void cli_register_connect(void)
{
	cli_connect_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID");
	cli_connect_args.password = arg_str1(NULL, NULL, "<pass>", "PSK");
	cli_connect_args.end = arg_end(2);

	const esp_console_cmd_t connect_cmd = {
		.command = "connect",
		.help = "Connect to WiFi AP",
		.hint = NULL,
		.func = &cli_connect,
		.argtable = &cli_connect_args
	};

	ESP_ERROR_CHECK( esp_console_cmd_register(&connect_cmd) );
}


///////////////////////////////////////////////////////////////////////////////
void cli_init()
{
	setvbuf(stdin, NULL, _IONBF, 0);

	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
	esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

	const uart_config_t uart_config = {
			.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.use_ref_tick = true
	};
	ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

	ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
			256, 0, 0, NULL, 0) );

	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

	esp_console_config_t console_config = {
			.max_cmdline_args = 8,
			.max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
			.hint_color = atoi(LOG_COLOR_CYAN)
#endif
	};
	ESP_ERROR_CHECK( esp_console_init(&console_config) );

	linenoiseSetMultiLine(1);
	linenoiseSetCompletionCallback(&esp_console_get_completion);
	linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);
	linenoiseHistorySetMaxLen(100);

	esp_console_register_help_command();
	cli_register_connect();
	cli_register_disconnect();
}

///////////////////////////////////////////////////////////////////////////////
void cli_run() {

#if CONFIG_LOG_COLORS
	const char* prompt = LOG_COLOR_I "> " LOG_RESET_COLOR;
#else
	const char* prompt = "> ";
#endif //CONFIG_LOG_COLORS

	/* Figure out if the terminal supports escape sequences */
	if (linenoiseProbe() != 0) {
		linenoiseSetDumbMode(1);
	}

	while(true) {
		char* line = linenoise(prompt);
		if (line == NULL) { /* Ignore empty lines */
			continue;
		}

		linenoiseHistoryAdd(line);

		/* Try to run the command */
		int ret;
		esp_err_t err = esp_console_run(line, &ret);
		if (err == ESP_ERR_NOT_FOUND) {
			printf("Unrecognized command\n");
		} else if (err == ESP_ERR_INVALID_ARG) {
			// command was empty
		} else if (err == ESP_OK && ret != ESP_OK) {
			printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
		} else if (err != ESP_OK) {
			printf("Internal error: %s\n", esp_err_to_name(err));
		}
		/* linenoise allocates line buffer on the heap, so need to free it */
		linenoiseFree(line);
	}
}
