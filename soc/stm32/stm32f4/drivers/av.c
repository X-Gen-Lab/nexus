/**
 * \file            av.c
 * \brief           Shared F407 audio/capture pin and producer admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_av.h"

nx_result_t nx_stm32_av_stream_validate(const nx_stm32_av_config_t* config,
                                        nx_stream_t* stream, size_t bytes,
                                        size_t alignment, const void* state,
                                        size_t state_bytes, const void* request,
                                        size_t request_bytes) {
    if (!nx_stm32_av_system_disjoint(state, state_bytes) ||
        !nx_stm32_av_system_disjoint(stream, sizeof(*stream)) ||
        (request != NULL &&
         !nx_stm32_av_system_disjoint(request, request_bytes)) ||
        stream->slots == NULL || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) || bytes == 0U)
        return NX_ERROR_INVALID;
    if (stream->stopping || stream->outstanding != 0U)
        return NX_ERROR_BUSY;
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (!nx_stm32_av_system_disjoint(stream->slots, metadata) ||
        nx_stm32_av_overlap(stream, sizeof(*stream), state, state_bytes) ||
        nx_stm32_av_overlap(stream->slots, metadata, state, state_bytes) ||
        (request != NULL &&
         (nx_stm32_av_overlap(request, request_bytes, stream,
                              sizeof(*stream)) ||
          nx_stm32_av_overlap(request, request_bytes, stream->slots,
                              metadata) ||
          nx_stm32_av_overlap(request, request_bytes, state, state_bytes))))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i) {
        nx_stream_slot_t* slot = &stream->slots[i];
        nx_result_t result = nx_dma_storage_validate(
            config->regions, config->region_count, slot->data, slot->capacity,
            bytes, alignment, 0U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        if (slot->state != NX_STREAM_SLOT_FREE ||
            !nx_stm32_av_system_disjoint(slot->data, slot->capacity) ||
            nx_stm32_av_overlap(slot->data, slot->capacity, state,
                                state_bytes) ||
            (request != NULL && nx_stm32_av_overlap(slot->data, slot->capacity,
                                                    request, request_bytes)))
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_av_pins(const nx_stm32_av_config_t* config) {
    if (config == NULL || config->pins == NULL || config->pin_count == 0U ||
        config->poll_limit == 0U || config->priority > 15U)
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &config->pins[i];
        if (pin->registers == NULL || pin->pin > 15U || pin->af > 15U ||
            pin->bank > 8U || pin->snapshot == NULL ||
            (NX_AV_READ(&pin->registers->LCKR) & (1U << pin->pin)) != 0U ||
            (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & (1U << pin->bank)) ==
                0U)
            return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &config->pins[i];
        uint32_t field = 3U << (2U * pin->pin);
        uint32_t af = 15U << (4U * (pin->pin & 7U));
        /* AF is established before MODER connects the peripheral to the pad. */
        if (!nx_stm32_av_bits(&pin->registers->AFR[pin->pin / 8U], af,
                              (uint32_t)pin->af << (4U * (pin->pin & 7U))) ||
            !nx_stm32_av_bits(&pin->registers->PUPDR, field, 0U) ||
            !nx_stm32_av_bits(&pin->registers->OSPEEDR, field, field) ||
            !nx_stm32_av_bits(&pin->registers->OTYPER, 1U << pin->pin, 0U) ||
            !nx_stm32_av_bits(&pin->registers->MODER, field,
                              2U << (2U * pin->pin)))
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
