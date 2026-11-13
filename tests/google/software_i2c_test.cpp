/**
 * \file            software_i2c_test.cpp
 * \brief           External two-wire edge, ACK, stretch and withdrawal models
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/software_i2c.h"
#include "nexus/components/software_sccb.h"
#include <array>
#include <functional>
#include <gtest/gtest.h>
#include <vector>

namespace {
/** \brief Target-side physical edges; no software driver logic is mocked. */
class Wire {
  public:
    struct Pin {
        Wire* wire;
        unsigned id;
        bool released;
    };
    std::array<Pin, 2> pins{{{this, 0U, true}, {this, 1U, true}}};
    std::array<nx_gpio_port_t, 2> ports{};
    std::vector<uint8_t> tx_bytes;
    std::vector<uint8_t> incoming{0x23U, 0x45U};
    std::vector<bool> acknowledge;
    std::vector<bool> master_nacks;
    nx_time_us_t now{};
    unsigned clock_reads{};
    unsigned writes{};
    unsigned starts{};
    unsigned stops{};
    unsigned rises{};
    unsigned stretch_reads{};
    unsigned stretch_each_release{};
    bool all_ack{true};
    bool freeze{};
    bool force_scl_low{};
    bool force_sda_low{};
    bool collide_on_one{};
    bool fail_stop{};
    bool fail_read{};
    std::function<void()> on_write;
    bool actual_scl{true};
    bool frame{};
    bool read_data{};
    bool pending_read{};
    bool nack{};
    bool ack{true};
    unsigned phase{};
    unsigned bits{};
    unsigned frame_bytes{};
    uint8_t outgoing{};
    size_t input_index{};

    Wire() {
        for (unsigned i = 0U; i < ports.size(); ++i) {
            ports[i] = {&ops, &pins[i]};
        }
    }
    void start() {
        ++starts;
        frame = true;
        read_data = false;
        pending_read = false;
        nack = false;
        phase = 0U;
        bits = 0U;
        frame_bytes = 0U;
        outgoing = 0U;
    }
    bool data_high() const {
        if (!pins[1].released || force_sda_low) {
            return false;
        }
        if (!frame) {
            return true;
        }
        if (collide_on_one && !read_data && phase < 8U && actual_scl) {
            return false;
        }
        if (phase == 8U && !read_data) {
            return !ack;
        }
        if (phase < 8U && read_data) {
            uint8_t value =
                input_index < incoming.size() ? incoming[input_index] : 0xffU;
            return (value & (0x80U >> phase)) != 0U;
        }
        return true;
    }
    void rise() {
        ++rises;
        if (!frame) {
            return;
        }
        phase = bits % 9U;
        if (phase < 8U && !read_data) {
            outgoing = static_cast<uint8_t>(outgoing << 1U);
            if (pins[1].released) {
                outgoing = static_cast<uint8_t>(outgoing | 1U);
            }
            if (phase == 7U) {
                tx_bytes.push_back(outgoing);
                ack = tx_bytes.size() <= acknowledge.size()
                          ? acknowledge[tx_bytes.size() - 1U]
                          : all_ack;
                if (frame_bytes == 0U) {
                    pending_read = (outgoing & 1U) != 0U;
                }
            }
        } else if (phase == 8U && read_data) {
            nack = pins[1].released;
            master_nacks.push_back(nack);
        }
        ++bits;
    }
    void fall() {
        if (!frame) {
            return;
        }
        if (phase == 8U) {
            ++frame_bytes;
            if (read_data) {
                ++input_index;
                if (nack) {
                    read_data = false;
                }
            } else if (frame_bytes == 1U) {
                read_data = pending_read;
            }
            outgoing = 0U;
        }
        phase = bits % 9U;
    }
    void observe_scl() {
        bool next = pins[0].released && !force_scl_low && stretch_reads == 0U;
        if (next != actual_scl) {
            actual_scl = next;
            if (next) {
                rise();
            } else {
                fall();
            }
        }
    }
    static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
        auto& pin = *static_cast<Pin*>(context);
        Wire& wire = *pin.wire;
        EXPECT_EQ(set | reset, 1U);
        EXPECT_EQ(set & reset, 0U);
        ++wire.writes;
        bool release = set != 0U;
        if (wire.on_write) {
            wire.on_write();
        }
        if (pin.id == 1U && release && !pin.released && wire.actual_scl &&
            wire.frame && wire.fail_stop) {
            return NX_ERROR_IO;
        }
        bool before = wire.data_high();
        bool changed = pin.released != release;
        pin.released = release;
        if (pin.id == 0U) {
            if (release && changed) {
                wire.stretch_reads = wire.stretch_each_release;
            }
            wire.observe_scl();
        } else if (wire.actual_scl) {
            bool after = wire.data_high();
            if (before && !after) {
                wire.start();
            } else if (!before && after) {
                ++wire.stops;
                wire.frame = false;
            }
        }
        return NX_SUCCESS;
    }
    static nx_result_t read(const void* context, uint32_t* value) {
        auto& pin = *static_cast<const Pin*>(context);
        Wire& wire = *pin.wire;
        if (wire.fail_read) {
            return NX_ERROR_IO;
        }
        if (pin.id == 0U) {
            if (wire.stretch_reads != 0U) {
                --wire.stretch_reads;
                *value = 0U;
                return NX_SUCCESS;
            }
            wire.observe_scl();
            *value = wire.actual_scl ? 1U : 0U;
        } else {
            *value = wire.data_high() ? 1U : 0U;
        }
        return NX_SUCCESS;
    }
    static nx_time_us_t clock(void* context) {
        auto& wire = *static_cast<Wire*>(context);
        ++wire.clock_reads;
        if (!wire.freeze) {
            ++wire.now;
        }
        return wire.now;
    }
    static const nx_gpio_ops_t ops;
    nx_software_i2c_config_t config() {
        return {{&ports[0], 1U}, {&ports[1], 1U}, {clock, this}, 5U, 32U};
    }
};
const nx_gpio_ops_t Wire::ops{Wire::write, Wire::read, nullptr};

