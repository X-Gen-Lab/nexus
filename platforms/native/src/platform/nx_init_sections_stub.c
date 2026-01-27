/**
 * \file            nx_init_sections_stub.c
 * \brief           Init function section symbols stub for Windows MinGW/GCC
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-27
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file provides stub symbols for init function linker
 *                  sections on Windows MinGW/GCC where linker scripts are
 *                  not used. The symbols contain only NULL entries which
 *                  are treated as boundary markers and skipped by the init
 *                  framework.
 */

#include "framework/init/include/nx_init.h"

/*---------------------------------------------------------------------------*/
/* Init Function Section Symbols                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start of init function section
 * \note            Contains only NULL (boundary marker) - will be skipped
 */
const nx_init_fn_t __nx_init_fn_start[1] = {NULL};

/**
 * \brief           End of init function section
 * \note            Contains only NULL (boundary marker) - will be skipped
 */
const nx_init_fn_t __nx_init_fn_end[1] = {NULL};
