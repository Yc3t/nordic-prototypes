#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(ble_scanner, LOG_LEVEL_INF);

/* Estructura para almacenar datos de advertisement*/
struct __packed raw_adv_data {
    uint8_t addr[6];          /* Dirección MAC */
    uint8_t addr_type;        /* Tipo de dirección */
    uint8_t adv_type;         /* Tipo de advertisement */
    int8_t  rssi;            /* Valor RSSI */
    uint8_t data_len;        /* Longitud de datos */
    uint8_t data[31];        /* Datos de advertisement */
};

/* Definiciones para el protocolo UART */
#define UART_HEADER_MAGIC      0x55    /* Patrón de sincronización: 01010101 */
#define UART_HEADER_LENGTH     4       /* Longitud de la cabecera */
#define MSG_TYPE_ADV_DATA      0x01    /* Tipo mensaje: datos advertisement */
#define UART_BAUD_RATE    115200
#define BITS_PER_BYTE     10      // 8 datos + 1 start + 1 stop


/* Estructura del mensaje UART */
struct __packed uart_message {
    uint8_t header[UART_HEADER_LENGTH];    /* [0x55, 0x55, 0x55, 0x55] */
    uint8_t type;                          /* Tipo de mensaje */
    uint8_t sequence;                      /* Número de secuencia */
    struct raw_adv_data adv_data;          /* Datos de advertisement */
};

static const struct device *uart_dev; // Puntero al dispositivo UART
static uint8_t msg_sequence = 0; //Contador de secuencia

#define METADATA_SIZE (sizeof(uint8_t) * 2)  // type + sequence

static void print_uart_stats(void)
{
    LOG_INF("-- MENSAJE UART--:");
    LOG_INF("- Header size: %d bytes", UART_HEADER_LENGTH);
    LOG_INF("- Metadata size: %d bytes", METADATA_SIZE);
    LOG_INF("- Advertisement data size: %d bytes", (int)sizeof(struct raw_adv_data));
    LOG_INF("- Total message size: %d bytes", (int)sizeof(struct uart_message));
    
    /* Calculamos el tiempo de transmisión en microsegundos */
    uint32_t bits_per_msg = sizeof(struct uart_message) * BITS_PER_BYTE;
    uint32_t time_us = (bits_per_msg * 1000000) / UART_BAUD_RATE;
    LOG_INF("- Transmission time: %u.%03u ms", time_us/1000, time_us%1000);
}


/* Función para enviar datos por UART */
static void send_uart_message(const struct raw_adv_data *adv_data)
{
    struct uart_message msg;
    
    /* Limpia toda la estructura del mensaje */
    memset(&msg, 0, sizeof(struct uart_message));
    
    /* Prepara el mensaje */
    memset(msg.header, UART_HEADER_MAGIC, UART_HEADER_LENGTH);
    msg.type = MSG_TYPE_ADV_DATA;
    msg.sequence = msg_sequence++;
    memcpy(&msg.adv_data, adv_data, sizeof(struct raw_adv_data));

    /* Mide tiempo de transmisión */
    int64_t start_time = k_uptime_get();

    /* Envía el mensaje byte por byte */
    const uint8_t *data = (const uint8_t *)&msg;
    for (size_t i = 0; i < sizeof(struct uart_message); i++) {
        uart_poll_out(uart_dev, data[i]);
    }

    /* Calcula tiempo transcurrido */
    int64_t elapsed_time = k_uptime_get() - start_time;
    LOG_INF("Mensaje %d transmitido en %lld ms", msg.sequence, elapsed_time);
}

/* Inicialización del UART */
static int uart_init(void)
{
    // Obtiene UART0 mediate DeviceTree
    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));  
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART no está listo");
        return -1;
    }
    return 0;
}

/* Callback de escaneo BLE*/
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi,
                   uint8_t adv_type, struct net_buf_simple *buf)
{
    /* Almacena los datos del advertisement */
    struct raw_adv_data adv_data = {0};
    
    memcpy(adv_data.addr, addr->a.val, sizeof(adv_data.addr)); // Dirección MAC
    adv_data.addr_type = addr->type; // Tipo de dirección
    adv_data.adv_type = adv_type; // Tipo Advertisiement
    adv_data.rssi = rssi; // RSSI

    /* Calcula y limita la longitud de los datos*/
    adv_data.data_len = MIN(buf->len, sizeof(adv_data.data));
    /* Copia los datos del advertisement */
    memcpy(adv_data.data, buf->data, adv_data.data_len);
    /* Envía a través del UART */
    send_uart_message(&adv_data);
}

/* Parámetros de escaneo BLE */
static struct bt_le_scan_param scan_param = {
    .type       = BT_LE_SCAN_TYPE_PASSIVE,
    .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
    .interval   = BT_GAP_SCAN_FAST_INTERVAL,
    .window     = BT_GAP_SCAN_FAST_WINDOW,
};

int main(void)
{
    int err;

    /* Inicializa UART */
    err = uart_init();
    if (err) {
        LOG_ERR("Falló inicialización UART (err %d)", err);
        return err;
    }

    /* Print UART statistics before starting BLE */
    LOG_INF("----------------------------------------");
    print_uart_stats();
    LOG_INF("----------------------------------------");

    LOG_INF("Iniciando scanner BLE...");

    /* Inicializa Bluetooth */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Falló inicialización Bluetooth (err %d)", err);
        return err;
    }

    /* Inicia escaneo */
    err = bt_le_scan_start(&scan_param, scan_cb);
    if (err) {
        LOG_ERR("Falló inicio de escaneo (err %d)", err);
        return err;
    }

    LOG_INF("Escaneo iniciado exitosamente");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