class SoftwareI2CTest : public testing::Test {
  protected:
    Wire wire;
    nx_software_i2c_t owner{};
    nx_software_i2c_config_t config = wire.config();
    nx_software_i2c_device_t child{&owner, 0x50U};
    void initialize() {
        ASSERT_EQ(nx_software_i2c_init(&owner, &config, 10000U), NX_SUCCESS);
    }
    nx_result_t transfer(nx_i2c_message_t* messages, size_t count,
                         size_t* transferred, nx_time_us_t deadline = 10000U) {
        nx_i2c_endpoint_t endpoint = nx_software_i2c_endpoint(&child);
        return nx_i2c_endpoint_transaction(&endpoint, messages, count, deadline,
                                           transferred);
    }
};

TEST_F(SoftwareI2CTest, RejectsInvalidElectricalFactsBeforeAnyPinWrite) {
    config.half_period_us = 0U;
    EXPECT_EQ(nx_software_i2c_init(&owner, &config, 100U), NX_ERROR_INVALID);
    config = wire.config();
    config.sda = config.scl;
    EXPECT_EQ(nx_software_i2c_init(&owner, &config, 100U), NX_ERROR_INVALID);
    config = wire.config();
    config.poll_limit = 0U;
    EXPECT_EQ(nx_software_i2c_init(&owner, &config, 100U), NX_ERROR_INVALID);
    EXPECT_EQ(wire.writes, 0U);
    EXPECT_EQ(owner.config, nullptr);
}

TEST_F(SoftwareI2CTest, InitDoesNotGenerateAStartOrDiscoverADevice) {
    initialize();
    EXPECT_EQ(wire.starts, 0U);
    EXPECT_TRUE(wire.tx_bytes.empty());
    EXPECT_TRUE(owner.initialized);
    EXPECT_FALSE(nx_software_i2c_faulted(&owner));
}

TEST_F(SoftwareI2CTest, StrictWriteHasRealAddressAckAndFinalStop) {
    initialize();
    std::array<uint8_t, 2> data{0x05U, 0xa6U};
    nx_i2c_message_t message = nx_i2c_message_write(data.data(), data.size());
    size_t transferred = 99U;
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_SUCCESS);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0xa0U, 0x05U, 0xa6U}));
    EXPECT_EQ(transferred, data.size());
    EXPECT_EQ(wire.starts, 1U);
    EXPECT_EQ(wire.stops, 1U);
}

