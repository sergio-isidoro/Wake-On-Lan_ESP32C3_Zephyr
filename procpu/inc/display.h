#ifndef DISPLAY_H
#define DISPLAY_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <stdbool.h>

#ifdef CONFIG_SOC_ESP32

/* On DevKitC (dual-core): display state lives in shared memory.
 * procpu only writes to shared state — no local display variables. */
#include "shared_mem.h"

static inline void display_set_ap_mode(bool v)      { SHARED_STATE()->display_ap_mode    = v; }
static inline void display_set_has_ip(bool v)        { SHARED_STATE()->has_ip              = v; }
static inline void display_notify_start(void)        { SHARED_STATE()->display_start_flag  = 1; }
static inline void display_notify_refresh(void)      { SHARED_STATE()->ui_refresh_flag     = 1; }

/* Compatibility shims so existing call sites in notify.c / wifi.c compile unchanged.
 * sem_ui_refresh and sem_display_start are never used on procpu in dual-core mode —
 * replaced by shared memory flags. Defined as 0 to satisfy the compiler without
 * requiring a real k_sem object. fix #9: removed extern _display_noop_sem. */
#define display_station_ready   (1)
#define sem_ui_refresh          (*((struct k_sem *)NULL))   /* never dereferenced on procpu */
#define sem_display_start       (*((struct k_sem *)NULL))

#else

/* On ESP32-C3 (single-core): original semaphore-based interface */
extern struct k_sem sem_display_start;
extern struct k_sem sem_ui_refresh;

extern bool display_ap_mode;
extern bool display_wifi_ready;
extern bool display_station_ready;
extern bool has_ip;

#endif /* CONFIG_SOC_ESP32 */

void display_task_entry(void *p1, void *p2, void *p3);

#endif /* DISPLAY_H */