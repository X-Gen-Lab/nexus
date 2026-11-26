/**
 * \file            FreeRTOSConfig.h
 * \brief           Pinned Secure context ABI for host mechanism tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_SECURE_HOST_CONFIG_H
#define NEXUS_SECURE_HOST_CONFIG_H
#ifdef NEXUS_SECURE_TEST_FP_BANK
#define configENABLE_FPU NEXUS_SECURE_TEST_FP_BANK
#else
#define configENABLE_FPU 0
#endif
#define configENABLE_MVE 0
#define configENABLE_MPU 0
#endif /* NEXUS_SECURE_HOST_CONFIG_H */