TEST_F(SoftwareI2CTest, RepeatedStartReadAcksAllButTheLastByte) {
    initialize();
    uint8_t address = 0x05U;
    std::array<uint8_t, 2> data{};
    nx_i2c_message_t messages[2]{nx_i2c_message_write(&address, 1U),
                                 nx_i2c_message_read(data.data(), data.size())};
    size_t transferred{};
    EXPECT_EQ(transfer(messages, 2U, &transferred), NX_SUCCESS);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0xa0U, 0x05U, 0xa1U}));
    EXPECT_EQ(data, (std::array<uint8_t, 2>{0x23U, 0x45U}));
    EXPECT_EQ(wire.master_nacks, (std::vector<bool>{false, true}));
    EXPECT_EQ(transferred, 3U);
    EXPECT_EQ(wire.starts, 2U);
    EXPECT_EQ(wire.stops, 1U);
}

TEST_F(SoftwareI2CTest,
       AddressNackReturnsNackAndDoesNotPublishPayloadProgress) {
    initialize();
    wire.acknowledge = {false};
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred = 99U;
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_NACK);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0xa0U}));
    EXPECT_EQ(wire.stops, 1U);
    EXPECT_FALSE(nx_software_i2c_faulted(&owner));
}

TEST_F(SoftwareI2CTest, DataNackKeepsOnlyCompletedAckedBytes) {
    initialize();
    wire.acknowledge = {true, true, false};
    std::array<uint8_t, 2> data{0x24U, 0x25U};
    nx_i2c_message_t message = nx_i2c_message_write(data.data(), data.size());
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_NACK);
    EXPECT_EQ(transferred, 1U);
    EXPECT_FALSE(nx_software_i2c_faulted(&owner));
}

TEST_F(SoftwareI2CTest, ClockStretchObservesTheInputInsteadOfOutputLatch) {
    initialize();
    wire.stretch_each_release = 2U;
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_SUCCESS);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0xa0U, 0x24U}));
    EXPECT_EQ(transferred, 1U);
}

TEST_F(SoftwareI2CTest, AStuckClockAndFailedStopRetainThePhysicalOwner) {
    initialize();
    wire.force_scl_low = true;
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    nx_result_t result = transfer(&message, 1U, &transferred, wire.now + 100U);
    EXPECT_NE(result, NX_SUCCESS);
    EXPECT_TRUE(nx_software_i2c_faulted(&owner));
    EXPECT_EQ(owner.config, &config);
    nx_i2c_port_t port = nx_software_i2c_port(&owner);
    EXPECT_EQ(nx_i2c_port_stop(&port), NX_ERROR_BUSY);
    wire.force_scl_low = false;
    EXPECT_EQ(nx_software_i2c_recover(&owner, wire.now + 10000U), NX_SUCCESS);
    EXPECT_FALSE(nx_software_i2c_faulted(&owner));
}

TEST_F(SoftwareI2CTest, FrozenClockWaitIsBoundedEvenWithAFarDeadline) {
    initialize();
    wire.freeze = true;
    unsigned reads = wire.clock_reads;
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_IO);
    EXPECT_LT(wire.clock_reads - reads, 300U);
    EXPECT_TRUE(nx_software_i2c_faulted(&owner));
}

TEST_F(SoftwareI2CTest, ArbitrationReleasesWithoutGeneratingStopOrReplay) {
    initialize();
    wire.collide_on_one = true;
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_ARBITRATION);
    EXPECT_EQ(wire.stops, 0U);
    EXPECT_TRUE(nx_software_i2c_faulted(&owner));
    EXPECT_TRUE(wire.pins[0].released);
    EXPECT_TRUE(wire.pins[1].released);
    EXPECT_EQ(nx_software_i2c_cleanup_result(&owner), NX_ERROR_STATE);
    wire.collide_on_one = false;
    wire.frame = false;
    unsigned rises = wire.rises;
    EXPECT_EQ(nx_software_i2c_recover(&owner, wire.now + 10000U), NX_SUCCESS);
    EXPECT_LE(wire.rises - rises, 1U);
    EXPECT_TRUE(wire.tx_bytes.empty());
}

TEST_F(SoftwareI2CTest,
       FailedStopDoesNotRetainPayloadButBlocksAnotherTransfer) {
    initialize();
    wire.fail_stop = true;
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_IO);
    EXPECT_EQ(transferred, 1U);
    EXPECT_TRUE(owner.faulted);
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_STATE);
    wire.fail_stop = false;
    EXPECT_EQ(nx_software_i2c_stop(&owner, wire.now + 10000U), NX_SUCCESS);
    EXPECT_EQ(owner.config, nullptr);
}

