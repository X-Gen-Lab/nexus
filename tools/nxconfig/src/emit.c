/**
 * \file            emit.c
 * \brief           Reviewed static C connections and owned bundle publication
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _GNU_SOURCE
#include "internal.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

/* The fixed set is both the publication boundary and the deletion allowlist. */
static const char* const names[] = {
    "nexus_instances.c", "nexus_instances_private.h",
    "nexus_factory.h",   "nexus_factory_strong.h",
    "nexus_config.h",    "selection.cmake",
    "manifest.json",     ".nxconfig-owner"};
static const char marker[] = "Nexus Host C static bundle schema 1\n";

static void header(FILE* output, const char* name, const char* brief) {
    (void)fprintf(output,
                  "/**\n * \\file            %s\n"
                  " * \\brief           %s\n"
                  " * \\author          Nexus Team\n"
                  " * \\version         0.1.0\n"
                  " * \\date            2026-11-07\n"
                  " * \\copyright       Copyright (c) 2026 Nexus Team\n */\n",
                  name, brief);
}

static size_t count(const nxconfig_assembly_t* assembly, nxconfig_kind_t kind) {
    size_t result = 0U;
    for (size_t i = 0U; i < assembly->controller_count; ++i)
        result += assembly->controllers[i].kind == kind ? 1U : 0U;
    return result;
}

static const char* kind_name(nxconfig_kind_t kind) {
    return kind == NXCONFIG_GPIO   ? "gpio"
           : kind == NXCONFIG_UART ? "uart"
                                   : "spi";
}

static size_t pin_count(const nxconfig_assembly_t* assembly) {
    size_t result = assembly->endpoint_count;
    for (size_t i = 0U; i < assembly->controller_count; ++i)
        result += assembly->controllers[i].pin_count;
    return result;
}

static void factory_header(FILE* output, const nxconfig_assembly_t* assembly,
                           bool strong) {
    const char* name = strong ? "nexus_factory_strong.h" : "nexus_factory.h";
    const char* guard = strong ? "NEXUS_GENERATED_FACTORY_STRONG_H"
                               : "NEXUS_GENERATED_FACTORY_H";
    header(output, name,
           "Selected typed factory; lookup performs no acquisition");
    (void)fprintf(output, "#ifndef %s\n#define %s\n\n", guard, guard);
    if (strong)
        (void)fputs(
            "#include \"nexus_factory.h\"\n"
            "#ifdef __cplusplus\n"
            "#define NX_FACTORY_REF_VALUE(type, value) type{value}\n"
            "#else\n"
            "#define NX_FACTORY_REF_VALUE(type, value) ((type){value})\n"
            "#endif\n",
            output);
    else
        (void)fputs("#include <stdint.h>\n#include \"nexus/io/fwd.h\"\n",
                    output);
    (void)fputs("\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", output);
    for (unsigned value = 0U; value < 4U; ++value) {
        nxconfig_kind_t kind = (nxconfig_kind_t)(value < 3U ? value : 2U);
        const char* category = value == 3U ? "spi_endpoint" : kind_name(kind);
        const char* face = value == 3U             ? "nx_spi_endpoint_t"
                           : kind == NXCONFIG_GPIO ? "nx_gpio_port_t"
                           : kind == NXCONFIG_UART ? "nx_uart_port_t"
                                                   : "nx_spi_port_t";
        size_t instances =
            value == 3U ? assembly->endpoint_count : count(assembly, kind);
        if (strong) {
            (void)fprintf(
                output,
                "/** \\brief Category-safe reference; no runtime registry. */\n"
                "typedef struct { nx_%s_id_t value; } nx_%s_ref_t;\n",
                category, category);
        } else {
            (void)fprintf(
                output,
                "/** \\brief Index into this exact generated assembly. */\n"
                "typedef uint16_t nx_%s_id_t;\nenum {\n",
                category);
        }
        size_t ordinal = 0U;
        size_t total =
            value == 3U ? assembly->endpoint_count : assembly->controller_count;
        char upper_category[32];
        nxconfig_identifier(category, upper_category, true);
        for (size_t i = 0U; i < total; ++i) {
            if (value != 3U && assembly->controllers[i].kind != kind)
                continue;
            const char* id = value == 3U ? assembly->endpoints[i].id
                                         : assembly->controllers[i].id;
            char identifier[64];
            nxconfig_identifier(id, identifier, true);
            if (strong) {
                (void)fprintf(
                    output,
                    "#define NX_%s_REF_%s "
                    "NX_FACTORY_REF_VALUE(nx_%s_ref_t, NX_%s_ID_%s)\n",
                    upper_category, identifier, category, upper_category,
                    identifier);
            } else
                (void)fprintf(output, "    NX_%s_ID_%s = %zu,\n",
                              upper_category, identifier, ordinal);
            ++ordinal;
        }
        if (strong)
            (void)fprintf(output,
                          "static inline const %s* "
                          "nx_factory_%s_ref(nx_%s_ref_t reference) {\n"
                          "    return nx_factory_%s(reference.value);\n}\n\n",
                          face, category, category, category);
        else
            (void)fprintf(output,
                          "    NX_%s_ID_COUNT = %zu\n};\n"
                          "/** \\brief Borrow selected face; invalid IDs "
                          "return NULL. */\n"
                          "const %s* nx_factory_%s(nx_%s_id_t id);\n\n",
                          upper_category, instances, face, category, category);
    }
    (void)fprintf(output, "#ifdef __cplusplus\n}\n#endif\n#endif /* %s */\n",
                  guard);
}

