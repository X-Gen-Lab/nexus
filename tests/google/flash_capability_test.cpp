/**
 * \file            flash_capability_test.cpp
 * \brief           Actual generated capabilities match physical Flash units
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#if defined(NX_FLASH_PROGRAM_STM32)
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
#else
#include "gd32f470_provider.h"
#endif
NVIC_Type g_gd32_model_nvic;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static unsigned system_calls;
#if defined(NX_FLASH_PROGRAM_STM32)
/** \brief Model only identity and system ports; real factory/I/O remain. */
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    ++system_calls;
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    ++system_calls;
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++system_calls;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++system_calls;
    return 0;
}
#else
/** \brief Reviewed fixed clock ports are external to this factory test. */
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    ++system_calls;
    return NX_SUCCESS;
}
extern "C" int nx_gd32_soc_start(void) {
    ++system_calls;
    return 0;
}
extern "C" int nx_gd32_soc_stop(void) {
    ++system_calls;
    return 0;
}
#endif

TEST(PhysicalFlashFactory, CapabilitiesMatchActualConstructedPhysicalGeometry) {
    g_gd32_model_nvic = {};
    const uintptr_t addresses[3]{0x08000000U, 0x40000000U, 0x1fff7000U};
    const size_t sizes[3]{1048576U, 0x80000U, 4096U};
    void* mappings[3]{};
    for (unsigned i = 0U; i < 3U; ++i) {
        mappings[i] =
            mmap(reinterpret_cast<void*>(addresses[i]), sizes[i],
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(addresses[i]), mappings[i]);
    }
#if !defined(NX_FLASH_PROGRAM_STM32)
    *reinterpret_cast<uint32_t*>(uintptr_t(0x1fff7a20U)) = 1024U << 16U;
#endif
    system_calls = 0U;
    const auto* face = nx_factory_flash(NX_FLASH_ID_PHYSICAL);
    const auto* capability = nx_factory_flash_capability(NX_FLASH_ID_PHYSICAL);
    ASSERT_NE(nullptr, face);
    ASSERT_NE(nullptr, capability);
    EXPECT_EQ(face, nx_factory_flash(NX_FLASH_ID_PHYSICAL));
    EXPECT_EQ(nullptr, nx_factory_flash(NX_FLASH_ID_COUNT));
    EXPECT_EQ(0U, system_calls);
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    const auto* geometry = nx_flash_port_geometry(face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(geometry->program_unit, capability->unit_bytes);
#if defined(NX_FLASH_PROGRAM_STM32)
    EXPECT_EQ(4U, capability->unit_bytes);
#else
    EXPECT_EQ(2U, capability->unit_bytes);
#endif
    EXPECT_EQ(1U, capability->buffer_alignment);
    EXPECT_EQ(0U, geometry->program_flags);
    EXPECT_EQ(1048576U, geometry->size);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_GT(system_calls, 0U);
    for (unsigned i = 0U; i < 3U; ++i)
        EXPECT_EQ(0, munmap(mappings[i], sizes[i]));
}
