#include "ota.h"
#include "display.h"

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Internos                                                            */
/* ------------------------------------------------------------------ */

#define OTA_RECV_BUF_SIZE   1024
#define OTA_STACK_SIZE      4096
#define OTA_THREAD_PRIORITY 5
#define OTA_CHECK_INTERVAL  K_HOURS(24)

/* Semáforo para disparar verificação OTA imediata (dado pelo wifi.c) */
K_SEM_DEFINE(sem_ota_trigger, 0, 1);

static char ota_recv_buf[OTA_RECV_BUF_SIZE];

/* ------------------------------------------------------------------ */
/*  Helpers de versão                                                   */
/* ------------------------------------------------------------------ */

/*
 * Compara versões no formato "X.Y.Z".
 * Retorna  1 se remote > local
 * Retorna  0 se igual
 * Retorna -1 se remote < local
 */
static int version_compare(const char *local, const char *remote) {
    int lmaj, lmin, lpat, rmaj, rmin, rpat;
    if (sscanf(local,  "%d.%d.%d", &lmaj, &lmin, &lpat) != 3) return -1;
    if (sscanf(remote, "%d.%d.%d", &rmaj, &rmin, &rpat) != 3) return -1;

    if (rmaj != lmaj) return (rmaj > lmaj) ? 1 : -1;
    if (rmin != lmin) return (rmin > lmin) ? 1 : -1;
    if (rpat != lpat) return (rpat > lpat) ? 1 : -1;
    return 0;
}

/*
 * Extrai a versão do nome do ficheiro: "firmware_1.0.1.bin" → "1.0.1"
 * Devolve true se conseguiu extrair.
 */
static bool extract_version_from_filename(const char *filename,
                                           char *out, size_t out_len) {
    /* Formato esperado: firmware_X.Y.Z.bin */
    const char *p = strstr(filename, "firmware_");
    if (!p) return false;
    p += strlen("firmware_");

    const char *end = strstr(p, ".bin");
    if (!end) return false;

    size_t len = (size_t)(end - p);
    if (len == 0 || len >= out_len) return false;

    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

/* ------------------------------------------------------------------ */
/*  HTTP(S) helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * Abre socket TCP para OTA_GITHUB_HOST:443.
 * TLS está configurado com CONFIG_NET_SOCKETS_SOCKOPT_TLS e
 * certificado raiz adicionado via TLS credentials (ver prj.conf).
 * Retorna fd >= 0 em caso de sucesso, negativo em erro.
 */
static int ota_connect(void) {
    struct zsock_addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct zsock_addrinfo *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", OTA_GITHUB_PORT);

    int err = zsock_getaddrinfo(OTA_GITHUB_HOST, port_str, &hints, &res);
    if (err || !res) {
        printk("[OTA] DNS failed: %d\n", err);
        return -1;
    }

    int fd = zsock_socket(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2);
    if (fd < 0) {
        printk("[OTA] Socket error: %d\n", errno);
        zsock_freeaddrinfo(res);
        return -1;
    }

    /* Verificação TLS do servidor */
    int verify = TLS_PEER_VERIFY_REQUIRED;
    zsock_setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));

    /* Tag do certificado raiz (definida em prj.conf + cert registado no arranque) */
    sec_tag_t tls_tag = 1;
    zsock_setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, &tls_tag, sizeof(tls_tag));

    /* Hostname para SNI */
    zsock_setsockopt(fd, SOL_TLS, TLS_HOSTNAME,
                     OTA_GITHUB_HOST, strlen(OTA_GITHUB_HOST));

    /* Timeout de receção: 10s */
    struct zsock_timeval tv = { .tv_sec = 10, .tv_usec = 0 };
    zsock_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (zsock_connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        printk("[OTA] Connect failed: %d\n", errno);
        zsock_close(fd);
        zsock_freeaddrinfo(res);
        return -1;
    }

    zsock_freeaddrinfo(res);
    return fd;
}

/*
 * Envia um GET e devolve ponteiro para o início do corpo HTTP (após \r\n\r\n).
 * Preenche buf com os dados recebidos.
 * Retorna NULL em caso de erro.
 */