static void private_header(FILE* output) {
    header(output, "nexus_instances_private.h",
           "Static C lifecycle plan; system owner is established externally");
    (void)fputs(
        "#ifndef NEXUS_GENERATED_INSTANCES_PRIVATE_H\n"
        "#define NEXUS_GENERATED_INSTANCES_PRIVATE_H\n\n"
        "#include \"stm32f4_lifecycle.h\"\n\n"
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n"
        "/** \\brief Exact selected pads and gates for effect-free preflight. "
        "*/\n"
        "extern const nx_stm32_platform_config_t "
        "nx_generated_platform_config;\n"
        "/** \\brief Borrow fixed plan; caller supplies one zero lifecycle "
        "state. */\n"
        "extern const nx_lifecycle_plan_t nx_generated_lifecycle_plan;\n\n"
        "#ifdef __cplusplus\n}\n#endif\n#endif\n",
        output);
}

static void factory_source(FILE* output, const nxconfig_assembly_t* assembly,
                           nxconfig_kind_t kind, bool endpoint) {
    const char* category = endpoint ? "spi_endpoint" : kind_name(kind);
    const char* face = endpoint                ? "nx_spi_endpoint_t"
                       : kind == NXCONFIG_GPIO ? "nx_gpio_port_t"
                       : kind == NXCONFIG_UART ? "nx_uart_port_t"
                                               : "nx_spi_port_t";
    size_t instances =
        endpoint ? assembly->endpoint_count : count(assembly, kind);
    if (instances != 0U) {
        (void)fprintf(output, "static const %s* const s_%s_faces[%zu] = {\n",
                      face, category, instances);
        size_t total =
            endpoint ? assembly->endpoint_count : assembly->controller_count;
        for (size_t i = 0U; i < total; ++i) {
            if (!endpoint && assembly->controllers[i].kind != kind)
                continue;
            char id[64];
            nxconfig_identifier(endpoint ? assembly->endpoints[i].id
                                         : assembly->controllers[i].id,
                                id, false);
            (void)fprintf(output, "    &s_%s_%s_face,\n", category, id);
        }
        (void)fputs("};\n", output);
    }
    (void)fprintf(output, "\nconst %s* nx_factory_%s(nx_%s_id_t id) {\n", face,
                  category, category);
    if (instances == 0U)
        (void)fputs("    (void)id;\n    return NULL;\n}\n\n", output);
    else
        (void)fprintf(
            output,
            "    return (size_t)id < %zuU ? s_%s_faces[id] : NULL;\n}\n\n",
            instances, category);
}

