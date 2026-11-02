/**
 * \file            cc.h
 * \brief           C11 pinned lwIP compiler and explicit product hook boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_LWIP_ARCH_CC_H
#define NEXUS_LWIP_ARCH_CC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_lwip_port_now_ms(void);
uint32_t nx_lwip_port_random_u32(void);
void nx_lwip_port_assert(const char* message);
#ifdef __cplusplus
}
#endif

#define LWIP_RAND()                   nx_lwip_port_random_u32()
#define LWIP_PLATFORM_ASSERT(text)    nx_lwip_port_assert(text)
#define LWIP_PLATFORM_DIAG(arguments) ((void)0)
#define sys_now                       nx_lwip_port_now_ms

#endif
