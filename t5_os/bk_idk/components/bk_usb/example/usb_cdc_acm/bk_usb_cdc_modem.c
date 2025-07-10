#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <driver/int.h>
#include <driver/pwr_clk.h>

#include <common/bk_err.h>
#include <common/bk_include.h>

#include "cli.h"
#include "mb_ipc_cmd.h"
#include "bk_usb_cdc_modem.h"
#include "tuya_iot_config.h"

#define TAG "cdc_modem"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

const char *cdc_evt2name[] = {
	"CDC_STATUS_OPEN",
	"CDC_STATUS_CLOSE",
	"CDC_STATUS_CONN",
	"CDC_STATUS_DISCON",
	"CDC_STATUS_BULKIN",
	"CDC_STATUS_BULKOUT",
	"CDC_STATUS_BULKIN_DONE",
	"CDC_STATUS_UPDATE_PARAM",
	"CDC_STATUS_OUT_DELAY",
	"CDC_STATUS_ABNORMAL",
	"CDC_STATUS_IDLE",
};

#if (CONFIG_BK_MODEM)
extern void bk_modem_usbh_conn_ind(uint32_t cnt);
extern void bk_modem_usbh_disconn_ind(void);
extern void bk_modem_usbh_close(void);
extern void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx);
extern void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx);
extern void bk_modem_usbh_poweron_ind(void);
extern uint8_t bk_modem_get_mode(void);
extern uint32_t bk_modem_get_usbdev_idx(void);

#else

void bk_modem_usbh_conn_ind(uint32_t cnt){ }
void bk_modem_usbh_disconn_ind(void){ }
void bk_modem_usbh_close(void){ }
void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx){ }
void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx){ }
void bk_modem_usbh_poweron_ind(void){ }
uint8_t bk_modem_get_mode(void){return 0;}
uint32_t bk_modem_get_usbdev_idx(void){return 0;}
#endif

IPC_CDC_DATA_T *g_cdc_ipc;
Multi_ACM_DEVICE_TOTAL_T *g_multi_acm_total = NULL;

static beken_queue_t cdc_msg_queue = NULL;
static beken_thread_t cdc_demo_task = NULL;
static beken_queue_t cdc_msg_rxqueue = NULL;
static beken_thread_t cdc_demo_rxtask = NULL;

static uint8_t g_cdc_close = 0;

static uint8_t g_temp_rx_buf[512] = {0};
static uint32_t g_temp_rx_len = 0;

#if CDC_CIRBUFFER_OUT
static uint32_t g_cdc_tx_block = 0;

static uint32_t _is_empty(uint8_t wd, uint8_t rd)
{
	return (wd == rd);
}
static uint32_t _is_full(uint8_t wd, uint8_t rd)
{
	return (((wd+1)&(CDC_TX_CIRBUFFER_NUM-1)) == rd);
}
#endif

