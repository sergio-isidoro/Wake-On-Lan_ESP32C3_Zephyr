#ifndef OTA_H
#define OTA_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* Versão atual do firmware — alterar a cada build antes de gerar o .bin */
#define FIRMWARE_VERSION "1.0.0"

/* URLs do GitHub (raw) para verificar e descarregar o firmware */
#define OTA_GITHUB_HOST    "raw.githubusercontent.com"
#define OTA_GITHUB_PORT    443
#define OTA_INDEX_URL      "/sergio-isidoro/Wake-On-Lan_ESP32C3_Zephyr/main/OTA/latest.txt"
#define OTA_BIN_URL_PREFIX "/sergio-isidoro/Wake-On-Lan_ESP32C3_Zephyr/main/OTA/"

/**
 * @brief Verifica se existe uma versão mais recente no GitHub e,
 *        se sim, descarrega e instala automaticamente.
 *        Chama sys_reboot() no final se a atualização for bem sucedida.
 */
void ota_check_and_update(void);

#endif /* OTA_H */