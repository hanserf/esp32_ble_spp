/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "shell_driver.h"
#include "argtable3/argtable3.h"
#include "ble_spp_server.h"
#include "cmd_decl.h"
#include "console_ll.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/unistd.h>

static const char *VFS_MOUNTPOINT = "/link/0";
static const char *TAG = "shell";
static TaskHandle_t console_task;
static int main_fd = -1;
static void shell_init(void *ctxt);
static int console_writefn(void *cookie, const char *data, int size);
static int console_readfn(void *cookie, char *data, int size);
static int console_openfn(const char *path, int flags, int mode);

static ssize_t vfs_write(int fd, const void *data, size_t size);
static ssize_t vfs_read(int fd, void *dst, size_t size);

static esp_vfs_t myfs;
/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem() {
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true};
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

//static int console_writefn(void *cookie, const char *data, int size) {
//    (void)cookie;
//    int rc = console_ll_putline(data, size);
//    return rc;
//}
//
//static int console_readfn(void *cookie, char *data, int size) {
//    (void)cookie;
//    int rc = console_ll_getline(data, size);
//    return rc;
//}

static int console_writefn(void *cookie, const char *data, int size) {
    int res = write(main_fd, data, size);
    if (res == -1) {
        /* Console connection closed */
        return 0;
    }
    return res;
}
static int console_readfn(void *cookie, char *data, int size) {
    int res = read(main_fd, data, size);
    if (res == -1) {
        return 0;
    }
    return res;
}

/*
'ssize_t (*)(int,  void *, size_t)' { aka 'int (*)(int,  void *, unsigned int)' }

'ssize_t (*)(void *, int,  void *, size_t)' {aka 'int (*)(void *, int,  void *, unsigned int)'}[-Wincompatible - pointer - types]
*/
static void
initialize_console() {

    stdout = fwopen(NULL, &console_writefn);
    setvbuf(stdout, NULL, _IONBF, 0);
    stdin = fropen(NULL, &console_readfn);
    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Initialize the console */
    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
        .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
}

void shell_driver_task(void *ctxt) {
    (void)ctxt;
    while (true) {
        if (ble_server_connected()) {
            if (NULL == console_task) {
                xTaskCreate(shell_init, "shell", 8092, NULL, 4, &console_task);
            }
        } else {
            if (NULL != console_task) {
                vTaskDelete(console_task);
                console_task = NULL;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void shell_init(void *ctxt) {
    (void)ctxt;
    ESP_LOGI(TAG, "Switch my console stdout and stdin to socket connection");
    vTaskDelay(pdMS_TO_TICKS(500));
    initialize_nvs();
    /* Register our driver in with the VFS */
    myfs.flags = ESP_VFS_FLAG_DEFAULT; // Leave this to ESP_VFS_FLAG_DEFAULT if you don't need the context
    myfs.write = &vfs_write;
    myfs.read = &vfs_read;
    myfs.open = &console_openfn;

    ESP_ERROR_CHECK(esp_vfs_register(VFS_MOUNTPOINT, &myfs, NULL));
    main_fd = esp_vfs_open(__getreent(), VFS_MOUNTPOINT, myfs.flags, O_RDWR);
    MY_ASSERT_NOT(main_fd, -1);

#if CONFIG_STORE_HISTORY
    initialize_filesystem();
#endif

    initialize_console();

    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_nvs();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char *prompt = LOG_COLOR_I "esp32> " LOG_RESET_COLOR;

    printf("\n"
           "This is an example of ESP-IDF console component.\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n");

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = "esp32> ";
#endif //CONFIG_LOG_COLORS
    }

    /* Main loop */
    while (true) {
        if (0 > main_fd) {
            ESP_LOGW(TAG, "fd is %d, breaking console", main_fd);
            break;
        }
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char *line = linenoise(prompt);
        if (line == NULL) { /* Ignore empty lines */
            continue;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);
#if CONFIG_STORE_HISTORY
        /* Save command history to filesystem */
        linenoiseHistorySave(HISTORY_PATH);
#endif

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
    vTaskDelete(NULL);
    console_task = NULL;
}

static ssize_t vfs_write(int fd, const void *data, size_t size) {
    ssize_t rc = fd;
    if (fd > 0) {
        rc = console_ll_putline(data, size);
    }
    return rc;
}
static ssize_t vfs_read(int fd, void *dst, size_t size) {
    ssize_t rc = fd;
    if (fd > 0) {
        rc = console_ll_getline(dst, size);
    }
    return rc;
}

static int console_openfn(const char *path, int flags, int mode) {
    // this is fairly primitive, we should check if file is opened read only,
    // and error out if write is requested
    int fd = 1;
    ESP_LOGI(TAG, "Custom vfs open @ %s", path);
    //if (strcmp(path, "/0") == 0) {
    //    fd = 0;
    //} else if (strcmp(path, "/1") == 0) {
    //    fd = 1;
    //} else if (strcmp(path, "/2") == 0) {
    //    fd = 2;
    //} else {
    //    errno = ENOENT;
    //    return fd;
    //}
    return fd;
}
