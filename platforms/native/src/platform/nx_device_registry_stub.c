/**
 * \file            nx_device_registry_stub.c
 * \brief           Device registry stub for MSVC builds
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Provides stub symbols for device registry on platforms
 *                  that don't support linker sections (e.g., MSVC).
 */

#include "hal/base/nx_device.h"

#if defined(_MSC_VER)

/*---------------------------------------------------------------------------*/
/* MSVC Device Registry Stub                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Empty device registry for MSVC
 * \note            MSVC doesn't support linker sections like GCC/Clang
 *                  Device registration on MSVC uses a different mechanism
 */
const nx_device_t __nx_device_start[1] = {{NULL, NULL, NULL, NULL, NULL}};
const nx_device_t __nx_device_end[1] = {{NULL, NULL, NULL, NULL, NULL}};

#endif /* _MSC_VER */
