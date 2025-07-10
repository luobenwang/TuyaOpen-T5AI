#include "tuya_error_code.h"
#include "tkl_ipc.h"
#include "lwip/netif.h"
#include "lwip/apps/dhcpserver.h"
#include "lwip/apps/dhcpserver_options.h"
#include "lwip/ethernetif.h"
#include "tkl_wired.h"
#include "tkl_wifi.h"
#include "tkl_system.h"
#include "FreeRTOS.h"

TKL_WIRED_STATUS_CHANGE_CB wired_cb = NULL;
TKL_WIRED_STAT_E wired_status = TKL_WIRED_LINK_DOWN;
static void __dhcp_cb(LWIP_EVENT_E event, void *arg)
{
    if(event == IPV4_DHCP_SUCC) {
        wired_status = TKL_WIRED_LINK_UP;
    }else if(event == IPV4_DHCP_FAIL){
        wired_status = TKL_WIRED_LINK_DOWN;
    }

    if(wired_cb)
        wired_cb(wired_status);
}

extern void tuya_eth_dhcpv4_client_start_by_wq(void (*cb)(LWIP_EVENT_E event, void *arg), FAST_DHCP_INFO_T *ip_info);
void tkl_wired_ipc_func(struct ipc_msg_s *msg)
{
    switch(msg->subtype) {
        case TKL_IPC_TYPE_WIRED_STATUS_CHANGED:
        {
            struct ipc_msg_param_s *res = (struct ipc_msg_param_s *)msg->req_param;
            uint8_t event = *(uint8_t *)(res->p1);
            if(event == 1) {
                //启动dhcp服务
                tuya_eth_dhcpv4_client_start_by_wq(__dhcp_cb, NULL);
            }else {
                wired_status = TKL_WIRED_LINK_DOWN;
                if(wired_cb)
                    wired_cb(wired_status);
            }
            tuya_ipc_send_no_sync(msg);
        }
            break;

        default:
            break;
    }

    return;
}
/**
 * @brief  init create wired link
 *
 * @param[in]   cfg: the configure for wired link
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_init(TKL_WIRED_BASE_CFG_T *cfg)
{
    struct ipc_msg_s wr_ipc_msg = {0};
#if LWIP_DUAL_NET_SUPPORT
    struct netif *netif= tkl_lwip_get_netif_by_index(NETIF_ETH_IDX);
    if (netif == NULL) {
        bk_printf("wired init, get netif failed\r\n");
        return -1;
    }

    memset(&wr_ipc_msg, 0, sizeof(struct ipc_msg_s));
    wr_ipc_msg.type = TKL_IPC_TYPE_WIRED;
    wr_ipc_msg.subtype = TKL_IPC_TYPE_WIRED_INIT;
    wr_ipc_msg.req_param = netif;
    wr_ipc_msg.req_len = 4;
    OPERATE_RET ret = tuya_ipc_send_sync(&wr_ipc_msg);
    if(ret) {
        return ret;
    }
#endif
    return wr_ipc_msg.ret_value;
}

/**
 * @brief  get the link status of wired link
 *
 * @param[out]  is_up: the wired link status is up or not
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_get_status(TKL_WIRED_STAT_E *status)
{
    *status = wired_status;
    return OPRT_OK;
}

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_set_status_cb(TKL_WIRED_STATUS_CHANGE_CB cb)
{
    wired_cb = cb;
    return OPRT_OK;
}

/**
 * @brief  get the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_get_ip(NW_IP_S *ip)
{
#ifdef LWIP_DUAL_NET_SUPPORT
    struct netif *netif= tkl_lwip_get_netif_by_index(NETIF_ETH_IDX);
    ip4addr_ntoa_r(&netif->ip_addr, ip->ip, 16);
    ip4addr_ntoa_r(&netif->netmask, ip->mask, 16);
    ip4addr_ntoa_r(&netif->gw, ip->gw, 16);
#endif

    return OPRT_OK;
}

/**
 * @brief  set the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_set_ip(NW_IP_S *ip)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief  get the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_get_mac(NW_MAC_S *mac)
{
    struct ipc_msg_s wr_ipc_msg = {0};
    memset(&wr_ipc_msg, 0, sizeof(struct ipc_msg_s));
    wr_ipc_msg.type = TKL_IPC_TYPE_WIRED;
    wr_ipc_msg.subtype = TKL_IPC_TYPE_WIRED_GET_MAC;
    wr_ipc_msg.res_param = mac;
    wr_ipc_msg.res_len = 6;
    OPERATE_RET ret = tuya_ipc_send_sync(&wr_ipc_msg);
    if(ret)
        return ret;

    return OPRT_OK;
}

/**
 * @brief  set the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_set_mac(CONST NW_MAC_S *mac)
{
    return OPRT_OK;
}

/**
 * @brief wired ioctl
 *
 * @param[in]       cmd     refer to WIRED_IOCTL_CMD_E
 * @param[in]       args    args associated with the command
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wired_ioctl(WIRED_IOCTL_CMD_E cmd,  VOID *args)
{
    OPERATE_RET ret = 0;

    struct ipc_msg_s wr_ipc_msg = {0};
#if LWIP_DUAL_NET_SUPPORT

    switch (cmd) {
        case WIRED_IOCTL_GPIO_CFG:
        {
            if (args == NULL) {
                bk_printf("WIRED_IOCTL_GPIO_CFG failed, invalid param\r\n");
                return -1;
            }

            uint32_t len = sizeof(WIRED_IOCTL_GPIO_T);
            uint8_t *buf = (uint8_t *) tkl_system_malloc(len);
            if (buf == NULL) {
                bk_printf("wired init, malloc failed\r\n");
                return -1;
            }

            memset(&wr_ipc_msg, 0, sizeof(struct ipc_msg_s));
            wr_ipc_msg.type = TKL_IPC_TYPE_WIRED;
            wr_ipc_msg.subtype = TKL_IPC_TYPE_WIRED_SET_GPIO;

            memcpy(buf, (uint8_t *)args, sizeof(WIRED_IOCTL_GPIO_T));

            wr_ipc_msg.req_param = buf;
            wr_ipc_msg.req_len = len;
            ret = tuya_ipc_send_sync(&wr_ipc_msg);
            if(ret) {
                tkl_system_free(buf);
                buf = NULL;
                return ret;
            }
        }
            break;
        default:
            bk_printf("not support cmd: %d\r\n", cmd);
            break;
    }
#endif
    return ret;
}