static void lifecycle(FILE* output, const nxconfig_assembly_t* assembly) {
    if (assembly->controller_count != 0U) {
        (void)fputs("static nx_stm32_platform_pin_receipt_t s_pin_receipt;\n",
                    output);
        for (size_t i = 0U; i < assembly->controller_count; ++i) {
            const nxconfig_controller_t* controller = &assembly->controllers[i];
            if (controller->kind == NXCONFIG_GPIO)
                continue;
            char id[64];
            nxconfig_identifier(controller->id, id, false);
            const char* category = kind_name(controller->kind);
            (void)fprintf(output,
                          "static const nx_stm32f4_%s_lifecycle_config_t "
                          "s_%s_%s_lifecycle = {\n"
                          "    &s_%s_%s_config, &s_%s_%s_receipt\n};\n",
                          category, category, id, category, id, category, id);
        }
        (void)fprintf(
            output,
            "static const nx_lifecycle_entry_t s_lifecycle_entries[%zu] = {\n"
            "    {&nx_stm32f4_pins_lifecycle_ops, &s_pin_receipt,\n"
            "     &nx_generated_platform_config},\n",
            assembly->controller_count + 1U);
        for (size_t i = 0U; i < assembly->controller_count; ++i) {
            const nxconfig_controller_t* controller = &assembly->controllers[i];
            char id[64];
            nxconfig_identifier(controller->id, id, false);
            const char* category = kind_name(controller->kind);
            const char* adapter = controller->kind == NXCONFIG_GPIO ? "gpio"
                                  : controller->kind == NXCONFIG_UART
                                      ? "uart_irq"
                                      : "spi_poll";
            (void)fprintf(output,
                          "    {&nx_stm32f4_%s_lifecycle_ops, &s_%s_%s_state,\n"
                          "     &s_%s_%s_%s},\n",
                          adapter, category, id, category, id,
                          controller->kind == NXCONFIG_GPIO ? "config"
                                                            : "lifecycle");
        }
        (void)fputs("};\n", output);
    }
    (void)fprintf(
        output,
        "const nx_lifecycle_plan_t nx_generated_lifecycle_plan = {\n"
        "    .entries = %s, .count = %zuU,\n"
        "    .context_check = nx_stm32f4_lifecycle_context, .context = "
        "NULL\n};\n\n",
        assembly->controller_count == 0U ? "NULL" : "s_lifecycle_entries",
        assembly->controller_count == 0U ? 0U
                                         : assembly->controller_count + 1U);
}

static void gpio_source(FILE* output, const nxconfig_controller_t* controller,
                        const char* id) {
    const nxconfig_gpio_t* gpio = &controller->options.gpio;
    (void)fprintf(
        output,
        "static nx_stm32_gpio_state_t s_gpio_%s_state;\n"
        "static const nx_stm32_gpio_config_t s_gpio_%s_config = {\n"
        "    .registers = %s, .mask = %uU, .initial = %uU,\n"
        "    .output = %s, .pull = %uU, .output_type = %uU\n};\n"
        "static const nx_gpio_port_t s_gpio_%s_face = {\n"
        "    .ops = &nx_stm32_gpio_ops, .context = &s_gpio_%s_state\n};\n\n",
        id, id, controller->controller, gpio->mask, gpio->initial,
        gpio->output ? "true" : "false", (unsigned)gpio->pull,
        (unsigned)gpio->output_type, id, id);
}

static void uart_source(FILE* output, const nxconfig_controller_t* controller,
                        const char* id, size_t snapshot_offset) {
    const nxconfig_uart_t* uart = &controller->options.uart;
    (void)fprintf(
        output,
        "static nx_stm32_uart_state_t s_uart_%s_state;\n"
        "static nx_stm32_uart_cold_receipt_t s_uart_%s_receipt;\n"
        "static %s s_uart_%s_rx[%u];\n"
        "static const IRQn_Type s_uart_%s_vectors[1] = {%s};\n"
        "static const nx_stm32_platform_pin_config_t s_uart_%s_pins[%zu] = {\n",
        id, id, uart->events ? "nx_uart_rx_event_t" : "uint8_t", id,
        uart->rx_capacity, id, uart->irq, id, controller->pin_count);
    for (size_t i = 0U; i < controller->pin_count; ++i) {
        const nxconfig_pin_t* pin = &controller->pins[i];
        (void)fprintf(output,
                      "    {GPIO%c, &s_pin_snapshots[%zu], %uU, %uU},\n",
                      (int)('A' + pin->port), snapshot_offset + i,
                      (unsigned)pin->port, (unsigned)pin->number);
    }
    (void)fprintf(
        output,
        "};\nstatic const nx_stm32_uart_cold_config_t s_uart_%s_config = {\n"
        "    .registers = %s, .pins = s_uart_%s_pins,\n"
        "    .vectors = s_uart_%s_vectors, .rx_storage = s_uart_%s_rx,\n"
        "    .format = NULL, .rx_capacity = %uU,\n"
        "    .clock_hz = %uU, .baud = %uU, .profile = %s,\n"
        "    .vector_count = 1U, .priority = %uU,\n"
        "    .kernel_bound = false, .preload_tx = false\n};\n"
        "static const nx_uart_port_t s_uart_%s_face = {\n"
        "    .ops = &nx_stm32_uart_ops, .context = &s_uart_%s_state\n};\n"
        "void %.*sHandler(void);\nvoid %.*sHandler(void) {\n"
        "    nx_stm32_uart_irq(&s_uart_%s_state);\n}\n\n",
        id, controller->controller, id, id, id, uart->rx_capacity,
        uart->clock_hz, uart->baud,
        uart->events ? "NX_UART_RX_EVENTS" : "NX_UART_RX_BYTES",
        (unsigned)uart->priority, id, id, (int)(strlen(uart->irq) - 1U),
        uart->irq, (int)(strlen(uart->irq) - 1U), uart->irq, id);
}

