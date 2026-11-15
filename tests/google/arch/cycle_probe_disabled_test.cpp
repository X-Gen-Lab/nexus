/**
 * \file            cycle_probe_disabled_test.cpp
 * \brief           Unselected probe macros do not evaluate or allocate.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/cycle_probe.h"
#include <gtest/gtest.h>

NX_IO_IRQ_PROBE_DECLARE(an_undefined_disabled_identifier());

TEST(CycleProbeDisabled, DefaultHooksNeverEvaluateTheirArguments) {
    unsigned calls = 0U;
    NX_IO_IRQ_PROBE_BEGIN(++calls);
    NX_IO_IRQ_PROBE_END(++calls);
    EXPECT_EQ(calls, 0U);
}

TEST(CycleProbeDisabled, DisabledHooksDoNotRequireAnyRuntimeProbeSymbol) {
    NX_IO_IRQ_PROBE_BEGIN(no_such_sidecar_or_function());
    NX_IO_IRQ_PROBE_END(no_such_sidecar_or_function());
    SUCCEED();
}
