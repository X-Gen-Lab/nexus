/**
 * \file            usb_host.c
 * \brief           Selected STM32F407 official HCD transport and packet drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32f407_clock.h"
#include "stm32f407_usb_host.h"
#include <string.h>

#ifndef NX_USB_HOST_WRITE
#define NX_USB_HOST_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_USB_HOST_IRQ_ENABLE
#define NX_USB_HOST_IRQ_ENABLE(irq, priority)                                  \
    do {                                                                       \
        NVIC_SetPriority((irq), (priority));                                   \
        NVIC_ClearPendingIRQ(irq);                                             \
        NVIC_EnableIRQ(irq);                                                   \
    } while (0)
#endif
#ifndef NX_USB_HOST_IRQ_DISABLE
#define NX_USB_HOST_IRQ_DISABLE(irq)                                           \
    do {                                                                       \
        NVIC_DisableIRQ(irq);                                                  \
        NVIC_ClearPendingIRQ(irq);                                             \
    } while (0)
#endif
#ifndef NX_USB_HOST_IRQ_ENABLED
#define NX_USB_HOST_IRQ_ENABLED(irq) (NVIC_GetEnableIRQ(irq) != 0U)
#endif
#ifndef NX_USB_HOST_IRQ_ACTIVE
#define NX_USB_HOST_IRQ_ACTIVE(irq) (NVIC_GetActive(irq) != 0U)
#endif
#ifndef NX_USB_HOST_CLOCK_VALID
#define NX_USB_HOST_CLOCK_VALID() nx_stm32_clock_profile_matches(RCC, NULL)
#endif
#ifndef NX_USB_HOST_AHB_HZ
#define NX_USB_HOST_AHB_HZ NEXUS_CORE_HZ
#endif
#ifndef NX_USB_HOST_PARK
#define NX_USB_HOST_PARK() nx_arch_dsb()
#endif

#define HOST_RESET_RETAINED  (1U << 2U)
#define HOST_RESUME_RETAINED (1U << 3U)

/** \brief Only the single privileged task may control this transport. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Detect wrapping spans before comparing caller/provider storage. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t av = (uintptr_t)a, bv = (uintptr_t)b;
    return an > UINTPTR_MAX - av || bn > UINTPTR_MAX - bv ||
           (an != 0U && bn != 0U && av < bv + bn && bv < av + an);
}

/** \brief Selected official channel register stride, no runtime MMIO table. */
static USB_OTG_HostChannelTypeDef*
channel_registers(const nx_stm32_usb_host_t* state, uint8_t number) {
    return (USB_OTG_HostChannelTypeDef*)((uintptr_t)state->hal.Instance +
                                         USB_OTG_HOST_CHANNEL_BASE +
                                         (uintptr_t)number * 32U);
}

/** \brief The core's host register bank shares the physical controller. */
static USB_OTG_HostTypeDef* host_registers(const nx_stm32_usb_host_t* state) {
    return (USB_OTG_HostTypeDef*)((uintptr_t)state->hal.Instance +
                                  USB_OTG_HOST_BASE);
}

/** \brief Port control is separate from the external VBUS power switch. */
static volatile uint32_t* port_register(const nx_stm32_usb_host_t* state) {
    return (volatile uint32_t*)((uintptr_t)state->hal.Instance +
                                USB_OTG_HOST_PORT_BASE);
}

/** \brief The two actual F407 vectors remain distinct static identities. */
static IRQn_Type vector(const nx_stm32_usb_host_t* state) {
    return state->config->phy == NX_STM32_USB_FS_INTERNAL ? OTG_FS_IRQn
                                                          : OTG_HS_IRQn;
}

/** \brief Public speed values are deliberately independent of HAL encodings. */
static uint8_t hal_speed(nx_usb_host_speed_t speed) {
    return speed == NX_USB_HOST_LOW_SPEED    ? HCD_DEVICE_SPEED_LOW
           : speed == NX_USB_HOST_FULL_SPEED ? HCD_DEVICE_SPEED_FULL
                                             : HCD_DEVICE_SPEED_HIGH;
}