static bk_err_t cdc_send_msg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	cdc_msg_t msg;

	if (cdc_msg_queue)
	{
		msg.type = type;
		msg.data = param;

		ret = rtos_push_to_queue(&cdc_msg_queue, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			LOGE("cdc_send_msg Fail, type:%s ret:%d\n", cdc_evt2name[type], ret);
			BK_ASSERT(0);// for debug
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}
 uint32_t queue_size_temp = 0;
static bk_err_t cdc_send_rxmsg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	cdc_msg_t msg;

	if (cdc_msg_rxqueue)
	{
             if (type == CDC_STATUS_BULKIN_DATA)
             {
                queue_size_temp++;
             }
                
		msg.type = type;
		msg.data = param;

		ret = rtos_push_to_queue(&cdc_msg_rxqueue, &msg, 100);//BEKEN_NO_WAIT);


		if(queue_size_temp >= 35)
		{
			//LOGE("cdc_send_rxmsg too more, queue_size_temp:%d\n", queue_size_temp);
		}
		if (kNoErr != ret)
		{
			bool val = rtos_is_queue_full(&cdc_msg_rxqueue);
			LOGE("cdc_send_rxmsg Fail, ret:%d, val:%d\n", ret, val);
		//	BK_ASSERT(0); // for debug
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

#if defined(ENABLE_BIG_CORE) && (ENABLE_BIG_CORE==1)
#else
extern void ipc_cdc_send_cmd(u8 cmd, u8 *cmd_buf, u16 cmd_len, u8 * rsp_buf, u16 rsp_buf_len);
#endif

static void bk_usb_cdc_send_ipc_cmd(IPC_CDC_SUBMSG_TYPE_T msg)
{
    g_cdc_ipc->msg = msg;

#if defined(ENABLE_BIG_CORE) && (ENABLE_BIG_CORE==1)
    extern void bk_usb_cdc_rcv_notify_cp1(IPC_CDC_DATA_T *p);
    bk_usb_cdc_rcv_notify_cp1(g_cdc_ipc);
#else
	ipc_cdc_send_cmd(IPC_USB_CDC_CP0_NOTIFY, (uint8_t *)g_cdc_ipc, g_cdc_ipc->cmd_len, NULL, 0);
#endif
}

void bk_cdc_acm_bulkin_cmd(void)
{
	cdc_send_rxmsg(CDC_STATUS_BULKIN_CMD, 0);
}
void bk_cdc_acm_bulkin_data(void)
{
	cdc_send_rxmsg(CDC_STATUS_BULKIN_DATA, 0);
}

void bk_cdc_acm_state_notify(CDC_STATUS_t * dev_state)
{
	uint32_t state = dev_state->status;
	switch(state)
	{
		case CDC_STATUS_CONN:
			LOGI("CDC_STATUS_CONN\n");
			cdc_send_msg(CDC_STATUS_CONN, dev_state->dev_cnt);
			break;
		case CDC_STATUS_DISCON:
			LOGI("CDC_STATUS_DISCON\n");
			cdc_send_msg(CDC_STATUS_DISCON, 0);
			break;
		default:
			break;
	}
}

void bk_usb_cdc_rcv_notify_cp0(IPC_CDC_DATA_T *p)
{
	IPC_CDC_SUBMSG_TYPE_T msg = p->msg;
	switch(msg)
	{
		case CPU1_UPDATE_USB_CDC_STATE:
			{
				Multi_ACM_DEVICE_TOTAL_T * p_dbg = (Multi_ACM_DEVICE_TOTAL_T *)(p->p_info);
				bk_cdc_acm_state_notify(p_dbg->p_status);
			}
			break;
		case CPU1_UPLOAD_USB_CDC_CMD:
			bk_cdc_acm_bulkin_cmd();
			break;
		case CPU1_UPLOAD_USB_CDC_DATA:
			bk_cdc_acm_bulkin_data();
			break;
			break;
		default:
			break;
	}
}

/*
void bk_usb_cdc_open(void)
{
	cdc_send_msg(CDC_STATUS_OPEN, 0);
}

void bk_usb_cdc_close(void)
{
	cdc_send_msg(CDC_STATUS_CLOSE, 0);
}
*/
int32_t bk_cdc_acm_modem_write(char *p_tx, uint32_t l_tx)
{
	bk_err_t __maybe_unused ret = BK_FAIL;
	if (l_tx > CDC_EXTX_MAX_SIZE) {
		LOGE("[+]%s, Transbuf overflow!\r\n", __func__);
	}

	g_multi_acm_total->idx = bk_modem_get_usbdev_idx();
	g_multi_acm_total->mode = bk_modem_get_mode();

	if (g_multi_acm_total->mode == 1) ///AT mode
	{
		g_multi_acm_total->p_cmd->p_cdc_cmd_tx->l_tx = l_tx;
		os_memcpy(g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf, p_tx, l_tx);
		cdc_send_msg(CDC_STATUS_BULKOUT_CMD, 0);
	} else if (g_multi_acm_total->mode == 2) /// Data mode
	{
		uint8_t * p_buf = NULL;
		uint8_t rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
		uint8_t wd = g_multi_acm_total->p_data->p_cdc_data_tx->wd;
        	while (1)
        	{
        		uint8_t t_rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
        		if (rd == t_rd)
        			break;
        		else
        			rd = t_rd;
        	}        
		while (1)
		{
			if (!_is_full(wd, rd))
			{
				g_cdc_tx_block = 0;
				break;
			} else {
				g_cdc_tx_block++;
				if (g_cdc_tx_block > (2*CDC_TX_CIRBUFFER_NUM))
				{
					LOGE("g_cdc_tx_block:%d, w:%d, r:%d\n", g_cdc_tx_block, wd, rd);
				}
				rtos_delay_milliseconds(2);
			}
			rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
			//wd = g_multi_acm_total->p_data->p_cdc_data_tx->wd;
		}

		wd = (wd+1)&(CDC_TX_CIRBUFFER_NUM-1);

		p_buf = g_multi_acm_total->p_data->p_cdc_data_tx->data[wd]->data;
		g_multi_acm_total->p_data->p_cdc_data_tx->data[wd]->len = l_tx;
		os_memcpy(p_buf, p_tx, l_tx);

	//	if (ret == kNoErr)
		{
			//rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
			g_multi_acm_total->p_data->p_cdc_data_tx->wd = wd;
		}
	//	if (!_is_empty(wd, rd))
		{
			cdc_send_msg(CDC_STATUS_BULKOUT_DATA, 0);
		}
	}
	return 0;
}

static bk_err_t bk_cdc_acm_init_malloc(void)
{
	uint32_t i = 0;
	g_cdc_ipc = (IPC_CDC_DATA_T *)psram_malloc(sizeof(IPC_CDC_DATA_T));
	if (g_cdc_ipc == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	os_memset(g_cdc_ipc, 0x00, sizeof(IPC_CDC_DATA_T));

	g_multi_acm_total = (Multi_ACM_DEVICE_TOTAL_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_TOTAL_T));
	if (g_multi_acm_total == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	os_memset(g_multi_acm_total, 0x00, sizeof(Multi_ACM_DEVICE_TOTAL_T));

	/* p_status */
	g_multi_acm_total->p_status = (CDC_STATUS_t *)psram_malloc(sizeof(CDC_STATUS_t));
	if (g_multi_acm_total->p_status == NULL)
	{
		LOGE("psram malloc error!\r\n");
	}
	g_multi_acm_total->p_status->dev_cnt = 0;
	g_multi_acm_total->p_status->status = 0;

	/* p_cmd */
	g_multi_acm_total->p_cmd = (Multi_ACM_DEVICE_CMD_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_CMD_T));
	if (g_multi_acm_total->p_cmd == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_tx = (CDC_CIRBUF_CMD_TX_T *)psram_malloc(sizeof(CDC_CIRBUF_CMD_TX_T));
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf = (uint8_t *)psram_malloc(CDC_EXTX_MAX_SIZE);
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_rx = (CDC_CIRBUF_CMD_RX_T *)psram_malloc(sizeof(CDC_CIRBUF_CMD_RX_T));
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf = (uint8_t *)psram_malloc(CDC_RX_MAX_SIZE);
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}

	/* p_data */
	g_multi_acm_total->p_data = (Multi_ACM_DEVICE_DATA_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_DATA_T));
	if (g_multi_acm_total->p_data == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
//////
	g_multi_acm_total->p_data->p_cdc_data_tx = (CDC_CIRBUF_DATA_TX_T *)psram_malloc(sizeof(CDC_CIRBUF_DATA_TX_T));
	if (g_multi_acm_total->p_data->p_cdc_data_tx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}

	for (i = 0; i < CDC_TX_CIRBUFFER_NUM; i++)
	{
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i] = (CDC_CIRBUF_TX_ELE_T *)psram_malloc(sizeof(CDC_CIRBUF_TX_ELE_T));
		if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data = (uint8_t *)psram_malloc(CDC_EXTX_MAX_SIZE * sizeof(uint8_t));
		if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->len = 0;
	}
//////
	g_multi_acm_total->p_data->p_cdc_data_rx = (CDC_CIRBUF_DATA_RX_T *)psram_malloc(sizeof(CDC_CIRBUF_DATA_RX_T));
	if (g_multi_acm_total->p_data->p_cdc_data_rx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	for (i = 0; i < CDC_RX_CIRBUFFER_NUM; i++)
	{
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i] = (CDC_CIRBUF_RX_ELE_T *)psram_malloc(sizeof(CDC_CIRBUF_RX_ELE_T));
		if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data = (uint8_t *)psram_malloc(CDC_RX_MAX_SIZE * sizeof(uint8_t));
		if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->len = 0;
	}
	return BK_OK;
}

static void bk_cdc_acm_init_free(void)
{
	uint32_t i = 0;
	if (g_cdc_ipc) {
		psram_free(g_cdc_ipc);
		g_cdc_ipc = NULL;
	}

	if (g_multi_acm_total) {

		for (i = 0; i < CDC_TX_CIRBUFFER_NUM; i++)
		{
			if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data);
				g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data = NULL;
			}
			if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_tx->data[i]);
				g_multi_acm_total->p_data->p_cdc_data_tx->data[i] = NULL;
			}
		}

		if (g_multi_acm_total->p_data->p_cdc_data_tx) {
			psram_free(g_multi_acm_total->p_data->p_cdc_data_tx);
			g_multi_acm_total->p_data->p_cdc_data_tx = NULL;
		}

		for (i = 0; i < CDC_RX_CIRBUFFER_NUM; i++)
		{
			if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data);
				g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data = NULL;
			}
			if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_rx->data[i]);
				g_multi_acm_total->p_data->p_cdc_data_rx->data[i] = NULL;
			}
		}

		if (g_multi_acm_total->p_data->p_cdc_data_rx) {
			psram_free(g_multi_acm_total->p_data->p_cdc_data_rx);
			g_multi_acm_total->p_data->p_cdc_data_rx = NULL;
		}

		if (g_multi_acm_total->p_data) {
			psram_free(g_multi_acm_total->p_data);
			g_multi_acm_total->p_data = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf);
			g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_tx);
			g_multi_acm_total->p_cmd->p_cdc_cmd_tx = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf);
			g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_rx);
			g_multi_acm_total->p_cmd->p_cdc_cmd_rx = NULL;
		}

		if (g_multi_acm_total->p_cmd) {
			psram_free(g_multi_acm_total->p_cmd);
			g_multi_acm_total->p_cmd = NULL;
		}

		if (g_multi_acm_total->p_status) {
			psram_free(g_multi_acm_total->p_status);
			g_multi_acm_total->p_status == NULL;
		}

		if (g_multi_acm_total) {
			psram_free(g_multi_acm_total);
			g_multi_acm_total = NULL;
		}
	}
}

