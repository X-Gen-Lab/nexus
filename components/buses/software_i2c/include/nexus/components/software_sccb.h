/**
 * \file            software_sccb.h
 * \brief           Explicit two-wire SCCB register protocol, separate from I2C
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SOFTWARE_SCCB_H
#define NEXUS_COMPONENTS_SOFTWARE_SCCB_H
#include "nexus/components/software_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Immutable reviewed SCCB child, not an I2C address face
 * \note            write_id is an even 8-bit device ID; OV2640 uses 60h, with
 *                  61h for reading. No identity discovery or address fallback.
 */
typedef struct {
    nx_software_i2c_t* owner;
    uint8_t write_id;
} nx_software_sccb_device_t;

/**
 * \brief           Select the separate SCCB two-wire personality
 * \param[in,out]   owner: Fresh zero/stopped physical wire storage
 * \param[in]       config: Same stable open-drain/pull-up wire contract
 * \param[in]       deadline: Finite absolute initialization budget
 * \return          Same binding/idle errors as strict I2C initialization
 * \note            Two-wire, one target, register8/data8 only. No SCCB_E,
 *                  push-pull conflict mode or multi-target discovery. The ninth
 *                  master-byte bit is released and ignored as Don't-Care.
 *                  A completed waveform does not prove target ACK or presence.
 */
nx_result_t nx_software_sccb_init(nx_software_i2c_t* owner,
                                  const nx_software_i2c_config_t* config,
                                  nx_time_us_t deadline);

/**
 * \brief           Emit one three-phase SCCB register write
 * \param[in]       device: Immutable child of an initialized SCCB owner
 * \param[in]       reg: Exact eight-bit sub-address
 * \param[in]       value: Exact eight-bit register value
 * \param[in]       deadline: One finite budget through final STOP
 * \return          SUCCESS for settled waveform, or exact wire/clock failure
 * \note            Device meaning, identity/readback and side effects are
 *                  external. SCCB Don't-Care never weakens a strict I2C face.
 */
nx_result_t
nx_software_sccb_write_register(const nx_software_sccb_device_t* device,
                                uint8_t reg, uint8_t value,
                                nx_time_us_t deadline);

/**
 * \brief           Pointer write then STOP-separated two-phase SCCB read
 * \param[in]       device: Immutable exact child on the same live wire owner
 * \param[in]       reg: Eight-bit sub-address
 * \param[out]      value: Separate byte, unchanged on every error
 * \param[in]       deadline: One finite budget for both cycles and final STOP
 * \return          SUCCESS only after a complete read and STOP; exact error
 * \note            Master emits logical-one NA after the read byte. There is
 *                  no repeated-START substitution or ignored-I2C-NACK fallback.
 *                  Recover/stop use the shared physical-owner budgeted APIs.
 */
nx_result_t
nx_software_sccb_read_register(const nx_software_sccb_device_t* device,
                               uint8_t reg, uint8_t* value,
                               nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_SOFTWARE_SCCB_H */
