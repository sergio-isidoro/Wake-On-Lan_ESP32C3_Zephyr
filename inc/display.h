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

/**
 * @brief Initializes the I2C OLED display.
 */
void display_init(void);

/**
 * @brief Updates the display with the current network and PC status.
 * * @param is_online Current status of the target PC.
 * @param ip Current IP address of the ESP32 (optional).
 */
void display_update_status(bool is_online, const char *ip);

#endif /* DISPLAY_H */