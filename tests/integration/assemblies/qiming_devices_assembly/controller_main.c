/**
 * \file            controller_main.c
 * \brief           Qiming public bus, SRAM, display and touch linkage fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         This ARM link contract is not an executed hardware test.
 *                  It never probes SRAM contents or enables the backlight.
 */
#include "nexus/components/software_spi.h"
#include "nexus/components/xpt2046.h"
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

#ifdef NX_PARALLEL16_REF_PANEL0
static nx_software_spi_t s_spi_owner;
static nx_software_spi_config_t s_spi_config;
static nx_spi_endpoint_t s_touch_endpoint;
static nx_xpt2046_t s_touch;
static nx_xpt2046_config_t s_touch_config;

/** \brief Use the already started RUN clock; no new timer owner is created. */
static nx_time_us_t fixture_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

/** \brief Observe actual selected faces without replacing a physical owner. */
static bool gpio_face(const nx_gpio_port_t* port) {
    const volatile nx_gpio_port_t* observed = port;
    return observed != NULL && observed->ops != NULL &&
           observed->context != NULL;
}

/** \brief Bind exact declared GPIO/EXTI faces to the actual common component.
 */
static nx_result_t touch_contract(nx_time_us_t deadline) {
    const nx_gpio_port_t* clk = nx_factory_gpio_ref(NX_GPIO_REF_TOUCH_CLK);
    const nx_gpio_port_t* mosi = nx_factory_gpio_ref(NX_GPIO_REF_TOUCH_MOSI);
    const nx_gpio_port_t* miso = nx_factory_gpio_ref(NX_GPIO_REF_TOUCH_MISO);
    const nx_gpio_port_t* cs = nx_factory_gpio_ref(NX_GPIO_REF_TOUCH_CS);
    const volatile nx_exti_port_t* pen = nx_factory_exti_ref(NX_EXTI_REF_PEN);
    const nx_gpio_port_t* light = nx_factory_gpio_ref(NX_GPIO_REF_BACKLIGHT);
    if (!gpio_face(clk) || !gpio_face(mosi) || !gpio_face(miso) ||
        !gpio_face(cs) || !gpio_face(light) || pen == NULL ||
        pen->ops == NULL || pen->context == NULL)
        return NX_ERROR_INVALID;
    s_spi_config = (nx_software_spi_config_t){.sck = {clk, UINT32_C(1) << 5U},
                                              .mosi = {mosi, UINT32_C(1) << 1U},
                                              .miso = {miso, UINT32_C(1) << 2U},
                                              .cs = {cs, UINT32_C(1)},
                                              .clock = {fixture_clock, NULL},
                                              .half_period_us = 1U,
                                              .select_setup_us = 1U,
                                              .select_hold_us = 1U,
                                              .poll_limit = 4096U,
                                              .max_bytes = 3U};
    nx_result_t result = nx_software_spi_init(&s_spi_owner, &s_spi_config);
    if (result != NX_SUCCESS)
        return result;
    s_touch_endpoint = nx_software_spi_endpoint(&s_spi_owner);
    s_touch_config =
        (nx_xpt2046_config_t){.spi = &s_touch_endpoint,
                              .clock = {fixture_clock, NULL},
                              .clock_domain = 1U,
                              .clock_epoch = 1U,
                              .pressure = true,
                              .pen_exti = nx_factory_exti_ref(NX_EXTI_REF_PEN)};
    result = nx_xpt2046_init(&s_touch, &s_touch_config, deadline);
    if (result != NX_SUCCESS)
        return result;
    result = nx_xpt2046_stop(&s_touch, deadline);
    if (result == NX_SUCCESS) {
        nx_spi_port_t owner = nx_software_spi_port(&s_spi_owner);
        result = nx_spi_port_stop(&owner);
    }
    return result;
}
#endif

/** \brief Consume strong factory identities and their real immutable methods.
 */
int main(void) {
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_EXT0);
    const volatile nx_external_bus_port_t* observed_bus = bus;
    if (observed_bus == NULL || observed_bus->ops == NULL ||
        observed_bus->context == NULL)
        return 1;
#ifdef NX_MEMORY_WINDOW_REF_RAM0
    const nx_memory_window_port_t* memory =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_RAM0);
    const volatile nx_memory_window_port_t* observed_memory = memory;
    if (observed_memory == NULL || observed_memory->ops == NULL ||
        observed_memory->context == NULL)
        return 1;
#endif
#ifdef NX_PARALLEL16_REF_PANEL0
    const volatile nx_parallel16_endpoint_t* panel =
        nx_factory_parallel16_ref(NX_PARALLEL16_REF_PANEL0);
    if (panel == NULL || panel->ops == NULL || panel->context == NULL)
        return 2;
#endif
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        for (;;) {
        }
    if (nx_external_bus_port_state(bus) != NX_EXTERNAL_BUS_PREPARED)
        for (;;) {
        }
#ifdef NX_MEMORY_WINDOW_REF_RAM0
    nx_memory_window_geometry_t geometry;
    nx_memory_window_lease_t lease = {0};
    if (nx_memory_window_port_geometry(memory, &geometry) != NX_SUCCESS ||
        geometry.startup_backing || geometry.address != UINT32_C(0x68000000) ||
        geometry.bytes != UINT32_C(0x100000) ||
        nx_memory_window_port_acquire(memory, &lease) != NX_SUCCESS ||
        nx_memory_window_port_release(memory, &lease, lease.epoch) !=
            NX_SUCCESS)
        for (;;) {
        }
#endif
#ifdef NX_PARALLEL16_REF_PANEL0
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 100000U, &deadline) !=
            NX_SUCCESS ||
        touch_contract(deadline) != NX_SUCCESS)
        for (;;) {
        }
#endif
    /* Any failed component cleanup parks with persistent owners intact.
     * No SRAM content access, panel initialization or backlight write occurs.
     */
    (void)nx_platform_stop();
    for (;;) {
    }
}
