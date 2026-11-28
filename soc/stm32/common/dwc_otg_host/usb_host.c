/**
 * \file            usb_host.c
 * \brief           Shared STM32 DWC official HCD transport and packet drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#if defined(NX_STM32F1_USB_HOST)
#include "drivers/usb/connectivity_host_transport.h"
#ifndef NEXUS_CLOCK_PLL48_HZ
#include "nexus_config.h"
#endif
#else
#include "drivers/usb/stm32f407_usb_host.h"
#include "system/stm32f407_clock.h"
#endif
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
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
#ifndef NX_USB_HOST_IRQ_PENDING
#define NX_USB_HOST_IRQ_PENDING(irq) (NVIC_GetPendingIRQ(irq) != 0U)
#endif
#ifndef NX_USB_HOST_CLOCK_VALID
#if defined(NX_STM32F1_USB_HOST)
#define NX_USB_HOST_CLOCK_VALID() nx_stm32f1_usb_host_clock_ready()
#else
#define NX_USB_HOST_CLOCK_VALID() nx_stm32_clock_profile_matches(RCC, NULL)
#endif
#endif
#ifndef NX_USB_HOST_AHB_HZ
#define NX_USB_HOST_AHB_HZ NEXUS_CORE_HZ
#endif
#ifndef NX_USB_HOST_PARK
#define NX_USB_HOST_PARK() nx_arch_dsb()
#endif

#define HOST_RESET_RETAINED  (1U << 2U)
#define HOST_RESUME_RETAINED (1U << 3U)
#define HOST_TRANSFER_ERRORS                                                   \
    (USB_OTG_HCINT_AHBERR | USB_OTG_HCINT_FRMOR | USB_OTG_HCINT_TXERR |        \
     USB_OTG_HCINT_BBERR | USB_OTG_HCINT_DTERR | USB_OTG_HCINT_STALL)

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

/** \brief Register ownership depends on the actual core and family gate. */
static IRQn_Type host_vector(nx_stm32_usb_phy_t phy) {
#if defined(NX_STM32F1_USB_HOST)
    (void)phy;
    return OTG_FS_IRQn;
#else
    return phy == NX_STM32_USB_FS_INTERNAL ? OTG_FS_IRQn : OTG_HS_IRQn;
#endif
}

static IRQn_Type vector(const nx_stm32_usb_host_t* state) {
    return host_vector(state->config->phy);
}

typedef struct {
    volatile uint32_t* gate;
    uint32_t mask;
    USB_OTG_GlobalTypeDef* core;
} core_selection_t;

