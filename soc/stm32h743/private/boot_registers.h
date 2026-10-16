/**
 * \file            boot_registers.h
 * \brief           Assembly-only H743 Run-star MMIO constants checked by C
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_BOOT_REGISTERS_H
#define NX_STM32H743_BOOT_REGISTERS_H
#define NX_H743_BOOT_PWR_CR3  0x5802480c
#define NX_H743_BOOT_PWR_CSR1 0x58024804
#define NX_H743_BOOT_LDO      2
#define NX_H743_BOOT_BYPASS   1
#define NX_H743_BOOT_READY    0x2000
#define NX_H743_BOOT_POLLS    1000000
#define NX_H743_BOOT_DBG_ID   0x5c001000
#define NX_H743_BOOT_AXI_FN   0x51008108
#define NX_H743_BOOT_AXI_READ 1
#define NX_H743_BOOT_REV_NEW  0x2000
#endif
