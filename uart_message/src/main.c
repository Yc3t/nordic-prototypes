

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <stdio.h>
#include <string.h>

const struct device *uart0 = DEVICE_DT_GET(DT_NODELABEL(uart0));

struct uart_config uart_cfg = {
	.baudrate = 115200,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	.data_bits = UART_CFG_DATA_BITS_8,
};

void send_str(const struct device *uart, char *str)
{
	int msg_len = strlen(str);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart, str[i]);
		k_busy_wait(100);
	}

	printk("Device %s sent: \"%s\"\n", uart->name, str);
}

int main(void)
{
	int rc;
	char send_buf[64];
	int count = 0;

	if (!device_is_ready(uart0)) {
		printk("UART device not ready\n");
		return -1;
	}

	rc = uart_configure(uart0, &uart_cfg);
	if (rc) {
		printk("Could not configure device %s\n", uart0->name);
		return rc;
	}

	send_str(uart0, "UART Test Starting...\r\n");
	k_sleep(K_MSEC(100));

	while (1) {
		snprintf(send_buf, 64, "Test message %d\r\n", count++);
		send_str(uart0, send_buf);
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