static core_selection_t select_core(nx_stm32_usb_phy_t phy, bool ulpi) {
#if defined(NX_STM32F1_USB_HOST)
    (void)phy;
    (void)ulpi;
    return (core_selection_t){&RCC->AHBENR, RCC_AHBENR_OTGFSEN, USB_OTG_FS};
#else
    if (phy == NX_STM32_USB_FS_INTERNAL)
        return (core_selection_t){&RCC->AHB2ENR, RCC_AHB2ENR_OTGFSEN,
                                  USB_OTG_FS};
    return (core_selection_t){&RCC->AHB1ENR,
                              RCC_AHB1ENR_OTGHSEN |
                                  (ulpi ? RCC_AHB1ENR_OTGHSULPIEN : 0U),
                              USB_OTG_HS};
#endif
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
#if defined(NX_STM32F1_USB_HOST)
    if (pipe->transfer != NX_USB_HOST_CONTROL ||
        pipe->speed != NX_USB_HOST_FULL_SPEED ||
        (pipe->endpoint_address & 0x0FU) != 0U)
        return false;
#endif
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
        const bool handshake =
            channel->pipe.transfer != NX_USB_HOST_ISOCHRONOUS &&
            (result == URB_NOTREADY || result == URB_NYET);
        channel->terminal = result == URB_DONE ? NX_SUCCESS
                            : handshake        ? NX_ERROR_NACK
                                               : NX_ERROR_IO;
        /* HCD updates xfer_count only for IN. OUT DONE proves this one
         * admitted packet, never an automatically retried packet queue. */
        channel->transferred =
            result == URB_DONE && (channel->pipe.endpoint_address & 0x80U) == 0U
                ? (uint32_t)channel->request->length
                : hal->hc[number].xfer_count;
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
#if defined(NX_STM32F1_USB_HOST)
    if (config->phy != NX_STM32_USB_FS_INTERNAL ||
        config->channel_count != 1U || config->rx_fifo_words < 64U ||
        config->nonperiodic_fifo_words < 16U ||
        config->irq_policy.external_irq_count != (uint16_t)OTG_FS_IRQn + 1U)
        return false;
#endif
    if (config->pipe_channels != NULL) {
        const size_t bytes = config->channel_count;
        if (overlap(config->pipe_channels, bytes, state, sizeof(*state)) ||
            overlap(config->pipe_channels, bytes, config, sizeof(*config)) ||
            overlap(config->pipe_channels, bytes, config->channels,
                    config->channel_count * sizeof(*config->channels)))
            return false;
        uint32_t selected = 0U;
        for (uint8_t pipe = 0U; pipe < config->channel_count; ++pipe) {
            const uint8_t channel = config->pipe_channels[pipe];
            if (channel >= config->channel_count ||
                (selected & (1U << channel)) != 0U)
                return false;
            selected |= 1U << channel;
        }
    }
    const nx_irq_source_t irq = {host_vector(config->phy), config->priority,
                                 3U};
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
#if defined(NX_STM32F1_USB_HOST)
    const uint32_t usb_clock = NEXUS_CLOCK_PLL48_HZ;
#else
    const uint32_t usb_clock = NEXUS_PLL48_HZ;
#endif
    if (!NX_USB_HOST_CLOCK_VALID() ||
        (config->phy != NX_STM32_USB_HS_ULPI && usb_clock != 48000000U)) {
        return NX_ERROR_STATE;
    }
    const IRQn_Type irq = host_vector(config->phy);
    const core_selection_t selected =
        select_core(config->phy, config->phy == NX_STM32_USB_HS_ULPI);
    uint32_t mask = selected.mask;
    volatile uint32_t* gate = selected.gate;
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
    state->hal.Instance = selected.core;
    state->hal.Init.Host_channels = config->channel_count;
#if defined(NX_STM32F1_USB_HOST)
    state->hal.Init.speed = HCD_SPEED_FULL;
#else
    state->hal.Init.speed =
        (uint8_t)(config->phy == NX_STM32_USB_HS_ULPI ? HCD_SPEED_HIGH
                                                      : HCD_SPEED_FULL);
#endif
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
#if defined(NX_STM32F1_USB_HOST)
    NX_USB_HOST_WRITE(core->GCCFG,
                      (core->GCCFG & ~(uint32_t)USB_OTG_GCCFG_VBUSBSEN) |
                          USB_OTG_GCCFG_VBUSASEN);
    if ((core->GCCFG & USB_OTG_GCCFG_VBUSASEN) == 0U ||
        (core->GOTGCTL & USB_OTG_GOTGCTL_ASVLD) == 0U) {
        return NX_ERROR_STATE;
    }
#endif
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

/** \brief ISO errors end this opportunity before HCD can retry DMA OUT.
 * The pinned HCD retries XACTERR even for ISO and loses FRMOR's terminal
 * result. Isolate only this channel, capture its error, then use HCD halt.
 */
static bool iso_errors(nx_stm32_usb_host_t* state) {
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        nx_stm32_usb_host_channel_t* channel = &state->config->channels[i];
        if (channel->request == NULL ||
            channel->pipe.transfer != NX_USB_HOST_ISOCHRONOUS ||
            nx_request_state(&channel->request->base) != NX_REQUEST_ACTIVE)
            continue;
        USB_OTG_HostChannelTypeDef* registers = channel_registers(state, i);
        const uint32_t pending =
            registers->HCINT & registers->HCINTMSK & HOST_TRANSFER_ERRORS;
        if (pending == 0U)
            continue;
        begin_drain(state, i, NX_ERROR_IO);
        channel->transferred = 0U;
        channel->complete = true;
        NX_USB_HOST_WRITE(registers->HCINTMSK, 0U);
        NX_USB_HOST_WRITE(registers->HCINT, pending);
        nx_arch_dsb();
        if (registers->HCINTMSK != 0U || (registers->HCINT & pending) != 0U) {
            state->snapshot.closing = true;
            NX_USB_HOST_IRQ_DISABLE(vector(state));
            nx_arch_dsb();
            return false;
        }
        (void)HAL_HCD_HC_Halt(&state->hal, i);
        channel->halt_requested = true;
    }
    return true;
}
/** \brief ES022 requires evidence that this F1 channel has started. */
static bool halt_started(nx_stm32_usb_host_t* state, uint8_t number) {
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
#if defined(NX_STM32F1_USB_HOST)
    if ((channel_registers(state, number)->HCCHAR & USB_OTG_HCCHAR_CHENA) == 0U)
        return true;
    if (!channel->wire_activity)
        return false;
#endif
    if (!channel->halt_requested) {
        (void)HAL_HCD_HC_Halt(&state->hal, number);
        channel->halt_requested = true;
    }
    return true;
}

