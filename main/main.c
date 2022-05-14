
#include "bsp.h"
#include "src/ble_spp_server.h"
#include "src/console_ll.h"
#define BUFSIZE 256

/*Bluetooth echo task*/
void app_main() {
    char buf[BUFSIZE];
    uint16_t idx = 0;
    while (true) {
        idx = 0;
        while (buf[idx] != '\n') {
            buf[idx] = console_ll_getc(true);
            if (buf[idx] != '\0') {
                idx = (idx + 1) % BUFSIZE;
            }
        }
        for (int i = 0; i < idx; i++) {
            console_ll_putc(buf[i]);
        }
    }
}