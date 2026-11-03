/**
 * \file            flash_map_backend_test.cpp
 * \brief           Ordinary Flash-map API over real Nexus region checks.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "flash_map_backend/flash_map_backend.h"
#include <array>
#include <cerrno>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class FlashPort {
  public:
    MOCK_METHOD(const nx_flash_geometry_t*, Geometry, ());
    MOCK_METHOD(nx_result_t, Read, (uint32_t, void*, size_t));
    MOCK_METHOD(nx_result_t, Program,
                (uint32_t, const void*, size_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Erase, (uint32_t, size_t, nx_time_us_t));
};

const nx_flash_geometry_t* geometry(const void* context) {
    return static_cast<FlashPort*>(const_cast<void*>(context))->Geometry();
}
nx_result_t read(const void* context, uint32_t offset, void* data,
                 size_t length) {
    return static_cast<FlashPort*>(const_cast<void*>(context))
        ->Read(offset, data, length);
}
nx_result_t program(void* context, uint32_t offset, const void* data,
                    size_t length, nx_time_us_t deadline) {
    return static_cast<FlashPort*>(context)->Program(offset, data, length,
                                                     deadline);
}
nx_result_t erase(void* context, uint32_t offset, size_t length,
                  nx_time_us_t deadline) {
    return static_cast<FlashPort*>(context)->Erase(offset, length, deadline);
}
const nx_flash_ops_t operations{geometry, read, program, erase};
const nx_mcuboot_flash_operation_t* product_operation = nullptr;
nx_time_us_t clock_now = 100U;
size_t clock_reads = 0U;

nx_time_us_t read_clock(void* context) {
    ++clock_reads;
    return *static_cast<const nx_time_us_t*>(context);
}

class FlashMap : public ::testing::Test {
  protected:
    void SetUp() override {
        EXPECT_CALL(mock, Geometry())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Return(&physical));
        product_operation = &operation;
        clock_now = 100U;
        clock_reads = 0U;
    }
    void TearDown() override {
        product_operation = nullptr;
    }
    const struct flash_area* open(uint8_t id) {
        const struct flash_area* area = nullptr;
        EXPECT_EQ(0, flash_area_open(id, &area));
        return area;
    }
    StrictMock<FlashPort> mock;
    const std::array<nx_flash_sector_t, 8> sectors{{{0U, 0x4000U},
                                                    {0x4000U, 0x4000U},
                                                    {0x8000U, 0x4000U},
                                                    {0xc000U, 0x4000U},
                                                    {0x10000U, 0x10000U},
                                                    {0x20000U, 0x20000U},
                                                    {0x40000U, 0x20000U},
                                                    {0x60000U, 0x20000U}}};
    nx_flash_geometry_t physical{0x08000000U,    0x80000U,       4U,
                                 sectors.data(), sectors.size(), 0U};
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
};

TEST_F(FlashMap, OpenIsPureStableAndNestedCloseNeverWithdrawsProvider) {
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_CALL(mock, Program(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, Erase(_, _, _)).Times(0);
    const auto* first = open(NX_MCUBOOT_PRIMARY_AREA_ID);
    const auto* nested = open(NX_MCUBOOT_PRIMARY_AREA_ID);
    ASSERT_EQ(&primary, first);
    EXPECT_EQ(first, nested);
    flash_area_close(first);
    flash_area_close(nested);
    EXPECT_EQ(first, open(NX_MCUBOOT_PRIMARY_AREA_ID));
    EXPECT_EQ(0U, clock_reads);
}

TEST_F(FlashMap, AccessorsUseTheRegionAndFullPhysicalDeviceBase) {
    const auto* area = open(NX_MCUBOOT_PRIMARY_AREA_ID);
    ASSERT_NE(nullptr, area);
    EXPECT_EQ(1U, flash_area_get_id(area));
    EXPECT_EQ(0U, flash_area_get_device_id(area));
    EXPECT_EQ(0x20000U, flash_area_get_off(area));
    EXPECT_EQ(0x20000U, flash_area_get_size(area));
    EXPECT_EQ(4U, flash_area_align(area));
    EXPECT_EQ(0xffU, flash_area_erased_val(area));
    uintptr_t base = 0U;
    ASSERT_EQ(0, flash_device_base(0U, &base));
    EXPECT_EQ(0x08000000U, base);
}

TEST_F(FlashMap, RoleMappingRejectsUnsupportedImageOrSlot) {
    EXPECT_EQ(1, flash_area_id_from_multi_image_slot(0, 0));
    EXPECT_EQ(2, flash_area_id_from_image_slot(1));
    EXPECT_EQ(0, flash_area_id_to_multi_image_slot(0, 1));
    EXPECT_EQ(1, flash_area_id_to_multi_image_slot(0, 2));
    EXPECT_EQ(-1, flash_area_id_from_multi_image_slot(1, 0));
    EXPECT_EQ(-1, flash_area_id_from_multi_image_slot(0, 2));
    EXPECT_EQ(-1, flash_area_id_to_multi_image_slot(1, 1));
    EXPECT_EQ(-1, flash_area_id_to_multi_image_slot(0, 0));
}

TEST_F(FlashMap, ActualNonuniformGeometryYieldsAreaRelativeSectorOffsets) {
    std::array<struct flash_sector, 1> output{};
    uint32_t count = 1U;
    ASSERT_EQ(0, flash_area_get_sectors(2, &count, output.data()));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0U, flash_sector_get_off(&output[0]));
    EXPECT_EQ(0x20000U, flash_sector_get_size(&output[0]));
    struct flash_sector containing {};
    ASSERT_EQ(0, flash_area_get_sector(&primary, 0x10000U, &containing));
    EXPECT_EQ(0U, containing.offset);
    EXPECT_EQ(0x20000U, containing.size);
    EXPECT_EQ(-EINVAL, flash_area_get_sector(&primary, 0x20000U, &containing));
}

TEST_F(FlashMap, SectorCapacityFailureReportsRequiredWithoutPartialOutput) {
    struct flash_sector untouched {
        77U, 88U
    };
    uint32_t count = 0U;
    EXPECT_EQ(-ENOMEM, flash_area_get_sectors(1, &count, &untouched));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(77U, untouched.offset);
    EXPECT_EQ(88U, untouched.size);
}

TEST_F(FlashMap, ReadUsesWholeSlotOffsetWithoutApplyingImagePrefix) {
    std::array<uint8_t, 4> output{};
    EXPECT_CALL(mock, Read(0x20000U, output.data(), output.size()))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(0, flash_area_read(&primary, 0U, output.data(), 4U));
}

TEST_F(FlashMap, ProgramUsesTheFixedDeadlineAndPreservesTheInputBorrow) {
    const std::array<uint8_t, 4> input{{1U, 2U, 3U, 4U}};
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_CALL(mock, Program(0x40004U, input.data(), input.size(), 1000U))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(0, flash_area_write(&secondary, 4U, input.data(), 4U));
}

TEST_F(FlashMap, TimeoutAndRetryKeepTheSameDeadlineWithoutRenewal) {
    const std::array<uint8_t, 4> input{};
    EXPECT_CALL(mock, Program(0x40000U, input.data(), input.size(), 1000U))
        .WillOnce(Return(NX_ERROR_TIMEOUT))
        .WillOnce(Return(NX_ERROR_TIMEOUT));
    EXPECT_EQ(-ETIMEDOUT, flash_area_write(&secondary, 0U, input.data(), 4U));
    clock_now = 999U;
    EXPECT_EQ(-ETIMEDOUT, flash_area_write(&secondary, 0U, input.data(), 4U));
    clock_now = 1000U;
    EXPECT_EQ(-ETIMEDOUT, flash_area_write(&secondary, 0U, input.data(), 4U));
}

TEST_F(FlashMap, NeverOrExpiredDeadlineIssuesNoProgramOrErase) {
    const std::array<uint8_t, 4> input{};
    EXPECT_CALL(mock, Program(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, Erase(_, _, _)).Times(0);
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    clock_now = 1000U;
    std::array<uint8_t, 4> output{};
    EXPECT_EQ(-ETIMEDOUT, flash_area_read(&primary, 0U, output.data(), 4U));
    EXPECT_EQ(-ETIMEDOUT, flash_area_erase(&secondary, 0U, 0x20000U));
    const nx_mcuboot_flash_operation_t infinite{
        &primary, &secondary, {read_clock, &clock_now}, NX_DEADLINE_NEVER};
    product_operation = &infinite;
    EXPECT_EQ(-EINVAL, flash_area_write(&secondary, 0U, input.data(), 4U));
}

TEST_F(FlashMap, RegionBoundsAndReadOnlyPermissionsBlockPhysicalAccess) {
    std::array<uint8_t, 4> data{};
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_CALL(mock, Program(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, Erase(_, _, _)).Times(0);
    EXPECT_EQ(-EINVAL, flash_area_read(&primary, 0x1ffffU, data.data(), 4U));
    EXPECT_EQ(-EINVAL, flash_area_erase(&secondary, 0U, 0x20004U));
    primary_region.writable = false;
    EXPECT_EQ(-EACCES, flash_area_write(&primary, 0U, data.data(), 4U));
}

TEST_F(FlashMap, WholeSectorEraseUsesTheRealRegionAndUnderlyingResult) {
    EXPECT_CALL(mock, Erase(0x40000U, 0x20000U, 1000U))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(0, flash_area_erase(&secondary, 0U, 0x20000U));
}

TEST_F(FlashMap, UnderlyingAlignmentAndDeviceErrorsAreNotSuccess) {
    const std::array<uint8_t, 4> input{};
    EXPECT_CALL(mock, Program(0x40001U, input.data(), input.size(), 1000U))
        .WillOnce(Return(NX_ERROR_INVALID));
    EXPECT_EQ(-EINVAL, flash_area_write(&secondary, 1U, input.data(), 4U));
    EXPECT_CALL(mock, Erase(0x40001U, 4U, 1000U))
        .WillOnce(Return(NX_ERROR_INVALID));
    EXPECT_EQ(-EINVAL, flash_area_erase(&secondary, 1U, 4U));
}

TEST_F(FlashMap, OpenRejectsPartialSectorOrIncompletePhysicalGeometry) {
    const struct flash_area* output = &primary;
    primary_region.offset += 4U;
    primary_region.size -= 4U;
    EXPECT_EQ(-EINVAL, flash_area_open(1U, &output));
    EXPECT_EQ(nullptr, output);
    primary_region.offset -= 4U;
    primary_region.size += 4U;
    physical.sector_count -= 1U;
    EXPECT_EQ(-EINVAL, flash_area_open(1U, &output));
}

TEST_F(FlashMap, EccAndForeignProgramUnitsAreNotThisSupportedProfile) {
    const struct flash_area* output = nullptr;
    physical.program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE;
    EXPECT_EQ(-ENOTSUP, flash_area_open(1U, &output));
    physical.program_flags = 0U;
    physical.program_unit = 32U;
    EXPECT_EQ(-ENOTSUP, flash_area_open(1U, &output));
}

TEST_F(FlashMap, UnknownAndForeignAreasCannotAcquirePhysicalAccess) {
    const struct flash_area* output = &primary;
    EXPECT_EQ(-EINVAL, flash_area_open(0U, &output));
    EXPECT_EQ(nullptr, output);
    EXPECT_EQ(-EINVAL, flash_area_open(1U, nullptr));
    const struct flash_area foreign { 1U, &primary_region };
    std::array<uint8_t, 4> data{};
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_EQ(-EINVAL, flash_area_read(&foreign, 0U, data.data(), 4U));
}

TEST_F(FlashMap, OperationAbsenceAndInvalidStaticSlotPairFailClosed) {
    const struct flash_area* output = nullptr;
    product_operation = nullptr;
    EXPECT_EQ(-EPERM, flash_area_open(1U, &output));
    const nx_mcuboot_flash_operation_t bad{
        &primary, &primary, {read_clock, &clock_now}, 1000U};
    product_operation = &bad;
    EXPECT_EQ(-EINVAL, flash_area_open(1U, &output));
}

TEST_F(FlashMap, ReadAndBusyFailuresPropagateWithoutLifecycleSideEffects) {
    std::array<uint8_t, 4> data{};
    EXPECT_CALL(mock, Read(0x20000U, data.data(), data.size()))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(-EIO, flash_area_read(&primary, 0U, data.data(), 4U));
    EXPECT_CALL(mock, Program(0x40000U, data.data(), data.size(), 1000U))
        .WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(-EBUSY, flash_area_write(&secondary, 0U, data.data(), 4U));
    flash_area_close(&secondary);
    EXPECT_EQ(&secondary, open(2U));
}
TEST_F(FlashMap, ReadOnlyMetadataRetainsNonuniformSectorSizes) {
    primary_region.offset = 0U;
    primary_region.size = 0x20000U;
    primary_region.writable = false;
    std::array<struct flash_sector, 5> output{};
    uint32_t count = 5U;
    ASSERT_EQ(0, flash_area_get_sectors(1, &count, output.data()));
    EXPECT_EQ(5U, count);
    EXPECT_EQ(0x4000U, output[0].size);
    EXPECT_EQ(0xc000U, output[3].offset);
    EXPECT_EQ(0x4000U, output[3].size);
    EXPECT_EQ(0x10000U, output[4].offset);
    EXPECT_EQ(0x10000U, output[4].size);
    EXPECT_EQ(0U, clock_reads);
}

TEST_F(FlashMap, DistinctStaticAreasStillRejectOverlappingOwnersOrDevices) {
    const struct flash_area* output = nullptr;
    secondary_region.offset = primary_region.offset;
    EXPECT_EQ(-EINVAL, flash_area_open(2U, &output));
    secondary_region.offset = 0x40000U;
    const nx_flash_port_t another_device{&operations, &mock};
    secondary_region.port = &another_device;
    EXPECT_EQ(-EINVAL, flash_area_open(2U, &output));
}

TEST_F(FlashMap, MissingClockReadRejectsPureAdmissionWithoutEffects) {
    const nx_mcuboot_flash_operation_t missing{
        &primary, &secondary, {nullptr, nullptr}, 1000U};
    product_operation = &missing;
    const struct flash_area* output = &primary;
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_CALL(mock, Program(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, Erase(_, _, _)).Times(0);
    EXPECT_EQ(-EINVAL, flash_area_open(1U, &output));
    EXPECT_EQ(nullptr, output);
    EXPECT_EQ(0U, clock_reads);
}

TEST_F(FlashMap, BorrowedClockDomainOverridesTheUnrelatedGlobalClock) {
    nx_time_us_t injected_now = 100U;
    const nx_mcuboot_flash_operation_t injected{
        &primary, &secondary, {read_clock, &injected_now}, 1000U};
    product_operation = &injected;
    clock_now = 9000U;
    const std::array<uint8_t, 4> input{};
    EXPECT_CALL(mock, Program(0x40000U, input.data(), input.size(), 1000U))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(0, flash_area_write(&secondary, 0U, input.data(), 4U));
    EXPECT_EQ(1U, clock_reads);
}

TEST_F(FlashMap, SerializedOperationsKeepIndependentClockContextsAndDeadline) {
    nx_time_us_t first_now = 100U;
    nx_time_us_t second_now = 2000U;
    const nx_mcuboot_flash_operation_t first{
        &primary, &secondary, {read_clock, &first_now}, 1000U};
    const nx_mcuboot_flash_operation_t second{
        &primary, &secondary, {read_clock, &second_now}, 1000U};
    const std::array<uint8_t, 4> input{};
    EXPECT_CALL(mock, Program(0x40000U, input.data(), input.size(), 1000U))
        .Times(2)
        .WillRepeatedly(Return(NX_SUCCESS));
    product_operation = &first;
    EXPECT_EQ(0, flash_area_write(&secondary, 0U, input.data(), 4U));
    product_operation = &second;
    EXPECT_EQ(-ETIMEDOUT, flash_area_write(&secondary, 0U, input.data(), 4U));
    product_operation = &first;
    EXPECT_EQ(0, flash_area_write(&secondary, 0U, input.data(), 4U));
}

} /* namespace */

extern "C" const nx_mcuboot_flash_operation_t*
nx_mcuboot_flash_operation(void) {
    return product_operation;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    ++clock_reads;
    return clock_now;
}