/** \brief Match packet geometry to its actual negotiated wire speed. */
static bool pipe_valid(const nx_stm32_usb_host_t* state,
                       const nx_usb_host_pipe_t* pipe) {
    if (pipe == NULL || pipe->device_address > 127U ||
        (pipe->endpoint_address & 0x70U) != 0U || pipe->max_packet == 0U ||
        (pipe->speed != NX_USB_HOST_LOW_SPEED &&
         pipe->speed != NX_USB_HOST_FULL_SPEED &&
         pipe->speed != NX_USB_HOST_HIGH_SPEED) ||
        (pipe->speed == NX_USB_HOST_HIGH_SPEED &&
         state->config->phy != NX_STM32_USB_HS_ULPI)) {
        return false;
    }
    const uint16_t words = (uint16_t)((pipe->max_packet + 3U) / 4U);
    const bool in = (pipe->endpoint_address & 0x80U) != 0U;
    const bool periodic = pipe->transfer == NX_USB_HOST_INTERRUPT ||
                          pipe->transfer == NX_USB_HOST_ISOCHRONOUS;
    const uint16_t tx = periodic ? state->config->periodic_fifo_words
                                 : state->config->nonperiodic_fifo_words;
    if ((!in && tx < words) ||
        (in && state->config->rx_fifo_words < (uint32_t)words + 2U) ||
        (in && state->config->phy == NX_STM32_USB_FS_INTERNAL &&
         (state->config->rx_fifo_words < (uint32_t)words * 2U + 2U ||
          (state->config->periodic_fifo_words < words &&
           state->config->nonperiodic_fifo_words < words)))) {
        return false;
    }
    if (pipe->transfer == NX_USB_HOST_CONTROL) {
        if (pipe->speed == NX_USB_HOST_HIGH_SPEED)
            return pipe->max_packet == 64U;
        return pipe->max_packet == 8U ||
               (pipe->speed == NX_USB_HOST_FULL_SPEED &&
                (pipe->max_packet == 16U || pipe->max_packet == 32U ||
                 pipe->max_packet == 64U));
    }
    if ((pipe->endpoint_address & 15U) == 0U) {
        return false;
    }
    if (pipe->transfer == NX_USB_HOST_BULK) {
        return pipe->speed == NX_USB_HOST_HIGH_SPEED
                   ? pipe->max_packet == 512U
                   : pipe->speed == NX_USB_HOST_FULL_SPEED &&
                         (pipe->max_packet == 8U || pipe->max_packet == 16U ||
                          pipe->max_packet == 32U || pipe->max_packet == 64U);
    }
    if (pipe->transfer == NX_USB_HOST_INTERRUPT) {
        return pipe->max_packet <= (pipe->speed == NX_USB_HOST_LOW_SPEED ? 8U
                                    : pipe->speed == NX_USB_HOST_FULL_SPEED
                                        ? 64U
                                        : 1024U);
    }
    return pipe->transfer == NX_USB_HOST_ISOCHRONOUS &&
           pipe->speed != NX_USB_HOST_LOW_SPEED &&
           pipe->max_packet <=
               (pipe->speed == NX_USB_HOST_HIGH_SPEED ? 1024U : 1023U);
}

/** \brief Retain cancellation/deadline responsibility before halt commands. */
static void begin_drain(nx_stm32_usb_host_t* state, uint8_t number,
                        nx_result_t reason) {
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    if (channel->request != NULL &&
        nx_request_state(&channel->request->base) == NX_REQUEST_ACTIVE) {
        channel->terminal = reason;
        if (nx_deadline_finite_after(nx_time_now_us(),
                                     state->config->drain_timeout_us,
                                     &channel->drain_deadline) != NX_SUCCESS) {
            /* Only real late withdrawal may release an exhausted borrow. */
            channel->drain_deadline = 0U;
        }
        (void)nx_request_transition(&channel->request->base,
                                    NX_REQUEST_DRAINING);
    }
}

/** \brief HAL completion only latches facts; it never releases a loan. */
static void completed(HCD_HandleTypeDef* hal, uint8_t number,
                      HCD_URBStateTypeDef result) {
    nx_stm32_usb_host_t* state = hal->pData;
    if (state == NULL || state->config == NULL ||
        number >= state->config->channel_count) {
        return;
    }
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    if (channel->request == NULL || result == URB_IDLE) {
        return;
    }
    if (nx_request_state(&channel->request->base) == NX_REQUEST_ACTIVE) {
        channel->terminal = result == URB_DONE ? NX_SUCCESS
                            : result == URB_NOTREADY || result == URB_NYET
                                ? NX_ERROR_NACK
                                : NX_ERROR_IO;
        channel->transferred = hal->hc[number].xfer_count;
        channel->complete = true;
    }
}

/** \brief Connection notifications are controller facts, never class policy. */
static void connected(HCD_HandleTypeDef* hal) {
    nx_stm32_usb_host_t* state = hal->pData;
    if (state != NULL) {
        state->snapshot.connected = true;
    }
}

/** \brief Disconnect invalidates channels without revoking their storage. */
static void disconnected(HCD_HandleTypeDef* hal) {
    nx_stm32_usb_host_t* state = hal->pData;
    if (state == NULL || state->config == NULL) {
        return;
    }
    state->snapshot.connected = false;
    state->snapshot.port_enabled = false;
    state->snapshot.closing = true;
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        begin_drain(state, i, NX_ERROR_STATE);
    }
}

/** \brief Port enable comes from the HCD IRQ's actual HPRT observation. */
static void port_enabled(HCD_HandleTypeDef* hal) {
    nx_stm32_usb_host_t* state = hal->pData;
    if (state != NULL) {
        state->snapshot.port_enabled = true;
    }
}

/** \brief A disabled port cannot admit further endpoint traffic. */
static void port_disabled(HCD_HandleTypeDef* hal) {
    nx_stm32_usb_host_t* state = hal->pData;
    if (state != NULL) {
        state->snapshot.port_enabled = false;
    }
}

/** \brief MSP is already owned by the outer static pin/clock transaction. */
static void msp_noop(HCD_HandleTypeDef* hal) {
    (void)hal;
}

