#ifndef WIFI_H
#define WIFI_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/icmp.h>
#include <stdio.h>
#include <string.h>

extern struct k_sem sem_ui_refresh;
extern char global_ip_str[INET_ADDRSTRLEN];
extern bool last_known_state;

/**
 * @brief Initializes network event callbacks, the WoL worker, and requests Wi-Fi connection.
 */
void wifi_init_and_connect(void);

/**
 * @brief Triggers the Wake-on-LAN Magic Packet transmission.
 */
void trigger_wol(void);

#endif /* WIFI_H */