TEST_F(SoftwareI2CTest, InvalidMessagesAndAliasesRejectBeforeAnyEdge) {
    initialize();
    unsigned writes = wire.writes;
    uint8_t data{};
    nx_i2c_message_t message = nx_i2c_message_write(&data, 0U);
    size_t transferred = 73U;
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_INVALID);
    message.length = 257U;
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_UNSUPPORTED);
    message.length = 1U;
    EXPECT_EQ(transfer(&message, 0U, &transferred), NX_ERROR_INVALID);
    EXPECT_EQ(transfer(&message, 9U, &transferred), NX_ERROR_UNSUPPORTED);
    message.buffer.tx = reinterpret_cast<uint8_t*>(&owner);
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_INVALID);
    EXPECT_EQ(wire.writes, writes);
    EXPECT_EQ(transferred, 73U);
}

TEST_F(SoftwareI2CTest,
       ExpiredAdmissionMakesNoEdgeAndPortRecoverIsUnsupported) {
    initialize();
    unsigned writes = wire.writes;
    uint8_t data{};
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred, wire.now), NX_ERROR_TIMEOUT);
    EXPECT_EQ(wire.writes, writes);
    nx_i2c_port_t port = nx_software_i2c_port(&owner);
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_i2c_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(owner.config, nullptr);
}

TEST_F(SoftwareI2CTest, OwnerIdentityAndAsyncMethodsRemainDistinct) {
    initialize();
    nx_i2c_endpoint_t endpoint = nx_software_i2c_endpoint(&child);
    nx_i2c_port_t port = nx_software_i2c_port(&owner);
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&endpoint, &port));
    ASSERT_NE(endpoint.ops, nullptr);
    EXPECT_EQ(endpoint.ops->submit, nullptr);
    EXPECT_EQ(endpoint.ops->start_admitted, nullptr);
    nx_software_i2c_t other{};
    nx_i2c_port_t wrong = nx_software_i2c_port(&other);
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&endpoint, &wrong));
    child.address = 0x78U;
    EXPECT_EQ(nx_software_i2c_endpoint(&child).ops, nullptr);
}

TEST_F(SoftwareI2CTest, ReentrantWireCallbacksCannotStopOrRecoverTheOwner) {
    initialize();
    bool called{};
    wire.on_write = [&] {
        if (!called) {
            called = true;
            EXPECT_EQ(nx_software_i2c_recover(&owner, 20000U), NX_ERROR_BUSY);
            EXPECT_EQ(nx_software_i2c_stop(&owner, 20000U), NX_ERROR_BUSY);
        }
    };
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_SUCCESS);
    EXPECT_TRUE(called);
}

TEST_F(SoftwareI2CTest, SccbHasDistinctDontCareAndStopSeparatedReadCycles) {
    ASSERT_EQ(nx_software_sccb_init(&owner, &config, 10000U), NX_SUCCESS);
    wire.all_ack = false;
    nx_software_sccb_device_t camera{&owner, 0x60U};
    EXPECT_EQ(nx_software_i2c_endpoint(&child).ops, nullptr);
    EXPECT_EQ(nx_software_sccb_write_register(&camera, 0x12U, 0x80U, 10000U),
              NX_SUCCESS);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0x60U, 0x12U, 0x80U}));
    wire.tx_bytes.clear();
    wire.incoming = {0x26U};
    wire.input_index = 0U;
    unsigned starts = wire.starts;
    unsigned stops = wire.stops;
    uint8_t result = 0xa5U;
    EXPECT_EQ(nx_software_sccb_read_register(&camera, 0x0aU, &result, 10000U),
              NX_SUCCESS);
    EXPECT_EQ(result, 0x26U);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0x60U, 0x0aU, 0x61U}));
    EXPECT_EQ(wire.starts - starts, 2U);
    EXPECT_EQ(wire.stops - stops, 2U);
    EXPECT_TRUE(wire.master_nacks.back());
}

