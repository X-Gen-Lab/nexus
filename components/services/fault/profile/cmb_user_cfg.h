/**
 * \file            cmb_user_cfg.h
 * \brief           Kernel-free M4F CmBacktrace scanner profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CMB_USER_CFG_H
#define NEXUS_CMB_USER_CFG_H
#define CMB_USING_BARE_METAL_PLATFORM
#define CMB_CPU_PLATFORM_TYPE    2
#define CMB_CSTACK_BLOCK_START   __nx_msp_start
#define CMB_CSTACK_BLOCK_END     __nx_msp_end
#define CMB_CODE_SECTION_START   __nx_flash_live_start
#define CMB_CODE_SECTION_END     __nx_flash_live_end
#define CMB_CALL_STACK_MAX_DEPTH 32
#define cmb_println(...)         ((void)0)
#endif /* NEXUS_CMB_USER_CFG_H */
