/**
 * \file            lwipopts.h
 * \brief           Explicit finite single-owner IPv4 lwIP reference profile
 * \author          Nexus Team
 */
#ifndef NEXUS_LWIP_OPTIONS_H
#define NEXUS_LWIP_OPTIONS_H

#include <stdint.h>

#define NO_SYS               1
#define SYS_LIGHTWEIGHT_PROT 0
#define LWIP_SOCKET          0
#define LWIP_NETCONN         0
#define LWIP_IPV4            1
#define LWIP_IPV6            0
#define LWIP_ARP             1
#define LWIP_ETHERNET        1
#define LWIP_ICMP            1
#define LWIP_RAW             1
#define LWIP_UDP             1
#define LWIP_TCP             1
#define LWIP_DHCP            0
#define LWIP_DNS             0
#define LWIP_AUTOIP          0
#define LWIP_IGMP            0
#define LWIP_ALTCP           0
#define LWIP_NETIF_HOSTNAME  0
#define LWIP_SINGLE_NETIF    1
#define LWIP_NETIF_LOOPBACK  0
#define LWIP_STATS           0
#define LWIP_DEBUG           0
#if UINTPTR_MAX > UINT32_MAX
#define MEM_ALIGNMENT 8
#else
#define MEM_ALIGNMENT 4
#endif
#define MEM_LIBC_MALLOC         0
#define MEMP_MEM_MALLOC         0
#define MEM_SIZE                16384
#define PBUF_POOL_SIZE          24
#define PBUF_POOL_BUFSIZE       512
#define MEMP_NUM_PBUF           16
#define MEMP_NUM_RAW_PCB        2
#define MEMP_NUM_UDP_PCB        4
#define MEMP_NUM_TCP_PCB        4
#define MEMP_NUM_TCP_PCB_LISTEN 2
#define MEMP_NUM_TCP_SEG        32
#define MEMP_NUM_SYS_TIMEOUT    8
#define MEMP_NUM_ARP_QUEUE      4
#define ARP_TABLE_SIZE          8
#define ARP_QUEUEING            1
#define IP_REASSEMBLY           0
#define IP_FRAG                 0
#define TCP_MSS                 1460
#define TCP_SND_BUF             (4 * TCP_MSS)
#define TCP_WND                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN        16
#define ETH_PAD_SIZE            0

#endif
