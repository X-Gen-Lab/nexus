/**
 * \file            mcuboot_memory_model.h
 * \brief           Shared normal Flash-port model for upstream API contracts.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_MCUBOOT_MEMORY_MODEL_H
#define NEXUS_TEST_MCUBOOT_MEMORY_MODEL_H
#include "flash_map_backend/flash_map_backend.h"
#include "nexus/components/mcuboot_control.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace nexus_mcuboot_test {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

class Device {
  public:
    MOCK_METHOD(const nx_flash_geometry_t*, Geometry, ());
    MOCK_METHOD(nx_result_t, Read, (uint32_t, void*, size_t));
    MOCK_METHOD(nx_result_t, Program,
                (uint32_t, const void*, size_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Erase, (uint32_t, size_t, nx_time_us_t));
};

inline const nx_flash_geometry_t* geometry(const void* context) {
    return static_cast<Device*>(const_cast<void*>(context))->Geometry();
}
inline nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    return static_cast<Device*>(const_cast<void*>(context))
        ->Read(offset, data, length);
}
inline nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    return static_cast<Device*>(context)->Program(offset, data, length,
                                                  deadline);
}
inline nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    return static_cast<Device*>(context)->Erase(offset, length, deadline);
}
inline const nx_flash_ops_t operations{geometry, read, program, erase, nullptr};
inline const nx_mcuboot_flash_operation_t* active = nullptr;
inline nx_time_us_t clock_now = 100U;

inline nx_time_us_t read_clock(void* context) {
    return *static_cast<const nx_time_us_t*>(context);
}

class Memory : public ::testing::Test {
  protected:
    void SetUp() override {
        bytes.fill(0xffU);
        active = &operation;
        clock_now = 100U;
        EXPECT_CALL(mock, Geometry())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Return(&physical));
        EXPECT_CALL(mock, Read(_, _, _))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(
                Invoke([this](uint32_t offset, void* data, size_t length) {
                    std::memcpy(data, bytes.data() + offset, length);
                    return NX_SUCCESS;
                }));
    }
    void TearDown() override {
        active = nullptr;
    }
    void allow_programs() {
        EXPECT_CALL(mock, Program(_, _, _, 1000U))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Invoke([this](uint32_t offset, const void* data,
                                          size_t length, nx_time_us_t) {
                const auto* input = static_cast<const uint8_t*>(data);
                for (size_t index = 0U; index < length; ++index) {
                    bytes[offset + index] &= input[index];
                }
                ++programs;
                return NX_SUCCESS;
            }));
    }
    void allow_erases() {
        EXPECT_CALL(mock, Erase(_, _, 1000U))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(
                Invoke([this](uint32_t offset, size_t length, nx_time_us_t) {
                    std::fill(bytes.begin() + offset,
                              bytes.begin() + offset + length, 0xffU);
                    ++erases;
                    return NX_SUCCESS;
                }));
    }
    StrictMock<Device> mock;
    const std::array<nx_flash_erase_span_t, 3> spans{
        {{0U, 0x4000U, 4U},
         {0x10000U, 0x10000U, 1U},
         {0x20000U, 0x20000U, 3U}}};
    nx_flash_geometry_t physical{0x08000000U,  0x80000U,     4U,
                                 spans.data(), spans.size(), 0U};
    const nx_flash_port_t port{&operations, &mock};
    nx_flash_region_t primary_region{&port, 0x20000U, 0x20000U, true};
    nx_flash_region_t secondary_region{&port, 0x40000U, 0x20000U, true};
    const struct flash_area primary {
        NX_MCUBOOT_PRIMARY_AREA_ID, &primary_region
    };
    const struct flash_area secondary {
        NX_MCUBOOT_SECONDARY_AREA_ID, &secondary_region
    };
    const nx_mcuboot_flash_operation_t operation{
        &primary, &secondary, {read_clock, &clock_now}, 1000U};
    std::array<uint8_t, 0x80000U> bytes{};
    size_t programs = 0U;
    size_t erases = 0U;
};

} /* namespace nexus_mcuboot_test */
#endif