#if defined(NX_STM32F1_USB_HOST)
/** \brief Preserve one attempt; official HCD otherwise rearms NAK/error IN. */
static void observe_fs_bus(nx_stm32_usb_host_t* state) {
    USB_OTG_HostChannelTypeDef* registers = channel_registers(state, 0U);
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[0];
    if (channel->request == NULL)
        return;
    const uint32_t pending = registers->HCINT & registers->HCINTMSK;
    if (pending & (HOST_TRANSFER_ERRORS | USB_OTG_HCINT_ACK |
                   USB_OTG_HCINT_NAK | USB_OTG_HCINT_XFRC))
        channel->wire_activity = true;
    const uint32_t errors =
        pending & (USB_OTG_HCINT_TXERR | USB_OTG_HCINT_DTERR);
    const uint32_t retry = errors | (pending & USB_OTG_HCINT_NAK);
    if (retry == 0U)
        return;

    /* These wire facts establish start before HAL can halt and reenable. */
    channel->complete = true;
    begin_drain(state, 0U, errors != 0U ? NX_ERROR_IO : NX_ERROR_NACK);
    NX_USB_HOST_WRITE(registers->HCINT, retry);
    state->hal.hc[0].urb_state = URB_IDLE;
    state->hal.hc[0].state = HC_IDLE;
    (void)halt_started(state, 0U);
}
#endif

/** \brief IRQ dispatch retains its publisher until all HAL callbacks return. */
void nx_stm32_usb_host_irq(nx_stm32_usb_host_t* state) {
    if (state == NULL || state->config == NULL || !state->snapshot.started) {
        return;
    }
    ++state->publishers;
#if defined(NX_STM32F1_USB_HOST)
    observe_fs_bus(state);
#endif
    if (iso_errors(state))
        HAL_HCD_IRQHandler(&state->hal);
    --state->publishers;
}

/** \brief All task methods reject absent or closing controller ownership. */
static nx_result_t ready(const nx_stm32_usb_host_t* state) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
#if defined(NX_STM32F1_USB_HOST)
    if (state != NULL && state->config != NULL &&
        !nx_stm32f1_usb_host_power_ready())
        return NX_ERROR_IO;
#endif
    return state == NULL || state->config == NULL || !state->snapshot.started ||
                   state->snapshot.closing || state->snapshot.suspended
               ? NX_ERROR_STATE
               : NX_SUCCESS;
}

/** \brief Boundary-only mapping; IRQ and retained requests use physical slots.
 */
