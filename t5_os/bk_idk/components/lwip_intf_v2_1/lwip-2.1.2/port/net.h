#ifndef _NET_H_
#define _NET_H_
#ifdef __cplusplus
extern "C" {
#endif

// Modified by TUYA Start
#include "stdbool.h"
// Modified by TUYA End
#include "lwip_netif_address.h"

extern void uap_ip_down(void);
extern void uap_ip_start(void);
extern void sta_ip_down(void);
extern void sta_ip_start(void);
extern uint32_t uap_ip_is_start(void);
extern uint32_t sta_ip_is_start(void);
extern void *net_get_sta_handle(void);
extern void *net_get_uap_handle(void);
extern int net_wlan_remove_netif(uint8_t *mac);
extern int net_get_if_macaddr(void *macaddr, void *intrfc_handle);
extern int net_get_if_addr(struct wlan_ip_config *addr, void *intrfc_handle);
extern void ip_address_set(int iface, int dhcp, char *ip, char *mask, char*gw, char*dns);
extern void sta_ip_mode_set(int dhcp);
#if CONFIG_WIFI6_CODE_STACK
extern bool etharp_tmr_flag;
extern void net_begin_send_arp_reply(bool is_send_arp, bool is_allow_send_req);
#endif
extern void net_restart_dhcp(void);
#ifdef CONFIG_ETH
extern int net_eth_add_netif(uint8_t *mac);
extern int net_eth_remove_netif(void);
extern void *net_get_eth_handle(void);
extern void eth_ip_start(void);
extern void eth_ip_down(void);
#endif
#if CONFIG_BRIDGE
extern void bridge_set_ip_start_flag(bool enable);
extern void bridge_ip_start(void);
extern void bridge_ip_stop(void);
extern uint32_t bridge_ip_is_start(void);
extern void *net_get_br_handle(void);
#endif
#if CONFIG_LWIP_PPP_SUPPORT
void *net_get_ppp_netif_handle(void);
void *net_get_ppp_pcb_handle(void);
void net_set_ppp_pcb_handle(void *ppp);  
#endif

#ifdef __cplusplus
}
#endif

#endif // _NET_H_
// eof

