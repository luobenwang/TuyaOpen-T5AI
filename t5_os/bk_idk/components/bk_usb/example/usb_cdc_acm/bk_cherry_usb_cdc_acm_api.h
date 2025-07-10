// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <components/log.h>
#include <components/usb.h>
#include <components/usb_types.h>
#include "bk_usb_cdc_modem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_CDC_TAG "USB_CDC"
#define USB_CDC_LOGI(...) BK_LOGI(USB_CDC_TAG, ##__VA_ARGS__)
#define USB_CDC_LOGW(...) BK_LOGW(USB_CDC_TAG, ##__VA_ARGS__)
#define USB_CDC_LOGE(...) BK_LOGE(USB_CDC_TAG, ##__VA_ARGS__)
#define USB_CDC_LOGD(...) BK_LOGD(USB_CDC_TAG, ##__VA_ARGS__)

typedef struct {
	uint8_t  type;
	uint32_t data;
}acm_msg_t;

typedef enum {
	ACM_OPEN_IND = 0,
	ACM_INIT_IND,
	ACM_BULKOUT_IND,
	ACM_EXIT_IND,
	ACM_CLOSE_IND,
	ACM_CONNECT_IND,
	ACM_DISCONNECT_IND,

	ACM_BULKIN_IND,  // 7
	ACM_UPLOAD_IND,   //8
	ACM_UPLOAD_TIMER_IND,   //9
	ACM_UNKNOW,
}acm_msg_type_t;

void bk_usb_update_cdc_interface(void *hport, uint8_t bInterfaceNumber, uint8_t interface_sub_class);
void bk_usb_cdc_free_enumerate_resources(void);
void bk_usb_cdc_exit(void);

void bk_usb_cdc_open(void);
void bk_usb_cdc_close(void);
void bk_cdc_acm_bulkout(void);

void bk_usb_cdc_param_init(IPC_CDC_DATA_T *p_cdc_data);


#ifdef __cplusplus
}
#endif
