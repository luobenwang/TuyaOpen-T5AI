#include "tuya_cloud_types.h"
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
#include "task.h"

typedef OPERATE_RET (*wired_status_cb)(uint8_t event_id);
OPERATE_RET wired_status_changed_cb(uint8_t event_id)
{
    struct ipc_msg_s wr_ipc_msg_cb = {0};
    memset(&wr_ipc_msg_cb, 0, sizeof(struct ipc_msg_s));

    wr_ipc_msg_cb.type = TKL_IPC_TYPE_WIRED;
    wr_ipc_msg_cb.subtype = TKL_IPC_TYPE_WIRED_STATUS_CHANGED;
    struct ipc_msg_param_s param = {0};
    param.p1 = &event_id;
    wr_ipc_msg_cb.req_param = &param;
    wr_ipc_msg_cb.req_len = sizeof(struct ipc_msg_param_s);;
    OPERATE_RET ret = tuya_ipc_send_sync(&wr_ipc_msg_cb);

    if(ret)
        return ret;
    return wr_ipc_msg_cb.ret_value;
}

// default
static WIRED_IOCTL_GPIO_T wired_cfg = {
    .dev_type = TKL_WIRED_RMII,
    .rmii = {
        .int_gpio  = TUYA_GPIO_NUM_46,
        .mdc_gpio  = TUYA_GPIO_NUM_29,
        .mdio_gpio = TUYA_GPIO_NUM_48,
        .rxd0_gpio = TUYA_GPIO_NUM_33,
        .rxd1_gpio = TUYA_GPIO_NUM_50,
        .rxdv_gpio = TUYA_GPIO_NUM_35,
        .txd0_gpio = TUYA_GPIO_NUM_52,
        .txd1_gpio = TUYA_GPIO_NUM_37,
        .txen_gpio = TUYA_GPIO_NUM_54,
        .ref_clk_gpio = TUYA_GPIO_NUM_55,
    }
};

WIRED_IOCTL_GPIO_T *tkl_wired_get_config(void)
{
    return &wired_cfg;
}

extern void ethernet_link_changed_register_cb(wired_status_cb cb);
extern void ethernet_link_thread(void *argument);
TaskHandle_t *__thread_handle = NULL;
struct netif *eth_netif = NULL;
void tkl_wired_ipc_func(struct ipc_msg_s *msg)
{
    switch(msg->subtype) {
        case TKL_IPC_TYPE_WIRED_INIT:
        {
            eth_netif = (struct netif *)msg->req_param;
            ethernet_link_changed_register_cb(wired_status_changed_cb);
            if (rtos_create_thread(NULL, 7, "eth_link",
                ethernet_link_thread, 0x1000, eth_netif))
                bk_printf("Create eth link thread failed\n");
            tuya_ipc_send_no_sync(msg);
        }
            break;

        case TKL_IPC_TYPE_WIRED_GET_MAC:
        {
            NW_MAC_S *mac = (NW_MAC_S *)msg->res_param;
            tkl_wired_get_mac(mac);
            tuya_ipc_send_no_sync(msg);
        }
            break;

        case TKL_IPC_TYPE_WIRED_SET_GPIO:
        {
            memcpy(&wired_cfg, msg->req_param, sizeof(WIRED_IOCTL_GPIO_T));
            if (eth_netif == NULL) {
                bk_printf("wired not init\r\n");
            } else {
                ethernetif_init(eth_netif);
            }
            tuya_ipc_send_no_sync(msg);
        }
            break;

        default:
            break;
    }

    return;
}

OPERATE_RET tkl_wired_get_mac(NW_MAC_S *mac)
{
    bk_get_mac((uint8_t *)mac, MAC_TYPE_ETH);
    return OPRT_OK;
}