static uint8_t physical_pipe(const nx_stm32_usb_host_t* state, uint8_t pipe) {
    return state->config->pipe_channels == NULL
               ? pipe
               : state->config->pipe_channels[pipe];
}

/** \brief The selected HCD accesses whole words and DMA owns a zero-OUT word.
 */
static size_t packet_capacity(const nx_stm32_usb_host_t* state,
                              const nx_usb_host_pipe_t* pipe, size_t length) {
    size_t capacity =
        (pipe->endpoint_address & 0x80U) != 0U ? pipe->max_packet : length;
    capacity = (capacity + 3U) & ~(size_t)3U;
    return state->config->dma && capacity == 0U ? 4U : capacity;
}

/** \brief Geometry output cannot overwrite live transport or packet storage. */
static nx_result_t requirements(const void* context,
                                nx_usb_host_pipe_id_t pipe_id, size_t length,
                                nx_usb_host_storage_requirements_t* output) {
    const nx_stm32_usb_host_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS)
        return status;
    const nx_stm32_usb_host_config_t* config = state->config;
    const size_t bytes = sizeof(*output);
    if (pipe_id >= config->channel_count || output == NULL ||
        (uintptr_t)output % _Alignof(nx_usb_host_storage_requirements_t) !=
            0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        overlap(output, bytes, state, sizeof(*state)) ||
        overlap(output, bytes, config, sizeof(*config)) ||
        overlap(output, bytes, config->channels,
                config->channel_count * sizeof(*config->channels)) ||
        overlap(output, bytes, config->regions,
                config->region_count * sizeof(*config->regions)) ||
        overlap(output, bytes, state->hal.Instance, 0x20000U) ||
        overlap(output, bytes, RCC, sizeof(*RCC)) ||
        (config->pipe_channels != NULL &&
         overlap(output, bytes, config->pipe_channels, config->channel_count)))
        return NX_ERROR_INVALID;
    const nx_stm32_usb_host_channel_t* channel =
        &config->channels[physical_pipe(state, pipe_id)];
    if (!channel->configured)
        return NX_ERROR_STATE;
    if (length > channel->pipe.max_packet)
        return NX_ERROR_INVALID;
    for (uint8_t i = 0U; i < config->channel_count; ++i) {
        const nx_usb_host_request_t* request = config->channels[i].request;
        if (request != NULL &&
            (overlap(output, bytes, request, sizeof(*request)) ||
             overlap(output, bytes, request->bytes, request->capacity)))
            return NX_ERROR_INVALID;
    }
    *output = (nx_usb_host_storage_requirements_t){
        packet_capacity(state, &channel->pipe, length), config->dma ? 4U : 1U};
    return NX_SUCCESS;
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
    number = physical_pipe(state, number);
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
    number = physical_pipe(state, number);
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
    number = physical_pipe(state, number);
    nx_stm32_usb_host_channel_t* channel = &state->config->channels[number];
    const bool in = (channel->pipe.endpoint_address & 0x80U) != 0U;
    if (!channel->configured || request->length > channel->pipe.max_packet ||
        (channel->pipe.transfer == NX_USB_HOST_ISOCHRONOUS && request->data1)) {
        return NX_ERROR_INVALID;
    }
    size_t extent = packet_capacity(state, &channel->pipe, request->length);
    if (request->capacity < extent ||
        (extent != 0U && request->bytes == NULL) ||
        (request->setup &&
         (in || channel->pipe.transfer != NX_USB_HOST_CONTROL ||
          request->length != 8U)) ||
        overlap(request, sizeof(*request), state, sizeof(*state)) ||
        overlap(request, sizeof(*request), state->config,
                sizeof(*state->config)) ||
        overlap(request, sizeof(*request), state->config->channels,
                state->config->channel_count * sizeof(*channel)) ||
        overlap(request->bytes, request->capacity, request, sizeof(*request)) ||
        overlap(request->bytes, request->capacity, state, sizeof(*state)) ||
        overlap(request->bytes, request->capacity, state->config,
                sizeof(*state->config)) ||
        overlap(request->bytes, request->capacity, state->config->channels,
                state->config->channel_count * sizeof(*channel)) ||
        (state->config->pipe_channels != NULL &&
         (overlap(request, sizeof(*request), state->config->pipe_channels,
                  state->config->channel_count) ||
          overlap(request->bytes, request->capacity,
                  state->config->pipe_channels,
                  state->config->channel_count)))) {
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
    if (!state->config->dma && !in) {
        /* The pinned PIO writer immediately copies a complete packet. An
         * empty queue or partial FIFO budget cannot be ACCEPTED safely. */
        const bool periodic = channel->pipe.transfer == NX_USB_HOST_INTERRUPT ||
                              channel->pipe.transfer == NX_USB_HOST_ISOCHRONOUS;
        const uint32_t available = periodic ? host_registers(state)->HPTXSTS
                                            : state->hal.Instance->HNPTXSTS;
        if ((available & 0xFFFFU) < extent / 4U ||
            ((available >> 16U) & 0xFFU) == 0U) {
            nx_arch_irq_restore(key);
            return NX_ERROR_BUSY;
        }
    }
    status = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (status != NX_SUCCESS) {
        nx_arch_irq_restore(key);
        return status;
    }
    channel->request = request;
    channel->transferred = 0U;
    channel->terminal = NX_SUCCESS;
    channel->complete = channel->halt_requested = false;
#if defined(NX_STM32F1_USB_HOST)
    channel->wire_activity = false;
#endif
    state->hal.hc[number].toggle_in = request->data1 ? 1U : 0U;
    state->hal.hc[number].toggle_out = request->data1 ? 1U : 0U;
    NX_USB_HOST_WRITE(channel_registers(state, number)->HCINTMSK,
                      channel->interrupt_mask);
    NX_USB_HOST_WRITE(host_registers(state)->HAINTMSK,
                      host_registers(state)->HAINTMSK | (1U << number));
    nx_arch_irq_restore(key);
    uint8_t* bytes =
        extent == 0U ? (uint8_t*)&state->zero_word : request->bytes;
    /* IN already reserves one MPS. HCD drains that complete admitted extent;
     * request.length remains the caller's overflow/reporting boundary. */
    const uint16_t wire_length =
        in ? channel->pipe.max_packet : (uint16_t)request->length;
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
        hc->xfer_len = wire_length;
        hc->xfer_count = 0U;
        hc->urb_state = URB_IDLE;
        hc->state = HC_IDLE;
        hc->ch_num = number;
#if defined(NX_STM32F1_USB_HOST)
        submitted = USB_HC_StartXfer(state->hal.Instance, hc);
#else
        submitted = USB_HC_StartXfer(state->hal.Instance, hc,
                                     (uint8_t)state->hal.Init.dma_enable);
#endif
    } else {
        submitted = HAL_HCD_HC_SubmitRequest(
            &state->hal, number, in ? 1U : 0U, (uint8_t)channel->pipe.transfer,
            (uint8_t)(request->setup ? 0U : 1U), bytes, wire_length, 0U);
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
        number >= state->config->channel_count || request == NULL) {
        return NX_ERROR_INVALID;
    }
    number = physical_pipe(state, number);
    if (state->config->channels[number].request != request) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t key = nx_arch_irq_save();
    begin_drain(state, number, NX_ERROR_CANCELLED);
    state->snapshot.closing = true;
