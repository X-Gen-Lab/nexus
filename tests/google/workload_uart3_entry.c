/**
 * \file            workload_uart3_entry.c
 * \brief           Compile the real UART workload with a returning host owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
int nx_workload_uart3_main(void);
#define main                 nx_workload_uart3_main
#define NEXUS_BACKEND_NATIVE 1
#define NEXUS_WORKLOAD_uart  1
#include "../../tools/measurement/workload.c"