static void spi_source(FILE* output, const nxconfig_assembly_t* assembly,
                       size_t controller_index, const char* id,
                       size_t snapshot_offset) {
    const nxconfig_controller_t* controller =
        &assembly->controllers[controller_index];
    size_t endpoints = 0U;
    size_t total_signal_pins = pin_count(assembly) - assembly->endpoint_count;
    for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
        const nxconfig_spi_endpoint_t* endpoint = &assembly->endpoints[i];
        if (endpoint->controller_index != controller_index)
            continue;
        ++endpoints;
        char endpoint_id[64];
        nxconfig_identifier(endpoint->id, endpoint_id, false);
        (void)fprintf(
            output,
            "static nx_stm32_spi_endpoint_state_t s_spi_endpoint_%s_state;\n"
            "static nx_stm32_gpio_state_t s_spi_endpoint_%s_cs;\n"
            "static const nx_spi_endpoint_t s_spi_endpoint_%s_face = {\n"
            "    .ops = &nx_stm32_spi_endpoint_ops,\n"
            "    .context = &s_spi_endpoint_%s_state\n};\n",
            endpoint_id, endpoint_id, endpoint_id, endpoint_id);
    }
    (void)fprintf(
        output,
        "static nx_stm32_spi_state_t s_spi_%s_state;\n"
        "static nx_stm32_spi_cold_receipt_t s_spi_%s_receipt;\n"
        "static const nx_stm32_spi_pin_config_t s_spi_%s_pins[%zu] = {\n",
        id, id, id, controller->pin_count + endpoints);
    for (size_t i = 0U; i < controller->pin_count; ++i) {
        const nxconfig_pin_t* pin = &controller->pins[i];
        (void)fprintf(
            output, "    {GPIO%c, &s_pin_snapshots[%zu], %uU, %uU, %uU},\n",
            (int)('A' + pin->port), snapshot_offset + i, (unsigned)pin->port,
            (unsigned)pin->number, (unsigned)pin->alternate);
    }
    for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
        const nxconfig_spi_endpoint_t* endpoint = &assembly->endpoints[i];
        if (endpoint->controller_index != controller_index)
            continue;
        const nxconfig_pin_t* pin = &endpoint->chip_select;
        (void)fprintf(output,
                      "    {GPIO%c, &s_pin_snapshots[%zu], %uU, %uU, 0U},\n",
                      (int)('A' + pin->port), total_signal_pins + i,
                      (unsigned)pin->port, (unsigned)pin->number);
    }
    (void)fputs("};\n", output);
    if (endpoints != 0U) {
        (void)fprintf(output,
                      "static const nx_stm32_spi_endpoint_config_t "
                      "s_spi_%s_endpoints[%zu] = {\n",
                      id, endpoints);
        for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
            const nxconfig_spi_endpoint_t* endpoint = &assembly->endpoints[i];
            if (endpoint->controller_index != controller_index)
                continue;
            char endpoint_id[64];
            nxconfig_identifier(endpoint->id, endpoint_id, false);
            (void)fprintf(output,
                          "    {&s_spi_endpoint_%s_state, "
                          "&s_spi_endpoint_%s_cs, %uU, %uU},\n",
                          endpoint_id, endpoint_id, endpoint->max_hz,
                          (unsigned)endpoint->mode);
        }
        (void)fputs("};\n", output);
    }
    (void)fprintf(output,
                  "static const nx_stm32_spi_config_t s_spi_%s_config = {\n"
                  "    .registers = %s, .clock_hz = %uU,\n"
                  "    .pins = s_spi_%s_pins, .pin_count = %zuU,\n"
                  "    .endpoints = ",
                  id, controller->controller, controller->options.spi.clock_hz,
                  id, controller->pin_count + endpoints);
    if (endpoints == 0U)
        (void)fputs("NULL", output);
    else
        (void)fprintf(output, "s_spi_%s_endpoints", id);
    (void)fprintf(
        output,
        ", .endpoint_count = %zuU\n};\n"
        "static const nx_spi_port_t s_spi_%s_face = {\n"
        "    .ops = &nx_stm32_spi_ops, .context = &s_spi_%s_state\n};\n\n",
        endpoints, id, id);
}

