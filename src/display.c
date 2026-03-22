#include "display.h"
#include "wifi.h"

K_SEM_DEFINE(sem_ui_refresh, 0, 1);
K_MUTEX_DEFINE(display_mutex);
K_SEM_DEFINE(sem_display_start, 0, 1);
K_SEM_DEFINE(sem_display_ready, 0, 1);  /* sinaliza ao wifi.c que o display está pronto */

extern char global_ip_str[INET_ADDRSTRLEN];
extern bool last_known_state;
extern char target_pc_ip[INET_ADDRSTRLEN];

bool display_ready         = false;  /* false = falhou; main.c para de alimentar o WDT */
bool display_ap_mode       = false;
bool display_wifi_ready    = false;
bool display_station_ready = false;
bool has_ip                = false;

void display_task_entry(void *p1, void *p2, void *p3) {

    k_msleep(1000);

    //k_sem_take(&sem_display_start, K_FOREVER);

    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    /* Tenta até 3s — resolve o problema intermitente de timing do I2C */
    int retries = 30;
    while (!device_is_ready(disp) && retries-- > 0) {
        k_msleep(100);
    }

    if (!device_is_ready(disp)) {
        printk("[DISPLAY] Device not ready — aguardando WDT reset.\n");
        /* display_ready fica false  → main.c para de alimentar o WDT
           sem_display_ready nunca é dado → wifi.c fica bloqueado em K_FOREVER
           WDT expira em ~3s e reinicia automaticamente */
        while (1) {
            k_msleep(1000);
        }
    }

    //k_mutex_lock(&display_mutex, K_FOREVER);
    int rc = cfb_framebuffer_init(disp);
    if (rc) {
        printk("[DISPLAY] cfb_framebuffer_init failed: %d — aguardando WDT reset.\n", rc);
        k_mutex_unlock(&display_mutex);
        while (1) {
            k_msleep(1000);
        }
    }
    cfb_framebuffer_set_font(disp, 0);
    cfb_framebuffer_invert(disp);

    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint8_t f_w, f_h;
    cfb_get_font_size(disp, 0, &f_w, &f_h);

    /* Limpa o ecrã antes de ligar — evita pixels residuais */
    cfb_framebuffer_clear(disp, false);
    cfb_framebuffer_finalize(disp);
    display_blanking_off(disp);
    k_mutex_unlock(&display_mutex);

    /* Display inicializado com sucesso — sinaliza à main e ao wifi */
    display_ready = true;
    k_sem_give(&sem_display_ready);

    /* ---- AP MODE: ecrã estático ---- */
    if (display_ap_mode) {
        //k_mutex_lock(&display_mutex, K_FOREVER);

        const char *line1 = "WOL_ESP";
        const char *line2 = "192.168";
        const char *line3 = ".4.1";

        int x1 = (screen_w - (strlen(line1) * f_w)) / 2;
        int x2 = (screen_w - (strlen(line2) * f_w)) / 2;
        int x3 = (screen_w - (strlen(line3) * f_w)) / 2;

        cfb_print(disp, line1, (x1 < 0) ? 0 : x1, -2);
        cfb_print(disp, line2, (x2 < 0) ? 0 : x2, 13);
        cfb_print(disp, line3, (x3 < 0) ? 0 : x3, 26);

        cfb_framebuffer_finalize(disp);
        k_mutex_unlock(&display_mutex);

        display_wifi_ready = true;

        while (1) { k_sem_take(&sem_ui_refresh, K_FOREVER); }
    }

    /* ---- STATION MODE: loop normal ---- */
    static char prev_ip[INET_ADDRSTRLEN] = "";
    uint8_t anim_state = 0;

    while (1) {
        k_sem_take(&sem_ui_refresh, K_MSEC(25));

        //k_mutex_lock(&display_mutex, K_FOREVER);

        /* 1. ANIMAÇÃO */
        const char *arrows[] = {
            ">      ", ">>     ", ">>>    ", ">>>>   ",
            ">>>>>  ", ">>>>>> ", ">>>>>>>", " >>>>>>",
            "  >>>>>", "   >>>>", "    >>>", "     >>", "      >"
        };
        cfb_print(disp, "                ", 0, 0);
        cfb_print(disp, arrows[anim_state], 0, 0);
        anim_state = (anim_state + 1) % 13;

        /* 2. ENDEREÇO IP (ou "Waiting" enquanto não ligado) */
        has_ip = (strlen(global_ip_str) > 0 && strcmp(global_ip_str, "0.0.0.0") != 0);

        if (!has_ip) {
            cfb_print(disp, "                ", 0, 12);
            cfb_print(disp, "Waiting", (screen_w - (7 * f_w)) / 2, 12);
            cfb_print(disp, "                ", 0, 26);
            cfb_print(disp, "WIFI", (screen_w - (4 * f_w)) / 2, 26);
            prev_ip[0] = '\0';
        } else {
            /* Ligado — mostra os dois últimos octetos do IP */
            char *short_ip = global_ip_str;
            int dots = 0;
            for (char *p = global_ip_str; *p != '\0'; p++) {
                if (*p == '.') {
                    dots++;
                    if (dots == 2) { short_ip = p + 1; break; }
                }
            }

            if (strcmp(short_ip, prev_ip) != 0 || strlen(prev_ip) == 0) {
                cfb_print(disp, "                ", 0, 12);
                int ip_x = (screen_w - (strlen(short_ip) * f_w)) / 2;
                cfb_print(disp, short_ip, (ip_x < 0) ? 0 : ip_x, 12);
                strncpy(prev_ip, short_ip, sizeof(prev_ip) - 1);
                prev_ip[sizeof(prev_ip) - 1] = '\0';
            } else {
                int ip_x = (screen_w - (strlen(prev_ip) * f_w)) / 2;
                cfb_print(disp, prev_ip, (ip_x < 0) ? 0 : ip_x, 12);
            }

            /* 3. IP DO PC ALVO */
            char *short_target = target_pc_ip;
            int tdots = 0;
            for (char *p = target_pc_ip; *p != '\0'; p++) {
                if (*p == '.') {
                    tdots++;
                    if (tdots == 2) { short_target = p + 1; break; }
                }
            }

            char target_line[20] = "";
            if (last_known_state) {
                snprintf(target_line, sizeof(target_line), "* %s", short_target);
            } else {
                snprintf(target_line, sizeof(target_line), "x %s", short_target);
            }

            int tgt_x = (screen_w - (strlen(target_line) * f_w)) / 2;
            cfb_print(disp, "                ", 0, 26);
            cfb_print(disp, target_line, (tgt_x < 0) ? 0 : tgt_x, 26);
        }

        cfb_framebuffer_finalize(disp);
        k_mutex_unlock(&display_mutex);
    }
}

K_THREAD_DEFINE(display_tid, 4096, display_task_entry, NULL, NULL, NULL, 7, 0, 0);