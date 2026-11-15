/**
 * \file            usbh_conf.h
 * \brief           Explicit polled, allocation-free official Host core tuple
 * \author          Nexus Team
 */
#ifndef NEXUS_ST_USB_HOST_CONF_H
#define NEXUS_ST_USB_HOST_CONF_H
#include <stdint.h>
#include <string.h>
#ifndef __IO
#define __IO volatile
#endif
#define USBH_MAX_NUM_ENDPOINTS       4U
#define USBH_MAX_NUM_INTERFACES      4U
#define USBH_MAX_NUM_CONFIGURATION   1U
#define USBH_KEEP_CFG_DESCRIPTOR     1U
#define USBH_MAX_NUM_SUPPORTED_CLASS 1U
#define USBH_MAX_SIZE_CONFIGURATION  512U
#define USBH_MAX_DATA_BUFFER         512U
#define USBH_DEBUG_LEVEL             0U
#define USBH_USE_OS                  0U
#define USBH_IN_NAK_PROCESS          0U
/* The pinned parser uses these USB wire values from its platform header. */
#define EP_TYPE_MSK      3U
#define EP_TYPE_CTRL     0U
#define EP_TYPE_ISOC     1U
#define EP_TYPE_BULK     2U
#define EP_TYPE_INTR     3U
#define USBH_memset      memset
#define USBH_memcpy      memcpy
#define USBH_UsrLog(...) ((void)0)
#define USBH_ErrLog(...) ((void)0)
#define USBH_DbgLog(...) ((void)0)
#define __ALIGN_BEGIN
#define __ALIGN_END
#endif
