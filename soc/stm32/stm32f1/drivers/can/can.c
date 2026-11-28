/**
 * \file            can.c
 * \brief           F103xB bxCAN1 polling with task-observed completion.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_validation.h"
#include "drivers/can/instance.h"
#include "private/hardware.h"

#ifndef STM32F103xB
#error "STM32F1 CAN polling requires the reviewed F103xB profile"
#endif
#ifndef NEXUS_CLOCK_APB1_HZ
#if defined(NEXUS_CMSIS_HOST_MODEL)
#define NEXUS_CLOCK_APB1_HZ 8000000U
#else
#include "nexus_config.h"
#endif
#endif

#if defined(NX_STM32F1_CAN_NORMAL)
#include "drivers/can/normal.h"
_Static_assert(NEXUS_CLOCK_APB1_HZ == 36000000U,
               "External CAN requires APB1 36 MHz");
#define CAN_EXTERNAL 1
#define CAN_TIMING   ((12U << CAN_BTR_TS1_Pos) | (3U << CAN_BTR_TS2_Pos) | 3U)
#else
_Static_assert(NEXUS_CLOCK_APB1_HZ == 8000000U,
               "Internal CAN loopback requires APB1 8 MHz");
#define CAN_EXTERNAL 0
#define CAN_TIMING                                                             \
    (CAN_BTR_SILM | CAN_BTR_LBKM | (12U << CAN_BTR_TS1_Pos) |                  \
     (1U << CAN_BTR_TS2_Pos))
#endif
_Static_assert(USB_HP_CAN1_TX_IRQn == 19 && USB_LP_CAN1_RX0_IRQn == 20 &&
                   CAN1_RX1_IRQn == 21 && CAN1_SCE_IRQn == 22,
               "Exact F103 CAN1/USB vector ownership");

#define CAN_TIMING_FIELDS                                                      \
    (CAN_BTR_SILM | CAN_BTR_LBKM | CAN_BTR_TS1 | CAN_BTR_TS2 | CAN_BTR_SJW |   \
     CAN_BTR_BRP)
#define CAN_CONTROL                                                            \
    (CAN_MCR_INRQ | CAN_MCR_SLEEP | CAN_MCR_TXFP | CAN_MCR_RFLM |              \
     CAN_MCR_NART | CAN_MCR_AWUM | CAN_MCR_ABOM | CAN_MCR_TTCM)
#define CAN_MODE     (CAN_MCR_NART | CAN_MCR_RFLM)
#define CAN_ACK_MASK (CAN_MSR_INAK | CAN_MSR_SLAK)
#define CAN_TX_PEERS (CAN_TSR_TME1 | CAN_TSR_TME2)
#define CAN_TX_IDLE  (CAN_TSR_TME0 | CAN_TX_PEERS)
#define CAN_RX_WORK  (CAN_RF0R_FMP0 | CAN_RF0R_FOVR0)

enum {
    GATE = 1U,
    CONTROLLER = 2U,
    STARTED = 4U,
    CLOSING = 8U,
    RESETTING = 16U,
    BUS_OFF = 32U,
    MODE_POLLS = 4096U,
    DRAIN_US = 1000U,
};

static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    const uintptr_t start = (uintptr_t)pointer;
    if (pointer == NULL || start % alignment != 0U ||
        bytes > UINTPTR_MAX - start)
        return false;
    const uintptr_t end = start + bytes;
    return !(start < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(start < UINT32_C(0xF0000000) && end > UINT32_C(0xE0000000));
}

static bool overlap(const void* left, size_t left_size, const void* right,
                    size_t right_size) {
    const uintptr_t a = (uintptr_t)left;
    const uintptr_t b = (uintptr_t)right;
    return a < b + right_size && b < a + left_size;
}

static bool usb_idle(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_USBEN) == 0U &&
           (RCC->APB1RSTR & RCC_APB1RSTR_USBRST) == 0U;
}

static bool gate_on(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_CAN1EN) != 0U;
}

static bool vectors_idle(void) {
    const IRQn_Type vectors[] = {USB_HP_CAN1_TX_IRQn, USB_LP_CAN1_RX0_IRQn,
                                 CAN1_RX1_IRQn, CAN1_SCE_IRQn};
    for (size_t i = 0U; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        if (NVIC_GetEnableIRQ(vectors[i]) != 0U ||
            NVIC_GetPendingIRQ(vectors[i]) != 0U ||
            NVIC_GetActive(vectors[i]) != 0U)
            return false;
    }
    return true;
}

static bool peers_idle(void) {
    return (CAN1->TSR & (CAN_TX_PEERS | CAN_TSR_RQCP1 | CAN_TSR_RQCP2)) ==
               CAN_TX_PEERS &&
           (CAN1->RF1R & (CAN_RF1R_FMP1 | CAN_RF1R_FOVR1)) == 0U;
}

static bool cold_idle(void) {
    return peers_idle() && CAN1->IER == 0U && CAN1->FA1R == 0U &&
           (CAN1->MCR & CAN_CONTROL) == CAN_MCR_SLEEP &&
           (CAN1->TSR & (CAN_TSR_TME0 | CAN_TSR_RQCP0)) == CAN_TSR_TME0 &&
           (CAN1->RF0R & CAN_RX_WORK) == 0U;
}

static bool running(const nx_stm32f1_can_t* port) {
    return (port->effects & STARTED) != 0U && gate_on() && usb_idle() &&
           (RCC->APB1RSTR & RCC_APB1RSTR_CAN1RST) == 0U && CAN1->IER == 0U &&
           (CAN1->BTR & CAN_TIMING_FIELDS) == CAN_TIMING &&
           (CAN1->MSR & CAN_ACK_MASK) == 0U;
}

static bool wait_mode(uint32_t state) {
    for (unsigned poll = 0U; poll < MODE_POLLS; ++poll) {
        if ((CAN1->MSR & CAN_ACK_MASK) == state)
            return true;
    }
    return false;
}

nx_result_t nx_stm32f1_can_admit(const nx_stm32f1_can_t* port) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32f1_can_t)))
        return NX_ERROR_INVALID;
    if (port->effects != 0U || port->tx != NULL || gate_on() || !usb_idle() ||
        (RCC->APB1RSTR & RCC_APB1RSTR_CAN1RST) != 0U || !vectors_idle())
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

static nx_result_t release_gate(nx_stm32f1_can_t* port) {
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_CAN1EN, 0U))
        return NX_ERROR_IO;
    *port = (nx_stm32f1_can_t){0};
    return NX_SUCCESS;
}

/** \brief Route one hardware accept-all bank exclusively to FIFO0. */
static bool configure_filter(void) {
    return nx_stm32f1_write_masked(&CAN1->FMR, CAN_FMR_FINIT, CAN_FMR_FINIT) &&
           nx_stm32f1_write_masked(&CAN1->FM1R, CAN_FM1R_FBM0, 0U) &&
           nx_stm32f1_write_masked(&CAN1->FS1R, CAN_FS1R_FSC0, CAN_FS1R_FSC0) &&
           nx_stm32f1_write_masked(&CAN1->FFA1R, CAN_FFA1R_FFA0, 0U) &&
           nx_stm32f1_write_masked(&CAN1->sFilterRegister[0].FR1, UINT32_MAX,
                                   0U) &&
           nx_stm32f1_write_masked(&CAN1->sFilterRegister[0].FR2, UINT32_MAX,
                                   0U) &&
           nx_stm32f1_write_masked(&CAN1->FA1R, CAN_FA1R_FACT0,
                                   CAN_FA1R_FACT0) &&
           nx_stm32f1_write_masked(&CAN1->FMR, CAN_FMR_FINIT, 0U);
}

