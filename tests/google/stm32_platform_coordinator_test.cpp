/**
 * \file            stm32_platform_coordinator_test.cpp
 * \brief           Fixed F4 system and shared-resource transaction ordering.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_ethernet_cold.h"
#include "stm32f4_platform_lifecycle.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
enum Step {
    Base = 1,
    Selected,
    Identity,
    PinsAcquire,
    PinsClocksRelease,
    SystemStart,
    PinsAbort,
    ResourcesAcquire,
    EthernetCapture,
    PermanentStop,
    CanClose,
    EthernetClose,
    PinsRelease,
    ResourcesRelease,
    SystemRelease
};

class Ports {
  public:
    MOCK_METHOD(nx_result_t, Call, (Step));
    nx_stm32_start_result_t system_result{};
};
Ports* ports;
bool privileged;
bool in_isr;
bool masked;
bool released_acquired;
uint32_t observed_flash;
uint32_t observed_poll;
uint32_t observed_dma_mask;

nx_result_t permanent_stop() {
    return ports->Call(PermanentStop);
}
nx_result_t can_close() {
    return ports->Call(CanClose);
}
void ethernet_capture(nx_stm32_eth_resource_receipt_t*, bool frames) {
    EXPECT_TRUE(frames);
    (void)ports->Call(EthernetCapture);
}
nx_result_t ethernet_close(nx_stm32_eth_resource_receipt_t*) {
    return ports->Call(EthernetClose);
}

class PlatformCoordinator : public testing::Test {
  protected:
    testing::StrictMock<Ports> mock;
    nx_stm32_system_t system{};
    nx_stm32_platform_config_t selected{};
    nx_stm32_platform_pin_receipt_t pins{};
    nx_stm32f4_platform_pin_owner_t pin_owner{
        nx_stm32_platform_selected_preflight, nx_stm32_platform_pins_acquire,
        nx_stm32_platform_gpio_clocks_release, nx_stm32_platform_pins_abort,
        nx_stm32_platform_pins_release};
    nx_stm32f4_platform_clock_owner_t clock_owner{
        nx_stm32_platform_resources_acquire,
        nx_stm32_platform_resources_release};
    nx_stm32_eth_resource_receipt_t ethernet{};
    uint32_t dma_added{};
    uint32_t syscfg_added{};
    nx_stm32f4_platform_config_t config{};

    void SetUp() override {
        ports = &mock;
        privileged = true;
        in_isr = masked = released_acquired = false;
        observed_flash = observed_poll = observed_dma_mask = 0U;
        config.system = &system;
        config.selected = &selected;
        config.pins = &pins;
        config.pin_owner = &pin_owner;
        config.clock_owner = &clock_owner;
        config.flash_bytes = 1048576U;
        config.poll_limit = 1000000U;
        config.dma_mask = RCC_AHB1ENR_DMA1EN;
        config.dma_added = &dma_added;
        config.syscfg_added = &syscfg_added;
    }
    void PacketAndPermanentOwners() {
        config.permanent_stop = permanent_stop;
        config.can_close = can_close;
        config.ethernet = &ethernet;
        config.ethernet_capture = ethernet_capture;
        config.ethernet_close = ethernet_close;
        config.ethernet_frames = true;
    }
    void Admit() {
        EXPECT_CALL(mock, Call(Base)).WillOnce(testing::Return(NX_SUCCESS));
        EXPECT_CALL(mock, Call(Selected)).WillOnce(testing::Return(NX_SUCCESS));
        EXPECT_CALL(mock, Call(Identity)).WillOnce(testing::Return(NX_SUCCESS));
    }
};
} /* namespace */

TEST_F(PlatformCoordinator, StartsOnlyAfterAllEffectFreeAdmission) {
    testing::InSequence order;
    Admit();
    EXPECT_CALL(mock, Call(PinsAcquire)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(SystemStart)).WillOnce(testing::Return(NX_SUCCESS));
    const auto result = nx_stm32f4_platform_system_start(&config);
    EXPECT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(observed_flash, config.flash_bytes);
    EXPECT_EQ(observed_poll, config.poll_limit);
}