static void source(FILE* output, const nxconfig_assembly_t* assembly) {
    header(output, "nexus_instances.c",
           "Static selected objects; hardware behavior lives in C drivers");
    (void)fputs("#include \"nexus_instances_private.h\"\n#include "
                "\"nexus_factory.h\"\n\n",
                output);
    size_t pins = pin_count(assembly);
    if (pins != 0U)
        (void)fprintf(output, "static uint16_t s_pin_snapshots[%zu];\n\n",
                      pins);
    size_t offset = 0U;
    for (size_t i = 0U; i < assembly->controller_count; ++i) {
        const nxconfig_controller_t* controller = &assembly->controllers[i];
        char id[64];
        nxconfig_identifier(controller->id, id, false);
        if (controller->kind == NXCONFIG_GPIO)
            gpio_source(output, controller, id);
        else if (controller->kind == NXCONFIG_UART)
            uart_source(output, controller, id, offset);
        else
            spi_source(output, assembly, i, id, offset);
        offset += controller->pin_count;
    }
    if (pins != 0U) {
        (void)fprintf(output,
                      "static const nx_stm32_platform_pin_config_t "
                      "s_platform_pins[%zu] = {\n",
                      pins);
        offset = 0U;
        for (size_t i = 0U; i < assembly->controller_count; ++i) {
            const nxconfig_controller_t* controller = &assembly->controllers[i];
            for (size_t j = 0U; j < controller->pin_count; ++j) {
                const nxconfig_pin_t* pin = &controller->pins[j];
                (void)fprintf(
                    output, "    {GPIO%c, &s_pin_snapshots[%zu], %uU, %uU},\n",
                    (int)('A' + pin->port), offset++, (unsigned)pin->port,
                    (unsigned)pin->number);
            }
        }
        for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
            const nxconfig_pin_t* pin = &assembly->endpoints[i].chip_select;
            (void)fprintf(output,
                          "    {GPIO%c, &s_pin_snapshots[%zu], %uU, %uU},\n",
                          (int)('A' + pin->port), offset++, (unsigned)pin->port,
                          (unsigned)pin->number);
        }
        (void)fputs("};\n", output);
    }
    (void)fprintf(
        output,
        "const nx_stm32_platform_config_t nx_generated_platform_config = {\n"
        "    .pins = %s, .pin_count = %zuU,\n"
        "    .dma_streams = NULL, .dma_stream_count = 0U,\n",
        pins == 0U ? "NULL" : "s_platform_pins", pins);
    for (unsigned bus = 1U; bus <= 2U; ++bus) {
        (void)fprintf(output, "    .apb%u_clocks = 0U", bus);
        for (size_t i = 0U; i < assembly->controller_count; ++i) {
            const nxconfig_controller_t* controller = &assembly->controllers[i];
            if (controller->kind != NXCONFIG_GPIO &&
                strcmp(controller->clock_domain, bus == 1U ? "apb1" : "apb2") ==
                    0)
                (void)fprintf(output, " | RCC_APB%uENR_%sEN", bus,
                              controller->controller);
        }
        (void)fputs(",\n", output);
    }
    uint32_t port_mask = 0U;
    for (size_t i = 0U; i < assembly->controller_count; ++i)
        for (size_t j = 0U; j < assembly->controllers[i].pin_count; ++j)
            port_mask |= UINT32_C(1) << assembly->controllers[i].pins[j].port;
    for (size_t i = 0U; i < assembly->endpoint_count; ++i)
        port_mask |= UINT32_C(1) << assembly->endpoints[i].chip_select.port;
    (void)fputs("    .ahb1_clocks = 0U", output);
    for (unsigned port = 0U; port < 9U; ++port)
        if ((port_mask & (UINT32_C(1) << port)) != 0U)
            (void)fprintf(output, " | RCC_AHB1ENR_GPIO%cEN", (int)('A' + port));
    (void)fputs(", .ahb2_clocks = 0U,\n    .irq_masks = {\n", output);
    for (unsigned bank = 0U; bank < 3U; ++bank) {
        (void)fputs("        0U", output);
        for (size_t i = 0U; i < assembly->controller_count; ++i)
            if (assembly->controllers[i].kind == NXCONFIG_UART)
                (void)fprintf(output, " | NX_STM32_PLATFORM_IRQ_MASK(%s, %uU)",
                              assembly->controllers[i].options.uart.irq, bank);
        (void)fputs(",\n", output);
    }
    (void)fputs("    }, .exti_lines = 0U\n};\n\n", output);
    lifecycle(output, assembly);
    for (unsigned kind = 0U; kind < 3U; ++kind) {
        factory_source(output, assembly, (nxconfig_kind_t)kind, false);
    }
    factory_source(output, assembly, NXCONFIG_SPI, true);
}

