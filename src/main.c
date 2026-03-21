#include <zephyr/kernel.h>
#include <stdio.h>

#include "notify.h"
#include "wifi.h"
#include "button.h"
#include "display.h"

int main(void) {
    printf("\n========================================\n");
    printf(" ESP32-C3 Wake-On-LAN Starting...\n");
    printf("========================================\n");
    
    /* Initialize the BOOT button hardware */
    button_init();

    /* Initialize Notify by LED */
    notify_init();
    
    /* Initialize WoL worker and connect to Wi-Fi */
    wifi_init_and_connect();
    
    return 0;
}