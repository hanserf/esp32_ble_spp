
#include "bsp.h"
#include "src/ble_spp_server.h"
#include "src/console_ll.h"
#define BUFSIZE 256
static const char *TAG = "main";

/*Bluetooth echo task*/
void app_main() {
    console_ll_init();
    while (true) {
        /*Application loop*/
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