void bk_cdc_acm_deinit(void)
{
	g_multi_acm_total->p_data->p_cdc_data_tx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_tx->wd = 0;

	g_multi_acm_total->p_data->p_cdc_data_rx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_rx->wd = 0;

	g_multi_acm_total->idx = 0;
	g_multi_acm_total->mode = 0;
}

void bk_cdc_acm_init(void)
{
	int ret = BK_FAIL;
	ret = bk_cdc_acm_init_malloc();
	BK_ASSERT(ret == BK_OK);

	g_multi_acm_total->p_data->p_cdc_data_tx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_tx->wd = 0;

	g_multi_acm_total->p_data->p_cdc_data_rx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_rx->wd = 0;

	g_multi_acm_total->idx = 0;
	g_multi_acm_total->mode = 0;

	g_cdc_ipc->p_info = (uint32_t)g_multi_acm_total;
	g_cdc_ipc->cmd_len = sizeof(IPC_CDC_DATA_T);

	cdc_send_msg(CDC_STATUS_INIT_PARAM, 0);
}

static void bk_cdc_demo_task(beken_thread_arg_t arg)
{
	int ret = BK_OK;
	cdc_msg_t msg;
	while (1)
	{
		ret = rtos_pop_from_queue(&cdc_msg_queue, &msg, BEKEN_WAIT_FOREVER);
		LOGD("[+]%s, type %s\n", __func__, cdc_evt2name[msg.type]);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case CDC_STATUS_OPEN:
					bk_usb_cdc_send_ipc_cmd(CPU0_OPEN_USB_CDC);
					break;
				case CDC_STATUS_CLOSE:
					g_cdc_close = 1;
					bk_usb_cdc_send_ipc_cmd(CPU0_CLOSE_USB_CDC);
					break;
				case CDC_STATUS_CONN:
					{
						uint32_t cnt = (uint32_t)msg.data;
						bk_modem_usbh_conn_ind(cnt);
					}
					break;
				case CDC_STATUS_DISCON:
					{
						bk_cdc_acm_deinit();
						bk_modem_usbh_disconn_ind();
						if (g_cdc_close == 1) {
							bk_cdc_acm_init_free();
							g_cdc_close = 0;
						}
					//	goto exit;
					}
					break;
				case CDC_STATUS_INIT_PARAM:
					bk_usb_cdc_send_ipc_cmd(CPU0_INIT_USB_CDC_PARAM);
					break;
				case CDC_STATUS_BULKOUT_CMD:
					bk_usb_cdc_send_ipc_cmd(CPU0_BULKOUT_USB_CDC_CMD);
					break;
				case CDC_STATUS_BULKOUT_DATA:
					bk_usb_cdc_send_ipc_cmd(CPU0_BULKOUT_USB_CDC_DATA);
					break;
				default:
					break;
			}
		}
	}
