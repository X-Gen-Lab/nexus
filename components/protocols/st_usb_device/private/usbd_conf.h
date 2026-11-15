/**
 * \file            usbd_conf.h
 * \brief           Selected official Device Core, no allocator or OS hooks
 * \author          Nexus Team
 */
#ifndef NEXUS_ST_USB_DEVICE_CONF_H
#define NEXUS_ST_USB_DEVICE_CONF_H
#include <stdint.h>
#include <string.h>
#define USBD_MAX_NUM_INTERFACES       4U
#define USBD_MAX_NUM_CONFIGURATION    1U
#define USBD_MAX_SUPPORTED_CLASS      1U
#define USBD_LPM_ENABLED              0U
#define USBD_SELF_POWERED             0U
#define USBD_DEBUG_LEVEL              0U
#define USBD_SUPPORT_USER_STRING_DESC 0U
#define USBD_CLASS_USER_STRING_DESC   0U
#define USBD_CLASS_BOS_ENABLED        0U
#define USBD_USER_REGISTER_CALLBACK   0U
#define USBD_memset                   memset
#define USBD_memcpy                   memcpy
#define UNUSED(value)                 ((void)(value))
#define __IO                          volatile
#define __STATIC_INLINE               static inline
#define __PACKED                      __attribute__((packed))
#define __ALIGN_BEGIN
#define __ALIGN_END
#endif
