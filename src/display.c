#include "display.h"

K_SEM_DEFINE(sem_ui_refresh, 0, 1);
K_MUTEX_DEFINE(display_mutex);

extern char global_ip_str[INET_ADDRSTRLEN];
extern bool last_known_state;

void display_task_entry(void *p1, void *p2, void *p3) {
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) return;

    /* Persistence buffers */
    static char prev_ip[INET_ADDRSTRLEN] = "";
    static int prev_status = -1;
    uint8_t anim_state = 0;

    k_mutex_lock(&display_mutex, K_FOREVER);
    cfb_framebuffer_init(disp);
    cfb_framebuffer_set_font(disp, 0);
    cfb_framebuffer_invert(disp);
    display_blanking_off(disp);
    
    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint8_t f_w, f_h;
    cfb_get_font_size(disp, 0, &f_w, &f_h);

    cfb_framebuffer_clear(disp, true);
    k_mutex_unlock(&display_mutex);

    while (1) {
        k_sem_take(&sem_ui_refresh, K_MSEC(25));

        k_mutex_lock(&display_mutex, K_FOREVER);
        
        /* 1. ANIMATION (Always refreshed to show life) */
        const char *arrows[] = {">      ", ">>     ", ">>>    ", ">>>>   ", ">>>>>  ", ">>>>>> ", ">>>>>>>", " >>>>>>", "  >>>>>", "   >>>>", "    >>>", "     >>", "      >"};

        /* Clear ONLY the animation line (top 8 pixels) */
        cfb_print(disp, "                ", 0, 0); 
        cfb_print(disp, arrows[anim_state], 0, 0);
        anim_state = (anim_state + 1) % 13;

        /* 2. IP ADDRESS (Filtering and persistent check) */
        char *short_ip = global_ip_str;
        int dots = 0;
        for (char *p = global_ip_str; *p != '\0'; p++) {
            if (*p == '.') {
                dots++;
                if (dots == 2) { short_ip = p + 1; break; }
            }
        }

        /* ONLY redraw IP if it's different from what's on screen */
        if (strcmp(short_ip, prev_ip) != 0 || strlen(prev_ip) == 0) {
            cfb_print(disp, "                ", 0, 12); /* Clear row 2 */
            
            if (strlen(short_ip) > 0 && strcmp(global_ip_str, "0.0.0.0") != 0) {
                int ip_x = (screen_w - (strlen(short_ip) * f_w)) / 2;
                cfb_print(disp, short_ip, (ip_x < 0) ? 0 : ip_x, 12);
                strncpy(prev_ip, short_ip, sizeof(prev_ip));
            } else {
                cfb_print(disp, "WAIT IP", (screen_w - (7 * f_w)) / 2, 12);
            }
        } else {
            /* Keep printing the existing IP so it doesn't disappear */
            int ip_x = (screen_w - (strlen(prev_ip) * f_w)) / 2;
            cfb_print(disp, prev_ip, (ip_x < 0) ? 0 : ip_x, 12);
        }

        /* 3. STATUS (Persistent check) */
        const char *st_str = last_known_state ? "ONLINE" : "OFFLINE";
        if ((int)last_known_state != prev_status) {
            cfb_print(disp, "                ", 0, 26); /* Clear row 3 */
            prev_status = (int)last_known_state;
        }
        int st_x = (screen_w - (strlen(st_str) * f_w)) / 2;
        cfb_print(disp, st_str, st_x, 26);

        /* Push changes to hardware */
        cfb_framebuffer_finalize(disp);
        k_mutex_unlock(&display_mutex);
    }
}

K_THREAD_DEFINE(display_tid, 2048, display_task_entry, NULL, NULL, NULL, 7, 0, 0);