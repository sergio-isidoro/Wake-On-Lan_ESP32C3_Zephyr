#ifndef DISPLAY_H
#define DISPLAY_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/net/net_ip.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <zephyr/sys/reboot.h>
 
/* Semáforo para arrancar a thread do display (dado por wifi.c / portal.c) */
extern struct k_sem sem_display_start;
 
/* Semáforo de confirmação: display inicializado com sucesso (dado por display.c) */
extern struct k_sem sem_display_ready;
 
/* Semáforo para forçar refresh imediato do display */
extern struct k_sem sem_ui_refresh;
 
/* true = display inicializou com sucesso
   false = falhou — main.c para de alimentar o WDT e o sistema reinicia */
extern bool display_ready;
 
extern bool display_station_ready;
extern bool has_ip;
 
/* State flags */
extern bool display_ap_mode;
extern bool display_wifi_ready;
 
#endif /* DISPLAY_H */
 