#include "notify.h"
#include "display.h"

static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);

static K_SEM_DEFINE(sem_blink, 0, 1);
static int blink_count;

static void blink_thread(void *p1, void *p2, void *p3) {
    while (1) {
        k_sem_take(&sem_blink, K_FOREVER);

        for (int i = 0; i < blink_count; i++) {
            gpio_pin_set_dt(&blue_led, 1);
            k_msleep(500);
            gpio_pin_set_dt(&blue_led, 0);
            if (i < blink_count - 1) {
                k_msleep(200);
            }
        }
    }
}
K_THREAD_DEFINE(blink_tid, 512, blink_thread, NULL, NULL, NULL, 10, 0, 0);

void notify_init(void) {
    if (!device_is_ready(blue_led.port)) {
        return;
    }
    gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_INACTIVE);
}

void notify_event(notify_type_t type) {
#ifdef CONFIG_SOC_ESP32
    /* Dual-core: signal appcpu via shared memory flag */
    display_notify_refresh();
#else
    /* Single-core: original semaphore */
    if (display_station_ready) {
        k_sem_give(&sem_ui_refresh);
    }
#endif

    blink_count = (type == NOTIFY_WOL_SENT) ? 2 : 1;
    k_sem_give(&sem_blink);
}