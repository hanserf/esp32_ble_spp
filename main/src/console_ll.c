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

#define CONSOLE_PRINT_SIZE (256)
static const char *TAG = "console_ll";
// static console_ll_t uart_control_struct;
bool running = false;
QueueHandle_t rx_queue;
QueueHandle_t tx_queue;

static void __link_tx(const char *src, size_t size);
static void __link_rx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait);
static size_t __get_tx_queue_len();
static size_t __get_rx_queue_len();

void console_ll_init() {
    if (NULL == rx_queue) {
        rx_queue = xQueueCreate(CONSOLE_PRINT_SIZE, sizeof(char));
    }
    if (NULL == tx_queue) {
        tx_queue = xQueueCreate(CONSOLE_PRINT_SIZE, sizeof(char));
    }
    if (false == running) {
        register_rw_callbacks(__link_tx, __link_rx);
        register_get_uplink_len_callback(__get_rx_queue_len);
        setup_ble_spp();
    }
}

/* Get one Char from USART */
char console_ll_getc(bool block) {
    uint8_t a_char = '\0';
    TickType_t timeout = 0;
    if (block) {
        timeout = portMAX_DELAY;
    }
    ESP_ERROR_CHECK((xQueueReceive(rx_queue, &a_char, timeout) == pdPASS) ? ESP_OK : ESP_FAIL);
    return (char)a_char;
}

void console_ll_putc(char c) {
    ESP_ERROR_CHECK((xQueueGenericSend(tx_queue, &c, 0, queueSEND_TO_BACK) != pdPASS) ? ESP_OK : ESP_FAIL);
    if (c == '\n') {
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

static void __link_tx(const char *src, size_t size) {
    for (int i = 0; i < size; i++) {
        ESP_ERROR_CHECK((xQueueGenericSend(rx_queue, (&src[0] + i), 0, queueSEND_TO_BACK) != pdPASS) ? ESP_OK : ESP_FAIL);
    }
}
static void __link_rx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait) {
    for (int i = 0; i < length; i++) {
        ESP_ERROR_CHECK((xQueueGenericReceive(tx_queue, (&buf[0] + i), ticks_to_wait, queueSEND_TO_BACK) != pdPASS) ? ESP_OK : ESP_FAIL);
    }
}

static size_t __get_tx_queue_len() {
    return (size_t)uxQueueMessagesWaiting(tx_queue);
}
static size_t __get_rx_queue_len() {
    return (size_t)uxQueueMessagesWaiting(tx_queue);
}