#if 0
exit:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		cdc_demo_task = NULL;
		rtos_delete_thread(NULL);
	}
#endif
}

static void bk_cdc_usbh_upload_ind(void)
{
	uint8_t *p_buf = NULL;
	uint32_t len = 0;
	uint8_t rd = g_multi_acm_total->p_data->p_cdc_data_rx->rd;
	uint8_t wd = g_multi_acm_total->p_data->p_cdc_data_rx->wd;

	while (1)
	{
		uint8_t t_wd = g_multi_acm_total->p_data->p_cdc_data_rx->wd;
		if (wd == t_wd)
			break;
		else
			wd = t_wd;
	}
	
	LOGD("[++]%s, rd:%d, wd:%d\r\n", __func__, rd, wd);
	while (!_is_empty(wd, rd))
	{
		rd = (rd+1)&(CDC_RX_CIRBUFFER_NUM-1);
		p_buf = g_multi_acm_total->p_data->p_cdc_data_rx->data[rd]->data;
		len = g_multi_acm_total->p_data->p_cdc_data_rx->data[rd]->len;
		if (len == 0 || len > 512) {
			LOGE("Error inLen!!!, rd:%d, len:%d\r\n", rd, len);
		}
		g_temp_rx_len = len;
		os_memcpy(&g_temp_rx_buf[0], p_buf, g_temp_rx_len);
		//g_multi_acm_total->p_data->p_cdc_data_rx->rd = rd;
		//if (g_multi_acm_total->p_data->p_cdc_data_rx->rd >= CDC_RX_CIRBUFFER_NUM)
		//{
			//g_multi_acm_total->p_data->p_cdc_data_rx->rd = 0;
		//}
		bk_modem_usbh_bulkin_ind(&g_temp_rx_buf[0], g_temp_rx_len);
		//rd = g_multi_acm_total->p_data->p_cdc_data_rx->rd;
	}
	g_multi_acm_total->p_data->p_cdc_data_rx->rd = rd;
}

