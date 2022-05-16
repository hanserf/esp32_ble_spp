
#include "bsp.h"
#include "src/ble_spp_server.h"
#include "src/console_ll.h"
#define BUFSIZE 256
static const char *TAG = "main";
SemaphoreHandle_t new_line_sem;
static size_t read_size = 0;
static void __release_sem();

/*Bluetooth echo task*/
void app_main() {
    char buf[BUFSIZE];
    read_size = 0;
    new_line_sem = xSemaphoreCreateBinary();
    uint16_t idx = 0;
    console_ll_init(__release_sem);
    while (true) {
        /* This will block until a new line is ready */
        if (pdPASS == xSemaphoreTake(new_line_sem, portMAX_DELAY)) {
            ESP_ERROR_CHECK((read_size < BUFSIZE) ? ESP_OK : ESP_FAIL);
            ESP_LOGI(TAG, "Processing new line");
            for (int i = 0; i < read_size; i++) {
                buf[i] = console_ll_getc(true);
            }
            /*Echo back reply*/
            buf[read_size] = '\0';
            ESP_LOGI(TAG, "New string len\t%d:\t[%s]", read_size, buf);
            for (int i = 0; i < read_size; i++) {
                console_ll_putc(buf[i]);
            }
            memset(buf, 0, idx);
            read_size = 0;
        }
    }
}

static void __release_sem(size_t num_elements) {
    ESP_LOGI(TAG, "newline");
    read_size = num_elements;
    MY_ASSERT_EQ(xSemaphoreGive(new_line_sem), pdPASS);
}