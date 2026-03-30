#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * @brief Initializes the BOOT button GPIO and its interrupt.
 *        On press: triggers WoL via trigger_wol().
 */
void button_init(void);

/**
 * @brief Checks if the BOOT button is currently pressed.
 *
 * @return true if pressed, false otherwise.
 */
bool button_is_pressed(void);

#endif /* BUTTON_H */