static char *ota_http_get(int fd, const char *path, char *buf, size_t buf_len) {
    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: WOL-ESP32C3/1.0\r\n"
        "\r\n",
        path, OTA_GITHUB_HOST);

    if (zsock_send(fd, req, req_len, 0) < 0) {
        printk("[OTA] Send failed: %d\n", errno);
        return NULL;
    }

    int total = 0;
    while (total < (int)buf_len - 1) {
        int n = zsock_recv(fd, buf + total, buf_len - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';

    /* Localiza o fim dos headers */
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) return NULL;
    return body + 4;
}

/* ------------------------------------------------------------------ */
/*  Passo 1: verificar versão disponível                               */
/* ------------------------------------------------------------------ */

/*
 * Lê latest.txt do GitHub. Formato do ficheiro:
 *   firmware_1.0.1.bin
 *
 * Preenche remote_version ("1.0.1") e remote_filename ("firmware_1.0.1.bin").
 * Retorna true se encontrou versão mais recente que FIRMWARE_VERSION.
 */
static bool ota_fetch_latest(char *remote_version, size_t rv_len,
                              char *remote_filename, size_t rf_len) {
    int fd = ota_connect();
    if (fd < 0) return false;

    char *body = ota_http_get(fd, OTA_INDEX_URL, ota_recv_buf, sizeof(ota_recv_buf));
    zsock_close(fd);

    if (!body) {
        printk("[OTA] Failed to get latest.txt\n");
        return false;
    }

    /* Remove espaços/newlines do final */
    char *end = body + strlen(body) - 1;
    while (end > body && (*end == '\n' || *end == '\r' || *end == ' ')) {
        *end-- = '\0';
    }

    printk("[OTA] latest.txt: '%s'\n", body);

    /* Copia nome do ficheiro */
    strncpy(remote_filename, body, rf_len - 1);
    remote_filename[rf_len - 1] = '\0';

    /* Extrai versão do nome */
    if (!extract_version_from_filename(remote_filename, remote_version, rv_len)) {
        printk("[OTA] Cannot parse version from '%s'\n", remote_filename);
        return false;
    }

    printk("[OTA] Remote version: %s | Local version: %s\n",
           remote_version, FIRMWARE_VERSION);

    return (version_compare(FIRMWARE_VERSION, remote_version) == 1);
}

/* ------------------------------------------------------------------ */
/*  Passo 2: descarregar e gravar no slot secundário                   */
/* ------------------------------------------------------------------ */