nx_result_t nx_stm32f1_can_prepare(nx_stm32f1_can_t* port) {
    const nx_result_t admitted = nx_stm32f1_can_admit(port);
    if (admitted != NX_SUCCESS)
        return admitted;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_CAN1EN,
                                 RCC_APB1ENR_CAN1EN))
        return NX_ERROR_IO;
    port->effects = GATE;
    if (!usb_idle() || !vectors_idle() || !cold_idle())
        return release_gate(port) == NX_SUCCESS ? NX_ERROR_BUSY : NX_ERROR_IO;
    port->effects |= CONTROLLER;
    if (!nx_stm32f1_write_masked(&CAN1->MCR, CAN_CONTROL,
                                 CAN_MODE | CAN_MCR_INRQ))
        return NX_ERROR_IO;
    if (!wait_mode(CAN_MSR_INAK))
        return NX_ERROR_TIMEOUT;
    if (!nx_stm32f1_write_masked(&CAN1->BTR, CAN_TIMING_FIELDS, CAN_TIMING) ||
        !configure_filter() ||
        !nx_stm32f1_write_masked(&CAN1->MCR, CAN_CONTROL, CAN_MODE))
        return NX_ERROR_IO;
    if (!wait_mode(0U))
        return NX_ERROR_TIMEOUT;
    port->effects |= STARTED;
    return NX_SUCCESS;
}

