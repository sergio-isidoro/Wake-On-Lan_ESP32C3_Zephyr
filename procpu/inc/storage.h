#ifndef STORAGE_H
#define STORAGE_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/net/net_ip.h>
#include <string.h>
#include <zephyr/types.h>

/* Unique IDs for each NVS entry */
#define STORAGE_ID_SSID     1
#define STORAGE_ID_PASS     2
#define STORAGE_ID_MAC      3
#define STORAGE_ID_IP       4

/**
 * @brief Initializes the NVS storage subsystem.
 */
void storage_init(void);

/**
 * @brief Saves Wi-Fi settings and target PC MAC/IP to Flash.
 *
 * @param s  Wi-Fi SSID.
 * @param p  Wi-Fi password.
 * @param m  Target MAC address ("AA:BB:CC:DD:EE:FF").
 * @param ip Target PC IP address ("192.168.1.100").
 * @return 0 on success.
 */
int storage_write_config(const char *s, const char *p,
                         const char *m, const char *ip);

/**
 * @brief Reads saved settings from Flash.
 *
 * @param s  Buffer for SSID  (min 32 bytes).
 * @param p  Buffer for password (min 64 bytes).
 * @param m  Buffer for MAC   (min 18 bytes).
 * @param ip Buffer for IP    (min INET_ADDRSTRLEN bytes).
 * @return 0 on success, -1 if Flash is empty.
 */
int storage_read_config(char *s, char *p, char *m, char *ip);

/**
 * @brief Erases all settings from Flash (Factory Reset).
 */
void storage_clear_all(void);

#endif /* STORAGE_H */