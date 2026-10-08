#include "nexus/industrial_supervisor.h"
#include "nexus/modbus_rtu.h"
#include "nexus_config.h"

#include <stdio.h>
#include <string.h>

/* Native wire/plant model. It exercises real protocol and supervisor code;
 * it is not a UART, board watchdog, persistent parameter store or HIL test. */
enum { SENSOR = 0, SETPOINT = 1, OUTPUT = 2, QUALITY = 3, ENABLE = 4, REGISTERS = 5 };
typedef struct {
    uint64_t time_us;
    uint16_t registers[REGISTERS];
    uint8_t reply[NEXUS_MODBUS_ADU_MAX];
    size_t reply_length;
    unsigned watchdog_feeds;
    unsigned safe_transitions;
    bool direction_tx;
    bool wire_busy;
    bool commissioning_interlock;
} controller_t;

static uint64_t now_us(void* context) {
    return ((controller_t*)context)->time_us;
}

static bool direction(void* context, bool tx) {
    controller_t* c = context;
    if (!tx && c->wire_busy) {
        return false;
    }
    c->direction_tx = tx;
    return true;
}

static bool wire_write(void* context, const uint8_t* bytes, size_t length,
                       uint64_t deadline) {
    controller_t* c = context;
    if (!c->direction_tx || c->wire_busy || length > sizeof(c->reply) ||
        c->time_us >= deadline) {
        return false;
    }
    memcpy(c->reply, bytes, length);
    c->reply_length = length;
    c->wire_busy = true;
    return true;
}

static bool wire_drain(void* context, uint64_t deadline) {
    controller_t* c = context;
    const uint64_t duration = c->reply_length * 96u; /* 11 bits at 115200 baud */
    if (!c->wire_busy || deadline - c->time_us < duration) {
        return false;
    }
    c->time_us += duration;
    c->wire_busy = false;
    return true;
}

static bool wire_abort(void* context, uint64_t deadline) {
    controller_t* c = context;
    if (c->time_us > deadline) {
        return false;
    }
    c->wire_busy = false;
    return true;
}

static nexus_modbus_register_result_t read_values(void* context, uint16_t start,
                                                 uint16_t count,
                                                 uint16_t* destination) {
    controller_t* c = context;
    if ((uint32_t)start + count > REGISTERS) {
        return NEXUS_MODBUS_ILLEGAL_ADDRESS;
    }
    memcpy(destination, &c->registers[start], (size_t)count * sizeof(uint16_t));
    return NEXUS_MODBUS_OK;
}

static nexus_modbus_register_result_t authorize(void* context, uint16_t start,
                                                uint16_t count,
                                                const uint16_t* values) {
    controller_t* c = context;
    if ((uint32_t)start + count > REGISTERS) {
        return NEXUS_MODBUS_ILLEGAL_ADDRESS;
    }
    /* Validate the complete transaction before changing any product state. */
    for (uint16_t i = 0; i < count; ++i) {
        const uint16_t address = (uint16_t)(start + i);
        if (address == SETPOINT && values[i] <= 1000) {
            continue;
        }
        if (address == ENABLE && values[i] <= 1) {
            if (values[i] != 0 && !c->commissioning_interlock) {
                return NEXUS_MODBUS_DEVICE_BUSY;
            }
            continue;
        }
        return NEXUS_MODBUS_ILLEGAL_VALUE;
    }
    return NEXUS_MODBUS_OK;
}

static nexus_modbus_register_result_t commit_values(void* context,
    uint16_t start, uint16_t count, const uint16_t* values) {
    controller_t* c = context;
    /* In this single-threaded model authorization and commit share one owner.
     * A threaded product must lock/recheck interlocks across this transaction. */
    memcpy(&c->registers[start], values, (size_t)count * sizeof(uint16_t));
    return NEXUS_MODBUS_OK;
}

static bool enter_safe(void* context, nexus_industrial_fault_t fault) {
    controller_t* c = context;
    (void)fault;
    c->registers[OUTPUT] = 0;
    c->registers[ENABLE] = 0;
    ++c->safe_transitions;
    return true;
}

static bool feed_watchdog(void* context) {
    ++((controller_t*)context)->watchdog_feeds;
    return true;
}