static uint32_t word(const nx_can_frame_t* frame, unsigned offset) {
    uint32_t value = 0U;
    if (frame->kind == NX_CAN_DATA) {
        for (unsigned i = 0; i < 4U && offset + i < frame->dlc; ++i)
            value |= (uint32_t)frame->data[offset + i] << (8U * i);
    }
    return value;
}

static bool mailbox_done(uint32_t status) {
    return (status & (CAN_TSR_TME0 | CAN_TSR_RQCP0)) ==
               (CAN_TSR_TME0 | CAN_TSR_RQCP0) &&
           (CAN1->sTxMailBox[0].TIR & CAN_TI0R_TXRQ) == 0U;
}

static void settle(nx_stm32f1_can_t* port, nx_result_t result) {
    nx_can_tx_request_t* request = port->tx;
    const size_t count =
        result == NX_SUCCESS && request->frame.kind == NX_CAN_DATA
            ? request->frame.dlc
            : 0U;
    port->tx = NULL;
    port->abort_result = NX_SUCCESS;
    nx_request_settle(&request->base, result, count);
}

static nx_result_t abort_tx(nx_stm32f1_can_t* port, nx_result_t reason,
                            nx_time_us_t now) {
    if (nx_request_state(&port->tx->base) != NX_REQUEST_ACTIVE)
        return NX_SUCCESS;
    port->abort_result = reason;
    if (nx_deadline_finite_after(now, DRAIN_US, &port->drain_deadline) !=
        NX_SUCCESS) {
        return nx_request_transition(&port->tx->base, NX_REQUEST_QUARANTINED);
    }
    const nx_result_t result =
        nx_request_transition(&port->tx->base, NX_REQUEST_DRAINING);
    if (result == NX_SUCCESS) {
        NX_STM32F1_WRITE(&CAN1->TSR, CAN_TSR_ABRQ0);
        nx_arch_dsb();
    }
    return result;
}

static nx_result_t observe_tx(nx_stm32f1_can_t* port, nx_time_us_t now) {
    if (port->tx == NULL)
        return NX_SUCCESS;
    const uint32_t status = CAN1->TSR;
    if (mailbox_done(status)) {
        NX_STM32F1_WRITE(&CAN1->TSR, CAN_TSR_RQCP0);
        nx_arch_dsb();
        const bool acknowledged = (CAN1->TSR & CAN_TSR_RQCP0) == 0U;
        const nx_result_t missed =
            port->abort_result == NX_SUCCESS ? NX_ERROR_IO : port->abort_result;
        /* TME and TXRQ already prove withdrawal. A stuck status latch blocks
         * the next submit, but it does not change the observed TXOK result. */
        settle(port, (status & CAN_TSR_TXOK0) != 0U ? NX_SUCCESS : missed);
        return acknowledged ? NX_SUCCESS : NX_ERROR_IO;
    }
    const nx_request_state_t request_state = nx_request_state(&port->tx->base);
    if (request_state == NX_REQUEST_ACTIVE &&
        nx_deadline_expired(port->tx->base.deadline, now))
        return abort_tx(port, NX_ERROR_TIMEOUT, now);
    if (request_state == NX_REQUEST_DRAINING &&
        nx_deadline_expired(port->drain_deadline, now))
        return nx_request_transition(&port->tx->base, NX_REQUEST_QUARANTINED);
    return NX_SUCCESS;
}

static nx_result_t submit(void* context, nx_can_tx_request_t* request) {
    nx_stm32f1_can_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(request, sizeof(*request), _Alignof(nx_can_tx_request_t)) ||
        overlap(request, sizeof(*request), port, sizeof(*port)) ||
        !nx_can_frame_valid(&request->frame))
        return NX_ERROR_INVALID;
    if (!running(port) || (port->effects & (CLOSING | BUS_OFF)) != 0U ||
        (CAN1->ESR & CAN_ESR_BOFF) != 0U)
        return NX_ERROR_STATE;
    if (port->tx != NULL ||
        (!CAN_EXTERNAL && (CAN1->RF0R & CAN_RX_WORK) != 0U) || !peers_idle() ||
        (CAN1->TSR & (CAN_TSR_TME0 | CAN_TSR_RQCP0)) != CAN_TSR_TME0)
        return NX_ERROR_BUSY;
    const nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER ||
        nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    const nx_result_t admitted =
        nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (admitted != NX_SUCCESS)
        return admitted;
    port->tx = request;

    const nx_can_frame_t* frame = &request->frame;
    const uint32_t id = frame->format == NX_CAN_STANDARD
                            ? frame->id << CAN_TI0R_STID_Pos
                            : (frame->id << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
    const uint32_t type = frame->kind == NX_CAN_REMOTE ? CAN_TI0R_RTR : 0U;
    CAN_TxMailBox_TypeDef* box = &CAN1->sTxMailBox[0];
    if (!nx_stm32f1_write_masked(&box->TDTR, CAN_TDT0R_DLC, frame->dlc) ||
        !nx_stm32f1_write_masked(&box->TDLR, UINT32_MAX, word(frame, 0U)) ||
        !nx_stm32f1_write_masked(&box->TDHR, UINT32_MAX, word(frame, 4U))) {
        settle(port, NX_ERROR_IO);
        return NX_SUCCESS;
    }
    NX_STM32F1_WRITE(&box->TIR, id | type | CAN_TI0R_TXRQ);
    nx_arch_dsb();
    if ((box->TIR & CAN_TI0R_TXRQ) == 0U &&
        (CAN1->TSR & (CAN_TSR_TME0 | CAN_TSR_RQCP0)) == CAN_TSR_TME0)
        settle(port, NX_ERROR_IO);
    return NX_SUCCESS;
}

