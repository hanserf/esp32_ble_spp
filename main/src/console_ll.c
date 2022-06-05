/*This low level driver holds the uplink and downlink fifos for the ble serial port profile,
and acts as a null-modem for rerouting our virtual com port to any peripheral.
Implemented are a getc and putc function, as well as a formatted safe print function.
These functions can be used as redefined stdin and stdout FD for socket/vfs tasks. ie Console example
*/

#include "console_ll.h"
#include "ble_spp_server.h"
#include "bsp.h"
#include "shell_driver.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/unistd.h>

#define CONSOLE_LL_DBG DEBUG_CONSOLE_INTERFACE
#define CONSOLE_LL_NEWLINE ('\n')
#define CONSOLE_LL_TIMEOUT_MS (1000)
#define CONSOLE_LL_USE_NL_SEM (0)

static const char *TAG = "console_ll";
// static console_ll_t uart_control_struct;
bool running = false;
static QueueHandle_t rx_queue;
static QueueHandle_t tx_queue;
static SemaphoreHandle_t new_line_sem;

static int __link_rx(const char *src, size_t size);
static int __link_tx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait);
static ble_spp_relase_uplink_t enable_tx_cb;
static void __release_sem(size_t num_elements);

void console_ll_init() {
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
        enable_tx_cb = setup_ble_spp();
        MY_ASSERT_NOT(enable_tx_cb, NULL);
        new_line_sem = xSemaphoreCreateBinary();
        ESP_LOGI(TAG, "Console_ll initialized");
        xTaskCreate(shell_driver_task, "shell_creator", 1024, NULL, 2, NULL);
        running = true;
    }
}

/* Get one Char from USART */
char console_ll_getc(bool block) {
    uint8_t a_char = '\0';
    TickType_t timeout = pdMS_TO_TICKS(CONSOLE_LL_TIMEOUT_MS);
    if (block) {
        timeout = portMAX_DELAY;
    }
    if (xQueueReceive(rx_queue, &a_char, timeout) != pdPASS) {
        a_char = '\0';
    }
    return (char)a_char;
}

void console_ll_putc(char c) {
    MY_ASSERT_EQ(xQueueGenericSend(tx_queue, &c, 0, queueSEND_TO_BACK), pdPASS);
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
        enable_tx_cb();
    }
}

static int __link_rx(const char *src, size_t size) {
    int i = 0;
    char tmp = CONSOLE_LL_NEWLINE;
    if (rx_queue != NULL) {
        for (int i = 0; i < size; i++) {
            tmp = src[i];
            if (xQueueSend(rx_queue, &tmp, 0) != pdPASS) {
                i++;
                break;
            }
        }
    }
#if (CONSOLE_LL_DBG == 1)
    ESP_LOGI(TAG, "BTrx\tw:%d\tr:%d\t[%s]", i, size, src);
#endif
    return i;
}
static int __link_tx(uint8_t *buf, uint32_t length, TickType_t ticks_to_wait) {
    char tmp = CONSOLE_LL_NEWLINE;
    int i = -1;
    if (tx_queue != NULL) {
        for (i = 0; i < length; i++) {
            if (xQueueReceive(tx_queue, &tmp, ticks_to_wait) == pdPASS) {
                if (tmp != '\0') {
                    buf[i] = tmp;
                    if (tmp == CONSOLE_LL_NEWLINE) {
                        i++;
                        break;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
#if (CONSOLE_LL_DBG == 1)
        ESP_LOGI(TAG, "tx\tw:%d[%s]", length, buf);
#endif
    }
    return i;
}

int console_ll_getline(char *data, size_t size) {
    int i = 0;
    char tmp = 0;
#if (CONSOLE_LL_USE_NL_SEM == 1)
    if (pdPASS == xSemaphoreTake(new_line_sem, pdMS_TO_TICKS(0))) {
#endif
        for (i = 0; i < size; i++) {
            tmp = console_ll_getc(false);
            if (tmp == '\0') {
                break;
            } else {
                data[i] = tmp;
                if (tmp == CONSOLE_LL_NEWLINE) {
                    i++;
                    break;
                }
            }
        }
#if (CONSOLE_LL_USE_NL_SEM == 1)
    }
#endif
    if (i == 0) {
        i = -1;
    }
    return i;
}
int console_ll_putline(const char *data, size_t size) {
    char tmp = 0;
    int tx_cntr = 0;
    if (size > 0) {
        for (int i = 0; i < size; i++) {
            tmp = data[i];
            if (xQueueSend(tx_queue, &tmp, pdMS_TO_TICKS(CONSOLE_LL_TIMEOUT_MS)) == pdPASS) {
                tx_cntr++;
            } else {
                /*Flush */
                break;
            }
        }
    }
    if (tx_cntr == 0) {
        tx_cntr = -1;
    }
    return tx_cntr;
}

static void __release_sem(size_t num_elements) {
#if (CONSOLE_LL_USE_NL_SEM == 1)
#if (CONSOLE_LL_DBG == 1)
    ESP_LOGI(TAG, "newline");
#endif
    MY_ASSERT_EQ(xSemaphoreGive(new_line_sem), pdPASS);
#endif
}