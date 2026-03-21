#include "wifi.h"
#include "notify.h"

#define SSID        "WIFI"           // Change to your Wi-Fi SSID
#define PASSWORD    "PASSWORD"       // Change to your Wi-Fi Password

#define WOL_PORT    9

#define TARGET_IP   "192.168.1.111"     // Target PC IP (Ensure Windows Firewall allows ICMP)

/* Target MAC address to be woken up */
static uint8_t target_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; 

/* Global variables for UI/Display synchronization */
char global_ip_str[INET_ADDRSTRLEN] = "";
extern struct k_sem sem_ui_refresh;

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static struct k_work wol_work;

/* --- PING (ICMP) VARIABLES --- */
static struct net_icmp_ctx ping_ctx;
static struct k_work_delayable ping_work;
static volatile bool target_is_online = false;
bool last_known_state = false;
static bool first_ping_done = false;
static uint16_t ping_seq = 0;
static uint8_t ping_attempts = 0;

/* --- WAKE-ON-LAN LOGIC --- */
static void send_wol_worker(struct k_work *work) {
    uint8_t pkt[102];
    struct sockaddr_in dest;
    
    /* Construct Magic Packet: 6 bytes of 0xFF followed by 16 repetitions of MAC */
    memset(pkt, 0xFF, 6);
    for (int i = 1; i <= 16; i++) {
        memcpy(&pkt[i * 6], target_mac, 6);
    }

    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;

    dest.sin_family = AF_INET;
    dest.sin_port = htons(WOL_PORT);
    dest.sin_addr.s_addr = 0xFFFFFFFF; /* Limited Broadcast */

    printf("Sending Magic Packet...\n");
    
    if (zsock_sendto(sock, pkt, sizeof(pkt), 0, (struct sockaddr *)&dest, sizeof(dest)) > 0) {
        printf(">>> WoL Magic Packet Sent Successfully!\n");
        notify_event(NOTIFY_WOL_SENT);
    } else {
        printf("Error: Failed to send WoL. Code: %d\n", errno);
    }
    
    zsock_close(sock);
}

void trigger_wol(void) {
    /* Submits the WoL task to the system workqueue */
    k_work_submit(&wol_work);
}

/* --- PING LOGIC --- */
static int ping_handler(struct net_icmp_ctx *ctx, struct net_pkt *pkt, 
                        struct net_icmp_ip_hdr *ip_hdr, struct net_icmp_hdr *icmp_hdr, 
                        void *user_data) {
    target_is_online = true; /* Echo Reply received! */
    return 0;
}

static void ping_worker_handler(struct k_work *work) {
    /* 1. Evaluate previous ping result (if any) */
    if (ping_attempts > 0) {
        if (target_is_online) {
            /* SUCCESS! Target is reachable */
            if (!first_ping_done || !last_known_state) {
                printf("\n[PING] Target PC (%s) is now ONLINE!\n", TARGET_IP);
                last_known_state = true;
                first_ping_done = true;
                
                /* Notify: Updates Display + 1 Blue LED blink (500ms) */
                notify_event(NOTIFY_PING_UPDATE); 
            }
            
            ping_attempts = 0; /* Reset counter for next cycle */
            k_work_reschedule(&ping_work, K_MINUTES(1)); /* Wait 1 min for next check */
            return;
            
        } else if (ping_attempts >= 3) {
            /* FAILED ALL 3 ATTEMPTS! Target is considered offline */
            if (!first_ping_done || last_known_state) {
                printf("\n[PING] Target PC (%s) is now OFFLINE.\n", TARGET_IP);
                last_known_state = false;
                first_ping_done = true;
                
                /* Notify: Updates Display + 1 Blue LED blink (500ms) */
                notify_event(NOTIFY_PING_UPDATE);
            }
            
            ping_attempts = 0; /* Reset counter for next cycle */
            k_work_reschedule(&ping_work, K_MINUTES(1)); /* Wait 1 min for next check */
            return;
        }
    }

    /* 2. Prepare and send an Echo Request (Attempt 1, 2, or 3) */
    target_is_online = false;
    ping_attempts++;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    zsock_inet_pton(AF_INET, TARGET_IP, &dest.sin_addr);

    struct net_icmp_ping_params params = {
        .identifier = 0x1234,
        .sequence = ping_seq++,
        .tc_tos = 0,
        .priority = 0,
    };

    /* Send the ICMP packet via Zephyr network stack */
    net_icmp_send_echo_request(&ping_ctx, net_if_get_default(), 
                               (struct sockaddr *)&dest, &params, NULL);

    /* 3. Wait 1 second for a response before the next attempt or evaluation */
    k_work_reschedule(&ping_work, K_SECONDS(1));
}

/* --- NETWORK CALLBACKS --- */
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
        
        /* Check if interface has a valid IPv4 address assigned via DHCP */
        if (iface->config.ip.ipv4) {
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

        /* Copy IP to global string and notify Display thread */
        strncpy(global_ip_str, ip_str, sizeof(global_ip_str));
        k_sem_give(&sem_ui_refresh);
        
        printf("\nSystem is READY. Press the BOOT button to send the WoL packet.\n");

        /* STARTUP INSTRUCTION: Start monitoring immediately */
        k_work_reschedule(&ping_work, K_NO_WAIT);
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
    /* Initialize workers for WoL and Ping logic */
    k_work_init(&wol_work, send_wol_worker);
    k_work_init_delayable(&ping_work, ping_worker_handler);
    
    /* Set up ICMP context for Echo Reply handling */
    net_icmp_init_ctx(&ping_ctx, AF_INET, NET_ICMPV4_ECHO_REPLY, 0, ping_handler);

    /* Register Wi-Fi and IPv4 management event callbacks */
    net_mgmt_init_event_callback(&wifi_cb, handle_wifi_events, 
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&ipv4_cb, handle_ipv4_events, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    connect_wifi();
}