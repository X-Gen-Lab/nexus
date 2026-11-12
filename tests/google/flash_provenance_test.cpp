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
    const nx_flash_erase_span_t spans[1]{{0U, 256U, 2U}};
    const nx_flash_geometry_t geometry{
        0U, 512U, 8U, spans, 1U, NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
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
    const nx_flash_ops_t ops{Geometry, Read, Program, Erase, nullptr};
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

namespace {
class FlashGeometry : public testing::Test {
  protected:
    std::array<nx_flash_erase_span_t, 3> spans{{{0U, 0x4000U, 4U},
                                                {0x10000U, 0x10000U, 1U},
                                                {0x20000U, 0x20000U, 3U}}};
    nx_flash_geometry_t geometry{0x08000000U,  0x80000U, 4U,
                                 spans.data(), 3U,       0U};
};

TEST_F(FlashGeometry, InternalFlashBoundariesAreValuesFromOnlyThreeSpans) {
    uint32_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_count(&geometry, &count));
    EXPECT_EQ(8U, count);
    for (const auto& expected :
         std::array<nx_flash_sector_t, 5>{{{0U, 0x4000U},
                                           {0xc000U, 0x4000U},
                                           {0x10000U, 0x10000U},
                                           {0x20000U, 0x20000U},
                                           {0x60000U, 0x20000U}}}) {
        uint32_t index = UINT32_MAX;
        ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_index(
                                  &geometry, expected.offset, &index));
        nx_flash_sector_t sector{17U, 23U};
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_geometry_sector_at(&geometry, index, &sector));
        EXPECT_EQ(expected.offset, sector.offset);
        EXPECT_EQ(expected.size, sector.size);
    }
    spans[2].count = 7U;
    geometry.size = 0x100000U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_count(&geometry, &count));
    EXPECT_EQ(12U, count);
    nx_flash_sector_t last{};
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_at(&geometry, 11U, &last));
    EXPECT_EQ(0xe0000U, last.offset);
    EXPECT_EQ(geometry.size, last.offset + last.size);
}

TEST_F(FlashGeometry, UniformSixteenMiBNorHasOneDescriptorAnd4096Sectors) {
    const nx_flash_erase_span_t uniform{0U, 4096U, 4096U};
    geometry = {0U, 0x1000000U, 1U, &uniform, 1U, 0U};
    uint32_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_count(&geometry, &count));
    EXPECT_EQ(4096U, count);
    nx_flash_sector_t last{};
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_at(&geometry, 4095U, &last));
    EXPECT_EQ(0xfff000U, last.offset);
    EXPECT_EQ(4096U, last.size);
    EXPECT_EQ(12U, sizeof(uniform));
}

TEST_F(FlashGeometry, IndexDoesNotRoundOffsetsOrAcceptTheOnePastEndSector) {
    uint32_t index = 71U;
    for (uint32_t offset : {1U, 0x4001U, 0x1ffffU, geometry.size, UINT32_MAX}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_geometry_sector_index(&geometry, offset, &index));
        EXPECT_EQ(71U, index);
    }
    nx_flash_sector_t sector{17U, 23U};
    for (uint32_t ordinal : {8U, UINT32_MAX}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_geometry_sector_at(&geometry, ordinal, &sector));
        EXPECT_EQ(17U, sector.offset);
        EXPECT_EQ(23U, sector.size);
    }
}

TEST_F(FlashGeometry, LargestRepresentableCountAndLastSectorDoNotWrap) {
    const nx_flash_erase_span_t uniform{0U, 1U, UINT32_MAX};
    geometry = {0U, UINT32_MAX, 1U, &uniform, 1U, 0U};
    uint32_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_count(&geometry, &count));
    EXPECT_EQ(UINT32_MAX, count);
    nx_flash_sector_t sector{};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_geometry_sector_at(&geometry, UINT32_MAX - 1U, &sector));
    EXPECT_EQ(UINT32_MAX - 1U, sector.offset);
    EXPECT_EQ(1U, sector.size);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_geometry_sector_at(&geometry, UINT32_MAX, &sector));
    EXPECT_EQ(UINT32_MAX - 1U, sector.offset);
}