static nx_result_t service(void* context) {
    nx_stm32f1_can_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!running(port))
        return NX_ERROR_STATE;
    if ((CAN1->ESR & CAN_ESR_BOFF) != 0U)
        port->effects |= BUS_OFF;
    return observe_tx(port, nx_time_now_us());
}

static nx_result_t cancel(void* context, nx_can_tx_request_t* request) {
    nx_stm32f1_can_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!running(port) || request == NULL || request != port->tx)
        return NX_ERROR_STATE;
    const nx_time_us_t now = nx_time_now_us();
    const nx_result_t seen = observe_tx(port, now);
    if (seen != NX_SUCCESS || port->tx == NULL)
        return seen;
    return abort_tx(port, NX_ERROR_CANCELLED, now);
}

static nx_can_event_t receive_value(void) {
    CAN_FIFOMailBox_TypeDef* box = &CAN1->sFIFOMailBox[0];
    const uint32_t id = box->RIR;
    const uint8_t raw_dlc = (uint8_t)(box->RDTR & CAN_RDT0R_DLC);
    const bool standard = (id & CAN_RI0R_IDE) == 0U;
    const bool remote = (id & CAN_RI0R_RTR) != 0U;
    nx_can_event_t event = {
        .frame = {.id =
                      id >> (standard ? CAN_RI0R_STID_Pos : CAN_RI0R_EXID_Pos),
                  .format = standard ? NX_CAN_STANDARD : NX_CAN_EXTENDED,
                  .kind = remote ? NX_CAN_REMOTE : NX_CAN_DATA,
                  .dlc = raw_dlc <= 8U ? raw_dlc : 8U},
        .timestamp_us = nx_time_now_us(),
        .flags =
            NX_CAN_FILTER_UNKNOWN | (raw_dlc > 8U ? NX_CAN_DLC_NORMALIZED : 0U),
        .kind = NX_CAN_EVENT_FRAME,
        .raw_dlc = raw_dlc,
    };
    if (!remote) {
        const uint32_t data[] = {box->RDLR, box->RDHR};
        for (unsigned i = 0U; i < event.frame.dlc; ++i)
            event.frame.data[i] = (uint8_t)(data[i / 4U] >> (8U * (i % 4U)));
    }
    return event;
}

static nx_result_t read_events(void* context, nx_can_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32f1_can_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (capacity == 0U ||
        !storage(events, sizeof(*events), _Alignof(nx_can_event_t)) ||
        !storage(count, sizeof(*count), _Alignof(size_t)) ||
        overlap(events, sizeof(*events), count, sizeof(*count)) ||
        overlap(events, sizeof(*events), port, sizeof(*port)) ||
        overlap(count, sizeof(*count), port, sizeof(*port)) ||
        (port->tx != NULL &&
         (overlap(events, sizeof(*events), port->tx, sizeof(*port->tx)) ||
          overlap(count, sizeof(*count), port->tx, sizeof(*port->tx)))))
        return NX_ERROR_INVALID;
    if (!running(port) || (port->effects & CLOSING) != 0U)
        return NX_ERROR_STATE;
    const uint32_t status = CAN1->RF0R;
    if ((status & CAN_RF0R_FMP0) != 0U) {
        const nx_can_event_t value = receive_value();
        NX_STM32F1_WRITE(&CAN1->RF0R, CAN_RF0R_RFOM0);
        nx_arch_dsb();
        /* Hardware clears RFOM after release. FMP may be replenished by RX. */
        if ((CAN1->RF0R & CAN_RF0R_RFOM0) != 0U) {
            port->effects |= CLOSING;
            return NX_ERROR_IO;
        }
        *events = value;
    } else if ((status & CAN_RF0R_FOVR0) != 0U) {
        NX_STM32F1_WRITE(&CAN1->RF0R, CAN_RF0R_FOVR0);
        nx_arch_dsb();
        if ((CAN1->RF0R & CAN_RF0R_FOVR0) != 0U)
            return NX_ERROR_IO;
        *events = (nx_can_event_t){.timestamp_us = nx_time_now_us(),
                                   .lost = 1U,
                                   .flags = NX_CAN_LOSS_HARDWARE_LOWER_BOUND,
                                   .kind = NX_CAN_EVENT_LOSS};
    } else {
        *count = 0U;
        return NX_ERROR_EMPTY;
    }
    *count = 1U;
    return NX_SUCCESS;
}