/** \brief Validate complete storage and physical facts before any effects. */
static bool config_valid(const nx_stm32_usb_host_t* state,
                         const nx_stm32_usb_host_config_t* config) {
    if (state == NULL || config == NULL || config->channels == NULL ||
        config->channel_count == 0U ||
        config->channel_count >
            (config->phy == NX_STM32_USB_FS_INTERNAL ? 8U : 12U) ||
        config->rx_fifo_words < 64U || config->nonperiodic_fifo_words < 16U ||
        config->periodic_fifo_words < 16U ||
        (uint32_t)config->rx_fifo_words + config->nonperiodic_fifo_words +
                config->periodic_fifo_words >
            (config->phy == NX_STM32_USB_FS_INTERNAL ? 320U
             : config->dma                           ? 992U
                                                     : 1024U) ||
        config->drain_timeout_us == 0U || config->priority > 15U ||
        (config->phy != NX_STM32_USB_FS_INTERNAL &&
         config->phy != NX_STM32_USB_HS_INTERNAL_FS &&
         config->phy != NX_STM32_USB_HS_ULPI) ||
        (config->phy == NX_STM32_USB_FS_INTERNAL && config->dma) ||
        (config->phy == NX_STM32_USB_HS_ULPI
             ? config->ulpi_clock_hz != 60000000U
             : config->ulpi_clock_hz != 0U) ||
        (config->dma &&
         (config->regions == NULL || config->region_count == 0U)) ||
        overlap(state, sizeof(*state), config, sizeof(*config)) ||
        overlap(state, sizeof(*state), config->channels,
                config->channel_count * sizeof(*config->channels)) ||
        overlap(config, sizeof(*config), config->channels,
                config->channel_count * sizeof(*config->channels))) {
        return false;
    }
    const nx_irq_source_t irq = {
        config->phy == NX_STM32_USB_FS_INTERNAL ? OTG_FS_IRQn : OTG_HS_IRQn,
        config->priority, 3U};
    return nx_irq_wake_validate(NULL, &config->irq_policy, &irq) == NX_SUCCESS;
}

/** \brief Cold pin owner can reject all transport facts before pin effects. */
nx_result_t
nx_stm32_usb_host_validate(const nx_stm32_usb_host_t* state,
                           const nx_stm32_usb_host_config_t* config) {
    return config_valid(state, config) ? NX_SUCCESS : NX_ERROR_INVALID;
}

/** \brief Initialize the sole official HCD transport for this physical core. */
nx_result_t
nx_stm32_usb_host_initialize(nx_stm32_usb_host_t* state,
                             const nx_stm32_usb_host_config_t* config,
                             nx_time_us_t deadline) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config_valid(state, config) || deadline == NX_DEADLINE_NEVER ||
        NX_USB_HOST_AHB_HZ <=
            (config->phy == NX_STM32_USB_FS_INTERNAL ? 14200000U : 30000000U)) {
        return NX_ERROR_INVALID;
    }
    if (state->config != NULL || state->clock_receipt != 0U) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (!NX_USB_HOST_CLOCK_VALID() ||
        (config->phy != NX_STM32_USB_HS_ULPI && NEXUS_PLL48_HZ != 48000000U)) {
        return NX_ERROR_STATE;
    }
    const bool fs = config->phy == NX_STM32_USB_FS_INTERNAL;
    const IRQn_Type irq = fs ? OTG_FS_IRQn : OTG_HS_IRQn;
    uint32_t mask = fs ? RCC_AHB2ENR_OTGFSEN : RCC_AHB1ENR_OTGHSEN;
    if (config->phy == NX_STM32_USB_HS_ULPI) {
        mask |= RCC_AHB1ENR_OTGHSULPIEN;
    }
    volatile uint32_t* gate = fs ? &RCC->AHB2ENR : &RCC->AHB1ENR;
    if ((*gate & mask) != 0U || NX_USB_HOST_IRQ_ENABLED(irq) ||
        NX_USB_HOST_IRQ_ACTIVE(irq)) {
        return NX_ERROR_BUSY;
    }
    for (uint8_t i = 0U; i < config->channel_count; ++i) {
        if (config->channels[i].request != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    memset(&state->hal, 0, sizeof(state->hal));
    state->config = config;
    state->clock_receipt = config->phy == NX_STM32_USB_HS_ULPI ? 3U : 1U;
    NX_USB_HOST_WRITE(*gate, *gate | mask);
    nx_arch_dsb();
    if ((*gate & mask) != mask) {
        return NX_ERROR_IO;
    }
    state->hal.Instance = fs ? USB_OTG_FS : USB_OTG_HS;
    state->hal.Init.Host_channels = config->channel_count;
    state->hal.Init.speed =
        (uint8_t)(config->phy == NX_STM32_USB_HS_ULPI ? HCD_SPEED_HIGH
                                                      : HCD_SPEED_FULL);
    state->hal.Init.phy_itface =
        config->phy == NX_STM32_USB_HS_ULPI ? HCD_PHY_ULPI : HCD_PHY_EMBEDDED;
    state->hal.Init.dma_enable = config->dma ? 1U : 0U;
    state->hal.pData = state;
    state->hal.MspInitCallback = msp_noop;
    state->hal.MspDeInitCallback = msp_noop;
    state->snapshot = (nx_usb_host_state_t){.closing = true};
    if (HAL_HCD_Init(&state->hal) != HAL_OK) {
        return NX_ERROR_IO;
    }
    state->hal.ConnectCallback = connected;
    state->hal.DisconnectCallback = disconnected;
    state->hal.PortEnabledCallback = port_enabled;
    state->hal.PortDisabledCallback = port_disabled;
    state->hal.HC_NotifyURBChangeCallback = completed;
    USB_OTG_GlobalTypeDef* core = state->hal.Instance;
    NX_USB_HOST_WRITE(core->GRXFSIZ, config->rx_fifo_words);
    NX_USB_HOST_WRITE(core->DIEPTXF0_HNPTXFSIZ,
                      ((uint32_t)config->nonperiodic_fifo_words << 16U) |
                          config->rx_fifo_words);
    NX_USB_HOST_WRITE(
        core->HPTXFSIZ,
        ((uint32_t)config->periodic_fifo_words << 16U) |
            ((uint32_t)config->rx_fifo_words + config->nonperiodic_fifo_words));
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (HAL_HCD_Start(&state->hal) != HAL_OK) {
        return NX_ERROR_IO;
    }
    NX_USB_HOST_IRQ_ENABLE(irq, config->priority);
    nx_arch_dsb();
    if (!NX_USB_HOST_IRQ_ENABLED(irq)) {
        return NX_ERROR_IO;
    }
    state->snapshot.started = true;
    state->snapshot.closing = false;
    return NX_SUCCESS;
}

/** \brief IRQ dispatch retains its publisher until all HAL callbacks return. */
void nx_stm32_usb_host_irq(nx_stm32_usb_host_t* state) {
    if (state == NULL || state->config == NULL || !state->snapshot.started) {
        return;
    }
    ++state->publishers;
    HAL_HCD_IRQHandler(&state->hal);
    --state->publishers;
}

/** \brief All task methods reject absent or closing controller ownership. */
static nx_result_t ready(nx_stm32_usb_host_t* state) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    return state == NULL || state->config == NULL || !state->snapshot.started ||
                   state->snapshot.closing || state->snapshot.suspended
               ? NX_ERROR_STATE
               : NX_SUCCESS;
}

