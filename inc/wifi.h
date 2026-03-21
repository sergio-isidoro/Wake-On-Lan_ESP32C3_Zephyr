#ifndef WIFI_H
#define WIFI_H

/**
 * @brief Initializes network event callbacks, the WoL worker, and requests Wi-Fi connection.
 */
void wifi_init_and_connect(void);

/**
 * @brief Triggers the Wake-on-LAN Magic Packet transmission.
 */
void trigger_wol(void);

#endif /* WIFI_H */