#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include <stdio.h>

#include "storage.h"
#include "notify.h"
#include "button.h"
#include "wifi.h"
#include "portal.h"
#include "display.h"

#define WDT_TIMEOUT_MS  5000

int main(void) {
    k_msleep(100);

#ifdef CONFIG_SOC_ESP32
    /* Zero shared memory BEFORE appcpu reads it */
    memset((void *)SHARED_STATE(), 0, sizeof(shared_display_state_t));
#endif

    const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt1));
    int wdt_channel = -1;

    char ssid[32], pass[64], mac[18], pc_ip[INET_ADDRSTRLEN];

    storage_init();
    notify_init();
    button_init();

    if (device_is_ready(wdt_dev)) {
        struct wdt_timeout_cfg wdt_config = {
            .window.max = WDT_TIMEOUT_MS,
            .flags = WDT_FLAG_RESET_SOC
        };
        wdt_channel = wdt_install_timeout(wdt_dev, &wdt_config);
        wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    }

    int rc = storage_read_config(ssid, pass, mac, pc_ip);

    if (rc == 0 && strlen(ssid) > 0 && strlen(mac) == 17) {
        printf("[SYSTEM] Config loaded. Starting Station mode...\n");
        wifi_init_and_connect(ssid, pass, mac, pc_ip);
    } else {
        if (rc == 0) {
            printf("[SYSTEM] Invalid config in Flash. Clearing...\n");
            storage_clear_all();
        }
        printf("[SYSTEM] Starting Portal...\n");
        start_portal();
    }

    printf("\n--- Wake-on-LAN ESP32 DevKitC ---\n");

    while (1) {
#ifdef CONFIG_SOC_ESP32
        bool current_has_ip = SHARED_STATE()->has_ip;
#else
        bool current_has_ip = has_ip;
#endif

        if (!current_has_ip) {
            if (button_is_pressed()) {
                printf("[SYSTEM] FACTORY RESET ACTIVATED!\n");
                storage_clear_all();
                notify_event(NOTIFY_WOL_SENT);
                k_msleep(1000);
                sys_reboot(SYS_REBOOT_COLD);
            }
        }

        if (wdt_channel >= 0) {
            wdt_feed(wdt_dev, wdt_channel);
        }

        k_msleep(500);
    }

    return 0;
}
