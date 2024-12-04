#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/uart.h>

// Definir el dispositivo UART
const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

// Callback para manejar los datos del scanner
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
            struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    char msg[100];
    
    // Convertir dirección BLE a string
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    
    // Formatear mensaje
    snprintf(msg, sizeof(msg), "Device: %s, RSSI: %d\n", addr_str, rssi);
    
    // Enviar por UART
    for(int i = 0; msg[i] != '\0'; i++) {
        uart_poll_out(uart_dev, msg[i]);
    }
}

// Parámetros del scanner
static struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_PASSIVE,
    .options = BT_LE_SCAN_OPT_NONE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_WINDOW,
};

void main(void)
{
    int err;

    // Inicializar UART
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return;
    }

    // Inicializar Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    // Iniciar scanner
    err = bt_le_scan_start(&scan_param, scan_cb);
    if (err) {
        printk("Starting scanning failed (err %d)\n", err);
        return;
    }

    // Loop principal
    while (1) {
        k_sleep(K_SECONDS(1));
    }
}