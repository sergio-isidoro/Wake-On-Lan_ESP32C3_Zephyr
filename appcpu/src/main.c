#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/net/net_ip.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "shared_mem.h"
#include "custom_font.h"

/* Convenience macro — appcpu only reads from shared state */
#define S (SHARED_STATE())

int main(void) {

    /* Wait for procpu to signal display start */
    printk("[APPCPU] Display core waiting...\n");
    while (!S->display_start_flag) {
        k_msleep(10);
    }
    printk("[APPCPU] Display core starting.\n");

    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    int rc = cfb_framebuffer_init(disp);
    if (rc) {
        printk("[APPCPU] cfb_framebuffer_init failed: %d\n", rc);
        return -1;
    }

    // Apply the font loaded from custom_font.h
    cfb_framebuffer_set_font(disp, 0);
    cfb_framebuffer_invert(disp);

    /* --- FETCH TOTAL SCREEN DIMENSIONS --- */
    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint16_t screen_h = cfb_get_display_parameter(disp, CFB_DISPLAY_HEIGHT);
    uint8_t f_w, f_h;
    cfb_get_font_size(disp, 0, &f_w, &f_h);

    /* --- VERTICAL CENTERING MATH --- */
    // Divide the empty space by 4 (top margin, between row 1-2, between row 2-3, bottom margin)
    int gap_y = (screen_h - (3 * f_h)) / 4; 
    
    int y1 = gap_y;                 // Y position of row 1
    int y2 = y1 + f_h + gap_y;      // Y position of row 2
    int y3 = y2 + f_h + gap_y;      // Y position of row 3

    cfb_framebuffer_clear(disp, false);
    cfb_framebuffer_finalize(disp);
    display_blanking_off(disp);

    /* --- AP MODE: static screen --- */
    if (S->display_ap_mode) {
        const char *line1 = "WOL_ESP";
        const char *line2 = "192.168";
        const char *line3 = ".4.1";

        int x1 = (screen_w - (strlen(line1) * f_w)) / 2;
        int x2 = (screen_w - (strlen(line2) * f_w)) / 2;
        int x3 = (screen_w - (strlen(line3) * f_w)) / 2;

        cfb_print(disp, line1, (x1 < 0) ? 0 : x1, y1);
        cfb_print(disp, line2, (x2 < 0) ? 0 : x2, y2);
        cfb_print(disp, line3, (x3 < 0) ? 0 : x3, y3);
        cfb_framebuffer_finalize(disp);

        /* AP mode is static — just idle forever */
        while (1) { k_msleep(1000); }
    }

    /* --- STATION MODE: animated loop --- */
    static char prev_ip[INET_ADDRSTRLEN] = "";
    uint8_t anim_state = 0;

    while (1) {
        /* Rate-limit to ~40 FPS; also wake on ui_refresh_flag */
        if (S->ui_refresh_flag) {
            S->ui_refresh_flag = 0;
        } else {
            k_msleep(10);
        }

        /* ANIMATION (Y = y1) - Expanded to 21 characters wide (full screen width) */
        const char *arrows[] = {
            ">                    ", // 1
            ">>                   ", // 2
            ">>>                  ", // 3
            ">>>>                 ", // 4
            ">>>>>                ", // 5
            ">>>>>>               ", // 6
            ">>>>>>>              ", // 7
            ">>>>>>>>             ", // 8
            " >>>>>>>>            ", // 9
            "  >>>>>>>>           ", // 10
            "   >>>>>>>>          ", // 11
            "    >>>>>>>>         ", // 12
            "     >>>>>>>>        ", // 13
            "      >>>>>>>>       ", // 14
            "       >>>>>>>>      ", // 15
            "        >>>>>>>>     ", // 16
            "         >>>>>>>>    ", // 17
            "          >>>>>>>>   ", // 18
            "           >>>>>>>>  ", // 19
            "            >>>>>>>> ", // 20
            "             >>>>>>>>", // 21
            "              >>>>>>>", // 22
            "               >>>>>>", // 23
            "                >>>>>", // 24
            "                 >>>>", // 25
            "                  >>>", // 26
            "                   >>", // 27
            "                    >"  // 28
        };
        int num_frames = sizeof(arrows) / sizeof(arrows[0]); 
        int anim_w = 21 * f_w; // Total width in pixels (126 px)
        int anim_x = (screen_w - anim_w) / 2;
        
        // 21 spaces to clear the entire row from edge to edge
        cfb_print(disp, "                     ", anim_x, y1); 
        cfb_print(disp, arrows[anim_state], anim_x, y1);
        anim_state = (anim_state + 1) % num_frames;

        /* IP ADDRESS (Y = y2) */
        bool has_ip = S->has_ip;

        if (!has_ip) {
            cfb_print(disp, "                     ",             0, y2);
            cfb_print(disp, "Waiting",  (screen_w - (7 * f_w)) / 2, y2);
            cfb_print(disp, "                     ",             0, y3);
            cfb_print(disp, "WIFI",     (screen_w - (4 * f_w)) / 2, y3);
            prev_ip[0] = '\0';
        } else {
            char ip_buf[INET_ADDRSTRLEN];
            strncpy(ip_buf, (const char *)S->global_ip_str, sizeof(ip_buf) - 1);
            ip_buf[sizeof(ip_buf) - 1] = '\0';

            // Compare and draw the FULL IP address
            if (strcmp(ip_buf, prev_ip) != 0 || strlen(prev_ip) == 0) {
                cfb_print(disp, "                     ", 0, y2); // 21 spaces to clear
                int ip_x = (screen_w - (strlen(ip_buf) * f_w)) / 2;
                if (ip_x < 0) ip_x = 0;
                cfb_print(disp, ip_buf, ip_x, y2);
                
                strncpy(prev_ip, ip_buf, sizeof(prev_ip) - 1);
                prev_ip[sizeof(prev_ip) - 1] = '\0';
            } else {
                int ip_x = (screen_w - (strlen(prev_ip) * f_w)) / 2;
                if (ip_x < 0) ip_x = 0;
                cfb_print(disp, prev_ip, ip_x, y2);
            }

            /* TARGET PC FULL IP (Y = y3) */
            char tgt_buf[INET_ADDRSTRLEN];
            strncpy(tgt_buf, (const char *)S->target_pc_ip, sizeof(tgt_buf) - 1);
            tgt_buf[sizeof(tgt_buf) - 1] = '\0';

            char target_line[24] = "";
            snprintf(target_line, sizeof(target_line),
                     S->last_known_state ? " * %s " : " x %s ", tgt_buf);

            int text_len = strlen(target_line);
            int tgt_x = (screen_w - (text_len * f_w)) / 2;
            if (tgt_x < 0) tgt_x = 0;

            cfb_print(disp, "                     ", 0, y3); // 21 spaces to clear
            cfb_print(disp, target_line, tgt_x, y3);

        }

        cfb_framebuffer_finalize(disp);
    }

    return 0;
}