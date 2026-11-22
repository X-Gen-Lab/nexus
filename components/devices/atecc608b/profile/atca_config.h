/**
 * \file            atca_config.h
 * \brief           Private fixed ATECC608B command-builder and execution
 * profile \author          Nexus Team
 */
#ifndef ATCA_CONFIG_H
#define ATCA_CONFIG_H
#define ATCA_ATECC608_SUPPORT
#define ATCA_NO_HEAP
#define ATCA_STRICT_C99
#define ATCA_CHECK_PARAMS_EN      1
#define ATCA_PREPROCESSOR_WARNING 0
#define MAX_PACKET_SIZE           80U
#define CALIB_CHECKMAC_EN         0
#define CALIB_COUNTER_EN          0
#define CALIB_DELETE_EN           0
#define CALIB_DERIVEKEY_EN        0
#define CALIB_ECDH_EN             0
#define CALIB_GENDIG_EN           0
#define CALIB_HMAC_EN             0
#define CALIB_LOCK_EN             0
#define CALIB_MAC_EN              0
#define CALIB_PRIVWRITE_EN        0
#define CALIB_RANDOM_EN           1
#define CALIB_SECUREBOOT_EN       0
#define CALIB_SHA_EN              0
#define CALIB_UPDATEEXTRA_EN      0
#define CALIB_VERIFY_EN           0
#define CALIB_WRITE_EN            0
#define CALIB_AES_EN              0
#define CALIB_KDF_EN              0
#define CALIB_SELFTEST_EN         0
#define CALIB_GENKEY_EN           1
#define CALIB_NONCE_EN            1
#define CALIB_READ_EN             1
#define CALIB_SIGN_EN             1
#endif