TEST_F(PlatformCoordinator, SelectedRejectionNeverAcquiresPinsOrSystem) {
    testing::InSequence order;
    EXPECT_CALL(mock, Call(Base)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(Selected)).WillOnce(testing::Return(NX_ERROR_BUSY));
    const auto result = nx_stm32f4_platform_system_start(&config);
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
}

TEST_F(PlatformCoordinator, FailedPinAcquireReleasesOnlyAttemptedClocks) {
    testing::InSequence order;
    Admit();
    EXPECT_CALL(mock, Call(PinsAcquire)).WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_CALL(mock, Call(PinsClocksRelease))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    const auto result = nx_stm32f4_platform_system_start(&config);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_EQ(result.remaining_effects, 1U);
}

TEST_F(PlatformCoordinator, SystemFailurePreservesSystemCleanupAndEffects) {
    testing::InSequence order;
    Admit();
    EXPECT_CALL(mock, Call(PinsAcquire)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(SystemStart)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(PinsAbort)).WillOnce(testing::Return(NX_SUCCESS));
    mock.system_result = {1, 1, 4U};
    const auto result = nx_stm32f4_platform_system_start(&config);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.remaining_effects, 4U);
}

TEST_F(PlatformCoordinator, FailedAbortMergesPinsWithoutReplacingPrimary) {
    testing::InSequence order;
    Admit();
    EXPECT_CALL(mock, Call(PinsAcquire)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(SystemStart)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(PinsAbort)).WillOnce(testing::Return(NX_ERROR_BUSY));
    mock.system_result = {1, 0, 4U};
    const auto result = nx_stm32f4_platform_system_start(&config);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_EQ(result.remaining_effects, 5U);
}

TEST_F(PlatformCoordinator, ResourcesCaptureEthernetBeforeClockAcquisition) {
    PacketAndPermanentOwners();
    testing::InSequence order;
    EXPECT_CALL(mock, Call(EthernetCapture))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(ResourcesAcquire))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_IO);
    EXPECT_EQ(observed_dma_mask, config.dma_mask);
}

TEST_F(PlatformCoordinator, WithdrawalKeepsPermanentAndSharedOwnerOrder) {
    PacketAndPermanentOwners();
    testing::InSequence order;
    for (Step step : {PermanentStop, CanClose, EthernetClose, PinsRelease,
                      ResourcesRelease}) {
        EXPECT_CALL(mock, Call(step)).WillOnce(testing::Return(NX_SUCCESS));
    }
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_SUCCESS);
}

TEST_F(PlatformCoordinator, WithdrawalStopsAtTheFirstRefusedOwner) {
    PacketAndPermanentOwners();
    testing::InSequence order;
    EXPECT_CALL(mock, Call(PermanentStop))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(CanClose)).WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_BUSY);
}

TEST_F(PlatformCoordinator, EmptySelectionStillAdmitsAndOwnsSystem) {
    config.selected = nullptr;
    config.pins = nullptr;
    config.pin_owner = nullptr;
    config.dma_mask = 0U;
    config.dma_added = nullptr;
    config.syscfg_added = nullptr;
    config.clock_owner = nullptr;
    testing::InSequence order;
    EXPECT_CALL(mock, Call(Base)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(Identity)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, Call(SystemStart)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary, NX_SUCCESS);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_SUCCESS);
}

TEST_F(PlatformCoordinator, FinalSystemReleasePassesTheRealOwnershipFlag) {
    EXPECT_CALL(mock, Call(SystemRelease))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(nx_stm32f4_platform_system_release(&config, true), NX_ERROR_IO);
    EXPECT_TRUE(released_acquired);
    EXPECT_EQ(observed_poll, config.poll_limit);
}

