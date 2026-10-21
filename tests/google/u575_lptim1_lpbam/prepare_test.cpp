/** \file prepare_test.cpp
 * \brief Dedicated finite tuple admission contract before implementation.
 * \author Nexus Team
 * \version 1.0.0
 * \date 2026-10-20
 * \copyright Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/stm32u575_lpdma_lptim1.h"
#include <cstring>
#include <gtest/gtest.h>
TEST(U575Lptim1Lpbam, PreparesExactlyOneTupleWithoutBorrowingOrDataWrites) {
    nx_stm32u575_lpdma_lptim1_request_t request{};
    nx_stm32u575_retained_time_storage_t clock{};
    nx_stm32u575_retained_time_snapshot_t sample{&clock, 1U, 0U, 0U};
    nx_request_initialize(&request.base);
    EXPECT_EQ(
        NX_SUCCESS,
        nx_stm32u575_lpdma_lptim1_request_prepare(
            &request, &sample,
            reinterpret_cast<const nx_stm32u575_lptim1_tuple_t*>(0x28000100U),
            reinterpret_cast<nx_stm32u575_lpdma_lptim1_workspace_t*>(
                0x28000300U),
            NX_DEADLINE_NEVER));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST(U575Lptim1Lpbam, RejectsNonSram4AndLeavesRequestBytesUnchanged) {
    nx_stm32u575_lpdma_lptim1_request_t request{};
    nx_stm32u575_retained_time_storage_t clock{};
    nx_stm32u575_retained_time_snapshot_t sample{&clock, 1U, 0U, 0U};
    nx_request_initialize(&request.base);
    auto before = request;
    EXPECT_EQ(
        NX_ERROR_PERMISSION,
        nx_stm32u575_lpdma_lptim1_request_prepare(
            &request, &sample,
            reinterpret_cast<const nx_stm32u575_lptim1_tuple_t*>(0x20000100U),
            reinterpret_cast<nx_stm32u575_lpdma_lptim1_workspace_t*>(
                0x28000300U),
            NX_DEADLINE_NEVER));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
}
