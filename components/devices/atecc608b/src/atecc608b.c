/**
 * \file            atecc608b.c
 * \brief           Locked Microchip signer through settled public I2C transport
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/atecc608b.h"
#include "cryptoauthlib.h"
#include <string.h>

typedef struct {
    nx_atecc608b_config_t config;
    nx_atecc608b_t* owner;
    nx_p256_signer_t signer;
    nx_atecc608b_identity_t identity;
    struct atca_device device;
    ATCAIfaceCfg iface;
    nx_time_us_t deadline;
    nx_time_us_t previous;
    nx_result_t result;
    uint16_t polls;
    bool ready;
    bool entered;
    bool clock_fault;
} atecc_state_t;

static nx_result_t public_key(void* context, nx_p256_public_key_t* out,
                              nx_time_us_t deadline);
static nx_result_t sign_digest(void* context, const nx_p256_digest_t* digest,
                               nx_p256_signature_t* out, nx_time_us_t deadline);
static const nx_p256_signer_ops_t signer_ops = {public_key, sign_digest};

static bool span(const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0 &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}

static bool overlaps(const void* a, size_t a_bytes, const void* b,
                     size_t b_bytes) {
    const uintptr_t left = (uintptr_t)a;
    const uintptr_t right = (uintptr_t)b;
    return left <= right ? right - left < a_bytes : left - right < b_bytes;
}

static bool binding_overlaps(const nx_atecc608b_config_t* config,
                             const void* payload, size_t bytes) {
    const nx_atecc608b_operation_t* operation = nx_atecc608b_operation_get();
    return overlaps(payload, bytes, config->endpoint,
                    sizeof(*config->endpoint)) ||
           overlaps(payload, bytes, config->endpoint->ops,
                    sizeof(*config->endpoint->ops)) ||
           (operation != NULL &&
            overlaps(payload, bytes, operation, sizeof(*operation)));
}

static bool safe_payload(const atecc_state_t* state, const void* payload,
                         size_t bytes) {
    return span(payload, bytes, 1) &&
           !overlaps(payload, bytes, state, sizeof(*state)) &&
           !overlaps(payload, bytes, state->owner, sizeof(*state->owner)) &&
           !binding_overlaps(&state->config, payload, bytes);
}

static nx_result_t failed(atecc_state_t* state, nx_result_t result) {
    if (state->result == NX_SUCCESS) {
        state->result = result;
    }
    return state->result;
}

static bool scope(const nx_atecc608b_t* device, nx_time_us_t deadline) {
    const nx_atecc608b_operation_t* operation = nx_atecc608b_operation_get();
    return operation != NULL && operation->device == device &&
           operation->deadline == deadline;
}

static nx_result_t check(atecc_state_t* state) {
    if (state->result != NX_SUCCESS) {
        return state->result;
    }
    if (state->clock_fault || !scope(state->owner, state->deadline)) {
        return failed(state, NX_ERROR_STATE);
    }
    const nx_time_us_t now =
        state->config.clock.read(state->config.clock.context);
    if (now < state->previous || now == NX_DEADLINE_NEVER) {
        state->clock_fault = true;
        state->ready = false;
        return failed(state, NX_ERROR_STATE);
    }
    state->previous = now;
    return now >= state->deadline ? failed(state, NX_ERROR_TIMEOUT)
                                  : NX_SUCCESS;
}

static ATCA_STATUS wire(atecc_state_t* state, nx_i2c_message_t message,
                        bool polling) {
    if (check(state) != NX_SUCCESS) {
        return ATCA_COMM_FAIL;
    }
    size_t transferred = 0;
    const nx_result_t result = nx_i2c_endpoint_transaction(
        state->config.endpoint, &message, 1, state->deadline, &transferred);
    if (check(state) != NX_SUCCESS) {
        return ATCA_COMM_FAIL;
    }
    if (polling && result == NX_ERROR_NACK) {
        return ATCA_RX_NO_RESPONSE;
    }
    if (result != NX_SUCCESS || transferred != message.length) {
        (void)failed(state, result != NX_SUCCESS ? result : NX_ERROR_IO);
        return ATCA_COMM_FAIL;
    }
    return ATCA_SUCCESS;
}

/* SDK transport functions bind only this fixed profile; no basic API global. */
ATCADeviceType atcab_get_device_type_ext(ATCADevice device) {
    return device != NULL && device->mIface.mIfaceCFG != NULL
               ? device->mIface.mIfaceCFG->devtype
               : ATCA_DEV_UNKNOWN;
}

uint8_t atcab_get_device_address(ATCADevice device) {
    (void)device;
    return 0xff;
}

bool atcab_is_ca2_device(ATCADeviceType type) {
    (void)type;
    return false;
}

