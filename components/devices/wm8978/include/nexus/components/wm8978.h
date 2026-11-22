/**
 * \file            wm8978.h
 * \brief           Selected WM8978 slave-I2S stereo OUT1 playback control
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_WM8978_H
#define NEXUS_COMPONENTS_WM8978_H
#include "nexus/io/i2c.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Exact write-only codec control binding and selected external clocks.
 * \note Endpoint is the reviewed7-bit WM8978 address0x1a. Clock and endpoint
 *       use one monotonic domain. Supply and MCLK are already stable; product
 *       owns I2S data/clock pins and keeps PCM stopped during initialization.
 *       This profile selects48kHz nominal, external MCLK/256, no codec PLL,
 *       slave I2S, stereo DAC-to-OUT1, no1.5x output boost. The actual mclk_hz
 *       must be within explicit maximum_clock_error_ppm (0..1000) of12.288MHz.
 *       vmid_settle_us is a positive product-qualified bound for its VMID
 *       capacitor/reference impedance; no generic settling time is invented.
 *       word_bits is16/20/24/32; selected I2S provider must independently
 * support its wire layout. ADC/microphone, speaker OUT2, EQ and PLL are
 * unselected. Immutable config, faces and opaque contexts live until successful
 * stop.
 */
typedef struct {
    const nx_i2c_endpoint_t* endpoint;
    nx_clock_t clock;
    uint32_t vmid_settle_us;
    uint32_t mclk_hz;
    uint32_t maximum_clock_error_ppm;
    uint8_t word_bits;
} nx_wm8978_config_t;

/** \brief Cooperative reference charging, ready and uncertain control states.
 */
typedef enum {
    NX_WM8978_STOPPED = 0,
    NX_WM8978_VMID_WAIT,
    NX_WM8978_READY,
    NX_WM8978_UNCERTAIN
} nx_wm8978_phase_t;

/**
 * \brief Caller-owned exact state; zero before first initialization.
 * \note One nonreentrant ordinary task owns all calls and PCM admission.
 *       Write-only register acceptance cannot prove codec identity, clock
 *       fitting, analogue settling, acoustic output or mute effectiveness.
 *       Fields are internal observations, never permission to rebind/reclaim.
 *       Failure retains the same binding until stop succeeds; clock rollback
 *       latches a terminal domain error that numeric time recovery cannot undo.
 */
typedef struct {
    const nx_wm8978_config_t* config;
    nx_time_us_t ready_at;
    nx_time_us_t deadline;
    nx_time_us_t last_clock;
    nx_wm8978_phase_t phase;
    int8_t left_db;
    int8_t right_db;
    bool muted;
    bool in_call;
    bool clock_failed;
} nx_wm8978_t;

/**
 * \brief Reset, mute outputs and begin explicit VMID reference charging.
 * \param[in,out] device: Zero/stopped or same-binding uncertain state.
 * \param[in] config: Stable reviewed control/clock settings through stop.
 * \param[in] deadline: Finite absolute budget covering startup and VMID wait.
 * \return SUCCESS awaiting service, INVALID/TIMEOUT before effects, BUSY for
 *         active/rebound ownership, or exact settled I2C/time failure.
 * \note No sleep, clock generation, task, allocation or automatic replay.
 *       Once a control write is attempted, failure retains UNCERTAIN binding.
 *       The9-bit register data is encoded in the actual two-wire byte format.
 */
nx_result_t nx_wm8978_initialize(nx_wm8978_t* device,
                                 const nx_wm8978_config_t* config,
                                 nx_time_us_t deadline);

/**
 * \brief Finish selected muted playback configuration after VMID wait.
 * \param[in,out] device: Same task/binding and original clock epoch.
 * \return BUSY before ready_at, SUCCESS in READY, or exact error.
 * \note Uses original whole-startup deadline. Default OUT1 gain is-57dB and
 *       muted; explicitly choose gain and unmute only after PCM is ready.
 */
nx_result_t nx_wm8978_service(nx_wm8978_t* device);

/**
 * \brief Stage stereo headphone gain then latch both channels together.
 * \param[in,out] device: Same READY task owner.
 * \param[in] left_db: Integer gain from -57 through +6 dB.
 * \param[in] right_db: Integer gain from -57 through +6 dB.
 * \param[in] deadline: Finite whole-update absolute deadline.
 * \return SUCCESS or exact validation/control/time failure.
 * \note 1 dB steps, no zero-cross waiting; current explicit mute is preserved.
 *       Success means register writes settled, not acoustic gain measurement.
 */
nx_result_t nx_wm8978_volume(nx_wm8978_t* device, int8_t left_db,
                             int8_t right_db, nx_time_us_t deadline);

/**
 * \brief Explicitly select digital soft mute and OUT1 analogue mute.
 * \param[in,out] device: Same READY task owner.
 * \param[in] muted: Product-selected mute state; no automatic unmute.
 * \param[in] deadline: Finite whole-update absolute deadline.
 * \return SUCCESS or exact control/time failure, retaining uncertain binding.
 * \note Muting starts with DAC soft mute; unmuting clears soft mute last.
 *       This does not drain I2S/DMA, stop power or establish electrical safety.
 */
nx_result_t nx_wm8978_mute(nx_wm8978_t* device, bool muted,
                           nx_time_us_t deadline);

/**
 * \brief Mute outputs and disable power registers before releasing binding.
 * \param[in,out] device: Same task after PCM/DMA and all users have joined.
 * \param[in] deadline: Finite cleanup budget, independent of startup deadline.
 * \return SUCCESS only after all control writes settle, otherwise exact error.
 * \note Failure retains config and recovery responsibility; caller retries
 *       only after restoring its physical owner. Does not stop shared I2C,
 *       remove power, discharge VMID, alter pins or reclaim a PCM loan.
 */
nx_result_t nx_wm8978_stop(nx_wm8978_t* device, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_WM8978_H */
