/**
 * \file            ds18b20.h
 * \brief           External-VDD DS18B20 identity and cooperative conversion
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_DS18B20_H
#define NEXUS_COMPONENTS_DS18B20_H
#include "nexus/components/onewire.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable selected sensor identity, not a bus or physical fitting.
 * \note Known ROM requires family28h and CRC8. Skip ROM additionally requires
 *       explicit single_device=true, reads/retains its CRC-valid ROM and checks
 *       that identity again for subsequent commands. external_vdd must be true
 *       and Read Power Supply must confirm it; parasite mode is unsupported.
 *       Sensor power and identity remain stable through each conversion.
 */
typedef struct {
    nx_onewire_t* bus;
    uint8_t rom[8];
    bool single_device;
    bool skip_rom;
    bool external_vdd;
} nx_ds18b20_config_t;
/** \brief Caller-owned serialized device state; zero before first init.
 * \note Configuration, bus and clock contexts remain borrowed while open.
 *       Conversion is autonomous; software stop/timeout never claims physical
 *       cancellation. No task, heap, hidden queue, calibration or retained CPU
 *       payload. After software closure and joins, no device reference remains.
 */
typedef struct {
    const nx_ds18b20_config_t* config;
    nx_time_us_t ready_at;
    nx_time_us_t deadline;
    uint8_t rom[8];
    uint8_t resolution_bits;
    bool initialized;
    bool converting;
    bool in_call;
} nx_ds18b20_t;
/** \brief Validate identity, actual supply mode and scratchpad configuration.
 * \param[in,out] device: Zero/stopped disjoint stable state.
 * \param[in] config: Immutable exact selection, with initialized wire owner.
 * \param[in] deadline: Finite absolute communication budget.
 * \return SUCCESS or INVALID/STATE/BUSY/TIMEOUT/UNSUPPORTED/IO.
 * \note Initial scratchpad temperature, including85C, is never a fresh result.
 */
nx_result_t nx_ds18b20_init(nx_ds18b20_t* device,
                            const nx_ds18b20_config_t* config,
                            nx_time_us_t deadline);
/** \brief Read actual resolution then start one bounded autonomous conversion.
 * \param[in,out] device: Initialized serialized owner; no current conversion.
 * \param[in] deadline: Finite overall communication/conversion/read budget.
 * \return SUCCESS when issued; otherwise bounded validation/bus/clock error.
 * \note Max waits are93750/187500/375000/750000us for9/10/11/12 bits.
 *       No blocking conversion delay. Command execution may have effects even
 *       when a later clock/budget observation fails; this is not cancellation.
 */
nx_result_t nx_ds18b20_start(nx_ds18b20_t* device, nx_time_us_t deadline);
/** \brief After the real resolution maximum, read/CRC-validate a fresh result.
 * \param[in,out] device: Same serialized state and live bus/clock epoch.
 * \param[out] raw_sixteenths: Separate signed1/16C output, unchanged on error.
 * \return BUSY before ready without wire activity, SUCCESS once, or a bounded
 *         timeout/clock/identity/CRC/configuration error closing admission.
 * \note Reads use a fresh reset/select, not a stale conversion-poll context.
 *       Undefined low bits are cleared; valid85C is not rejected by value.
 *       Temperature range is -55..125C. No UTC or product calibration policy.
 */
nx_result_t nx_ds18b20_service(nx_ds18b20_t* device, int16_t* raw_sixteenths);
/** \brief Close only software admission; never reset or release the shared bus.
 * \param[in,out] device: Stable same-executor state, no in-progress call.
 * \return SUCCESS idempotently, INVALID or BUSY for an overlapping call.
 * \note Physical conversion may continue; no physical cancel/off claim.
 */
nx_result_t nx_ds18b20_stop(nx_ds18b20_t* device);
#ifdef __cplusplus
}
#endif
#endif