static void config_header(FILE* output, const nxconfig_assembly_t* assembly) {
    const nxconfig_platform_t* platform = &assembly->platform;
    header(output, "nexus_config.h",
           "Selected compile facts; no vendor types in public factory");
    (void)fprintf(
        output,
        "#ifndef NEXUS_GENERATED_CONFIG_H\n#define NEXUS_GENERATED_CONFIG_H\n"
        "#define NEXUS_CPU_EXTERNAL_IRQ_COUNT %uU\n"
        "#define NEXUS_IRQ_PRIORITY_BITS %uU\n"
        "#define NEXUS_IRQ_KERNEL_POLICY 0\n"
        "#define NEXUS_IRQ_SYSCALL_PRIORITY 0U\n"
        "#define NEXUS_CORE_HZ %uU\n"
        "#define NEXUS_APB1_HZ %uU\n"
        "#define NEXUS_APB2_HZ %uU\n#endif\n",
        (unsigned)platform->external_irq_count,
        (unsigned)platform->irq_priority_bits, platform->core_hz,
        platform->apb1_hz, platform->apb2_hz);
}

static bool manifest(FILE* output, const nxconfig_assembly_t* assembly) {
    cJSON* object = cJSON_CreateObject();
    if (object == NULL)
        return false;
    bool valid =
        cJSON_AddNumberToObject(object, "schema", 1) != NULL &&
        cJSON_AddStringToObject(object, "producer", "nxconfig-host-c/0.1.0") !=
            NULL &&
        cJSON_AddStringToObject(object, "scope",
                                "f4-gpio-uart-irq-spi-short-poll-static") !=
            NULL &&
        cJSON_AddStringToObject(object, "board", assembly->platform.board) !=
            NULL &&
        cJSON_AddStringToObject(object, "part", assembly->platform.part) !=
            NULL;
    cJSON* inputs = cJSON_AddArrayToObject(object, "inputs");
    valid = valid && inputs != NULL;
    static const char* const roles[] = {"assembly", "board", "soc", "routes",
                                        "devices"};
    for (size_t i = 0U; valid && i < assembly->input_count; ++i) {
        cJSON* item = cJSON_CreateObject();
        if (item == NULL) {
            valid = false;
            break;
        }
        valid = cJSON_AddStringToObject(item, "role", roles[i]) != NULL &&
                cJSON_AddNumberToObject(
                    item, "bytes", (double)assembly->inputs[i].size) != NULL &&
                cJSON_AddStringToObject(item, "sha256",
                                        assembly->inputs[i].sha256) != NULL;
        if (!cJSON_AddItemToArray(inputs, item)) {
            cJSON_Delete(item);
            valid = false;
        }
    }
    char* encoded = valid ? cJSON_Print(object) : NULL;
    valid = encoded != NULL && fputs(encoded, output) >= 0 &&
            fputc('\n', output) != EOF;
    cJSON_free(encoded);
    cJSON_Delete(object);
    return valid;
}