static bool ota_download_and_flash(const char *filename) {
    char url_path[256];
    snprintf(url_path, sizeof(url_path), "%s%s", OTA_BIN_URL_PREFIX, filename);

    printk("[OTA] Downloading: https://%s%s\n", OTA_GITHUB_HOST, url_path);

    /* Abre slot secundário do MCUboot */
    const struct flash_area *fa;
    if (flash_area_open(FIXED_PARTITION_ID(slot1_partition), &fa) != 0) {
        printk("[OTA] Cannot open secondary slot\n");
        return false;
    }

    /* Apaga o slot secundário */
    if (flash_area_erase(fa, 0, fa->fa_size) != 0) {
        printk("[OTA] Erase failed\n");
        flash_area_close(fa);
        return false;
    }

    struct flash_img_context ctx;
    if (flash_img_init_id(&ctx, FIXED_PARTITION_ID(slot1_partition)) != 0) {
        printk("[OTA] flash_img_init failed\n");
        flash_area_close(fa);
        return false;
    }

    /* Download em stream — nova ligação pois a anterior já fechou */
    int fd = ota_connect();
    if (fd < 0) {
        flash_area_close(fa);
        return false;
    }

    /* Envia o pedido GET para o .bin */
    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: WOL-ESP32C3/1.0\r\n"
        "\r\n",
        url_path, OTA_GITHUB_HOST);
    zsock_send(fd, req, req_len, 0);

    /* Salta headers HTTP */
    bool headers_done = false;
    size_t bytes_written = 0;
    bool success = false;
    int leftover_len = 0;
    char leftover[OTA_RECV_BUF_SIZE];

    while (1) {
        int n = zsock_recv(fd, ota_recv_buf, sizeof(ota_recv_buf) - 1, 0);
        if (n <= 0) break;
        ota_recv_buf[n] = '\0';

        const char *data = ota_recv_buf;
        int data_len = n;

        if (!headers_done) {
            /* Procura \r\n\r\n neste chunk (pode estar dividido) */
            char combined[OTA_RECV_BUF_SIZE * 2];
            int combined_len = leftover_len + n;
            memcpy(combined, leftover, leftover_len);
            memcpy(combined + leftover_len, ota_recv_buf, n);
            combined[combined_len] = '\0';

            char *body_start = strstr(combined, "\r\n\r\n");
            if (body_start) {
                headers_done = true;
                body_start += 4;
                data = body_start;
                data_len = combined_len - (int)(body_start - combined);
            } else {
                /* Guarda para combinar com o próximo chunk */
                int keep = (combined_len > (int)sizeof(leftover) - 1)
                           ? (int)sizeof(leftover) - 1 : combined_len;
                memcpy(leftover, combined + combined_len - keep, keep);
                leftover_len = keep;
                continue;
            }
        }

        if (data_len > 0) {
            if (flash_img_buffered_write(&ctx, (const uint8_t *)data,
                                         data_len, false) != 0) {
                printk("[OTA] Flash write error at byte %zu\n", bytes_written);
                goto done;
            }
            bytes_written += data_len;

            if ((bytes_written % (64 * 1024)) == 0) {
                printk("[OTA] Written %zu KB...\n", bytes_written / 1024);
            }
        }
    }

    /* Flush final */
    if (flash_img_buffered_write(&ctx, NULL, 0, true) != 0) {
        printk("[OTA] Final flush error\n");
        goto done;
    }

    printk("[OTA] Download complete: %zu bytes written\n", bytes_written);
    success = (bytes_written > 0);

done:
    zsock_close(fd);
    flash_area_close(fa);
    return success;
}

/* ------------------------------------------------------------------ */
/*  Passo 3: solicitar swap ao MCUboot e reiniciar                     */
/* ------------------------------------------------------------------ */

static void ota_apply(void) {
    printk("[OTA] Requesting MCUboot upgrade...\n");

    if (boot_request_upgrade(BOOT_UPGRADE_PERMANENT) != 0) {
        printk("[OTA] boot_request_upgrade failed!\n");
        return;
    }

    printk("[OTA] Rebooting to apply update...\n");
    k_msleep(500);
    sys_reboot(SYS_REBOOT_COLD);
}

/* ------------------------------------------------------------------ */
/*  Função pública — chamada pelo wifi.c após ligar                    */
/* ------------------------------------------------------------------ */

void ota_check_and_update(void) {
    char remote_version[16];
    char remote_filename[64];

    printk("[OTA] Checking for updates (local: %s)...\n", FIRMWARE_VERSION);

    if (!ota_fetch_latest(remote_version, sizeof(remote_version),
                          remote_filename, sizeof(remote_filename))) {
        printk("[OTA] No update available or check failed.\n");
        return;
    }

    printk("[OTA] New version found: %s — starting download.\n", remote_version);

    if (!ota_download_and_flash(remote_filename)) {
        printk("[OTA] Download/flash failed — aborting.\n");
        return;
    }

    ota_apply();
    /* nunca chega aqui se bem sucedido */
}

/* ------------------------------------------------------------------ */
/*  Thread OTA — verifica logo ao arranque e depois 1x por dia        */
/* ------------------------------------------------------------------ */

static void ota_thread_entry(void *p1, void *p2, void *p3) {
    /* Aguarda o sinal do wifi.c (IP obtido) */
    k_sem_take(&sem_ota_trigger, K_FOREVER);

    while (1) {
        ota_check_and_update();

        /* Aguarda 24h ou novo trigger manual */
        k_sem_take(&sem_ota_trigger, OTA_CHECK_INTERVAL);
    }
}

K_THREAD_DEFINE(ota_tid, OTA_STACK_SIZE,
                ota_thread_entry, NULL, NULL, NULL,
                OTA_THREAD_PRIORITY, 0, 0);