#if CAN_EXTERNAL
/** \brief Decode the last hardware protocol error without clearing ESR. */
static nx_can_protocol_error_t protocol_error(uint32_t status) {
    static const nx_can_protocol_error_t errors[] = {
        NX_CAN_PROTOCOL_NONE,          NX_CAN_PROTOCOL_STUFF,
        NX_CAN_PROTOCOL_FORM,          NX_CAN_PROTOCOL_ACK,
        NX_CAN_PROTOCOL_BIT_RECESSIVE, NX_CAN_PROTOCOL_BIT_DOMINANT,
        NX_CAN_PROTOCOL_CRC,           NX_CAN_PROTOCOL_UNKNOWN,
    };
    return errors[(status & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos];
}

void nx_stm32f1_can_block(nx_stm32f1_can_t* port) {
    if (port->effects != 0U)
        port->effects |= CLOSING;
}
#endif

static nx_result_t state(void* context, nx_can_state_t* result) {
    nx_stm32f1_can_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(result, sizeof(*result), _Alignof(nx_can_state_t)) ||
        overlap(result, sizeof(*result), port, sizeof(*port)) ||
        (port->tx != NULL &&
         overlap(result, sizeof(*result), port->tx, sizeof(*port->tx))))
        return NX_ERROR_INVALID;
    const bool active = running(port);
    const uint32_t esr = active ? CAN1->ESR : 0U;
    if ((esr & CAN_ESR_BOFF) != 0U)
        port->effects |= BUS_OFF;
    *result =
        (nx_can_state_t){.started = (port->effects & STARTED) != 0U,
                         .closing = (port->effects & CLOSING) != 0U,
                         .bus_off = (port->effects & BUS_OFF) != 0U,
                         .pending = port->tx != NULL ? 1U : 0U,
                         .transmit_errors = (uint8_t)(esr >> CAN_ESR_TEC_Pos),
                         .receive_errors = (uint8_t)(esr >> CAN_ESR_REC_Pos),
                         .last_error = NX_CAN_PROTOCOL_UNKNOWN};
#if CAN_EXTERNAL
    if (active)
        result->last_error = protocol_error(esr);
#endif
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_can_close(nx_stm32f1_can_t* port) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32f1_can_t)))
        return NX_ERROR_INVALID;
    if (port->effects == 0U)
        return NX_SUCCESS;
    port->effects |= CLOSING;
    if ((port->effects & CONTROLLER) == 0U)
        return release_gate(port);
    if (!gate_on() || !usb_idle() || !vectors_idle())
        return NX_ERROR_BUSY;
    if ((port->effects & RESETTING) == 0U) {
        if ((RCC->APB1RSTR & RCC_APB1RSTR_CAN1RST) != 0U || CAN1->IER != 0U ||
            !peers_idle())
            return NX_ERROR_BUSY;
        if (port->tx != NULL) {
            (void)observe_tx(port, nx_time_now_us());
            if (port->tx != NULL && port->abort_result == NX_SUCCESS)
                port->abort_result = NX_ERROR_CANCELLED;
        }
        if (!nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_CAN1RST,
                                     RCC_APB1RSTR_CAN1RST))
            return NX_ERROR_IO;
        port->effects |= RESETTING;
    }
    if (!nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_CAN1RST, 0U) ||
        !cold_idle())
        return NX_ERROR_IO;
    if (port->tx != NULL)
        settle(port, port->abort_result);
    port->effects = GATE;
    return release_gate(port);
}

static nx_result_t stop(void* context) {
#if CAN_EXTERNAL
    return nx_stm32f1_can_normal_close(context);
#else
    return nx_stm32f1_can_close(context);
#endif
}

const nx_can_ops_t nx_stm32f1_can_ops = {
    .submit = submit,
    .cancel = cancel,
    .service = service,
    .read_events = read_events,
    .state = state,
    .stop = stop,
};
