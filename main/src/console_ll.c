/*This low level driver holds the uplink and downlink fifos for the ble serial port profile, 
and acts as a null-modem for rerouting our virtual com port to any peripheral.
Implemented are a getc and putc function, as well as a formatted safe print function.
These functions can be used as redefined stdin and stdout FD for socket/vfs tasks. ie Console example
*/

#include "console_ll.h"
#include "ble_spp_server.h"
#include "bsp.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/unistd.h>

#define CONSOLE_LL_DBG DEBUG_CONSOLE_INTERFACE
#define CONSOLE_PRINT_SIZE (256)
#define CONSOLE_LL_NEWLINE ('\n')
static const char *TAG = "console_ll";
// static console_ll_t uart_control_struct;
bool running = false;
QueueHandle_t rx_queue;
QueueHandle_t tx_queue;

static void __link_rx(const char *src, size_t size);
static void __link_tx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait);
static size_t __get_tx_queue_len();
static size_t __get_rx_queue_len();
static ble_spp_relase_uplink_t enable_tx_cb;
static ble_spp_new_downlink_t signal_newline_callback;
void console_ll_init(ble_spp_new_downlink_t signal_newline_cb) {
    if (NULL == rx_queue) {
        rx_queue = xQueueCreate(CONSOLE_PRINT_SIZE, sizeof(char));
    }
    if (NULL == tx_queue) {
        tx_queue = xQueueCreate(CONSOLE_PRINT_SIZE, sizeof(char));
    }
    if (false == running) {
        enable_tx_cb = NULL;
        ESP_LOGI(TAG, "Starting up ble link");
        register_rw_callbacks(__link_rx, __link_tx);
        register_get_uplink_len_callback(__get_tx_queue_len);
        enable_tx_cb = setup_ble_spp();
        MY_ASSERT_NOT(enable_tx_cb, NULL);
        MY_ASSERT_NOT(signal_newline_cb, NULL);
        signal_newline_callback = signal_newline_cb;
        ESP_LOGI(TAG, "Console_ll initialized");
        running = true;
    }
}

/* Get one Char from USART */
char console_ll_getc(bool block) {
    uint8_t a_char = CONSOLE_LL_NEWLINE;
    TickType_t timeout = 0;
    if (block) {
        timeout = portMAX_DELAY;
    }
    MY_ASSERT_EQ(xQueueReceive(rx_queue, &a_char, timeout), pdPASS);
    return (char)a_char;
}

void console_ll_putc(char c) {
    MY_ASSERT_EQ(xQueueGenericSend(tx_queue, &c, 0, queueSEND_TO_BACK), pdPASS);
    if (CONSOLE_LL_NEWLINE == c) {
#if (CONSOLE_LL_DBG == 1)
        ESP_LOGI(TAG, "Relasing TX");
#endif
        enable_tx_cb();
    }
}

void console_printf(const char *str, ...) {
    char buf[CONSOLE_PRINT_SIZE];
    int rc = 0;
    va_list ptr;
    va_start(ptr, str);
    rc = vsnprintf(buf, CONSOLE_PRINT_SIZE, str, ptr);
    va_end(ptr);
    if (rc > 0) {
        /*Copy to tx buffer*/
        for (int i = 0; i < rc; i++) {
            console_ll_putc(buf[i]);
        }
    }
}

static void __link_rx(const char *src, size_t size) {
#if (CONSOLE_LL_DBG == 1)
    ESP_LOGI(TAG, "rx: %s", src);
#endif
    char tmp = CONSOLE_LL_NEWLINE;
    if (rx_queue != NULL) {
        for (int i = 0; i < size; i++) {
            tmp = src[i];
            MY_ASSERT_EQ(xQueueGenericSend(rx_queue, &tmp, 0, queueSEND_TO_BACK), pdPASS);
        }
        //tmp = '\0';
        //MY_ASSERT_EQ(xQueueGenericSend(rx_queue, &tmp, 0, queueSEND_TO_BACK), pdPASS);
#if (CONSOLE_LL_DBG == 1)
        ESP_LOGI(TAG, "New data %s", src);
#endif
        signal_newline_callback(size);
    }
}
static void __link_tx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait) {
    char tmp = CONSOLE_LL_NEWLINE;
    if (tx_queue != NULL) {
        for (int i = 0; i < length; i++) {
            MY_ASSERT_EQ(xQueueGenericReceive(tx_queue, &tmp, ticks_to_wait, queueSEND_TO_BACK), pdPASS);
            buf[i] = tmp;
        }
    }
#if (CONSOLE_LL_DBG == 1)
    ESP_LOGI(TAG, "tx: %s", buf);
#endif
}

static size_t __get_tx_queue_len() {
    return (size_t)uxQueueMessagesWaiting(tx_queue);
}
static size_t __get_rx_queue_len() {
    return (size_t)uxQueueMessagesWaiting(rx_queue);
}