TEST_F(FlashGeometry, MalformedSpansNeverPublishPartialQueryResults) {
    const auto original = spans;
    for (unsigned fault = 0U; fault < 9U; ++fault) {
        spans = original;
        geometry.size = 0x80000U;
        geometry.erase_span_count = spans.size();
        switch (fault) {
            case 0U:
                spans[0].count = 0U;
                break;
            case 1U:
                spans[0].erase_bytes = 0U;
                break;
            case 2U:
                spans[1].offset += 4U;
                break;
            case 3U:
                spans[1].offset -= 4U;
                break;
            case 4U:
                spans[2].count -= 1U;
                break;
            case 5U:
                spans[2].count = UINT32_MAX;
                break;
            case 6U:
                spans[0].offset = UINT32_MAX;
                break;
            case 7U:
                geometry.size = 0U;
                break;
            case 8U:
                geometry.erase_span_count = 0U;
                break;
        }
        SCOPED_TRACE(fault);
        uint32_t count = 71U;
        nx_flash_sector_t sector{17U, 23U};
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_geometry_sector_count(&geometry, &count));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_geometry_sector_at(&geometry, 0U, &sector));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_geometry_sector_index(&geometry, 0U, &count));
        EXPECT_EQ(71U, count);
        EXPECT_EQ(17U, sector.offset);
        EXPECT_EQ(23U, sector.size);
    }
}

TEST_F(FlashGeometry, OutputAliasesCannotModifyAnyInputMetadata) {
    const auto original = geometry;
    const auto original_spans = spans;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_geometry_sector_count(&geometry, &geometry.size));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_geometry_sector_index(&geometry, 0U, &spans[0].count));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_flash_geometry_sector_at(
            &geometry, 0U, reinterpret_cast<nx_flash_sector_t*>(&spans[0])));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_flash_geometry_sector_at(
            &geometry, 0U, reinterpret_cast<nx_flash_sector_t*>(&geometry)));
    EXPECT_EQ(0, std::memcmp(&original, &geometry, sizeof(geometry)));
    EXPECT_EQ(0,
              std::memcmp(original_spans.data(), spans.data(), sizeof(spans)));
}

TEST_F(FlashGeometry, OutputImmediatelyAfterTheSpanExtentIsDisjoint) {
    struct {
        nx_flash_erase_span_t spans[3];
        nx_flash_sector_t output;
    } storage{
        {{0U, 0x4000U, 4U}, {0x10000U, 0x10000U, 1U}, {0x20000U, 0x20000U, 3U}},
        {17U, 23U}};
    geometry.erase_spans = storage.spans;
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_geometry_sector_at(&geometry, 7U, &storage.output));
    EXPECT_EQ(0x60000U, storage.output.offset);
    EXPECT_EQ(0x20000U, storage.output.size);
}

class ExternalFlash {
  public:
    const nx_flash_geometry_t* selected;
    MOCK_METHOD(nx_result_t, Read, (uint32_t, void*, size_t));
    MOCK_METHOD(nx_result_t, Program,
                (uint32_t, const void*, size_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Erase, (uint32_t, size_t, nx_time_us_t));
};
const nx_flash_ops_t operations{
    [](const void* p) {
        return static_cast<const ExternalFlash*>(p)->selected;
    },
    [](const void* p, uint32_t o, void* d, size_t n) {
        return const_cast<ExternalFlash*>(static_cast<const ExternalFlash*>(p))
            ->Read(o, d, n);
    },
    [](void* p, uint32_t o, const void* d, size_t n, nx_time_us_t deadline) {
        return static_cast<ExternalFlash*>(p)->Program(o, d, n, deadline);
    },
    [](void* p, uint32_t o, size_t n, nx_time_us_t deadline) {
        return static_cast<ExternalFlash*>(p)->Erase(o, n, deadline);
    },
    nullptr};

TEST_F(FlashGeometry,
       RealRegionRejectsMalformedGeometryBeforePhysicalDispatch) {
    testing::StrictMock<ExternalFlash> hardware;
    hardware.selected = &geometry;
    const nx_flash_port_t flash{&operations, &hardware};
    const nx_flash_region_t region{&flash, 0U, geometry.size, true};
    spans[1].offset += 4U;
    uint32_t output = 0x55555555U;
    EXPECT_CALL(hardware, Read).Times(0);
    EXPECT_CALL(hardware, Program).Times(0);
    EXPECT_CALL(hardware, Erase).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_region_validate(&region));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_region_read(&region, 0U, &output, sizeof(output)));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_region_program(&region, 0U, &output,
                                                        sizeof(output), 100U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_region_erase(&region, 0U, 0x4000U, 100U));
    EXPECT_EQ(0x55555555U, output);
}

TEST_F(FlashGeometry,
       RealRegionCrossesSpansOnlyAtTheSelectedPhysicalBoundaries) {
    testing::StrictMock<ExternalFlash> hardware;
    hardware.selected = &geometry;
    const nx_flash_port_t flash{&operations, &hardware};
    const nx_flash_region_t region{&flash, 0xc000U, 0x34000U, true};
    EXPECT_CALL(hardware, Erase(0xc000U, 0x34000U, 100U))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_region_erase(&region, 0U, region.size, 100U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_region_erase(&region, 0U, region.size + 4U, 100U));
}
} /* namespace */
