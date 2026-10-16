/**
 * \file            flash_provenance_test.cpp
 * \brief           Physical erase provenance through the real storage adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/flash_storage.h"
#include "nexus/io/native/model.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
/** \brief Observe external physical pulses, never mock the storage code. */
class PhysicalPulses {
  public:
    MOCK_METHOD(void, Erase, (uint32_t));
    MOCK_METHOD(void, Program, (uint32_t));
};
/** \brief An ECC device whose hidden pulse history cannot be read as data. */
class EccDevice {
  public:
    testing::StrictMock<PhysicalPulses> pulses;
    std::array<uint8_t, 512> memory;
    std::array<bool, 64> fresh{};
    bool fail_next = false;
    const nx_flash_sector_t sectors[2]{{0U, 256U}, {256U, 256U}};
    const nx_flash_geometry_t geometry{
        0U, 512U, 8U, sectors, 2U, NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
    EccDevice() {
        memory.fill(0xffU);
    }
    static const nx_flash_geometry_t* Geometry(const void* context) {
        return &static_cast<const EccDevice*>(context)->geometry;
    }
    static nx_result_t Read(const void* context, uint32_t offset, void* data,
                            size_t length) {
        const auto& self = *static_cast<const EccDevice*>(context);
        if (offset > self.memory.size() || length > self.memory.size() - offset)
            return NX_ERROR_INVALID;
        std::memcpy(data, self.memory.data() + offset, length);
        return NX_SUCCESS;
    }
    static nx_result_t Program(void* context, uint32_t offset, const void* data,
                               size_t length, nx_time_us_t) {
        auto& self = *static_cast<EccDevice*>(context);
        if (length != 8U || offset % 8U != 0U || offset > 504U)
            return NX_ERROR_INVALID;
        const auto* input = static_cast<const uint8_t*>(data);
        bool erased_input = true;
        for (size_t i = 0U; i < length; ++i)
            erased_input &= input[i] == UINT8_MAX;
        if (erased_input)
            return NX_SUCCESS;
        if (!self.fresh[offset / 8U])
            return NX_ERROR_IO;
        /* Even a failed pulse with no visible data consumes its ECC unit. */
        self.fresh[offset / 8U] = false;
        self.pulses.Program(offset);
        if (self.fail_next) {
            self.fail_next = false;
            return NX_ERROR_IO;
        }
        std::memcpy(self.memory.data() + offset, data, length);
        return NX_SUCCESS;
    }
    static nx_result_t Erase(void* context, uint32_t offset, size_t length,
                             nx_time_us_t) {
        auto& self = *static_cast<EccDevice*>(context);
        if (length != 256U || (offset != 0U && offset != 256U))
            return NX_ERROR_INVALID;
        self.pulses.Erase(offset);
        std::memset(self.memory.data() + offset, 0xff, length);
        for (size_t i = offset / 8U; i < (offset + length) / 8U; ++i)
            self.fresh[i] = true;
        return NX_SUCCESS;
    }
    const nx_flash_ops_t ops{Geometry, Read, Program, Erase};
    const nx_flash_port_t face{&ops, this};
};
class FlashProvenance : public testing::Test {
  protected:
    EccDevice device;
    nx_flash_storage_adapter_t adapter{};
    nx_storage_port_t port{};
    nx_storage_t storage{};
    uint8_t workspace[32]{};
    void SetUp() override {
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_configure(true, 0U));
        const nx_flash_region_t region{&device.face, 0U, 512U, true};
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_storage_bind(&adapter, region, 10000U, &port));
    }
    void Open() {
        ASSERT_EQ(NX_STORAGE_OK, nx_storage_open(&storage, &port, 0U, 256U,
                                                 workspace, sizeof(workspace)));
    }
    void ExpectSave(uint32_t bank) {
        testing::InSequence ordered;
        EXPECT_CALL(device.pulses, Erase(bank));
        for (uint32_t offset : {0U, 8U, 16U, 24U, 40U, 48U})
            EXPECT_CALL(device.pulses, Program(bank + offset));
        EXPECT_CALL(device.pulses, Program(bank + 32U));
    }
};
TEST_F(FlashProvenance, AllFfReadCannotEstablishPhysicalEraseHistory) {
    EXPECT_EQ(NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE,
              nx_flash_port_geometry(&device.face)->program_flags);
    uint8_t observed[8]{};
    ASSERT_EQ(NX_STORAGE_OK,
              port.read(port.ctx, 0U, observed, sizeof(observed)));
    EXPECT_THAT(observed, testing::Each(0xffU));
    const uint8_t value[8]{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    EXPECT_EQ(NX_STORAGE_IO, port.program(port.ctx, 0U, value, sizeof(value)));
    EXPECT_CALL(device.pulses, Erase(0U));
    ASSERT_EQ(NX_STORAGE_OK, port.erase(port.ctx, 0U, 256U));
    EXPECT_CALL(device.pulses, Program(0U));
    ASSERT_EQ(NX_STORAGE_OK, port.program(port.ctx, 0U, value, sizeof(value)));
    EXPECT_EQ(NX_STORAGE_IO, port.program(port.ctx, 0U, value, sizeof(value)));
}
TEST_F(FlashProvenance, RepeatedSaveErasesFirstAndCommitsSeparateFullUnitLast) {
    Open();
    const uint8_t payload[16]{1U, 2U,  3U,  4U,  5U,  6U,  7U,  8U,
                              9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U};
    ExpectSave(0U);
    ASSERT_EQ(NX_STORAGE_OK,
              nx_storage_save(&storage, payload, sizeof(payload)));
    ExpectSave(256U);
    ASSERT_EQ(NX_STORAGE_OK,
              nx_storage_save(&storage, payload, sizeof(payload)));
    ExpectSave(0U);
    ASSERT_EQ(NX_STORAGE_OK,
              nx_storage_save(&storage, payload, sizeof(payload)));
    uint8_t result[16]{};
    size_t size = sizeof(result);
    ASSERT_EQ(NX_STORAGE_OK, nx_storage_load(&storage, result, &size));
    EXPECT_EQ(3U, storage.generation);
    EXPECT_EQ(0, std::memcmp(result, payload, sizeof(payload)));
}
TEST_F(FlashProvenance, InvisibleFailedPulseRequiresReeraseAfterReopen) {
    Open();
    const uint8_t old[16]{1U};
    const uint8_t next[16]{2U};
    ExpectSave(0U);
    ASSERT_EQ(NX_STORAGE_OK, nx_storage_save(&storage, old, sizeof(old)));
    {
        testing::InSequence ordered;
        EXPECT_CALL(device.pulses, Erase(256U));
        EXPECT_CALL(device.pulses, Program(256U));
    }
    device.fail_next = true;
    EXPECT_EQ(NX_STORAGE_IO, nx_storage_save(&storage, next, sizeof(next)));
    EXPECT_FALSE(storage.opened);
    EXPECT_EQ(0xffU, device.memory[256U]);
    EXPECT_FALSE(device.fresh[32U]);
    Open();
    EXPECT_EQ(1U, storage.generation);
    ExpectSave(256U);
    ASSERT_EQ(NX_STORAGE_OK, nx_storage_save(&storage, next, sizeof(next)));
    uint8_t result[16]{};
    size_t size = sizeof(result);
    ASSERT_EQ(NX_STORAGE_OK, nx_storage_load(&storage, result, &size));
    EXPECT_EQ(0, std::memcmp(next, result, sizeof(next)));
}
} /* namespace */
