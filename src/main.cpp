#include "picovga.h"
#include "vga_config.h"

#include "object_store.h"
#include "nextion_parse.h"
#include "render_vga.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include <stdio.h>

#include "demo_main_nxb.h"

#ifndef NEXTION_UART
#define NEXTION_UART uart0
#endif
#ifndef NEXTION_BAUD
#define NEXTION_BAUD 115200
#endif
#ifndef NEXTION_TX_PIN
#define NEXTION_TX_PIN 16
#endif
#ifndef NEXTION_RX_PIN
#define NEXTION_RX_PIN 17
#endif

static object_store_t Store;

static void uart_init_nextion(void)
{
	uart_init(NEXTION_UART, NEXTION_BAUD);
	gpio_set_function(NEXTION_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(NEXTION_RX_PIN, GPIO_FUNC_UART);
	uart_set_hw_flow(NEXTION_UART, false, false);
	uart_set_format(NEXTION_UART, 8, 1, UART_PARITY_NONE);
}

int main()
{
	stdio_init_all();
	uart_init_nextion();

	if (store_load_nxb(&Store, demo_main_nxb, demo_main_nxb_len) != 0) {
		/* Fallback: black screen if scene missing/invalid */
		while (true)
			tight_loop_contents();
	}

	nextion_init(&Store);
	render_init(&Store);

	/* Demo: bump n0 once so something moves without UART */
	store_set_val(&Store, "n0", 1234);
	render_update(&Store);

	while (true) {
		while (uart_is_readable(NEXTION_UART))
			nextion_feed((uint8_t)uart_getc(NEXTION_UART));

		if (Store.dirty)
			render_update(&Store);

		tight_loop_contents();
	}
}
