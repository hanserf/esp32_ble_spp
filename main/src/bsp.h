#pragma once
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>

#define BLE_SPP_USART (UART_NUM_0)
#define DEBUG_CONSOLE_INTERFACE 0
#define DEBUG_SPP_BT (1)
#define MY_ASSERT_EQ(x, y)                             \
    do {                                               \
        ESP_ERROR_CHECK((x == y) ? ESP_OK : ESP_FAIL); \
    } while (0)

#define MY_ASSERT_NOT(x, y)                            \
    do {                                               \
        ESP_ERROR_CHECK((x != y) ? ESP_OK : ESP_FAIL); \
    } while (0)
