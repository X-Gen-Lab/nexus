/**
 * \file            mcuboot_logging.h
 * \brief           Explicit no-logging port for the fixed software profile.
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_REFERENCE_LOGGING_H
#define NEXUS_MCUBOOT_REFERENCE_LOGGING_H
#define MCUBOOT_LOG_MODULE_DECLARE(domain)
#define MCUBOOT_LOG_MODULE_REGISTER(domain)
#define MCUBOOT_LOG_ERR(...)
#define MCUBOOT_LOG_WRN(...)
#define MCUBOOT_LOG_INF(...)
#define MCUBOOT_LOG_DBG(...)
#define MCUBOOT_LOG_SIM(...)
#endif