/** \brief Channel targets are copied, never retained as external metadata. */
static nx_result_t open_channel(void* context, uint8_t number,
                                const nx_usb_host_pipe_t* pipe) {
    nx_stm32_usb_host_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (number >= state->config->channel_count || !pipe_valid(state, pipe)) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    if (channel->request != NULL || (channel_registers(state, number)->HCCHAR &
                                     USB_OTG_HCCHAR_CHENA) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (HAL_HCD_HC_Init(&state->hal, number, pipe->endpoint_address,
                        pipe->device_address, hal_speed(pipe->speed),
                        (uint8_t)pipe->transfer, pipe->max_packet) != HAL_OK) {
        return NX_ERROR_IO;
    }
    channel->pipe = *pipe;
    channel->interrupt_mask = channel_registers(state, number)->HCINTMSK;
    channel->configured = true;
    return NX_SUCCESS;
}

/** \brief Closing an idle channel joins only its selected interrupt source. */
static nx_result_t close_channel(void* context, uint8_t number) {
    nx_stm32_usb_host_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (number >= state->config->channel_count) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    USB_OTG_HostChannelTypeDef* registers = channel_registers(state, number);
    if (channel->request != NULL ||
        (registers->HCCHAR & USB_OTG_HCCHAR_CHENA) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t key = nx_arch_irq_save();
    NX_USB_HOST_WRITE(registers->HCINTMSK, 0U);
    NX_USB_HOST_WRITE(host_registers(state)->HAINTMSK,
                      host_registers(state)->HAINTMSK & ~(1U << number));
    NX_USB_HOST_WRITE(registers->HCDMA, 0U);
    state->hal.hc[number].xfer_buff = NULL;
    channel->configured = false;
    nx_arch_irq_restore(key);
    return NX_SUCCESS;
}

/** \brief Admission checks all spans before the one ACCEPTED publication. */
static nx_result_t submit(void* context, uint8_t number,
                          nx_usb_host_request_t* request) {
    nx_stm32_usb_host_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (number >= state->config->channel_count || request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    const bool in = (channel->pipe.endpoint_address & 0x80U) != 0U;
    if (!channel->configured || request->length > channel->pipe.max_packet) {
        return NX_ERROR_INVALID;
    }
    /* The pinned PIO writer reads a full final word. DMA also owns padding. */
    size_t extent = in ? channel->pipe.max_packet : request->length;
    extent = (extent + 3U) & ~(size_t)3U;
    if (state->config->dma && extent == 0U) {
        extent = 4U;
    }
    if (request->capacity < extent ||
        (extent != 0U && request->bytes == NULL) ||
        (request->setup &&
         (in || channel->pipe.transfer != NX_USB_HOST_CONTROL ||
          request->length != 8U)) ||
        overlap(request, sizeof(*request), state, sizeof(*state)) ||
        overlap(request, sizeof(*request), state->config->channels,
                state->config->channel_count * sizeof(*channel)) ||
        overlap(request->bytes, request->capacity, request, sizeof(*request)) ||
        overlap(request->bytes, request->capacity, state, sizeof(*state)) ||
        overlap(request->bytes, request->capacity, state->config->channels,
                state->config->channel_count * sizeof(*channel))) {
        return NX_ERROR_INVALID;
    }
    if (channel->request != NULL) {
        return NX_ERROR_BUSY;
    }
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        const nx_usb_host_request_t* other = state->config->channels[i].request;
        if (other != NULL &&
            (overlap(request, sizeof(*request), other, sizeof(*other)) ||
             overlap(request, sizeof(*request), other->bytes,
                     other->capacity) ||
             overlap(request->bytes, request->capacity, other,
                     sizeof(*other)) ||
             overlap(request->bytes, request->capacity, other->bytes,
                     other->capacity))) {
            return NX_ERROR_INVALID;
        }
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (state->config->dma && extent != 0U) {
        status = nx_dma_buffer_validate(
            state->config->regions, state->config->region_count, request->bytes,
            extent, 4U, in ? NX_DMA_FROM_DEVICE : NX_DMA_TO_DEVICE);
        if (status != NX_SUCCESS) {
            return status;
        }
    }
    nx_arch_irq_state_t key = nx_arch_irq_save();
    status = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (status != NX_SUCCESS) {
        nx_arch_irq_restore(key);
        return status;
    }
    channel->request = request;
    channel->transferred = 0U;
    channel->terminal = NX_SUCCESS;
    channel->complete = channel->halt_requested = false;
    state->hal.hc[number].toggle_in = request->data1 ? 1U : 0U;
    state->hal.hc[number].toggle_out = request->data1 ? 1U : 0U;
    NX_USB_HOST_WRITE(channel_registers(state, number)->HCINTMSK,
                      channel->interrupt_mask);
    NX_USB_HOST_WRITE(host_registers(state)->HAINTMSK,
                      host_registers(state)->HAINTMSK | (1U << number));
    nx_arch_irq_restore(key);
    uint8_t* bytes =
        extent == 0U ? (uint8_t*)&state->zero_word : request->bytes;
    HAL_StatusTypeDef submitted;
    if (channel->pipe.transfer == NX_USB_HOST_CONTROL && !request->setup) {
        /* The pinned HAL forces DATA1 for every unsplit control IN and for
         * zero OUT. Nexus submits one packet with an explicit PID, including
         * DATA0 after the first packet. Keep official LL as the wire owner. */
        USB_OTG_HCTypeDef* hc = &state->hal.hc[number];
        hc->ep_is_in = in ? 1U : 0U;
        hc->ep_type = EP_TYPE_CTRL;
        hc->data_pid = request->data1 ? HC_PID_DATA1 : HC_PID_DATA0;
        hc->do_ping = 0U;
        hc->xfer_buff = bytes;
        hc->xfer_len = (uint32_t)request->length;
        hc->xfer_count = 0U;
        hc->urb_state = URB_IDLE;
        hc->state = HC_IDLE;
        hc->ch_num = number;
        submitted = USB_HC_StartXfer(state->hal.Instance, hc,
                                     (uint8_t)state->hal.Init.dma_enable);
    } else {
        submitted = HAL_HCD_HC_SubmitRequest(
            &state->hal, number, in ? 1U : 0U, (uint8_t)channel->pipe.transfer,
            (uint8_t)(request->setup ? 0U : 1U), bytes,
            (uint16_t)request->length, 0U);
    }
    if (submitted != HAL_OK) {
        begin_drain(state, number, NX_ERROR_IO);
        state->snapshot.closing = true;
    }
    /* Submission failure after ACCEPTED is an owned drain, never REJECTED. */
    return NX_SUCCESS;
}

/** \brief Cancel uses global drain because PIO FIFOs are shared by channels. */
static nx_result_t cancel(void* context, uint8_t number,
                          nx_usb_host_request_t* request) {
    nx_stm32_usb_host_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || state->config == NULL ||
        number >= state->config->channel_count || request == NULL ||
        state->config->channels[number].request != request) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t key = nx_arch_irq_save();
    begin_drain(state, number, NX_ERROR_CANCELLED);
    state->snapshot.closing = true;
    (void)HAL_HCD_HC_Halt(&state->hal, number);
    state->config->channels[number].halt_requested = true;
    nx_arch_irq_restore(key);
    return NX_SUCCESS;
}

/** \brief Quarantine keeps the accepted storage until explicit late progress.
 */
static void quarantine(nx_stm32_usb_host_channel_t* channel) {
    if (channel->request != NULL &&
        nx_request_state(&channel->request->base) == NX_REQUEST_DRAINING &&
        nx_deadline_expired(channel->drain_deadline, nx_time_now_us())) {
        (void)nx_request_transition(&channel->request->base,
                                    NX_REQUEST_QUARANTINED);
    }
}

/** \brief Detach every retained address before final request publication. */
static bool settle(nx_stm32_usb_host_t* state, uint8_t number) {
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    nx_usb_host_request_t* request = channel->request;
    size_t transferred = channel->transferred;
    nx_result_t terminal = channel->terminal;
    if (transferred > request->length) {
        transferred = request->length;
        terminal = NX_ERROR_OVERFLOW;
    }
    NX_USB_HOST_WRITE(channel_registers(state, number)->HCDMA, 0U);
    nx_arch_dsb();
    if (channel_registers(state, number)->HCDMA != 0U) {
        return false;
    }
    state->hal.hc[number].xfer_buff = NULL;
    state->hal.hc[number].xfer_len = 0U;
    channel->request = NULL;
    channel->complete = channel->halt_requested = false;
    nx_arch_dsb();
    nx_request_settle(&request->base, terminal, transferred);
    return true;
}

/** \brief Snapshot requires no active channel, DMA or pending RX references. */
static bool channel_idle(nx_stm32_usb_host_t* state, uint8_t number) {
    USB_OTG_HostChannelTypeDef* registers = channel_registers(state, number);
    if ((registers->HCCHAR & USB_OTG_HCCHAR_CHENA) != 0U ||
        (state->hal.Instance->GINTSTS & USB_OTG_GINTSTS_RXFLVL) != 0U) {
        return false;
    }
    const nx_stm32_usb_host_channel_t* channel =
        &state->config->channels[number];
    if ((channel->pipe.endpoint_address & 0x80U) == 0U) {
        const bool periodic = channel->pipe.transfer == NX_USB_HOST_INTERRUPT ||
                              channel->pipe.transfer == NX_USB_HOST_ISOCHRONOUS;
        uint32_t available = periodic ? host_registers(state)->HPTXSTS
                                      : state->hal.Instance->HNPTXSTS;
        uint16_t words = periodic ? state->config->periodic_fifo_words
                                  : state->config->nonperiodic_fifo_words;
        if ((available & 0xFFFFU) != words) {
            return false;
        }
    }
    uint32_t remaining = registers->HCTSIZ;
    nx_arch_dsb();
    return registers->HCTSIZ == remaining &&
           (state->hal.Instance->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) != 0U;
}

static nx_result_t stop(void* context);

/** \brief Service owns deadline decisions and completion, not the HAL IRQ. */
static nx_result_t service(void* context) {
    nx_stm32_usb_host_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || state->config == NULL) {
        return NX_ERROR_STATE;
    }
    if (state->snapshot.closing) {
        return stop(state);
    }
    bool busy = false;
    nx_arch_irq_state_t key = nx_arch_irq_save();
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        nx_stm32_usb_host_channel_t* channel = &state->config->channels[i];
        if (channel->request == NULL) {
            continue;
        }
        if (nx_deadline_expired(channel->request->base.deadline,
                                nx_time_now_us()) &&
            !channel->complete) {
            begin_drain(state, i, NX_ERROR_TIMEOUT);
            state->snapshot.closing = true;
            busy = true;
            continue;
        }
        if (!channel->complete) {
            busy = true;
            continue;
        }
        begin_drain(state, i, channel->terminal);
        if (!channel_idle(state, i)) {
            if (!channel->halt_requested) {
                (void)HAL_HCD_HC_Halt(&state->hal, i);
                channel->halt_requested = true;
            }
            quarantine(channel);
            busy = true;
            continue;
        }
        USB_OTG_HostChannelTypeDef* registers = channel_registers(state, i);
        NX_USB_HOST_WRITE(registers->HCINTMSK, 0U);
        NX_USB_HOST_WRITE(host_registers(state)->HAINTMSK,
                          host_registers(state)->HAINTMSK & ~(1U << i));
        if (registers->HCINTMSK != 0U ||
            (host_registers(state)->HAINTMSK & (1U << i)) != 0U) {
            busy = true;
            continue;
        }
        if (!settle(state, i)) {
            quarantine(channel);
            busy = true;
        }
    }
    nx_arch_irq_restore(key);
    return busy ? NX_ERROR_BUSY : NX_SUCCESS;
}