#if defined(NX_STM32F1_USB_HOST)
    (void)halt_started(state, number);
#else
    (void)HAL_HCD_HC_Halt(&state->hal, number);
    state->config->channels[number].halt_requested = true;
#endif
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
    USB_OTG_HostChannelTypeDef* registers = channel_registers(state, number);
#if !defined(NX_STM32F1_USB_HOST)
    NX_USB_HOST_WRITE(registers->HCDMA, 0U);
    nx_arch_dsb();
    if (registers->HCDMA != 0U) {
        return false;
    }
#endif
    const uint32_t pending = registers->HCINT & 0x7FFU;
    if (pending != 0U) {
        /* Masked late facts still belong to this incarnation. A real error
         * invalidates the old DONE count, including any overflow prefix. */
        if ((pending & HOST_TRANSFER_ERRORS) != 0U) {
            channel->terminal = NX_ERROR_IO;
            channel->transferred = 0U;
        }
        NX_USB_HOST_WRITE(registers->HCINT, pending);
        nx_arch_dsb();
        if ((registers->HCINT & 0x7FFU) != 0U) {
            return false;
        }
    }
    size_t transferred = channel->transferred;
    nx_result_t terminal = channel->terminal;
    if (transferred > request->length) {
        transferred = request->length;
        terminal = NX_ERROR_OVERFLOW;
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
#if defined(NX_STM32F1_USB_HOST)
    if (!nx_stm32f1_usb_host_power_ready())
        state->snapshot.closing = true;
#endif
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
            (void)halt_started(state, i);
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
#if defined(NX_STM32F1_USB_HOST)
    (void)context;
    (void)suspended;
    (void)deadline;
    return NX_ERROR_UNSUPPORTED;
#else
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
#endif
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
    const core_selection_t selected =
        select_core(state->config->phy, (state->clock_receipt & 2U) != 0U);
    uint32_t mask = selected.mask;
    volatile uint32_t* gate = selected.gate;
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
#if defined(NX_STM32F1_USB_HOST)
            busy = !halt_started(state, i);
#else
            (void)HAL_HCD_HC_Halt(&state->hal, i);
#endif
            quarantine(channel);
        }
    }
