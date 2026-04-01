#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/net/net_ip.h>

/*
 * Shared memory between procpu and appcpu.
 *
 * Memory layout (ESP32 DRAM):
 *   0x3FFB0000 - 0x3FFBFFFF : AMP heap for appcpu (ESP_APPCPU_DRAM_SIZE=0x10000)
 *   0x3FFC0000 - 0x3FFFFFFF : procpu DRAM
 *   0x3FFE8000 - 0x3FFED4A4 : appcpu .data/.bss loaded by AMP loader
 *
 * Safe zone for shared mem: 0x3FFD8000
 * - Above AMP heap (0x3FFC0000)
 * - Below appcpu loaded DRAM (0x3FFE8000)
 * - In procpu DRAM region (accessible by both cores)
 */
#define SHARED_MEM_ADDR     0x3FFD8000

typedef struct {
    char     global_ip_str[INET_ADDRSTRLEN];
    char     target_pc_ip[INET_ADDRSTRLEN];
    bool     last_known_state;
    bool     display_ap_mode;
    bool     has_ip;
    volatile uint8_t  ui_refresh_flag;
    volatile uint8_t  display_start_flag;
} shared_display_state_t;

#define SHARED_STATE() ((volatile shared_display_state_t *)(SHARED_MEM_ADDR))

#endif /* SHARED_MEM_H */
