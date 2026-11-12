/**
 * \file            dac.h
 * \brief           Raw DAC output, explicit triggers and digital observation
 * \author          Nexus Team
 * \note            One serialized task owner controls a port. Factory lookup
 *                  has no hardware effect. Digital codes do not measure
 *                  voltage, settling, precision or the external load.
 */
#ifndef NEXUS_IO_DAC_H
#define NEXUS_IO_DAC_H
#include "nexus/core/time.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NX_DAC_CHANNEL_1 1U
#define NX_DAC_CHANNEL_2 2U
/** \brief Input storage encoding, distinct from the 12-bit digital output. */
typedef enum {
    NX_DAC_FORMAT_U8,
    NX_DAC_FORMAT_U12_RIGHT,
    NX_DAC_FORMAT_U12_LEFT
} nx_dac_format_t;
/** \brief Optional deterministic hardware waveform, never an entropy source. */
typedef enum {
    NX_DAC_WAVE_NONE,
    NX_DAC_WAVE_NOISE,
    NX_DAC_WAVE_TRIANGLE
} nx_dac_wave_t;
/** \brief Immutable selected facts; no physical qualification is inferred. */
typedef struct {
    nx_dac_format_t format;
    nx_dac_wave_t wave;
    uint16_t maximum_code;
    uint16_t amplitude;
    uint8_t channel_mask;
    bool software_trigger;
    bool timer_trigger;
    bool output_buffer;
    bool external_trigger; /**< Physical source; rate remains unknown. */
} nx_dac_info_t;
/** \brief DOR codes always use 12-bit right alignment, including u8 input. */
typedef struct {
    uint16_t digital_output12[2];
    uint32_t timer_clock_hz;
    uint32_t trigger_divisor;
    uint8_t enabled_channels;
    bool running;
    bool faulted;
} nx_dac_state_t;
/** \brief Shared immutable operations; callbacks receive instance context. */
typedef struct {
    nx_result_t (*info)(const void* context, nx_dac_info_t* info);
    nx_result_t (*write)(void* context, uint8_t channel, uint16_t code);
    nx_result_t (*write_pair)(void* context, uint16_t first, uint16_t second);
    nx_result_t (*start)(void* context, uint32_t trigger_hz,
                         nx_time_us_t deadline);
    nx_result_t (*trigger)(void* context, uint8_t channels);
    nx_result_t (*stop)(void* context, nx_time_us_t deadline);
    nx_result_t (*state)(const void* context, nx_dac_state_t* state);
} nx_dac_ops_t;
/** \brief Face and caller/provider storage outlive all uses and full close. */
struct nx_dac_port {
    const nx_dac_ops_t* ops;
    void* context;
};
/**
 * \brief Query fixed raw format and authorized channels without starting I/O.
 * \param[in] port: Stable initialized face; serialized task owner.
 * \param[out] info: Independent output storage, unchanged on rejection.
 * \return SUCCESS, INVALID, STATE or UNSUPPORTED.
 */
nx_result_t nx_dac_port_info(const nx_dac_port_t* port, nx_dac_info_t* info);
/**
 * \brief Stage one input code; left alignment is encoded by the provider.
 * \param[in] port: Selected controller under one serialized task owner.
 * \param[in] channel: Physical channel 1 or 2, authorized by this face.
 * \param[in] code: 0..255 for u8 or 0..4095 for either 12-bit storage format.
 * \return SUCCESS, INVALID, PERMISSION, STATE, CONTEXT or UNSUPPORTED.
 * \note No buffer is retained. Triggered output waits for a later trigger;
 *       waveform offsets must leave room for the selected amplitude.
 *       External-event ports require a running source; stopped writes do not
 *       implicitly reopen trigger detection.
 */
nx_result_t nx_dac_port_write(const nx_dac_port_t* port, uint8_t channel,
                              uint16_t code);
/**
 * \brief Stage both channels using one hardware dual-data write.
 * \param[in] port: Exclusive owner authorized for both channels.
 * \param[in] first: Channel-one input code in the selected raw unit.
 * \param[in] second: Channel-two input code in the selected raw unit.
 * \return SUCCESS, INVALID, PERMISSION, STATE, CONTEXT or UNSUPPORTED.
 * \note No retained buffers. A common trigger determines simultaneous output;
 *       atomic staging alone does not establish analog simultaneity.
 */
nx_result_t nx_dac_port_write_pair(const nx_dac_port_t* port, uint16_t first,
                                   uint16_t second);
/**
 * \brief Enable selected outputs and their explicitly owned trigger source.
 * \param[in] port: Prepared exclusive task owner; no concurrent controls.
 * \param[in] trigger_hz: Positive for an owned timer; zero for direct/software
 *                        or an external source with unknown frequency.
 * \param[in] deadline: Absolute monotonic deadline including startup delay.
 * \return SUCCESS, INVALID, BUSY, STATE, TIMEOUT, EXHAUSTED, IO, CONTEXT or
 *         UNSUPPORTED.
 * \note Failure may retain enabled hardware; stop/close must observe release.
 *       Declared digital startup does not qualify an unknown analog load.
 */
nx_result_t nx_dac_port_start(const nx_dac_port_t* port, uint32_t trigger_hz,
                              nx_time_us_t deadline);
/**
 * \brief Emit one common software trigger for an authorized channel mask.
 * \param[in] port: Running software-trigger owner, serialized task context.
 * \param[in] channels: Nonempty NX_DAC_CHANNEL_* mask within authorization.
 * \return SUCCESS, INVALID, PERMISSION, STATE, CONTEXT or UNSUPPORTED.
 * \note This never substitutes for an external trigger or a DMA request.
 */
nx_result_t nx_dac_port_trigger(const nx_dac_port_t* port, uint8_t channels);
/**
 * \brief Stop progression and hold selected idle codes until full close.
 * \param[in] port: Same serialized owner, with all caller storage still live.
 * \param[in] deadline: Absolute deadline for the digital idle observation.
 * \return SUCCESS, INVALID, STATE, BUSY, TIMEOUT, IO or CONTEXT.
 * \note BUSY/failure retains hardware responsibility. Full Platform release
 *       later disables the DAC and restores its selected original pin fields.
 */
nx_result_t nx_dac_port_stop(const nx_dac_port_t* port, nx_time_us_t deadline);
/**
 * \brief Read digital DOR and declared timer division, not analog voltage.
 * \param[in] port: Initialized task owner; no concurrent state mutation.
 * \param[out] state: Independent snapshot; u8 input reads back as code<<4.
 * \return SUCCESS, INVALID, STATE, CONTEXT or UNSUPPORTED.
 */
nx_result_t nx_dac_port_state(const nx_dac_port_t* port, nx_dac_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
