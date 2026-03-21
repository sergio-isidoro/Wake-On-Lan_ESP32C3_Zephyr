#include "wifi.h"
#include "button.h"
#include <zephyr/kernel.h>
#include <stdio.h>

int main(void) {
    printf("\n========================================\n");
    printf(" ESP32-C3 Wake-On-LAN Starting...\n");
    printf("========================================\n");
    
    /* Initialize the BOOT button hardware */
    button_init();

    k_sleep(K_SECONDS(1));
    
    /* Initialize WoL worker and connect to Wi-Fi */
    wifi_init_and_connect();
    
    while (1) {
        k_sleep(K_FOREVER);
    }
    
    return 0;
}