bool atca_iface_is_kit(ATCAIface iface) {
    (void)iface;
    return false;
}

bool atca_iface_is_swi(ATCAIface iface) {
    (void)iface;
    return false;
}

int atca_iface_get_retries(ATCAIface iface) {
    (void)iface;
    return 0;
}

ATCA_STATUS atcontrol(ATCAIface iface, uint8_t option, void* param,
                      size_t paramlen) {
    (void)iface;
    (void)option;
    (void)param;
    (void)paramlen;
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS atsend(ATCAIface iface, uint8_t word_address, uint8_t* data,
                   int bytes) {
    if (iface == NULL || iface->hal_data == NULL || bytes < 0 ||
        (size_t)bytes > MAX_PACKET_SIZE || (bytes != 0 && data == NULL)) {
        return ATCA_BAD_PARAM;
    }
    atecc_state_t* state = iface->hal_data;
    uint8_t command[MAX_PACKET_SIZE + 1];
    command[0] = word_address;
    if (bytes != 0) {
        memcpy(command + 1, data, (size_t)bytes);
    }
    return wire(state, nx_i2c_message_write(command, (size_t)bytes + 1), false);
}

ATCA_STATUS atreceive(ATCAIface iface, uint8_t address, uint8_t* data,
                      uint16_t* bytes) {
    (void)address;
    if (iface == NULL || iface->hal_data == NULL || data == NULL ||
        bytes == NULL || *bytes == 0 || *bytes > MAX_PACKET_SIZE) {
        return ATCA_BAD_PARAM;
    }
    atecc_state_t* state = iface->hal_data;
    if (state->polls == state->config.maximum_polls) {
        (void)failed(state, NX_ERROR_TIMEOUT);
        return ATCA_RX_NO_RESPONSE;
    }
    ++state->polls;
    return wire(state, nx_i2c_message_read(data, *bytes), true);
}

ATCA_STATUS calib_wakeup(ATCADevice device) {
    atecc_state_t* state = device->mIface.hal_data;
    if (check(state) != NX_SUCCESS) {
        return ATCA_WAKE_FAILED;
    }
    const nx_result_t result =
        state->config.wake(state->config.context, state->deadline);
    if (result != NX_SUCCESS) {
        (void)failed(state, result);
        return ATCA_WAKE_FAILED;
    }
    uint8_t response[4];
    const ATCA_STATUS received =
        wire(state, nx_i2c_message_read(response, sizeof(response)), false);
    static const uint8_t expected[4] = {4, 0x11, 0x33, 0x43};
    if (received != ATCA_SUCCESS ||
        memcmp(response, expected, sizeof(response)) != 0) {
        (void)failed(state, NX_ERROR_IO);
        return ATCA_WAKE_FAILED;
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS calib_idle(ATCADevice device) {
    return atsend(&device->mIface, 2, NULL, 0);
}

void atca_delay_us(uint32_t us) {
    const nx_atecc608b_operation_t* operation = nx_atecc608b_operation_get();
    if (operation == NULL || operation->device == NULL ||
        operation->device->state == NULL) {
        return;
    }
    atecc_state_t* state = operation->device->state;
    if (check(state) != NX_SUCCESS) {
        return;
    }
    const nx_result_t result =
        state->config.delay(state->config.context, us, state->deadline);
    if (result != NX_SUCCESS) {
        (void)failed(state, result);
    }
    (void)check(state);
}

void atca_delay_ms(uint32_t ms) {
    if (ms > UINT32_MAX / 1000) {
        atca_delay_us(UINT32_MAX);
        return;
    }
    atca_delay_us(ms * 1000);
}

size_t nx_atecc608b_workspace_bytes(void) {
    return sizeof(atecc_state_t);
}

size_t nx_atecc608b_workspace_alignment(void) {
    return _Alignof(atecc_state_t);
}

static nx_result_t execute(atecc_state_t* state, ATCAPacket* packet,
                           uint8_t expected) {
    if (check(state) != NX_SUCCESS) {
        return state->result;
    }
    const ATCA_STATUS status = calib_execute_command(packet, &state->device);
    if (check(state) != NX_SUCCESS) {
        return state->result;
    }
    if (status != ATCA_SUCCESS || packet->data[0] != (uint8_t)(expected + 3)) {
        return failed(state, NX_ERROR_IO);
    }
    return NX_SUCCESS;
}

static nx_result_t begin(atecc_state_t* state, nx_time_us_t deadline,
                         bool require_ready) {
    if (state->entered) {
        return NX_ERROR_BUSY;
    }
    if (deadline == NX_DEADLINE_NEVER || !scope(state->owner, deadline) ||
        (require_ready && !state->ready)) {
        return NX_ERROR_STATE;
    }
    state->entered = true;
    state->deadline = deadline;
    state->result = NX_SUCCESS;
    state->polls = 0;
    const nx_result_t result = check(state);
    if (result != NX_SUCCESS) {
        state->entered = false;
    }
    return result;
}

static nx_result_t finish(atecc_state_t* state, nx_result_t result) {
    state->entered = false;
    if (result != NX_SUCCESS) {
        state->ready = false;
    }
    return result;
}

static nx_result_t read_config(atecc_state_t* state, uint8_t data[128]) {
    for (uint16_t block = 0; block < 4; ++block) {
        ATCAPacket packet = {0};
        packet.param1 = ATCA_ZONE_CONFIG | ATCA_ZONE_READWRITE_32;
        packet.param2 = (uint16_t)(block * 8);
        if (atRead(ATECC608, &packet) != ATCA_SUCCESS) {
            return NX_ERROR_UNSUPPORTED;
        }
        const nx_result_t result = execute(state, &packet, 32);
        if (result != NX_SUCCESS) {
            return result;
        }
        memcpy(data + (size_t)block * 32U, packet.data + 1, 32);
    }
    return NX_SUCCESS;
}

static nx_result_t inspect(atecc_state_t* state) {
    ATCAPacket packet = {0};
    packet.param1 = INFO_MODE_REVISION;
    if (atInfo(ATECC608, &packet) != ATCA_SUCCESS) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = execute(state, &packet, 4);
    static const uint8_t reviewed[4] = {0, 0, 0x60, 3};
    if (result != NX_SUCCESS) {
        return result;
    }
    if (memcmp(packet.data + 1, reviewed, sizeof(reviewed)) != 0) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint8_t configuration[128];
    result = read_config(state, configuration);
    if (result != NX_SUCCESS) {
        return result;
    }
    const size_t slot = state->config.signing_slot;
    const uint16_t slot_config =
        (uint16_t)((uint16_t)configuration[20 + slot * 2] |
                   ((uint16_t)configuration[21 + slot * 2] << 8));
    const uint16_t key_config =
        (uint16_t)((uint16_t)configuration[96 + slot * 2] |
                   ((uint16_t)configuration[97 + slot * 2] << 8));
    if (configuration[86] != 0 || configuration[87] != 0 ||
        (configuration[88 + slot / 8] & (1U << (slot % 8))) != 0 ||
        (slot_config & 0x81U) != 0x81U || (slot_config & 0x20U) != 0 ||
        (key_config & 0x1fU) != 0x13U || (key_config & 0xc0U) != 0) {
        return NX_ERROR_PERMISSION;
    }
    if (configuration[0] != 1 || configuration[1] != 0x23 ||
        configuration[12] != 0xee) {
        return NX_ERROR_UNSUPPORTED;
    }
    memcpy(state->identity.serial, configuration, 4);
    memcpy(state->identity.serial + 4, configuration + 8, 5);
    memcpy(state->identity.revision, reviewed, sizeof(reviewed));
    state->identity.signing_slot = state->config.signing_slot;
    return NX_SUCCESS;
}

nx_result_t nx_atecc608b_init(nx_atecc608b_t* device,
                              const nx_atecc608b_config_t* config,
                              void* workspace, size_t bytes,
                              nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_atecc608b_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_atecc608b_config_t)) ||
        !span(workspace, sizeof(atecc_state_t), _Alignof(atecc_state_t)) ||
        bytes < sizeof(atecc_state_t) ||
        !span(config->endpoint, sizeof(*config->endpoint),
              _Alignof(nx_i2c_endpoint_t)) ||
        !span(config->endpoint->ops, sizeof(*config->endpoint->ops),
              _Alignof(nx_i2c_endpoint_ops_t)) ||
        config->endpoint->ops->transaction == NULL ||
        config->clock.read == NULL || config->wake == NULL ||
        config->delay == NULL || config->maximum_polls == 0 ||
        config->signing_slot > 15 || deadline == NX_DEADLINE_NEVER ||
        overlaps(device, sizeof(*device), config, sizeof(*config)) ||
        overlaps(device, sizeof(*device), workspace, sizeof(atecc_state_t)) ||
        overlaps(config, sizeof(*config), workspace, sizeof(atecc_state_t)) ||
        binding_overlaps(config, workspace, sizeof(atecc_state_t)) ||
        binding_overlaps(config, device, sizeof(*device))) {
        return NX_ERROR_INVALID;
    }
    if (device->state != NULL || !scope(device, deadline)) {
        return NX_ERROR_STATE;
    }
    const nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (now >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    atecc_state_t* state = workspace;
    memset(state, 0, sizeof(*state));
    state->owner = device;
    state->config = *config;
    state->previous = now;
    state->signer = (nx_p256_signer_t){&signer_ops, state};
    state->iface.iface_type = ATCA_CUSTOM_IFACE;
    state->iface.devtype = ATECC608;
    state->device.mIface.mIfaceCFG = &state->iface;
    state->device.mIface.hal_data = state;
    device->state = state;
    nx_result_t result = begin(state, deadline, false);
    if (result == NX_SUCCESS) {
        result = inspect(state);
        state->ready = result == NX_SUCCESS;
        result = finish(state, result);
    }
    return result;
}

const nx_p256_signer_t* nx_atecc608b_signer(const nx_atecc608b_t* device) {
    if (device == NULL || device->state == NULL) {
        return NULL;
    }
    const atecc_state_t* state = device->state;
    return state->ready && !state->entered ? &state->signer : NULL;
}

nx_result_t nx_atecc608b_read_identity(const nx_atecc608b_t* device,
                                       nx_atecc608b_identity_t* out) {
    if (device == NULL || device->state == NULL || out == NULL) {
        return NX_ERROR_INVALID;
    }
    const atecc_state_t* state = device->state;
    if (!state->ready || state->entered) {
        return NX_ERROR_STATE;
    }
    if (!safe_payload(state, out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    *out = state->identity;
    return NX_SUCCESS;
}

static nx_result_t public_key(void* context, nx_p256_public_key_t* out,
                              nx_time_us_t deadline) {
    atecc_state_t* state = context;
    if (!safe_payload(state, out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(state, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    ATCAPacket packet = {0};
    packet.param1 = GENKEY_MODE_PUBLIC;
    packet.param2 = state->config.signing_slot;
    result = atGenKey(ATECC608, &packet) == ATCA_SUCCESS
                 ? execute(state, &packet, 64)
                 : NX_ERROR_UNSUPPORTED;
    if (result == NX_SUCCESS) {
        memcpy(out->xy, packet.data + 1, sizeof(out->xy));
    }
    return finish(state, result);
}

/* Public scalar range validation is separate from signature verification. */
static bool scalar_valid(const uint8_t scalar[32]) {
    static const uint8_t order[32] = {
        0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
        0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};
    uint8_t combined = 0;
    for (size_t index = 0; index < 32; ++index) {
        combined |= scalar[index];
    }
    return combined != 0 && memcmp(scalar, order, sizeof(order)) < 0;
}

static nx_result_t sign_digest(void* context, const nx_p256_digest_t* digest,
                               nx_p256_signature_t* out,
                               nx_time_us_t deadline) {
    atecc_state_t* state = context;
    if (!safe_payload(state, digest, sizeof(*digest)) ||
        !safe_payload(state, out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(state, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    ATCAPacket packet = {0};
    packet.param1 = RANDOM_SEED_UPDATE;
    result = atRandom(ATECC608, &packet) == ATCA_SUCCESS
                 ? execute(state, &packet, 32)
                 : NX_ERROR_UNSUPPORTED;
    if (result == NX_SUCCESS) {
        memset(&packet, 0, sizeof(packet));
        packet.param1 = NONCE_MODE_PASSTHROUGH | NONCE_MODE_TARGET_MSGDIGBUF;
        memcpy(packet.data, digest->bytes, sizeof(digest->bytes));
        result = atNonce(ATECC608, &packet) == ATCA_SUCCESS
                     ? execute(state, &packet, 1)
                     : NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS) {
        memset(&packet, 0, sizeof(packet));
        packet.param1 = SIGN_MODE_EXTERNAL | SIGN_MODE_SOURCE_MSGDIGBUF;
        packet.param2 = state->config.signing_slot;
        result = atSign(ATECC608, &packet) == ATCA_SUCCESS
                     ? execute(state, &packet, 64)
                     : NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS &&
        (!scalar_valid(packet.data + 1) || !scalar_valid(packet.data + 33))) {
        result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        memcpy(out->rs, packet.data + 1, sizeof(out->rs));
    }
    return finish(state, result);
}

nx_result_t nx_atecc608b_stop(nx_atecc608b_t* device, nx_time_us_t deadline) {
    if (device == NULL) {
        return NX_ERROR_INVALID;
    }
    if (device->state == NULL) {
        return NX_SUCCESS;
    }
    atecc_state_t* state = device->state;
    nx_result_t result = begin(state, deadline, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->ready = false;
    if (calib_wakeup(&state->device) != ATCA_SUCCESS ||
        atsend(&state->device.mIface, 1, NULL, 0) != ATCA_SUCCESS) {
        result = state->result != NX_SUCCESS ? state->result : NX_ERROR_IO;
    }
    result = finish(state, result);
    if (result == NX_SUCCESS) {
        device->state = NULL;
    }
    return result;
}