#if defined(NX_STM32F1_USB_HOST)
    /* Keep the publisher live while awaiting a genuine first wire event. */
    if (busy)
        return NX_ERROR_BUSY;
#else
    (void)HAL_HCD_Stop(&state->hal);
#endif
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
#if defined(NX_STM32F1_USB_HOST)
    if (NX_USB_HOST_IRQ_PENDING(vector(state)))
        return NX_ERROR_BUSY;
#endif
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
#if !defined(NX_STM32F1_USB_HOST)
    NX_USB_HOST_WRITE(core->GAHBCFG,
                      core->GAHBCFG & ~(uint32_t)USB_OTG_GAHBCFG_DMAEN);
    if ((core->GAHBCFG & USB_OTG_GAHBCFG_DMAEN) != 0U) {
        return NX_ERROR_BUSY;
    }
#endif
    for (uint8_t i = 0U; i < state->config->channel_count; ++i) {
        USB_OTG_HostChannelTypeDef* registers = channel_registers(state, i);
        NX_USB_HOST_WRITE(registers->HCINTMSK, 0U);
#if defined(NX_STM32F1_USB_HOST)
        if (registers->HCINTMSK != 0U)
            return NX_ERROR_BUSY;
#else
        NX_USB_HOST_WRITE(registers->HCDMA, 0U);
        if (registers->HCINTMSK != 0U || registers->HCDMA != 0U) {
            return NX_ERROR_BUSY;
        }
#endif
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
#if defined(NX_STM32F1_USB_HOST)
    volatile uint32_t* hprt = port_register(state);
    NX_USB_HOST_WRITE(*hprt,
                      port_control(*hprt) & ~(uint32_t)USB_OTG_HPRT_PPWR);
    if ((*hprt & USB_OTG_HPRT_PPWR) != 0U)
        return NX_ERROR_BUSY;
    const uint32_t phy = USB_OTG_GCCFG_VBUSASEN | USB_OTG_GCCFG_PWRDWN;
    NX_USB_HOST_WRITE(core->GCCFG, core->GCCFG & ~phy);
    if ((core->GCCFG & phy) != 0U)
        return NX_ERROR_BUSY;
#endif
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
    snapshot,     reset,         suspend, stop,   requirements};