static bool write_file(const char* directory, size_t index,
                       const nxconfig_assembly_t* assembly) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/%s", directory, names[index]);
    if (length <= 0 || (size_t)length >= sizeof(path))
        return false;
    int descriptor =
        open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0)
        return false;
    FILE* output = fdopen(descriptor, "w");
    if (output == NULL) {
        (void)close(descriptor);
        return false;
    }
    bool valid = true;
    switch (index) {
        case 0U:
            source(output, assembly);
            break;
        case 1U:
            private_header(output);
            break;
        case 2U:
            factory_header(output, assembly, false);
            break;
        case 3U:
            factory_header(output, assembly, true);
            break;
        case 4U:
            config_header(output, assembly);
            break;
        case 5U:
            (void)fprintf(
                output,
                "# Generated selected facts; bootstrap uses the native C "
                "compiler.\n"
                "set(NEXUS_CONFIG_FAMILY stm32f4)\n"
                "set(NEXUS_CONFIG_PART %s)\n"
                "set(NEXUS_CONFIG_DEVICE_DEFINE %s)\n"
                "set(NEXUS_CONFIG_STARTUP %s)\n"
                "set(NEXUS_CONFIG_BACKEND baremetal)\n"
                "set(NEXUS_CONFIG_OPTIMIZATION %s)\n",
                assembly->platform.part, assembly->platform.device_define,
                assembly->platform.startup, assembly->platform.optimization);
            break;
        case 6U:
            valid = manifest(output, assembly);
            break;
        default:
            (void)fputs(marker, output);
            break;
    }
    valid = valid && !ferror(output) && fflush(output) == 0 &&
            fsync(fileno(output)) == 0;
    if (fclose(output) != 0)
        valid = false;
    return valid;
}

static bool known_file(const char* name) {
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); ++i)
        if (strcmp(name, names[i]) == 0)
            return true;
    return false;
}

static bool owned(const char* directory) {
    DIR* stream = opendir(directory);
    if (stream == NULL)
        return false;
    bool valid = true;
    struct dirent* entry;
    while ((entry = readdir(stream)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char path[4096];
        int length =
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        struct stat status;
        if (!known_file(entry->d_name) || length <= 0 ||
            (size_t)length >= sizeof(path) || lstat(path, &status) != 0 ||
            !S_ISREG(status.st_mode)) {
            valid = false;
            break;
        }
    }
    (void)closedir(stream);
    char path[4096];
    static const char suffix[] = "/.nxconfig-owner";
    size_t prefix = strlen(directory);
    if (!valid || prefix > sizeof(path) - sizeof(suffix))
        return false;
    memcpy(path, directory, prefix + 1U);
    memcpy(path + prefix, suffix, sizeof(suffix));
    FILE* input = fopen(path, "r");
    if (input == NULL)
        return false;
    char bytes[sizeof(marker) + 1U];
    size_t size = fread(bytes, 1U, sizeof(bytes), input);
    valid = size == sizeof(marker) - 1U && memcmp(bytes, marker, size) == 0 &&
            !ferror(input);
    (void)fclose(input);
    return valid;
}

static void remove_owned(const char* directory) {
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[4096];
        int length = snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        if (length > 0 && (size_t)length < sizeof(path))
            (void)unlink(path);
    }
    (void)rmdir(directory);
}

static bool equal_file(const char* left, const char* right, const char* name) {
    char a[4096], b[4096];
    int first = snprintf(a, sizeof(a), "%s/%s", left, name);
    int second = snprintf(b, sizeof(b), "%s/%s", right, name);
    if (first <= 0 || second <= 0 || (size_t)first >= sizeof(a) ||
        (size_t)second >= sizeof(b))
        return false;
    FILE* one = fopen(a, "rb");
    FILE* two = fopen(b, "rb");
    bool equal = one != NULL && two != NULL;
    while (equal) {
        unsigned char x[4096], y[4096];
        size_t nx = fread(x, 1U, sizeof(x), one);
        size_t ny = fread(y, 1U, sizeof(y), two);
        if (nx != ny || memcmp(x, y, nx) != 0 || ferror(one) || ferror(two))
            equal = false;
        if (nx < sizeof(x) || ny < sizeof(y))
            break;
    }
    if (one != NULL)
        (void)fclose(one);
    if (two != NULL)
        (void)fclose(two);
    return equal;
}

/** \brief Reject directly shared parents before cooperative publication. */
static bool private_parent(const char* path) {
    int descriptor =
        open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0)
        return false;
    struct stat status;
    bool valid = fstat(descriptor, &status) == 0 &&
                 status.st_uid == geteuid() &&
                 (status.st_mode & (S_IWGRP | S_IWOTH)) == 0;
    (void)close(descriptor);
    return valid;
}

