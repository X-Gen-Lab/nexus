/**
 * \file            secure_batch.c
 * \brief           Single-slot copied Secure GPIO command execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_secure_batch.h"
#include <string.h>

/* A region large enough for the message can also fit a complete reply;
 * each object's offset is still checked independently before subtraction. */
_Static_assert(sizeof(nx_stm32u575_secure_batch_message_t) >=
                   sizeof(nx_stm32u575_secure_batch_reply_t),
               "Secure batch message must not be smaller than its reply");

static bool span(const void* pointer, size_t bytes) {
    return pointer != NULL && bytes != 0U &&
           (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* left, size_t left_bytes, const void* right,
                    size_t right_bytes) {
    return (uintptr_t)left < (uintptr_t)right + right_bytes &&
           (uintptr_t)right < (uintptr_t)left + left_bytes;
}
static bool valid(const nx_stm32u575_secure_batch_t* slot) {
    return span(slot, sizeof(*slot)) && slot->port != NULL &&
           slot->port->ops != NULL && slot->port->ops->write != NULL &&
           slot->port->context != NULL && slot->allowed != 0U;
}
static bool external(const nx_stm32u575_secure_batch_t* slot,
                     const void* pointer, size_t bytes) {
    return span(pointer, bytes) &&
           !overlap(pointer, bytes, slot, sizeof(*slot)) &&
           !overlap(pointer, bytes, slot->port, sizeof(*slot->port)) &&
           !overlap(pointer, bytes, slot->port->ops, sizeof(*slot->port->ops));
}
static bool matches(const nx_stm32u575_secure_batch_t* slot, uint32_t ticket) {
    return ticket != 0U && ticket == slot->status.epoch &&
           slot->status.phase != NX_STM32U575_BATCH_IDLE;
}
nx_result_t nx_stm32u575_secure_batch_submit(
    nx_stm32u575_secure_batch_t* slot,
    const nx_stm32u575_secure_gpio_command_t* commands, size_t count,
    uint32_t* ticket) {
    if (!valid(slot) || count == 0U || count > NX_STM32U575_SECURE_BATCH_MAX ||
        !external(slot, commands, count * sizeof(*commands)) ||
        !external(slot, ticket, sizeof(*ticket)) ||
        overlap(ticket, sizeof(*ticket), commands, count * sizeof(*commands)))
        return NX_ERROR_INVALID;
    if (slot->executing || slot->status.phase != NX_STM32U575_BATCH_IDLE)
        return NX_ERROR_BUSY;
    if (slot->status.epoch == UINT32_MAX)
        return NX_ERROR_STATE;
    for (size_t index = 0U; index < count; ++index)
        if (((commands[index].set | commands[index].clear) & ~slot->allowed) !=
                0U ||
            (commands[index].set & commands[index].clear) != 0U)
            return NX_ERROR_PERMISSION;
    /* Acceptance retains only this Secure copy, never the source span. */
    memcpy(slot->commands, commands, count * sizeof(*commands));
    slot->count = (uint32_t)count;
    slot->status.epoch++;
    slot->status.completed = 0U;
    slot->status.result = NX_SUCCESS;
    slot->status.phase = NX_STM32U575_BATCH_RUNNING;
    *ticket = slot->status.epoch;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_secure_batch_service(nx_stm32u575_secure_batch_t* slot,
                                              uint32_t ticket) {
    if (!valid(slot))
        return NX_ERROR_INVALID;
    if (slot->executing)
        return NX_ERROR_BUSY;
    if (!matches(slot, ticket) ||
        slot->status.phase != NX_STM32U575_BATCH_RUNNING || slot->count == 0U ||
        slot->count > NX_STM32U575_SECURE_BATCH_MAX ||
        slot->status.completed >= slot->count)
        return NX_ERROR_STATE;
    const nx_stm32u575_secure_gpio_command_t command =
        slot->commands[slot->status.completed];
    slot->executing = true;
    nx_result_t result =
        nx_gpio_port_write(slot->port, command.set, command.clear);
    slot->executing = false;
    slot->status.result = result;
    if (result != NX_SUCCESS)
        slot->status.phase = NX_STM32U575_BATCH_FAILED;
    else if (++slot->status.completed == slot->count)
        slot->status.phase = NX_STM32U575_BATCH_COMPLETE;
    return result;
}
nx_result_t nx_stm32u575_secure_batch_cancel(nx_stm32u575_secure_batch_t* slot,
                                             uint32_t ticket) {
    if (!valid(slot))
        return NX_ERROR_INVALID;
    if (slot->executing)
        return NX_ERROR_BUSY;
    if (!matches(slot, ticket) ||
        slot->status.phase != NX_STM32U575_BATCH_RUNNING)
        return NX_ERROR_STATE;
    slot->status.result = NX_ERROR_CANCELLED;
    slot->status.phase = NX_STM32U575_BATCH_CANCELLED;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32u575_secure_batch_poll(const nx_stm32u575_secure_batch_t* slot,
                               uint32_t ticket,
                               nx_stm32u575_secure_batch_status_t* status) {
    if (!valid(slot) || !external(slot, status, sizeof(*status)))
        return NX_ERROR_INVALID;
    if (slot->executing)
        return NX_ERROR_BUSY;
    if (!matches(slot, ticket))
        return NX_ERROR_STATE;
    *status = slot->status;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_secure_batch_consume(nx_stm32u575_secure_batch_t* slot,
                                              uint32_t ticket) {
    if (!valid(slot))
        return NX_ERROR_INVALID;
    if (slot->executing)
        return NX_ERROR_BUSY;
    if (!matches(slot, ticket))
        return NX_ERROR_STATE;
    if (slot->status.phase == NX_STM32U575_BATCH_RUNNING)
        return NX_ERROR_BUSY;
    /* Completion has no residual external loan; erase copied command data. */
    memset(slot->commands, 0, sizeof(slot->commands));
    slot->count = 0U;
    slot->status.phase = NX_STM32U575_BATCH_IDLE;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_secure_batch_dispatch(
    nx_stm32u575_secure_batch_t* slot,
    const nx_stm32u575_secure_batch_message_t* message,
    nx_stm32u575_secure_batch_reply_t* reply) {
    if (!valid(slot) || !external(slot, message, sizeof(*message)) ||
        !external(slot, reply, sizeof(*reply)) ||
        overlap(message, sizeof(*message), reply, sizeof(*reply)) ||
        message->action < NX_STM32U575_BATCH_SUBMIT ||
        message->action > NX_STM32U575_BATCH_CONSUME ||
        (message->action != NX_STM32U575_BATCH_SUBMIT && message->count != 0U))
        return NX_ERROR_INVALID;
    nx_result_t result;
    uint32_t ticket = message->ticket;
    switch (message->action) {
        case NX_STM32U575_BATCH_SUBMIT:
            if (ticket != 0U)
                return NX_ERROR_INVALID;
            result = nx_stm32u575_secure_batch_submit(slot, message->commands,
                                                      message->count, &ticket);
            break;
        case NX_STM32U575_BATCH_SERVICE:
            result = nx_stm32u575_secure_batch_service(slot, ticket);
            break;
        case NX_STM32U575_BATCH_CANCEL:
            result = nx_stm32u575_secure_batch_cancel(slot, ticket);
            break;
        case NX_STM32U575_BATCH_CONSUME:
            result = nx_stm32u575_secure_batch_consume(slot, ticket);
            break;
        default: {
            nx_stm32u575_secure_batch_status_t status;
            result = nx_stm32u575_secure_batch_poll(slot, ticket, &status);
            break;
        }
    }
    if (result != NX_SUCCESS)
        return result;
    *reply = (nx_stm32u575_secure_batch_reply_t){
        slot->status.epoch, slot->status.completed,
        (uint32_t)slot->status.result, (uint32_t)slot->status.phase};
    return NX_SUCCESS;
}
bool nx_stm32u575_secure_batch_ns_spans(
    uintptr_t ram_base, size_t ram_bytes,
    const nx_stm32u575_secure_batch_message_t* message,
    const nx_stm32u575_secure_batch_reply_t* reply) {
    const uintptr_t input = (uintptr_t)message;
    const uintptr_t output = (uintptr_t)reply;
    if (ram_bytes == 0U || ram_base > UINTPTR_MAX - ram_bytes ||
        !span(message, sizeof(*message)) || !span(reply, sizeof(*reply)) ||
        input % _Alignof(nx_stm32u575_secure_batch_message_t) != 0U ||
        output % _Alignof(nx_stm32u575_secure_batch_reply_t) != 0U ||
        input < ram_base || output < ram_base || sizeof(*message) > ram_bytes ||
        input - ram_base > ram_bytes - sizeof(*message) ||
        output - ram_base > ram_bytes - sizeof(*reply) ||
        overlap(message, sizeof(*message), reply, sizeof(*reply)))
        return false;
    return true;
}