TEST_F(PlatformCoordinator, InvalidPairRejectsBeforeAnyDependencyCall) {
    config.pins = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, MissingPinOwnerRejectsBeforeAnyDependencyCall) {
    config.pin_owner = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, UnselectedPinStateRejectsRetainedOwnerConnections) {
    config.selected = nullptr;
    config.pins = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, IncompletePinOwnerRejectsBeforeAnyDependencyCall) {
    pin_owner.release = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, MissingClockOwnerRejectsBeforeAnyDependencyCall) {
    config.clock_owner = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, UnselectedClocksRejectRetainedOwnerConnections) {
    config.dma_mask = 0U;
    config.dma_added = nullptr;
    config.syscfg_added = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator,
       IncompleteClockOwnerRejectsBeforeAnyDependencyCall) {
    clock_owner.release = nullptr;
    EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_platform_resources_release(&config), NX_ERROR_INVALID);
}

TEST_F(PlatformCoordinator, SyscfgOnlySelectionStillAcquiresItsClockOwner) {
    config.dma_mask = 0U;
    config.dma_added = nullptr;
    EXPECT_CALL(mock, Call(ResourcesAcquire))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_SUCCESS);
    EXPECT_EQ(observed_dma_mask, 0U);
}

TEST_F(PlatformCoordinator, EthernetCaptureDoesNotRequireDmaOrSyscfgOwner) {
    PacketAndPermanentOwners();
    config.dma_mask = 0U;
    config.dma_added = nullptr;
    config.syscfg_added = nullptr;
    config.clock_owner = nullptr;
    EXPECT_CALL(mock, Call(EthernetCapture))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config), NX_SUCCESS);
}

TEST_F(PlatformCoordinator, ColdContextRejectsBeforeAnyDependencyCall) {
    for (unsigned context = 0U; context < 3U; ++context) {
        privileged = context != 0U;
        in_isr = context == 1U;
        masked = context == 2U;
        EXPECT_EQ(nx_stm32f4_platform_system_start(&config).primary,
                  NX_ERROR_CONTEXT);
        EXPECT_EQ(nx_stm32f4_platform_resources_acquire(&config),
                  NX_ERROR_CONTEXT);
        EXPECT_EQ(nx_stm32f4_platform_resources_release(&config),
                  NX_ERROR_CONTEXT);
    }
}

extern "C" {
bool nx_arch_is_privileged(void) {
    return privileged;
}
bool nx_arch_in_isr(void) {
    return in_isr;
}
bool nx_arch_irq_is_masked(void) {
    return masked;
}
nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return ports->Call(Base);
}
nx_result_t
nx_stm32_platform_selected_preflight(const nx_stm32_platform_config_t*) {
    return ports->Call(Selected);
}
nx_result_t nx_stm32_validate_identity(uint32_t bytes) {
    observed_flash = bytes;
    return ports->Call(Identity);
}
nx_result_t nx_stm32_platform_pins_acquire(const nx_stm32_platform_config_t*,
                                           nx_stm32_platform_pin_receipt_t*) {
    return ports->Call(PinsAcquire);
}
nx_result_t
nx_stm32_platform_gpio_clocks_release(nx_stm32_platform_pin_receipt_t*) {
    return ports->Call(PinsClocksRelease);
}
nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                              uint32_t poll) {
    observed_poll = poll;
    (void)ports->Call(SystemStart);
    return ports->system_result;
}
nx_result_t nx_stm32_platform_pins_abort(nx_stm32_platform_pin_receipt_t*) {
    return ports->Call(PinsAbort);
}
nx_result_t nx_stm32_platform_resources_acquire(uint32_t mask, uint32_t*,
                                                uint32_t*) {
    observed_dma_mask = mask;
    return ports->Call(ResourcesAcquire);
}
nx_result_t nx_stm32_platform_pins_release(const nx_stm32_platform_config_t*,
                                           nx_stm32_platform_pin_receipt_t*) {
    return ports->Call(PinsRelease);
}
nx_result_t nx_stm32_platform_resources_release(uint32_t*, uint32_t*) {
    return ports->Call(ResourcesRelease);
}
nx_result_t nx_stm32_platform_system_release(nx_stm32_system_t*, bool acquired,
                                             uint32_t poll) {
    released_acquired = acquired;
    observed_poll = poll;
    return ports->Call(SystemRelease);
}
}
