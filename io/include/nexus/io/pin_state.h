/**
 * \file            pin_state.h
 * \brief           Explicit static default and sleep pad states
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_PIN_STATE_H
#define NEXUS_IO_PIN_STATE_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Only explicitly authored GPIO states; no implicit power policy. */
typedef enum {
    NX_PIN_STATE_DEFAULT = 0,
    NX_PIN_STATE_SLEEP = 1,
    NX_PIN_STATE_INVALID = 255
} nx_pin_state_selection_t;
/** \brief One statically owned provider; no discovery or per-register dispatch.
 */
typedef struct {
    nx_result_t (*select)(void* context, nx_pin_state_selection_t state);
} nx_pin_state_ops_t;
/** \brief Immutable face; provider storage and plans outlive every use. */
typedef struct {
    const nx_pin_state_ops_t* ops;
    void* context;
} nx_pin_state_port_t;
/**
 * \brief Select one explicitly supported GPIO state without waiting.
 * \param[in] port: Exclusive static pad/gate lease, with all writers quiescent.
 * \param[in] state: Default or sleep; neither name implies electrical safety.
 * \return SUCCESS after actual target readback, or INVALID/UNSUPPORTED/
 *         CONTEXT/STATE/BUSY/IO while preserving provider ownership.
 * \note Serialized same-CPU privileged Thread with incoming masks clear.
 *       No ISR/NMI, competing pad/RCC owner, AF switching, clock transition,
 *       wake or retention policy. Partial failure retains the lease; retry an
 *       explicit supported target or let the owner perform checked cleanup.
 *       This does not prove physical transients, external bias or PCB safety.
 */
nx_result_t nx_pin_state_select(const nx_pin_state_port_t* port,
                                nx_pin_state_selection_t state);
#ifdef __cplusplus
}
#endif
#endif
