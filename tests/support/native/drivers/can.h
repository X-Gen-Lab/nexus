/**
 * \file            can.h
 * \brief           Private explicit Native classic-CAN hardware model storage
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_CAN_H
#define NEXUS_NATIVE_CAN_H
#include "can_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model-only register facts; no implicit controller discovery/storage.
 */
typedef struct {
    uint32_t registers[200];
    uint32_t clock_enable;
    uint32_t receive[2][3][4];
    uint8_t receive_head[2];
    uint8_t receive_count[2];
    uint8_t enabled_irqs;
    bool hold_abort;
    bool hold_enter;
    bool hold_exit;
    bool hold_irq_disable;
    bool hold_irq_enable;
    bool hold_fifo_release;
    bool hold_interrupt_clear;
    bool hold_completion_clear;
} nx_native_can_registers_t;
/** \brief An explicit model instance with the same production mailbox engine.
 */
typedef struct {
    nx_can_controller_t controller;
    nx_native_can_registers_t hardware;
} nx_native_can_t;
/** \brief Bind existing storage; configuration/domain/ring are caller-owned. */
nx_result_t nx_native_can_initialize(nx_native_can_t* instance);
/** \brief Inject actual completion facts; empty alone never proves success. */
nx_result_t nx_native_can_complete(nx_native_can_t* instance, unsigned mailbox,
                                   nx_result_t result);
/** \brief Queue at most three received frames per modeled hardware FIFO. */
nx_result_t nx_native_can_receive(nx_native_can_t* instance, unsigned fifo,
                                  const nx_can_frame_t* frame, uint8_t raw_dlc,
                                  uint8_t filter);
/** \brief Latch error counters/bus-off separately from the receive queue. */
void nx_native_can_error(nx_native_can_t* instance, bool bus_off, uint8_t tec,
                         uint8_t rec, uint8_t last_error);
/** \brief Native MMIO leaf read, compiled only for the selected Native model.
 */
uint32_t nx_native_can_read(uintptr_t base, unsigned offset);
/** \brief Native MMIO W1C/abort/FIFO/INAK store semantics, never a success
 * stub.
 */
void nx_native_can_write(uintptr_t base, unsigned offset, uint32_t value);
/** \brief Model selected vector detach; can explicitly retain a stuck source.
 */
void nx_native_can_irq_set(nx_can_controller_t* controller, unsigned vector,
                           bool enabled);
/** \brief Model vector readback, independent from peripheral interrupt flags.
 */
bool nx_native_can_irq_enabled(nx_can_controller_t* controller,
                               unsigned vector);
#ifdef __cplusplus
}
#endif
#endif
