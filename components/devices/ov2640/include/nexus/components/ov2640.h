/**
 * \file            ov2640.h
 * \brief           Reviewed ST OV2640 RGB565 profiles over explicit SCCB ports
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_OV2640_H
#define NEXUS_COMPONENTS_OV2640_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Exact settled8-bit subaddress/data control and bounded external wait.
 * \note All calls use one ordinary serialized executor across this supplier
 *       bridge's instances. The port addresses the selected OV2640 SCCB child
 *       (write ID0x60/read ID0x61), never a NACK-ignoring I2C fallback. Reads
 *       preserve output on failure; wait_ms releases all arguments before
 *       returning and obeys its declared finite budget and unchanged deadline.
 *       Clock, callback code and opaque context survive successful stop. An
 *       application may adapt nx_software_sccb_*_register directly; this port
 *       starts no bus, GPIO, sensor clock, capture engine, task or allocation.
 */
typedef struct {
    void* context;
    nx_result_t (*write)(void*, uint8_t, uint8_t, nx_time_us_t);
    nx_result_t (*read)(void*, uint8_t, uint8_t*, nx_time_us_t);
    nx_result_t (*wait_ms)(void*, uint32_t, nx_time_us_t);
    nx_clock_t clock;
} nx_ov2640_port_t;

/** \brief The four complete RGB565 tables reviewed in the locked ST driver. */
typedef enum {
    NX_OV2640_UNKNOWN = 0,
    NX_OV2640_QQVGA,
    NX_OV2640_QVGA,
    NX_OV2640_480X272,
    NX_OV2640_VGA
} nx_ov2640_resolution_t;

/**
 * \brief Immutable exact control profile, retained until successful stop.
 * \note External reset/supply and6..24MHz XCLK are stable before init. XCLK
 *       and PCB/data-lane facts are product responsibilities; frequency is an
 *       explicit declared input, not generated/physically measured here.
 *       expected_pid_l is the reviewed module revision's exact product ID LSB;
 *       datasheet1.6 specifies0x41. No probe scan, revision or mode fallback.
 *       QQVGA160x120 RGB565 uses38400 bytes; QVGA320x240 uses153600 bytes and
 *       cannot fit F407's128KiB DMA main SRAM. Bigger frames need independently
 *       admitted memory. Format is8-bit DVP, RGB565 low byte first; selected
 *       capture layout/polarities/bandwidth need separate admission. No JPEG,
 *       gapless frames, PCLK frequency or physical timing is claimed.
 */
typedef struct {
    nx_ov2640_port_t port;
    nx_ov2640_resolution_t resolution;
    uint32_t xclk_hz;
    uint8_t expected_pid_l;
} nx_ov2640_config_t;

/**
 * \brief Caller-owned per-sensor state; zero before first initialization.
 * \note Software readiness means the exact table and readbacks completed.
 *       An error retains immutable binding and uncertain source responsibility;
 *       no MCU payload reference survives these settled control calls.
 */
typedef struct {
    const nx_ov2640_config_t* config;
    nx_time_us_t deadline;
    nx_time_us_t last_clock;
    nx_result_t error;
    bool initialized;
    bool uncertain;
    bool in_call;
    bool clock_failed;
} nx_ov2640_t;

/**
 * \brief Check exact identity and execute the real locked ST profile table.
 * \param[in,out] device: Zero/stopped or same-binding uncertain sensor.
 * \param[in] config: Stable reviewed binding/profile through successful stop.
 * \param[in] deadline: Finite whole-operation absolute port-clock budget.
 * \return SUCCESS, INVALID/TIMEOUT before admission, BUSY for live ownership,
 *         UNSUPPORTED for exact identity mismatch, or first port/time error.
 * \note Task-only synchronous startup, including upstream's200ms reset wait
 *       and explicit bounded per-register waits. The caller chooses a suitable
 *       maintenance/startup execution context. On first callback failure all
 *       later upstream void callbacks suppress physical access and the exact
 *       error returns; partial configuration is never reported as initialized.
 *       No power, XCLK, DCMI, DMA or buffer is acquired. Config/device/opaque
 *       owner extents are disjoint; multiple live instances share only code.
 */
nx_result_t nx_ov2640_initialize(nx_ov2640_t* device,
                                 const nx_ov2640_config_t* config,
                                 nx_time_us_t deadline);

/**
 * \brief Enter sensor soft standby and verify the selected source predicate.
 * \param[in,out] device: Same sole task after capture source/users have joined.
 * \param[in] deadline: Finite cleanup budget in the original live clock domain.
 * \return SUCCESS after COM2 standby readback; exact retained error otherwise.
 * \note Caller first closes and joins DCMI/DMA/IRQ and every frame consumer;
 *       this control operation cannot revoke those independent loans. Uses
 *       sensor-bank COM2 read/modify/write, preserving drive bits. It does not
 *       remove power/XCLK, stop shared SCCB or infer physical quiescence from
 *       elapsed time. Failed stop retains binding for explicit owner recovery.
 */
nx_result_t nx_ov2640_stop(nx_ov2640_t* device, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_OV2640_H */
