#ifndef PORTAL_H
#define PORTAL_H

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/reboot.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief Starts the Captive Portal.
 *
 * 1. Enables Wi-Fi Access Point mode (SSID: WOL_ESP).
 * 2. Sets a static IP (192.168.4.1).
 * 3. Starts the DNS redirector task (intercepts all DNS queries).
 * 4. Starts the HTTP server (serves the configuration form).
 *
 * On ESP32 DevKitC (dual-core): also signals the appcpu display
 * thread via shared memory before starting the AP.
 */
void start_portal(void);

/**
 * @brief Stops the Captive Portal and disables AP mode.
 */
void stop_portal(void);

#endif /* PORTAL_H */