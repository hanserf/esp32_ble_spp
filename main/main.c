
#include "bsp.h"
#include "src/ble_spp_server.h"
void app_main() {
    //xTaskCreate(&ble_spp_task, "BLE_SPP", 4096, NULL, 3, NULL);
    setup_ble_spp();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}