TEST_F(SoftwareI2CTest,
       MaximumMessageGeometryHasNoHiddenSmallerTransportLimit) {
    initialize();
    std::array<uint8_t, NX_I2C_BOUNDED_MAX_MESSAGE_BYTES> payload{};
    std::array<nx_i2c_message_t, NX_I2C_BOUNDED_MAX_MESSAGES> messages{};
    for (auto& message : messages) {
        message = nx_i2c_message_write(payload.data(), payload.size());
    }
    size_t transferred{};
    EXPECT_EQ(
        transfer(messages.data(), messages.size(), &transferred, 1000000U),
        NX_SUCCESS);
    EXPECT_EQ(transferred, payload.size() * messages.size());
    EXPECT_EQ(wire.tx_bytes.size(), (payload.size() + 1U) * messages.size());
    EXPECT_EQ(wire.starts, messages.size());
    EXPECT_EQ(wire.stops, 1U);
}

TEST_F(SoftwareI2CTest, OneDeadlineIncludesStopAndCannotWithdrawAnIssuedByte) {
    initialize();
    uint8_t data = 0x24U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    bool interrupted{};
    wire.on_write = [&] {
        if (!interrupted && wire.tx_bytes.size() == 2U) {
            interrupted = true;
            wire.now = 10000U;
        }
    };
    EXPECT_EQ(transfer(&message, 1U, &transferred, 10000U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(interrupted);
    EXPECT_EQ(wire.tx_bytes, (std::vector<uint8_t>{0xa0U, 0x24U}));
    EXPECT_EQ(nx_software_i2c_cleanup_result(&owner), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(owner.faulted);
    EXPECT_EQ(owner.config, &config);
    wire.on_write = nullptr;
    EXPECT_EQ(nx_software_i2c_recover(&owner, 20000U), NX_SUCCESS);
    EXPECT_EQ(wire.tx_bytes.size(), 2U);
}

TEST_F(SoftwareI2CTest, RegressedClockCannotBeRepairedByANumericalDeadline) {
    initialize();
    wire.now = 0U;
    unsigned writes = wire.writes;
    uint8_t data{};
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_IO);
    EXPECT_EQ(wire.writes, writes);
    EXPECT_TRUE(owner.clock_failed);
    wire.now = 20000U;
    EXPECT_EQ(nx_software_i2c_recover(&owner, 30000U), NX_ERROR_IO);
    EXPECT_EQ(nx_software_i2c_stop(&owner, 30000U), NX_SUCCESS);
    EXPECT_EQ(owner.config, nullptr);
    EXPECT_EQ(nx_software_i2c_init(&owner, &config, 30000U), NX_SUCCESS);
}

TEST_F(SoftwareI2CTest, PinReadFailurePreservesTheActualErrorAndRetainedOwner) {
    initialize();
    wire.fail_read = true;
    uint8_t data{};
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t transferred{};
    EXPECT_EQ(transfer(&message, 1U, &transferred), NX_ERROR_IO);
    EXPECT_EQ(transferred, 0U);
    EXPECT_TRUE(owner.faulted);
    wire.fail_read = false;
    EXPECT_EQ(nx_software_i2c_stop(&owner, 20000U), NX_SUCCESS);
}

TEST_F(SoftwareI2CTest, SccbReadCannotPublishBeforeItsFinalStopSettles) {
    ASSERT_EQ(nx_software_sccb_init(&owner, &config, 10000U), NX_SUCCESS);
    nx_software_sccb_device_t camera{&owner, 0x60U};
    wire.all_ack = false;
    wire.on_write = [&] {
        if (wire.starts == 2U) {
            wire.fail_stop = true;
        }
    };
    uint8_t result = 0xa5U;
    EXPECT_EQ(nx_software_sccb_read_register(&camera, 0x0aU, &result, 10000U),
              NX_ERROR_IO);
    EXPECT_EQ(result, 0xa5U);
    EXPECT_EQ(wire.stops, 1U);
    EXPECT_TRUE(owner.faulted);
}

TEST_F(SoftwareI2CTest, SccbCannotSilentlyWeakenAStrictOwnerOrI2cFace) {
    initialize();
    nx_software_sccb_device_t camera{&owner, 0x60U};
    uint8_t result = 0xa5U;
    EXPECT_EQ(nx_software_sccb_write_register(&camera, 0x12U, 0x80U, 10000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_software_sccb_read_register(&camera, 0x0aU, &result, 10000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(result, 0xa5U);
    EXPECT_TRUE(wire.tx_bytes.empty());
}
} /* namespace */
