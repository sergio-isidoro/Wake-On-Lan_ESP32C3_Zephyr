#ifndef NOTIFY_H
#define NOTIFY_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Notification types */
typedef enum {
    NOTIFY_WOL_SENT,    /* 2 Blinks (500ms ON, 200ms OFF) */
    NOTIFY_PING_UPDATE  /* 1 Blink  (500ms ON) */
} notify_type_t;

/**
 * @brief Initialize GPIO for the Blue LED.
 */
void notify_init(void);

/**
 * @brief Notify the user of an event by blinking the blue LED.
 *        Also signals the display to refresh (shared memory on DevKitC,
 *        semaphore on ESP32-C3).
 *
 *        - NOTIFY_WOL_SENT:   2 blinks (500ms ON, 200ms OFF)
 *        - NOTIFY_PING_UPDATE: 1 blink  (500ms ON)
 */
void notify_event(notify_type_t type);

#endif /* NOTIFY_H */