/** \brief Read facts into a disjoint snapshot without retaining output. */
static nx_result_t snapshot(void* context, nx_usb_host_state_t* output) {
    nx_stm32_usb_host_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || output == NULL ||
        overlap(output, sizeof(*output), state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t key = nx_arch_irq_save();
    nx_usb_host_state_t value = state->snapshot;
    /* A service-driven disconnect may have fully released the core. The
     * protocol owner still needs its final cold facts, with no gated MMIO. */
    if (state->config != NULL && state->snapshot.started) {
        value.frame = host_registers(state)->HFNUM & 0xFFFFU;
        uint32_t speed = (*port_register(state) >> 17U) & 3U;
        value.speed = speed == 0U   ? NX_USB_HOST_HIGH_SPEED
                      : speed == 1U ? NX_USB_HOST_FULL_SPEED
                                    : NX_USB_HOST_LOW_SPEED;
    }
    nx_arch_irq_restore(key);
    *output = value;
    return NX_SUCCESS;
}

/** \brief Port signaling cannot overlap a borrowed channel transaction. */
static bool loans(const nx_stm32_usb_host_t* state) {
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        if (state->config->channels[i].request != NULL) {
            return true;
        }
    }
    return false;
}

/** \brief Exclude HPRT W1C/change and write-one-disable bits from RMW. */
static uint32_t port_control(uint32_t value) {
    return value & ~(uint32_t)(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
                               USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
}

/** \brief An invalid clock cannot authorize a short pulse or release effects.
 */
static _Noreturn void signal_clock_failed(nx_stm32_usb_host_t* state) {
    state->snapshot.closing = true;
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_STATE, __FILE__, __LINE__);
    for (;;) {
        NX_USB_HOST_PARK();
    }
}

