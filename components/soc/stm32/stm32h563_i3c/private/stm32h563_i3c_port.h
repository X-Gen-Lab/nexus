/**
 * \file            stm32h563_i3c_port.h
 * \brief           Private bounded STM32H563 I3C1 polling transport
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32H563_I3C_PORT_H
#define NEXUS_STM32H563_I3C_PORT_H
#include "nexus/components/i3c_controller_storage.h"
#include "stm32h5xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Trusted serialized resource and physical-proof leaves, never tasks.
 * \note acquire proves exclusive I3C1/clock/pins/IRQ ownership, a correctly
 *       timed prepared controller, ES0565 SDA preparation for at least 1 ms
 *       before enable, and invalid dynamic addresses for EVERY powered target.
 *       It neither calls applications nor reenters the controller. Rejection
 *       has no effects. quiescent proves detached IRQ/pending observers and
 *       wire idle. reset_targets additionally proves every target address
 *       invalidated; RSTDAA, FIFO flush or peripheral reset alone do not
 * suffice. Failed proof retains ownership. After successful quiescent,
 * release is infallible and removes every reference and observer.
 * All callbacks are bounded
 * Thread-only leaves; no hidden waits, buffer references, scheduler or
 * allocation.
 */
typedef struct {
    nx_result_t (*acquire)(void* context, const void* owner);
    void (*release)(void* context, const void* owner);
    nx_result_t (*quiescent)(void* context);
    nx_result_t (*reset_targets)(void* context);
} nx_h563_i3c_resources_t;
/** \brief Immutable assembly references, alive through final port release. */
typedef struct {
    I3C_TypeDef* registers;
    const nx_h563_i3c_resources_t* resources;
    void* resource_context;
} nx_h563_i3c_port_config_t;
/**
 * \brief Caller-owned private transport, separate from core/request storage.
 * \note Zero-initialize once; do not copy, inspect or mutate while claimed.
 *       Firmware uses I3C1 only; host models substitute the register boundary.
 *       Polling exclusively owns hardware with IER/DMA/control/status FIFO
 *       disabled and byte thresholds. Board/SoC C preparation owns timing and
 *       electrical facts, including a valid separate controller address;
 *       binding does not initialize hardware or claim support. Drain closes
 *       HJ/IBI acknowledgement before proof; a subsequent begin/idle pump
 *       explicitly resumes it. Targets can receive NACK during this pause.
 *       Successful final proof leaves admission closed through infallible
 *       release. Reset drops old hardware event latches; the core retains
 *       already-copied events with their original endpoint epochs.
 */
typedef struct {
    nx_h563_i3c_port_config_t config;
    const void* owner;
    nx_i3c_command_t command;
    nx_result_t error;
    uint32_t moved;
    uint8_t identity[8];
    uint8_t identity_bytes;
    uint8_t pending_address;
    uint8_t pending_bcr;
    uint8_t ibi_slots;
    bool active;
    bool withdrawing;
    bool frame_complete;
    bool identity_waiting;
    bool assignment_pending;
    bool direct_pending;
    bool events_paused;
    bool quiescence_proven;
} nx_h563_i3c_port_t;
/**
 * \brief Bind fresh private storage without hardware effects or allocation.
 * \param[out] port: Zero-initialized storage; remains live until release.
 * \param[in] config: Copied references to separate live hardware/proof owners.
 * \return SUCCESS, INVALID or STATE; rejection does not change storage.
 * \note Serialized Thread context; core owns absolute deadlines and borrowing.
 */
nx_result_t nx_h563_i3c_port_bind(nx_h563_i3c_port_t* port,
                                  const nx_h563_i3c_port_config_t* config);
/** \brief One shared immutable table; no factory or forwarding allocation. */
extern const nx_i3c_port_ops_t nx_h563_i3c_port_ops;
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32H563_I3C_PORT_H */