static void bk_cdc_demo_rxtask(beken_thread_arg_t arg)
{
	int ret = BK_OK;
	cdc_msg_t msg;
	while (1)
	{
		ret = rtos_pop_from_queue(&cdc_msg_rxqueue, &msg, BEKEN_WAIT_FOREVER);
		LOGD("[+]%s, type %d\n", __func__, msg.type);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case CDC_STATUS_BULKIN_CMD:
					bk_modem_usbh_bulkin_ind(g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf, g_multi_acm_total->p_cmd->p_cdc_cmd_rx->l_rx);
					g_multi_acm_total->p_cmd->p_cdc_cmd_rx->l_rx = 0;
					break;
				case CDC_STATUS_BULKIN_DATA:
					//extern uin32_t queue_size_temp;
					queue_size_temp--;
					bk_cdc_usbh_upload_ind();
					break;
				default:
					break;
			}
		}
	}
#if 0
exit:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		cdc_demo_task = NULL;
		rtos_delete_thread(NULL);
	}
#endif
}

void bk_usb_cdc_modem(void)
{
	int ret = kNoErr;

//#if USB_CDC_CP0_IPC
//	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);
//#endif

	if (cdc_msg_queue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_queue, "cdc_msg_queue", sizeof(cdc_msg_t), 64);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}
	if (cdc_demo_task == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_task,
							4,
							"cdc_modem_task",
							(beken_thread_function_t)bk_cdc_demo_task,
							4*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}
	if (cdc_msg_rxqueue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_rxqueue, "cdc_msg_rxqueue", sizeof(cdc_msg_t), 64);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}
	if (cdc_demo_rxtask == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_rxtask,
							4,
							"cdc_modem_rxtask",
							(beken_thread_function_t)bk_cdc_demo_rxtask,
							4*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}
	return;
error:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		cdc_demo_task = NULL;
		rtos_delete_thread(NULL);
	}
	if (cdc_msg_rxqueue)
	{
		rtos_deinit_queue(&cdc_msg_rxqueue);
		cdc_msg_rxqueue = NULL;
	}
	if (cdc_demo_rxtask)
	{
		cdc_demo_rxtask = NULL;
		rtos_delete_thread(NULL);
	}
}