/** \brief A failed signal withdrawal remains part of the existing clock lease.
 * \note This is reached only after the full minimum hold, or by a later stop
 *       retry. Invalid-clock holds fail-stop before either path can release.
 */
static bool withdraw_signal(nx_stm32_usb_host_t* state) {
    uint8_t retained =
        state->clock_receipt & (HOST_RESET_RETAINED | HOST_RESUME_RETAINED);
    if (retained == 0U)
        return true;
    uint32_t mask =
        (retained & HOST_RESET_RETAINED) != 0U ? USB_OTG_HPRT_PRST : 0U;
    if ((retained & HOST_RESUME_RETAINED) != 0U)
        mask |= USB_OTG_HPRT_PRES;
    volatile uint32_t* port = port_register(state);
    NX_USB_HOST_WRITE(*port, port_control(*port) & ~mask);
    nx_arch_dsb();
    uint32_t actual = *port;
    if ((actual & mask) != 0U)
        return false;
    state->clock_receipt &=
        (uint8_t) ~(HOST_RESET_RETAINED | HOST_RESUME_RETAINED);
    if ((retained & HOST_RESUME_RETAINED) != 0U)
        state->snapshot.suspended = (actual & USB_OTG_HPRT_PSUSP) != 0U;
    return true;
}

/** \brief Once asserted, the minimum electrical hold outlives cancellation.
 * \note The deadline admits the operation and decides its eventual result;
 *       preemption cannot make it permission to shorten reset or resume.
 */
