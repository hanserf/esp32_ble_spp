
#include "bsp.h"
#include "src/ble_spp_server.h"
#include "src/console_ll.h"
#define BUFSIZE 256
static const char *TAG = "main";

/*Bluetooth echo task*/
void app_main() {
    char buf[BUFSIZE];
    size_t read_size = 0;
    uint16_t idx = 0;
    char tmp = 0;
    console_ll_init();
    while (true) {
        /* This will block until a new line is ready */
        read_size = console_ll_getline(buf, BUFSIZE);
        /* Append the string with a '\0' as this is stripped away by transport layer */
        buf[read_size] = '\0';
        /* We keep the newline character in the string since it is needed by linenoise and is part of shell command,
        but we substitute it when doing the print to make debug nicer */
        tmp = buf[read_size - 1];
        buf[read_size - 1] = '\0';
        ESP_LOGI(TAG, "New string len\t%d:\t%s", read_size, buf);
        buf[read_size - 1] = tmp;
        console_ll_putline(buf, read_size);
        memset(buf, 0, idx);
        read_size = 0;
    }
}
