/** Production Flash facade with a deterministic, nonuniform physical model. */
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "arch/nx_arch.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>
extern "C" void typed_test_set_isr(bool);
namespace {
struct Flash {
    nx_internal_flash_t api{};
    nx_flash_operations_t operations{};
    nx_lifecycle_t life{};
    nx_device_state_t state = NX_DEV_STATE_UNINITIALIZED;
    uint8_t memory[1024]{};
    bool unlocked = false, malformed_block = false, malformed_geometry = false;
    uint32_t programs = 0, erases = 0, last_budget = 0;
    nx_status_t failure = NX_OK;
};
static Flash* model;
static nx_status_t init(nx_lifecycle_t*) { model->state = NX_DEV_STATE_RUNNING; return NX_OK; }
static nx_status_t deinit(nx_lifecycle_t*) { model->state = NX_DEV_STATE_UNINITIALIZED; return NX_OK; }
static nx_device_state_t state(nx_lifecycle_t*) { return model->state; }
static nx_lifecycle_t* life(nx_internal_flash_t*) { return &model->life; }
static nx_flash_operations_t* operations(nx_internal_flash_t*) { return &model->operations; }
static nx_status_t construct(const nx_device_t*, void** out) { *out = &model->api; return NX_OK; }
static nx_status_t geometry(nx_flash_operations_t*, nx_flash_geometry_t* g) {
    *g = {0x08000000u, 1024, model->malformed_geometry ? 0u : 4u, 4, NX_FLASH_ERASE_STALLS_EXEC, 0xff};
    return NX_OK;
}
static nx_status_t block(nx_flash_operations_t*, uint32_t offset, nx_flash_block_t* out) {
    constexpr uint32_t starts[] = {0,128,256,512};
    constexpr uint32_t sizes[] = {128,128,256,512};
    if (offset >= 1024) return NX_ERR_INVALID_PARAM;
    unsigned i = offset < 128 ? 0 : offset < 256 ? 1 : offset < 512 ? 2 : 3;
    *out = {starts[i], model->malformed_block ? 0u : sizes[i], i}; return NX_OK;
}
static nx_status_t read(nx_flash_operations_t*, uint32_t offset, uint8_t* out, size_t size) {
    std::memcpy(out, model->memory + offset, size); return model->failure;
}
static nx_status_t program(nx_flash_operations_t*, uint32_t offset, const uint8_t* data, size_t size, uint32_t budget) {
    ++model->programs; model->last_budget = budget;
    if (!model->unlocked) return NX_ERR_PERMISSION;
    if (model->failure != NX_OK) return model->failure;
    for (size_t i=0; i<size; ++i) model->memory[offset+i] &= data[i];
    return NX_OK;
}
static nx_status_t erase(nx_flash_operations_t*, uint32_t offset, size_t size, uint32_t budget) {
    ++model->erases; model->last_budget = budget;
    if (!model->unlocked) return NX_ERR_PERMISSION;
    if (model->failure != NX_OK) return model->failure;
    std::memset(model->memory + offset, 0xff, size); return NX_OK;
}
static nx_status_t sync(nx_flash_operations_t*, uint32_t budget) { model->last_budget = budget; return model->failure; }
static nx_status_t lock(nx_internal_flash_t*) { model->unlocked = false; return NX_OK; }
static nx_status_t unlock(nx_internal_flash_t*) { model->unlocked = true; return NX_OK; }
class TypedFlash : public ::testing::Test {
protected:
    Flash flash;
    nx_device_config_state_t storage{};
    nx_device_t descriptor{};
    nx_device_ref_t controller{};
    std::vector<nx_device_flash_region_t> opened;
    void SetUp() override {
        ASSERT_EQ(nx_device_registry_reset(), NX_OK); model = &flash;
        std::memset(flash.memory, 0xff, sizeof(flash.memory));
        flash.life.init = init; flash.life.deinit = deinit; flash.life.get_state = state;
        flash.api.get_lifecycle = life; flash.api.get_operations = operations;
        flash.api.lock = lock; flash.api.unlock = unlock;
        flash.operations = {geometry, block, read, program, erase, sync};
        descriptor.name = "FLASH0"; descriptor.state = &storage;
        descriptor.device_class = NX_DEVICE_CLASS_FLASH; descriptor.construct = construct;
        ASSERT_EQ(nx_device_register(&descriptor), NX_OK);
        ASSERT_EQ(nx_device_open("FLASH0", NX_DEVICE_CLASS_FLASH, 0x55, &controller), NX_OK);
    }
    void TearDown() override {
        typed_test_set_isr(false);
        for (auto r: opened) { auto s = nx_device_flash_region_close(r); EXPECT_TRUE(s == NX_OK || s == NX_ERR_INVALID_STATE); }
        EXPECT_EQ(nx_device_close(controller), NX_OK);
        EXPECT_EQ(nx_device_registry_reset(), NX_OK); model = nullptr;
    }
    nx_device_flash_region_t region(uint32_t offset, uint32_t size, uint32_t permissions) {
        nx_device_flash_region_t out{};
        EXPECT_EQ(nx_device_flash_region_open(controller, offset, size, permissions, &out), NX_OK);
        if (out.slot) opened.push_back(out);
        return out;
    }
};
TEST_F(TypedFlash, ReportsNonuniformGeometryAndCompleteContainingBlock) {
    nx_flash_geometry_t g{}; ASSERT_EQ(nx_device_flash_geometry(controller, &g), NX_OK);
    EXPECT_EQ(g.size_bytes, 1024u); EXPECT_EQ(g.block_count, 4u);
    nx_flash_block_t b{}; ASSERT_EQ(nx_device_flash_block(controller, 400, &b), NX_OK);
    EXPECT_EQ(b.offset, 256u); EXPECT_EQ(b.size, 256u); EXPECT_EQ(b.index, 2u);
    EXPECT_EQ(nx_device_flash_block(controller, 1024, &b), NX_ERR_INVALID_PARAM);
}
TEST_F(TypedFlash, RegionProtectsParentAndCopiedHandleCannotAccessReusedSlot) {
    auto old = region(0, 128, NX_FLASH_REGION_READ);
    EXPECT_EQ(nx_device_close(controller), NX_ERR_BUSY);
    ASSERT_EQ(nx_device_flash_region_close(old), NX_OK);
    auto next = region(0, 128, NX_FLASH_REGION_READ); uint8_t byte=0;
    EXPECT_NE(old.generation, next.generation);
    EXPECT_EQ(nx_device_flash_read(old, 0, &byte, 1), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_flash_read(next, 0, &byte, 1), NX_OK); EXPECT_EQ(byte, 0xff);
}
TEST_F(TypedFlash, EraseRegionRejectsPartialOrCrossingPhysicalBlocks) {
    nx_device_flash_region_t r{};
    EXPECT_EQ(nx_device_flash_region_open(controller, 1, 127, NX_FLASH_REGION_ERASE, &r), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_device_flash_region_open(controller, 128, 256, NX_FLASH_REGION_ERASE, &r), NX_ERR_INVALID_PARAM);
    auto exact = region(256, 256, NX_FLASH_REGION_ERASE);
    EXPECT_EQ(nx_device_flash_erase(exact, 0, 128, 40), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(flash.erases, 0u);
}
TEST_F(TypedFlash, WriteOverlapAndIntegerOverflowAreRejectedWithoutAdmission) {
    auto r = region(128, 128, NX_FLASH_REGION_PROGRAM); (void)r;
    nx_device_flash_region_t next{};
    EXPECT_EQ(nx_device_flash_region_open(controller, 128, 8, NX_FLASH_REGION_READ, &next), NX_ERR_RESOURCE_BUSY);
    EXPECT_EQ(nx_device_flash_region_open(controller, UINT32_MAX-3, 12, NX_FLASH_REGION_READ, &next), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_device_flash_region_open(controller, 0, UINT32_MAX, NX_FLASH_REGION_READ, &next), NX_ERR_INVALID_PARAM);
}
TEST_F(TypedFlash, PermissionsAlignmentAndBoundsPreventProviderWrites) {
    uint8_t bytes[8]{}; auto read_only = region(0,128,NX_FLASH_REGION_READ);
    EXPECT_EQ(nx_device_flash_program(read_only, 0, bytes, 4, 20), NX_ERR_PERMISSION);
    auto write = region(128,128,NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM);
    EXPECT_EQ(nx_device_flash_program(write, 1, bytes, 4, 20), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_device_flash_program(write, 124, bytes, 8, 20), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_device_flash_read(write, UINT32_MAX, bytes, 4), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(flash.programs, 0u);
}
TEST_F(TypedFlash, CallerUnlockRequiredAndExplicitBudgetPropagates) {
    uint8_t bytes[4]{1,2,3,4}; auto r = region(256,256,NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE);
    EXPECT_EQ(nx_device_flash_program(r,0,bytes,4,30), NX_ERR_PERMISSION);
    ASSERT_EQ(nx_device_flash_set_write_enabled(controller,true), NX_OK);
    EXPECT_EQ(nx_device_flash_program(r,0,bytes,4,37), NX_OK); EXPECT_EQ(flash.last_budget,37u);
    uint8_t actual[4]{}; ASSERT_EQ(nx_device_flash_read(r,0,actual,4), NX_OK);
    EXPECT_EQ(std::memcmp(actual,bytes,4),0);
    ASSERT_EQ(nx_device_flash_erase(r,0,256,29), NX_OK); EXPECT_EQ(flash.last_budget,29u);
    EXPECT_EQ(flash.memory[256],0xff); EXPECT_EQ(flash.memory[512],0xff);
    EXPECT_EQ(nx_device_flash_set_write_enabled(controller,false), NX_OK);
}
TEST_F(TypedFlash, ZeroDeadlineAndProviderFailureReleaseOnlySynchronousPin) {
    uint8_t bytes[4]{}; auto r=region(0,128,NX_FLASH_REGION_PROGRAM);
    EXPECT_EQ(nx_device_flash_program(r,0,bytes,4,0), NX_ERR_TIMEOUT); EXPECT_EQ(flash.programs,0u);
    ASSERT_EQ(nx_device_flash_set_write_enabled(controller,true),NX_OK); flash.failure=NX_ERR_HARDWARE;
    EXPECT_EQ(nx_device_flash_program(r,0,bytes,4,20),NX_ERR_HARDWARE);
    EXPECT_EQ(nx_device_flash_region_close(r),NX_OK); EXPECT_EQ(storage.child_refs,0u);
}
TEST_F(TypedFlash, MalformedGeometryAndBlockAreRejected) {
    nx_flash_geometry_t g{}; flash.malformed_geometry=true;
    EXPECT_EQ(nx_device_flash_geometry(controller,&g),NX_ERR_INVALID_STATE);
    flash.malformed_geometry=false; flash.malformed_block=true; nx_flash_block_t b{};
    EXPECT_EQ(nx_device_flash_block(controller,0,&b),NX_ERR_INVALID_STATE);
    nx_device_flash_region_t r{};
    EXPECT_EQ(nx_device_flash_region_open(controller,0,128,NX_FLASH_REGION_ERASE,&r),NX_ERR_INVALID_STATE);
}
TEST_F(TypedFlash, AbsentGeometryPortDoesNotAdvertiseOrEmulateFlash) {
    flash.api.get_operations=nullptr; nx_device_caps_t caps{};
    EXPECT_EQ(nx_device_query(controller,&caps),NX_OK); EXPECT_EQ(caps.flags,0u);
    nx_flash_geometry_t g{}; EXPECT_EQ(nx_device_flash_geometry(controller,&g),NX_ERR_NOT_SUPPORTED);
}
TEST_F(TypedFlash, FlashWaitRejectsISRAndMaskedTaskContext) {
    auto r=region(0,128,NX_FLASH_REGION_READ); uint8_t b=0;
    typed_test_set_isr(true); EXPECT_EQ(nx_device_flash_read(r,0,&b,1),NX_ERR_CONTEXT); typed_test_set_isr(false);
    auto token=nx_arch_irq_save(); EXPECT_EQ(nx_device_flash_read(r,0,&b,1),NX_ERR_INVALID_STATE); nx_arch_irq_restore(token);
}
}