static nx_result_t signal_port(nx_stm32_usb_host_t* state,
                               nx_time_us_t deadline, uint32_t duration,
                               bool resume) {
    nx_time_us_t observed = nx_time_now_us();
    nx_time_us_t end;
    /* An integer microsecond sample hides the fractional phase at assertion.
     * One full tick of guard prevents a nearly-one-tick-short physical hold. */
    const uint32_t hold = duration + 1U;
    if (nx_deadline_finite_after(observed, hold, &end) != NX_SUCCESS ||
        end >= deadline)
        return NX_ERROR_TIMEOUT;
    volatile uint32_t* port = port_register(state);
    uint32_t bit = resume ? USB_OTG_HPRT_PRES : USB_OTG_HPRT_PRST;
    uint32_t control = port_control(*port);
    if (resume)
        control &= ~(uint32_t)USB_OTG_HPRT_PSUSP;
    state->clock_receipt |= resume ? HOST_RESUME_RETAINED : HOST_RESET_RETAINED;
    NX_USB_HOST_WRITE(*port, control | bit);
    nx_arch_dsb();
    bool asserted =
        (*port & bit) != 0U && (!resume || (*port & USB_OTG_HPRT_PSUSP) == 0U);
    /* Sampling after the write also covers preemption between admission and
     * assertion. Extra hold is safe; an old admission timestamp is not proof
     * that the physical signal has already lasted the required interval. */
    nx_time_us_t previous = nx_time_now_us();
    if (previous < observed ||
        nx_deadline_finite_after(previous, hold, &end) != NX_SUCCESS)
        signal_clock_failed(state);
    const uint32_t budget = NX_USB_HOST_AHB_HZ / 1000U + 1U;
    uint32_t remaining = budget;
    for (;;) {
        nx_time_us_t now = nx_time_now_us();
        if (now == NX_DEADLINE_NEVER || now < previous ||
            !NX_USB_HOST_CLOCK_VALID())
            signal_clock_failed(state);
        if (nx_deadline_expired(end, now)) {
            previous = now;
            break;
        }
        if (now == previous) {
            if (remaining == 0U)
                signal_clock_failed(state);
            --remaining;
        } else {
            previous = now;
            remaining = budget;
        }
    }
    if (!withdraw_signal(state) || !asserted ||
        (resume && state->snapshot.suspended)) {
        state->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    nx_time_us_t finished = nx_time_now_us();
    if (finished == NX_DEADLINE_NEVER || finished < previous)
        signal_clock_failed(state);
    return nx_deadline_expired(deadline, finished) ? NX_ERROR_TIMEOUT
                                                   : NX_SUCCESS;
}

/** \brief The HAL's fixed HAL_Delay reset is replaced for deadline semantics.
 */
static nx_result_t reset(void* context, nx_time_us_t deadline) {
    nx_stm32_usb_host_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (loans(state)) {
        return NX_ERROR_BUSY;
    }
    if (!state->snapshot.connected) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    return signal_port(state, deadline, 10000U, false);
}

/** \brief Suspend keeps the Core timebase running and owns resume signaling. */
static nx_result_t suspend(void* context, bool suspended,
                           nx_time_us_t deadline) {
    nx_stm32_usb_host_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || state->config == NULL || !state->snapshot.started ||
        state->snapshot.closing || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (loans(state)) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (!suspended)
        return signal_port(state, deadline, 20000U, true);
    volatile uint32_t* port = port_register(state);
    NX_USB_HOST_WRITE(*port, port_control(*port) | USB_OTG_HPRT_PSUSP);
    nx_arch_dsb();
    if (((*port & USB_OTG_HPRT_PSUSP) != 0U) != suspended) {
        return NX_ERROR_IO;
    }
    state->snapshot.suspended = suspended;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

/** \brief Stop verifies actual withdrawal independently of HAL return values.
 */
static nx_result_t stop(void* context) {
    nx_stm32_usb_host_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (state->config == NULL) {
        return NX_SUCCESS;
    }
    const bool fs = state->config->phy == NX_STM32_USB_FS_INTERNAL;
    uint32_t mask = fs ? RCC_AHB2ENR_OTGFSEN : RCC_AHB1ENR_OTGHSEN;
    if ((state->clock_receipt & 2U) != 0U) {
        mask |= RCC_AHB1ENR_OTGHSULPIEN;
    }
    volatile uint32_t* gate = fs ? &RCC->AHB2ENR : &RCC->AHB1ENR;
    /* A rejected gate readback occurred before HAL or core MMIO admission. */
    if (state->hal.pData == NULL) {
        NX_USB_HOST_WRITE(*gate, *gate & ~mask);
        nx_arch_dsb();
        if ((*gate & mask) != 0U) {
            return NX_ERROR_IO;
        }
        state->clock_receipt = 0U;
        state->snapshot = (nx_usb_host_state_t){0};
        state->config = NULL;
        return NX_SUCCESS;
    }
    state->snapshot.closing = true;
    if (!withdraw_signal(state))
        return NX_ERROR_BUSY;
    bool busy = false;
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        nx_stm32_usb_host_channel_t* channel = &state->config->channels[i];
        begin_drain(state, i, NX_ERROR_STATE);
        if (channel->request != NULL || (channel_registers(state, i)->HCCHAR &
                                         USB_OTG_HCCHAR_CHENA) != 0U) {
            (void)HAL_HCD_HC_Halt(&state->hal, i);
            quarantine(channel);
        }
    }
    (void)HAL_HCD_Stop(&state->hal);
    USB_OTG_GlobalTypeDef* core = state->hal.Instance;
    NX_USB_HOST_WRITE(core->GINTMSK, 0U);
    NX_USB_HOST_WRITE(core->GAHBCFG,
                      core->GAHBCFG & ~(uint32_t)USB_OTG_GAHBCFG_GINT);
    NX_USB_HOST_IRQ_DISABLE(vector(state));
    nx_arch_dsb();
    if (NX_USB_HOST_IRQ_ENABLED(vector(state)) ||
        NX_USB_HOST_IRQ_ACTIVE(vector(state)) || state->publishers != 0U ||
        core->GINTMSK != 0U || (core->GAHBCFG & USB_OTG_GAHBCFG_GINT) != 0U) {
        return NX_ERROR_BUSY;
    }
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        if ((channel_registers(state, i)->HCCHAR & USB_OTG_HCCHAR_CHENA) !=
            0U) {
            busy = true;
        }
    }
    if (busy || (core->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U) {
        return NX_ERROR_BUSY;
    }
    if (USB_FlushRxFifo(core) != HAL_OK ||
        USB_FlushTxFifo(core, 0x10U) != HAL_OK ||
        (core->GRSTCTL & (USB_OTG_GRSTCTL_RXFFLSH | USB_OTG_GRSTCTL_TXFFLSH)) !=
            0U ||
        (core->GINTSTS & USB_OTG_GINTSTS_RXFLVL) != 0U) {
        return NX_ERROR_BUSY;
    }
    NX_USB_HOST_WRITE(core->GAHBCFG,
                      core->GAHBCFG & ~(uint32_t)USB_OTG_GAHBCFG_DMAEN);
    if ((core->GAHBCFG & USB_OTG_GAHBCFG_DMAEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        USB_OTG_HostChannelTypeDef* registers = channel_registers(state, i);
        NX_USB_HOST_WRITE(registers->HCINTMSK, 0U);
        NX_USB_HOST_WRITE(registers->HCDMA, 0U);
        if (registers->HCINTMSK != 0U || registers->HCDMA != 0U) {
            return NX_ERROR_BUSY;
        }
    }
    NX_USB_HOST_WRITE(host_registers(state)->HAINTMSK, 0U);
    nx_arch_dsb();
    if (host_registers(state)->HAINTMSK != 0U) {
        return NX_ERROR_BUSY;
    }
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        if (state->config->channels[i].request != NULL) {
            if (!settle(state, i)) {
                return NX_ERROR_BUSY;
            }
        }
        state->hal.hc[i].xfer_buff = NULL;
        state->config->channels[i].configured = false;
    }
    state->hal.pData = NULL;
    NX_USB_HOST_WRITE(*gate, *gate & ~mask);
    nx_arch_dsb();
    if ((*gate & mask) != 0U) {
        return NX_ERROR_IO;
    }
    state->clock_receipt = 0U;
    state->snapshot = (nx_usb_host_state_t){0};
    state->config = NULL;
    return NX_SUCCESS;
}

const nx_usb_host_ops_t nx_stm32_usb_host_ops = {
    open_channel, close_channel, submit,  cancel, service,
    snapshot,     reset,         suspend, stop};