static bool transact(controller_t* c, nexus_modbus_rtu_t* rtu, uint8_t function,
                     uint16_t address, uint16_t value) {
    uint8_t request[8] = {17, function, (uint8_t)(address >> 8), (uint8_t)address,
                         (uint8_t)(value >> 8), (uint8_t)value, 0, 0};
    const uint16_t crc = nexus_modbus_crc16(request, 6);
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8);
    c->time_us += rtu->t3_5_us + 100;
    for (size_t i = 0; i < sizeof(request); ++i) {
        if (!nexus_modbus_rtu_feed(rtu, request[i], c->time_us)) {
            return false;
        }
        c->time_us += 96;
    }
    c->time_us += rtu->t3_5_us;
    return nexus_modbus_rtu_poll(rtu, c->time_us) && c->reply_length >= 5 &&
           nexus_modbus_crc16(c->reply, c->reply_length) == 0;
}

int main(void) {
    controller_t controller = {.time_us = 10000,
                               .registers = {250, 300, 0, NEXUS_DATA_VALID, 0}};
    nexus_modbus_rtu_t rtu;
    const nexus_modbus_config_t config = {
        .address = 17, .baudrate = 115200, .bits_per_character = 11,
        .tx_timeout_us = 100000, .abort_timeout_us = 1000,
        .registers = {&controller, read_values, authorize, commit_values},
        .port = {&controller, now_us, direction, wire_write, wire_drain, wire_abort}
    };
    if (!nexus_modbus_rtu_init(&rtu, &config) ||
        !transact(&controller, &rtu, 6, SETPOINT, 350) ||
        controller.registers[SETPOINT] != 350 ||
        !transact(&controller, &rtu, 6, ENABLE, 1) ||
        controller.reply[1] != 0x86 || controller.reply[2] != 6 ||
        controller.registers[ENABLE] != 0) {
        return 1;
    }

    /* Physical commissioning input in this model is explicitly asserted by
     * the local operator; the network request cannot assert that interlock. */
    controller.commissioning_interlock = true;
    if (!transact(&controller, &rtu, 6, ENABLE, 1) ||
        controller.registers[ENABLE] != 1) {
        return 1;
    }
    /* Communication commissioning finishes before the periodic control domain
     * starts. A product schedules those domains independently. */
    nexus_industrial_supervisor_t supervisor;
    const nexus_industrial_config_t supervision = {
        .cycle_us = 10000, .job_count = 3, .jobs = {{2000}, {5000}, {9000}},
        .build_id = NEXUS_SOURCE_REVISION, .reset_reason = 0,
        .context = &controller, .enter_safe = enter_safe,
        .feed_watchdog = feed_watchdog
    };
    if (!nexus_industrial_init(&supervisor, &supervision, controller.time_us)) {
        return 1;
    }
    controller.registers[ENABLE] = 1; /* locally authorized control re-enable */
    const uint64_t cycle_start = controller.time_us;
    const uint64_t token = nexus_industrial_cycle(&supervisor);
    if (!nexus_industrial_report(&supervisor, 0, token, cycle_start + 1000,
                                 NEXUS_DATA_VALID)) {
        return 1;
    }
    controller.registers[OUTPUT] =
        controller.registers[SENSOR] < controller.registers[SETPOINT] ? 1 : 0;
    if (!nexus_industrial_report(&supervisor, 1, token, cycle_start + 3000,
                                 NEXUS_DATA_VALID) ||
        !nexus_industrial_report(&supervisor, 2, token, cycle_start + 6000,
                                 NEXUS_DATA_VALID) || controller.watchdog_feeds != 1) {
        return 1;
    }
    if (!nexus_industrial_poll(&supervisor, cycle_start + 10000)) {
        return 1;
    }
    const uint64_t next = nexus_industrial_cycle(&supervisor);
    controller.registers[QUALITY] = NEXUS_DATA_STALE;
    if (nexus_industrial_report(&supervisor, 0, next, cycle_start + 11000,
                               NEXUS_DATA_STALE) ||
        supervisor.state != NEXUS_INDUSTRIAL_SAFE_FAULT ||
        controller.registers[OUTPUT] != 0 || controller.registers[ENABLE] != 0 ||
        controller.watchdog_feeds != 1) {
        return 1;
    }
    printf("{\"backend\":\"native-wire-plant-model\",\"source\":\"%s\","
           "\"config_sha256\":\"%s\",\"board\":\"%s\",\"setpoint\":%u,"
           "\"critical_write_blocked\":true,\"watchdog_feeds\":%u,"
           "\"safe_output\":%u,\"fault\":\"stale-sensor\","
           "\"rtu_bytes\":%zu,\"supervisor_bytes\":%zu}\n",
           supervisor.build_id, NEXUS_CONFIG_SHA256, NX_CONFIG_BOARD_NAME,
           controller.registers[SETPOINT], controller.watchdog_feeds,
           controller.registers[OUTPUT], sizeof(rtu), sizeof(supervisor));
    return 0;
}