nxconfig_result_t nxconfig_emit(const nxconfig_assembly_t* assembly,
                                const char* output_directory,
                                nxconfig_diagnostic_t* diagnostic) {
    if (diagnostic != NULL)
        *diagnostic = (nxconfig_diagnostic_t){0};
    if (assembly == NULL || output_directory == NULL ||
        output_directory[0] == '\0')
        return nxconfig_error(diagnostic, NXCONFIG_INVALID, output_directory,
                              "output",
                              "Missing resolved owner or output path");
    if (!nxconfig_inputs_unchanged(assembly))
        return nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                              "inputs", "Input bytes changed after resolution");
    char* authored = strdup(output_directory);
    if (authored == NULL)
        return nxconfig_error(diagnostic, NXCONFIG_MEMORY, output_directory,
                              "output", "Cannot allocate output path");
    char* slash = strrchr(authored, '/');
    const char* base = slash != NULL ? slash + 1 : authored;
    if (*base == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        free(authored);
        return nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                              "output",
                              "Output must be a dedicated named directory");
    }
    char* parent;
    if (slash != NULL) {
        *slash = '\0';
        parent = realpath(authored[0] != '\0' ? authored : "/", NULL);
    } else
        parent = realpath(".", NULL);
    char destination[4096], lock_path[4096], stage[4096];
    bool trusted_parent = parent != NULL && private_parent(parent);
    int length = trusted_parent ? snprintf(destination, sizeof(destination),
                                           "%s/%s", parent, base)
                                : -1;
    int lock_length = parent != NULL
                          ? snprintf(lock_path, sizeof(lock_path),
                                     "%s/.%s.nxconfig.lock", parent, base)
                          : -1;
    int stage_length = parent != NULL
                           ? snprintf(stage, sizeof(stage),
                                      "%s/.%s.nxconfig-XXXXXX", parent, base)
                           : -1;
    free(parent);
    free(authored);
    if (length <= 0 || lock_length <= 0 || stage_length <= 0 ||
        (size_t)length >= sizeof(destination) ||
        (size_t)lock_length >= sizeof(lock_path) ||
        (size_t)stage_length >= sizeof(stage))
        return nxconfig_error(
            diagnostic, NXCONFIG_IO, output_directory, "output",
            "Output parent is missing, shared or path too long");
    for (size_t i = 0U; i < assembly->input_count; ++i) {
        const char* path = assembly->inputs[i].path;
        if (strncmp(path, destination, (size_t)length) == 0 &&
            (path[length] == '/' || path[length] == '\0'))
            return nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                                  "output", "Output overlaps an input owner");
    }
    int lock = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock < 0 || flock(lock, LOCK_EX) != 0) {
        if (lock >= 0)
            (void)close(lock);
        return nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                              "output", "Cannot acquire output ownership lock");
    }
    struct stat status;
    bool exists = lstat(destination, &status) == 0;
    bool valid = (!exists && errno == ENOENT) ||
                 (exists && S_ISDIR(status.st_mode) && owned(destination));
    if (!valid || mkdtemp(stage) == NULL) {
        (void)close(lock);
        return nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                              "output",
                              "Output is unowned, unsafe or cannot be staged");
    }
    for (size_t i = 0U; valid && i < sizeof(names) / sizeof(names[0]); ++i)
        valid = write_file(stage, i, assembly);
    valid = valid && nxconfig_inputs_unchanged(assembly);
    bool equal = valid && exists;
    for (size_t i = 0U; equal && i < sizeof(names) / sizeof(names[0]); ++i)
        equal = equal_file(stage, destination, names[i]);
    /* A first publication cannot overwrite an entry appearing after preflight.
     * Existing owned bundles exchange under the same private-parent lock. */
    if (valid && !equal)
        valid = syscall(SYS_renameat2, AT_FDCWD, stage, AT_FDCWD, destination,
                        exists ? RENAME_EXCHANGE : RENAME_NOREPLACE) == 0;
    if (exists || equal || !valid)
        remove_owned(stage);
    (void)close(lock);
    return valid ? NXCONFIG_OK
                 : nxconfig_error(diagnostic, NXCONFIG_IO, output_directory,
                                  "output",
                                  "Complete bundle publication failed; "
                                  "previous bundle retained");
}
