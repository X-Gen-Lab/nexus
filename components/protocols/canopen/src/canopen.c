/**
 * \file            canopen.c
 * \brief           Caller-owned CANopenNode objects and explicit CAN progress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/canopen.h"
#include "cia402_private.h"
#define OD_DEFINITION
#include "301/CO_Emergency.h"
#include "301/CO_HBconsumer.h"
#include "301/CO_NMT_Heartbeat.h"
#include "301/CO_ODinterface.h"
#include "301/CO_PDO.h"
#include "301/CO_SDOserver.h"
#include <string.h>

enum { OD_INTERNAL = 9, RX_COUNT = 4, TX_COUNT = 3 };
typedef struct {
    nx_canopen_config_t config;
    nx_can_tx_request_t request;
    CO_CANmodule_t module;
    CO_CANrx_t receive[RX_COUNT];
    CO_CANtx_t transmit[TX_COUNT];
    CO_EM_t emergency;
    CO_NMT_t nmt;
    CO_HBconsumer_t heartbeat;
    CO_HBconsNode_t peer;
    CO_SDOserver_t sdo;
    CO_RPDO_t rpdo;
    CO_TPDO_t tpdo;
    OD_t dictionary;
    OD_obj_var_t device_var;
    OD_obj_var_t error_var;
    OD_obj_var_t heartbeat_var;
    OD_obj_record_t consumer_record[2];
    OD_obj_record_t sdo_record[3];
    OD_obj_record_t rpdo_record[3];
    OD_obj_record_t tpdo_record[3];
    OD_obj_record_t receive_mapping[9];
    OD_obj_record_t transmit_mapping[9];
    uint32_t receive_map[8];
    uint32_t transmit_map[8];
    uint32_t device_type;
    uint32_t consumer;
    uint32_t sdo_receive_id;
    uint32_t sdo_transmit_id;
    uint32_t rpdo_id;
    uint32_t tpdo_id;
    uint16_t heartbeat_ms;
    uint8_t error_register;
    uint8_t one;
    uint8_t two;
    uint8_t asynchronous;
    uint8_t receive_count;
    uint8_t transmit_count;
    nx_time_us_t previous;
    nx_time_us_t current_deadline;
    size_t head;
    size_t count;
    nx_result_t error;
    nx_canopen_reset_t reset;
    bool borrowed;
    bool entered;
    bool closing;
} canopen_state_t;

size_t nx_canopen_workspace_bytes(size_t object_count) {
    if (object_count == 0 || object_count > UINT16_MAX - OD_INTERNAL) {
        return 0;
    }
    const size_t overhead =
        sizeof(canopen_state_t) + (OD_INTERNAL + 1) * sizeof(OD_entry_t);
    const size_t per_object = sizeof(OD_entry_t) + sizeof(OD_obj_var_t);
    if (object_count > (SIZE_MAX - overhead) / per_object) {
        return 0;
    }
    return overhead + object_count * per_object;
}

size_t nx_canopen_workspace_alignment(void) {
    return _Alignof(canopen_state_t);
}

static bool drive_index(uint16_t index) {
    switch (index) {
        case 0x603f:
        case 0x6040:
        case 0x6041:
        case 0x6060:
        case 0x6061:
        case 0x6064:
        case 0x6067:
        case 0x6068:
        case 0x606c:
        case 0x607a:
        case 0x607f:
        case 0x6081:
        case 0x6083:
        case 0x6084:
        case 0x6085:
        case 0x60ff:
        case 0x6502:
            return true;
        default:
            return false;
    }
}

static bool valid_config(const nx_canopen_config_t* config, bool drive) {
    if (config == NULL || config->port == NULL || config->clock.read == NULL ||
        config->objects == NULL || config->node_id == 0 ||
        config->node_id > 127 || config->peer_node_id == 0 ||
        config->peer_node_id > 127 || config->peer_node_id == config->node_id ||
        config->maximum_step_us == 0 || config->producer_heartbeat_ms == 0 ||
        config->consumer_heartbeat_ms == 0 || config->sdo_timeout_ms == 0 ||
        config->receive_budget == 0 || config->tx_slots == NULL ||
        config->tx_capacity == 0 ||
        config->tx_capacity > SIZE_MAX / sizeof(nx_canopen_tx_slot_t) ||
        nx_canopen_workspace_bytes(config->object_count) == 0) {
        return false;
    }
    size_t receive_bytes = 0;
    size_t transmit_bytes = 0;
    uint16_t previous = 0;
    for (size_t index = 0; index < config->object_count; ++index) {
        const nx_canopen_object_t* object = &config->objects[index];
        if ((drive ? !drive_index(object->index)
                   : object->index < 0x2000 || object->index > 0x5fff) ||
            object->index <= previous || object->value == NULL ||
            (object->bytes != 1 && object->bytes != 2 && object->bytes != 4) ||
            (object->receive_pdo && !object->writable)) {
            return false;
        }
        previous = object->index;
        if (object->receive_pdo) {
            receive_bytes += object->bytes;
        }
        if (object->transmit_pdo) {
            transmit_bytes += object->bytes;
        }
    }
    return receive_bytes > 0 && receive_bytes <= 8 && transmit_bytes > 0 &&
           transmit_bytes <= 8;
}

static void variable(OD_entry_t* entry, uint16_t index, OD_obj_var_t* object,
                     void* value, size_t bytes) {
    *object = (OD_obj_var_t){value, ODA_SDO_R, (OD_size_t)bytes};
    *entry = (OD_entry_t){index, 1, ODT_VAR, object, NULL};
}

static void record(OD_entry_t* entry, uint16_t index, OD_obj_record_t* fields,
                   size_t count) {
    *entry = (OD_entry_t){index, (uint8_t)count, ODT_REC, fields, NULL};
}

static OD_obj_record_t field(void* value, uint8_t subindex, size_t bytes) {
    return (OD_obj_record_t){value, subindex, ODA_SDO_R, (OD_size_t)bytes};
}

static void dictionary_init(canopen_state_t* state) {
    const nx_canopen_config_t* config = &state->config;
    OD_entry_t* entries = (OD_entry_t*)(state + 1);
    OD_obj_var_t* variables =
        (OD_obj_var_t*)(entries + OD_INTERNAL + config->object_count + 1);
    state->dictionary =
        (OD_t){(uint16_t)(OD_INTERNAL + config->object_count), entries};
    state->one = 1;
    state->two = 2;
    state->asynchronous = 255;
    state->consumer =
        ((uint32_t)config->peer_node_id << 16) | config->consumer_heartbeat_ms;
    state->heartbeat_ms = config->producer_heartbeat_ms;
    state->sdo_receive_id = 0x600U + config->node_id;
    state->sdo_transmit_id = 0x580U + config->node_id;
    state->rpdo_id = 0x200U + config->node_id;
    state->tpdo_id = 0x180U + config->node_id;
    variable(&entries[0], 0x1000, &state->device_var, &state->device_type, 4);
    variable(&entries[1], 0x1001, &state->error_var, &state->error_register, 1);
    state->consumer_record[0] = field(&state->one, 0, 1);
    state->consumer_record[1] = field(&state->consumer, 1, 4);
    record(&entries[2], 0x1016, state->consumer_record, 2);
    variable(&entries[3], 0x1017, &state->heartbeat_var, &state->heartbeat_ms,
             2);
    state->sdo_record[0] = field(&state->two, 0, 1);
    state->sdo_record[1] = field(&state->sdo_receive_id, 1, 4);
    state->sdo_record[2] = field(&state->sdo_transmit_id, 2, 4);
    record(&entries[4], 0x1200, state->sdo_record, 3);
    state->rpdo_record[0] = field(&state->two, 0, 1);
    state->rpdo_record[1] = field(&state->rpdo_id, 1, 4);
    state->rpdo_record[2] = field(&state->asynchronous, 2, 1);
    record(&entries[5], 0x1400, state->rpdo_record, 3);
    state->tpdo_record[0] = field(&state->two, 0, 1);
    state->tpdo_record[1] = field(&state->tpdo_id, 1, 4);
    state->tpdo_record[2] = field(&state->asynchronous, 2, 1);
    record(&entries[7], 0x1800, state->tpdo_record, 3);
    for (size_t index = 0; index < config->object_count; ++index) {
        const nx_canopen_object_t* source = &config->objects[index];
        OD_attr_t attributes = ODA_SDO_R;
        if (source->writable) {
            attributes |= ODA_SDO_W;
        }
        if (source->receive_pdo) {
            attributes |= ODA_RPDO;
            const uint8_t at = state->receive_count++;
            state->receive_map[at] = ((uint32_t)source->index << 16) |
                                     ((uint32_t)source->bytes * 8U);
            state->receive_mapping[at + 1] =
                field(&state->receive_map[at], (uint8_t)(at + 1), 4);
        }
        if (source->transmit_pdo) {
            attributes |= ODA_TPDO;
            const uint8_t at = state->transmit_count++;
            state->transmit_map[at] = ((uint32_t)source->index << 16) |
                                      ((uint32_t)source->bytes * 8U);
            state->transmit_mapping[at + 1] =
                field(&state->transmit_map[at], (uint8_t)(at + 1), 4);
        }
        variables[index] =
            (OD_obj_var_t){source->value, attributes, source->bytes};
        entries[OD_INTERNAL + index] =
            (OD_entry_t){source->index, 1, ODT_VAR, &variables[index], NULL};
    }
    state->receive_mapping[0] = field(&state->receive_count, 0, 1);
    state->transmit_mapping[0] = field(&state->transmit_count, 0, 1);
    record(&entries[6], 0x1600, state->receive_mapping,
           (size_t)state->receive_count + 1);
    record(&entries[8], 0x1a00, state->transmit_mapping,
           (size_t)state->transmit_count + 1);
}

static CO_ReturnError_t objects_init(canopen_state_t* state) {
    OD_entry_t* od = state->dictionary.list;
    CO_ReturnError_t result = CO_EM_init(&state->emergency, &state->module,
                                         &od[1], state->config.node_id, NULL);
    if (result == CO_ERROR_NO) {
        result = CO_NMT_init(&state->nmt, &od[3], &state->emergency,
                             state->config.node_id,
                             CO_NMT_ERR_ON_BUSOFF_HB | CO_NMT_ERR_TO_STOPPED,
                             state->config.producer_heartbeat_ms,
                             &state->module, 0, 0, &state->module, 0,
                             (uint16_t)(0x700U + state->config.node_id), NULL);
    }
    if (result == CO_ERROR_NO) {
        result = CO_HBconsumer_init(&state->heartbeat, &state->emergency,
                                    &state->peer, 1, &od[2], &state->module, 1,
                                    NULL);
    }
    if (result == CO_ERROR_NO) {
        result = CO_SDOserver_init(&state->sdo, &state->dictionary, &od[4],
                                   state->config.node_id,
                                   state->config.sdo_timeout_ms, &state->module,
                                   2, &state->module, 1, NULL);
    }
    if (result == CO_ERROR_NO) {
        result = CO_RPDO_init(&state->rpdo, &state->dictionary,
                              &state->emergency, (uint16_t)state->rpdo_id,
                              &od[5], &od[6], &state->module, 3, NULL);
    }
    if (result == CO_ERROR_NO) {
        result = CO_TPDO_init(&state->tpdo, &state->dictionary,
                              &state->emergency, (uint16_t)state->tpdo_id,
                              &od[7], &od[8], &state->module, 2, NULL);
    }
    return result;
}

static nx_result_t initialize(nx_canopen_t* node,
                              const nx_canopen_config_t* config,
                              void* workspace, size_t bytes, bool drive) {
    if (node == NULL || node->state != NULL || !valid_config(config, drive) ||
        workspace == NULL ||
        (uintptr_t)workspace % nx_canopen_workspace_alignment() != 0 ||
        bytes < nx_canopen_workspace_bytes(config->object_count)) {
        return NX_ERROR_INVALID;
    }
    const nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    canopen_state_t* state = workspace;
    memset(state, 0, nx_canopen_workspace_bytes(config->object_count));
    state->config = *config;
    state->previous = now;
    state->module.CANptr = state;
    state->module.rxArray = state->receive;
    state->module.rxSize = RX_COUNT;
    state->module.txArray = state->transmit;
    state->module.txSize = TX_COUNT;
    state->module.CANnormal = true;
    state->device_type = drive ? 402U : 0U;
    nx_request_initialize(&state->request.base);
    dictionary_init(state);
    if (objects_init(state) != CO_ERROR_NO) {
        return NX_ERROR_UNSUPPORTED;
    }
    node->state = state;
    return NX_SUCCESS;
}

nx_result_t nx_canopen_init(nx_canopen_t* node,
                            const nx_canopen_config_t* config, void* workspace,
                            size_t bytes) {
    return initialize(node, config, workspace, bytes, false);
}

nx_result_t nx_canopen_init_drive(nx_canopen_t* node,
                                  const nx_canopen_config_t* config,
                                  void* workspace, size_t bytes) {
    return initialize(node, config, workspace, bytes, true);
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t* module, uint16_t index,
                                    uint16_t ident, uint16_t mask, bool_t rtr,
                                    void* object,
                                    void (*callback)(void*, void*)) {
    if (module == NULL || index >= module->rxSize || callback == NULL || rtr) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    module->rxArray[index] = (CO_CANrx_t){ident, mask, object, callback};
    return CO_ERROR_NO;
}

CO_CANtx_t* CO_CANtxBufferInit(CO_CANmodule_t* module, uint16_t index,
                               uint16_t ident, bool_t rtr, uint8_t bytes,
                               bool_t synchronous) {
    if (module == NULL || index >= module->txSize || bytes > 8 || rtr ||
        synchronous) {
        return NULL;
    }
    CO_CANtx_t* buffer = &module->txArray[index];
    buffer->ident = ident;
    buffer->DLC = bytes;
    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t* module, CO_CANtx_t* buffer) {
    canopen_state_t* state = module->CANptr;
    if (state->closing || state->error != NX_SUCCESS) {
        return CO_ERROR_INVALID_STATE;
    }
    if (state->count == state->config.tx_capacity) {
        state->error = NX_ERROR_EXHAUSTED;
        return CO_ERROR_TX_OVERFLOW;
    }
    const size_t tail =
        (state->head + state->count) % state->config.tx_capacity;
    nx_canopen_tx_slot_t* slot = &state->config.tx_slots[tail];
    slot->frame = (nx_can_frame_t){
        buffer->ident, NX_CAN_STANDARD, NX_CAN_DATA, buffer->DLC, {0}};
    memcpy(slot->frame.data, buffer->data, buffer->DLC);
    slot->deadline = state->current_deadline;
    ++state->count;
    return CO_ERROR_NO;
}

void CO_CANmodule_process(CO_CANmodule_t* module) {
    (void)module;
}

static nx_result_t collect_tx(canopen_state_t* state) {
    if (!state->borrowed ||
        nx_request_state(&state->request.base) != NX_REQUEST_SETTLED) {
        return NX_SUCCESS;
    }
    nx_result_t result;
    size_t transferred;
    const nx_result_t observed =
        nx_request_result(&state->request.base, &result, &transferred);
    if (observed != NX_SUCCESS) {
        return observed;
    }
    state->borrowed = false;
    if (state->closing && result == NX_ERROR_CANCELLED) {
        return NX_SUCCESS;
    }
    return result == NX_SUCCESS && transferred != state->request.frame.dlc
               ? NX_ERROR_IO
               : result;
}

static nx_result_t send_one(canopen_state_t* state, nx_time_us_t now) {
    if (state->count == 0 || state->borrowed) {
        return NX_SUCCESS;
    }
    const nx_canopen_tx_slot_t* slot = &state->config.tx_slots[state->head];
    if (now >= slot->deadline) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result =
        nx_can_tx_prepare(&state->request, &slot->frame, slot->deadline);
    if (result == NX_SUCCESS) {
        result = nx_can_port_submit(state->config.port, &state->request);
        if (result == NX_SUCCESS) {
            state->borrowed = true;
            state->head = (state->head + 1) % state->config.tx_capacity;
            --state->count;
        } else if (result == NX_ERROR_BUSY || result == NX_ERROR_EXHAUSTED) {
            return NX_SUCCESS;
        }
    }
    return result;
}

static nx_result_t receive_frames(canopen_state_t* state) {
    for (size_t budget = 0; budget < state->config.receive_budget; ++budget) {
        nx_can_event_t event;
        size_t count = 0;
        const nx_result_t result =
            nx_can_port_read_events(state->config.port, &event, 1, &count);
        if (result != NX_SUCCESS && result != NX_ERROR_EMPTY) {
            return result;
        }
        if (count == 0) {
            return NX_SUCCESS;
        }
        if (count != 1 || event.kind != NX_CAN_EVENT_FRAME ||
            event.frame.dlc > 8) {
            return NX_ERROR_IO;
        }
        if (event.frame.format != NX_CAN_STANDARD ||
            event.frame.kind != NX_CAN_DATA) {
            continue;
        }
        for (size_t index = 0; index < RX_COUNT; ++index) {
            const CO_CANrx_t* receiver = &state->receive[index];
            if (receiver->CANrx_callback != NULL &&
                ((event.frame.id ^ receiver->ident) & receiver->mask) == 0) {
                if (index == 0 && event.frame.dlc == 2 &&
                    (event.frame.data[1] == 0 ||
                     event.frame.data[1] == state->config.node_id) &&
                    state->nmt.internalCommand != CO_NMT_NO_COMMAND) {
                    return NX_ERROR_EXHAUSTED;
                }
                if (index == 3) {
                    if (event.frame.dlc != state->rpdo.PDO_common.dataLength) {
                        return NX_ERROR_IO;
                    }
                    if (CO_FLAG_READ(state->rpdo.CANrxNew[0])) {
                        return NX_ERROR_EXHAUSTED;
                    }
                }
                if (index == 2 && CO_FLAG_READ(state->sdo.CANrxNew)) {
                    return NX_ERROR_EXHAUSTED;
                }
                receiver->CANrx_callback(receiver->object, &event.frame);
                break;
            }
        }
    }
    return NX_SUCCESS;
}

static nx_canopen_report_t observation(const canopen_state_t* state) {
    nx_canopen_report_t report = {0};
    switch (state->nmt.operatingState) {
        case CO_NMT_PRE_OPERATIONAL:
            report.nmt = NX_CANOPEN_PRE_OPERATIONAL;
            break;
        case CO_NMT_OPERATIONAL:
            report.nmt = NX_CANOPEN_OPERATIONAL;
            break;
        case CO_NMT_STOPPED:
            report.nmt = NX_CANOPEN_STOPPED;
            break;
        default:
            report.nmt = NX_CANOPEN_INITIALIZING;
            break;
    }
    report.peer = state->peer.HBstate == CO_HBconsumer_ACTIVE
                      ? NX_CANOPEN_PEER_ACTIVE
                  : state->peer.HBstate == CO_HBconsumer_TIMEOUT
                      ? NX_CANOPEN_PEER_TIMEOUT
                      : NX_CANOPEN_PEER_UNKNOWN;
    report.error = state->error;
    report.reset = state->reset;
    report.queued = state->count;
    report.borrowed = state->borrowed;
    report.closing = state->closing;
    return report;
}

nx_result_t nx_canopen_step(nx_canopen_t* node, nx_time_us_t deadline,
                            nx_canopen_report_t* report) {
    if (node == NULL || node->state == NULL || report == NULL ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    canopen_state_t* state = node->state;
    if (state->entered) {
        return NX_ERROR_BUSY;
    }
    if (state->closing) {
        return NX_ERROR_STATE;
    }
    state->entered = true;
    const nx_time_us_t now =
        state->config.clock.read(state->config.clock.context);
    nx_result_t result = NX_SUCCESS;
    if (now < state->previous ||
        now - state->previous > state->config.maximum_step_us) {
        result = NX_ERROR_STATE;
    } else if (now >= deadline) {
        result = NX_ERROR_TIMEOUT;
    }
    const uint32_t elapsed = (uint32_t)(now - state->previous);
    if (result == NX_SUCCESS) {
        result = nx_can_port_service(state->config.port);
        if (result == NX_ERROR_BUSY) {
            result = NX_SUCCESS;
        }
    }
    if (result == NX_SUCCESS) {
        result = collect_tx(state);
    }
    nx_can_state_t can;
    if (result == NX_SUCCESS) {
        result = nx_can_port_state(state->config.port, &can);
        if (result == NX_SUCCESS &&
            (!can.started || can.closing || can.bus_off)) {
            result = NX_ERROR_IO;
        }
    }
    if (result == NX_SUCCESS) {
        result = receive_frames(state);
    }
    state->current_deadline = deadline;
    if (result == NX_SUCCESS) {
        const CO_NMT_internalState_t nmt = CO_NMT_getInternalState(&state->nmt);
        const bool_t live =
            nmt == CO_NMT_PRE_OPERATIONAL || nmt == CO_NMT_OPERATIONAL;
        CO_HBconsumer_process(&state->heartbeat, live, elapsed, NULL);
        CO_EM_process(&state->emergency, live, elapsed, NULL);
        const CO_NMT_reset_cmd_t reset =
            CO_NMT_process(&state->nmt, NULL, elapsed, NULL);
        if (reset != CO_RESET_NOT) {
            state->reset = reset == CO_RESET_COMM
                               ? NX_CANOPEN_RESET_COMMUNICATION
                               : NX_CANOPEN_RESET_APPLICATION;
            state->closing = true;
        } else {
            const CO_NMT_internalState_t next =
                CO_NMT_getInternalState(&state->nmt);
            const bool_t operational = next == CO_NMT_OPERATIONAL;
            (void)CO_SDOserver_process(
                &state->sdo, next == CO_NMT_PRE_OPERATIONAL || operational,
                elapsed, NULL);
            CO_RPDO_process(&state->rpdo, operational, false);
            CO_TPDO_process(&state->tpdo, operational, false);
        }
        result = state->error;
        if (result == NX_SUCCESS && !state->closing) {
            result = send_one(state, now);
        }
    }
    if (result != NX_SUCCESS) {
        state->error = result;
        state->closing = true;
    }
    state->previous = now;
    *report = observation(state);
    state->entered = false;
    return result;
}

nx_result_t nx_canopen_request_tpdo(nx_canopen_t* node) {
    if (node == NULL || node->state == NULL) {
        return NX_ERROR_INVALID;
    }
    canopen_state_t* state = node->state;
    if (state->entered) {
        return NX_ERROR_BUSY;
    }
    if (state->closing ||
        CO_NMT_getInternalState(&state->nmt) != CO_NMT_OPERATIONAL) {
        return NX_ERROR_STATE;
    }
    CO_TPDOsendRequest(&state->tpdo);
    return NX_SUCCESS;
}

nx_result_t nx_canopen_stop(nx_canopen_t* node) {
    if (node == NULL) {
        return NX_ERROR_INVALID;
    }
    if (node->state == NULL) {
        return NX_SUCCESS;
    }
    canopen_state_t* state = node->state;
    if (state->entered) {
        return NX_ERROR_BUSY;
    }
    state->entered = true;
    state->closing = true;
    state->count = 0;
    nx_result_t result = NX_SUCCESS;
    if (state->borrowed) {
        if (nx_request_state(&state->request.base) != NX_REQUEST_SETTLED) {
            const nx_result_t cancelled =
                nx_can_port_cancel(state->config.port, &state->request);
            if (cancelled != NX_SUCCESS && cancelled != NX_ERROR_BUSY) {
                result = cancelled;
            }
            /* A failed cancel can retain hardware that still needs service. */
            const nx_result_t serviced =
                nx_can_port_service(state->config.port);
            if (result == NX_SUCCESS && serviced != NX_SUCCESS &&
                serviced != NX_ERROR_BUSY) {
                result = serviced;
            }
        }
        const nx_result_t collected = collect_tx(state);
        if (result == NX_SUCCESS) {
            result = collected;
        }
    }
    if (result == NX_SUCCESS && state->borrowed) {
        result = NX_ERROR_BUSY;
    }
    state->entered = false;
    if (result == NX_SUCCESS && !state->borrowed) {
        node->state = NULL;
    }
    return result;
}
