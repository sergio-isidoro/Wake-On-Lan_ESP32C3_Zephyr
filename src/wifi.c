#include "wifi.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <string.h>

#define SSID "wifi_ssid"          //Change to your Wi-Fi SSID
#define PASSWORD "wifi_password"  //Change to your Wi-Fi Password

#define WOL_PORT 9

static uint8_t target_mac[] = {0xE0, 0xD5, 0x5E, 0x22, 0x33, 0x44};       //Replace with the MAC address of the target computer's network card

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static struct k_work wol_work;

/* --- WAKE-ON-LAN LOGIC --- */
static void send_wol_worker(struct k_work *work) {
    uint8_t pkt[102];
    struct sockaddr_in dest;
    
    memset(pkt, 0xFF, 6);
    for (int i = 1; i <= 16; i++) {
        memcpy(&pkt[i * 6], target_mac, 6);
    }

    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;

    dest.sin_family = AF_INET;
    dest.sin_port = htons(WOL_PORT);
    dest.sin_addr.s_addr = 0xFFFFFFFF; 

    printf("Sending Magic Packet...\n");
    
    if (zsock_sendto(sock, pkt, sizeof(pkt), 0, (struct sockaddr *)&dest, sizeof(dest)) > 0) {
        printf(">>> WoL Magic Packet Sent Successfully!\n");
    } else {
        printf("Error: Failed to send WoL. Code: %d\n", errno);
    }
    
    zsock_close(sock);
}

void trigger_wol(void) {
    /* Submits the WoL task to the system workqueue */
    k_work_submit(&wol_work);
}

/* --- NETWORK CALLBACKS (Fixed for Zephyr 4.3 - uint64_t) --- */
static void handle_wifi_events(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        printf(">>> Router accepted the Wi-Fi connection!\n");
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        printf(">>> Wi-Fi Disconnected.\n");
    }
}

static void handle_ipv4_events(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        char ip_str[INET_ADDRSTRLEN];
        
        /* Access the interface's IPv4 configuration to read the assigned IP */
        if (iface->config.ip.ipv4) {
            /* Zephyr can store multiple IPs, let's read the first valid one */
            for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
                if (iface->config.ip.ipv4->unicast[i].ipv4.is_used) {
                    zsock_inet_ntop(AF_INET, 
                                    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr, 
                                    ip_str, sizeof(ip_str));
                    
                    printf("\n========================================\n");
                    printf(" >>> SUCCESS! Connected to the Network!\n");
                    printf(" >>> Assigned IP: %s\n", ip_str);
                    printf("========================================\n");
                    break;
                }
            }
        }
        
        printf("\nSystem is READY. Press the BOOT button to send the WoL packet.\n");
    }
}

/* --- WI-FI INITIALIZATION --- */
static void connect_wifi(void) {
    struct net_if *iface = net_if_get_default();
    
    struct wifi_connect_req_params params = {
        .ssid = (uint8_t *)SSID,
        .ssid_length = strlen(SSID),
        .psk = (uint8_t *)PASSWORD,
        .psk_length = strlen(PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
    };

    printf("Connecting to Wi-Fi network: %s...\n", SSID);
    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(struct wifi_connect_req_params));
}

void wifi_init_and_connect(void) {
    /* Initialize the Wake-on-LAN worker */
    k_work_init(&wol_work, send_wol_worker);

    /* Clean registration, no casts, just as the compiler expects */
    net_mgmt_init_event_callback(&wifi_cb, handle_wifi_events, 
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&ipv4_cb, handle_ipv4_events, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    connect_wifi();
}