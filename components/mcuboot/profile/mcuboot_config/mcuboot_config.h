/**
 * \file            mcuboot_config.h
 * \brief           Fixed ordinary F407 software reference profile.
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_REFERENCE_CONFIG_H
#define NEXUS_MCUBOOT_REFERENCE_CONFIG_H

#define MCUBOOT_SIGN_EC256
#define MCUBOOT_USE_TINYCRYPT
#define MCUBOOT_OVERWRITE_ONLY
#define MCUBOOT_VALIDATE_PRIMARY_SLOT
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS
#define MCUBOOT_MAX_IMG_SECTORS 1
#define MCUBOOT_IMAGE_NUMBER    1
#define MCUBOOT_BOOT_MAX_ALIGN  8
#define MCUBOOT_FIH_PROFILE_OFF
#define MCUBOOT_HAVE_ASSERT_H

void nx_mcuboot_assert_fail(void);
void nx_mcuboot_watchdog_progress(void);
#define MCUBOOT_WATCHDOG_FEED() nx_mcuboot_watchdog_progress()

#endif
