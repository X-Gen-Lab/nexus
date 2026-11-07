/**
 * \file            config.c
 * \brief           Host TOML authoring to immutable F4 static configuration
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _XOPEN_SOURCE 700
#include "internal.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Parser objects belong only to this host owner, never to the public IR. */
typedef struct {
    nxconfig_assembly_t* assembly;
    nxconfig_diagnostic_t* diagnostic;
    const cJSON* variant;
    const char* source;
    char context[192];
} resolver_t;

nxconfig_result_t nxconfig_error(nxconfig_diagnostic_t* diagnostic,
                                 nxconfig_result_t code, const char* source,
                                 const char* context, const char* format, ...) {
    if (diagnostic != NULL) {
        *diagnostic = (nxconfig_diagnostic_t){.code = code};
        (void)snprintf(diagnostic->source, sizeof(diagnostic->source), "%s",
                       source != NULL ? source : "");
        (void)snprintf(diagnostic->context, sizeof(diagnostic->context), "%s",
                       context != NULL ? context : "input");
        va_list arguments;
        va_start(arguments, format);
        (void)vsnprintf(diagnostic->message, sizeof(diagnostic->message),
                        format, arguments);
        va_end(arguments);
    }
    return code;
}

static bool fail(resolver_t* resolver, nxconfig_result_t code,
                 const char* message) {
    (void)nxconfig_error(resolver->diagnostic, code, resolver->source,
                         resolver->context, "%s", message);
    return false;
}

static const cJSON* field(const cJSON* object, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static const char* string(const cJSON* item) {
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static const char* text(const cJSON* object, const char* key) {
    return string(field(object, key));
}

static bool same(const char* left, const char* right) {
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

static bool copy(resolver_t* resolver, char* target, size_t capacity,
                 const char* value) {
    if (value == NULL || strlen(value) >= capacity)
        return fail(resolver, NXCONFIG_INVALID, "Missing or overlong string");
    (void)snprintf(target, capacity, "%s", value);
    return true;
}

static bool json_uint(resolver_t* resolver, const cJSON* value,
                      uint32_t* result) {
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0 ||
        value->valuedouble > (double)UINT32_MAX)
        return fail(resolver, NXCONFIG_INVALID,
                    "Expected unsigned integer fact");
    uint32_t converted = (uint32_t)value->valuedouble;
    if ((double)converted != value->valuedouble)
        return fail(resolver, NXCONFIG_INVALID, "Expected integral fact");
    *result = converted;
    return true;
}

static bool json_number(resolver_t* resolver, const cJSON* object,
                        const char* key, uint32_t* result) {
    return json_uint(resolver, field(object, key), result);
}

static bool contains(const cJSON* array, const char* value) {
    if (!cJSON_IsArray(array))
        return false;
    const cJSON* item;
    cJSON_ArrayForEach(item, array) {
        if (same(string(item), value))
            return true;
    }
    return false;
}

static const cJSON* find(const cJSON* array, const char* key,
                         const char* value) {
    const cJSON* found = NULL;
    const cJSON* item;
    cJSON_ArrayForEach(item, array) {
        if (same(text(item, key), value)) {
            if (found != NULL)
                return NULL;
            found = item;
        }
    }
    return found;
}

static bool unique_json(resolver_t* resolver, const cJSON* node) {
    const cJSON* item;
    cJSON_ArrayForEach(item, node) {
        if (cJSON_IsObject(node)) {
            for (const cJSON* before = node->child; before != item;
                 before = before->next) {
                if (same(item->string, before->string))
                    return fail(resolver, NXCONFIG_INVALID,
                                "Duplicate JSON fact key");
            }
        }
        if ((cJSON_IsArray(item) || cJSON_IsObject(item)) &&
            !unique_json(resolver, item))
            return false;
    }
    return true;
}

/* O_NOFOLLOW and a stable descriptor prevent parser reads of a swapped file. */
static bool read_input(const char* path, nxconfig_input_t* input) {
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (descriptor < 0)
        return false;
    struct stat before;
    bool valid = fstat(descriptor, &before) == 0 && S_ISREG(before.st_mode) &&
                 before.st_size >= 0 &&
                 (uintmax_t)before.st_size <= NXCONFIG_MAX_INPUT;
    size_t size = valid ? (size_t)before.st_size : 0U;
    char* bytes = valid ? malloc(size + 1U) : NULL;
    if (bytes == NULL)
        valid = false;
    size_t offset = 0U;
    while (valid && offset < size) {
        ssize_t count = read(descriptor, bytes + offset, size - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            valid = false;
            break;
        }
        offset += (size_t)count;
    }
    struct stat after;
    valid = valid && fstat(descriptor, &after) == 0 &&
            before.st_size == after.st_size &&
            before.st_mtim.tv_sec == after.st_mtim.tv_sec &&
            before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
            memchr(bytes, '\0', size) == NULL;
    (void)close(descriptor);
    if (!valid) {
        free(bytes);
        return false;
    }
    bytes[size] = '\0';
    char* canonical = realpath(path, NULL);
    if (canonical == NULL) {
        free(bytes);
        return false;
    }
    *input =
        (nxconfig_input_t){.path = canonical, .bytes = bytes, .size = size};
    nxconfig_sha256(bytes, size, input->sha256);
    return true;
}

bool nxconfig_inputs_unchanged(const nxconfig_assembly_t* assembly) {
    for (size_t i = 0U; i < assembly->input_count; ++i) {
        nxconfig_input_t observed = {0};
        const nxconfig_input_t* expected = &assembly->inputs[i];
        if (!read_input(expected->path, &observed))
            return false;
        bool equal = observed.size == expected->size &&
                     strcmp(observed.sha256, expected->sha256) == 0;
        free(observed.path);
        free(observed.bytes);
        if (!equal)
            return false;
    }
    return true;
}

static bool load(resolver_t* resolver, const char* path, cJSON** json) {
    nxconfig_assembly_t* assembly = resolver->assembly;
    if (assembly->input_count >= NXCONFIG_INPUT_COUNT)
        return fail(resolver, NXCONFIG_INVALID,
                    "Internal input count exceeded");
    nxconfig_input_t* input = &assembly->inputs[assembly->input_count];
    if (!read_input(path, input))
        return fail(resolver, NXCONFIG_IO, "Cannot read stable regular input");
    ++assembly->input_count;
    if (json != NULL) {
        const char* end = NULL;
        *json = cJSON_ParseWithLengthOpts(input->bytes, input->size + 1U, &end,
                                          true);
        if (*json == NULL || !cJSON_IsObject(*json))
            return fail(resolver, NXCONFIG_INVALID, "Invalid JSON fact object");
        if (!unique_json(resolver, *json))
            return false;
        uint32_t schema;
        if (!json_number(resolver, *json, "schema_version", &schema) ||
            schema != 1U)
            return fail(resolver, NXCONFIG_INVALID, "Unsupported fact schema");
    }
    return true;
}

static bool known_key(const char* key, const char* const* allowed) {
    for (size_t i = 0U; allowed[i] != NULL; ++i) {
        if (strcmp(key, allowed[i]) == 0)
            return true;
    }
    return false;
}

static bool keys(resolver_t* resolver, const toml_table_t* table,
                 const char* const* allowed) {
    if (table == NULL)
        return true;
    for (int index = 0;; ++index) {
        const char* key = toml_key_in(table, index);
        if (key == NULL)
            return true;
        if (!known_key(key, allowed)) {
            char message[192];
            (void)snprintf(message, sizeof(message), "Unknown option: %s", key);
            return fail(resolver, NXCONFIG_INVALID, message);
        }
    }
}

static bool authored_string(resolver_t* resolver, const toml_table_t* table,
                            const char* key, char* target, size_t capacity,
                            const char* fallback) {
    if (table == NULL)
        return fallback != NULL
                   ? copy(resolver, target, capacity, fallback)
                   : fail(resolver, NXCONFIG_INVALID, "Expected string option");
    toml_datum_t value = toml_string_in(table, key);
    if (!value.ok) {
        if (toml_key_exists(table, key) || fallback == NULL)
            return fail(resolver, NXCONFIG_INVALID, "Expected string option");
        return copy(resolver, target, capacity, fallback);
    }
    bool valid = copy(resolver, target, capacity, value.u.s);
    free(value.u.s);
    return valid;
}

static bool authored_uint(resolver_t* resolver, const toml_table_t* table,
                          const char* key, uint32_t* result, uint32_t fallback,
                          uint32_t minimum, uint32_t maximum) {
    if (table == NULL || !toml_key_exists(table, key)) {
        *result = fallback;
        return true;
    }
    toml_datum_t value = toml_int_in(table, key);
    if (!value.ok || value.u.i < (int64_t)minimum ||
        value.u.i > (int64_t)maximum)
        return fail(resolver, NXCONFIG_INVALID,
                    "Integer option outside bounds");
    *result = (uint32_t)value.u.i;
    return true;
}

static bool name_valid(const char* id) {
    if (id == NULL || id[0] == '\0' || strlen(id) > 63U ||
        (!isalpha((unsigned char)id[0]) && id[0] != '_'))
        return false;
    for (size_t i = 0U; id[i] != '\0'; ++i) {
        unsigned char value = (unsigned char)id[i];
        if (value > 127U || (!isalnum(value) && value != '_' && value != '-'))
            return false;
    }
    return true;
}

void nxconfig_identifier(const char* input, char* output, bool uppercase) {
    do {
        unsigned char value = (unsigned char)*input++;
        *output++ = value == '-' ? '_'
                    : uppercase  ? (char)toupper(value)
                                 : (char)value;
        if (value == 0U)
            break;
    } while (true);
}

static bool hardware_choices(resolver_t* resolver) {
    toml_table_t* choices =
        toml_table_in(resolver->assembly->authored, "hardware");
    if (choices == NULL)
        return !toml_key_exists(resolver->assembly->authored, "hardware") ||
               fail(resolver, NXCONFIG_INVALID, "hardware must be a table");
    const cJSON* declared =
        field(resolver->assembly->board, "hardware_options");
    for (int index = 0;; ++index) {
        const char* key = toml_key_in(choices, index);
        if (key == NULL)
            return true;
        toml_datum_t selected = toml_string_in(choices, key);
        bool valid =
            selected.ok && contains(field(declared, key), selected.u.s);
        if (selected.ok)
            free(selected.u.s);
        if (!valid)
            return fail(resolver, NXCONFIG_INVALID,
                        "Unknown hardware choice or value");
    }
}

static bool platform(resolver_t* resolver) {
    nxconfig_assembly_t* assembly = resolver->assembly;
    nxconfig_platform_t* value = &assembly->platform;
    (void)snprintf(resolver->context, sizeof(resolver->context),
                   "board/part/device");
    if (!same(text(assembly->board, "soc_family"), "stm32f4"))
        return fail(resolver, NXCONFIG_UNSUPPORTED,
                    "Only stm32f4 family is implemented");
    if (!copy(resolver, value->board, sizeof(value->board),
              text(assembly->board, "id")) ||
        !copy(resolver, value->part, sizeof(value->part),
              text(assembly->board, "soc")))
        return false;
    const cJSON* variant = field(field(assembly->soc, "variants"), value->part);
    resolver->variant = variant;
    if (!cJSON_IsObject(variant))
        return fail(resolver, NXCONFIG_UNSUPPORTED,
                    "Part has no reviewed resource facts");
    const cJSON* device = find(field(assembly->devices, "devices"), "id",
                               text(variant, "device"));
    const char* startup = text(device, "startup");
    const char* basename = startup != NULL ? strrchr(startup, '/') : NULL;
    uint32_t count, bits;
    if (!copy(resolver, value->package, sizeof(value->package),
              text(variant, "package")) ||
        !copy(resolver, value->device_define, sizeof(value->device_define),
              text(device, "define")) ||
        !copy(resolver, value->startup, sizeof(value->startup),
              basename != NULL ? basename + 1 : startup) ||
        !json_number(resolver, device, "external_irq_count", &count) ||
        !json_number(resolver, device, "nvic_priority_bits", &bits))
        return false;
    if (count == 0U || count > 240U || bits == 0U || bits > 8U)
        return fail(resolver, NXCONFIG_INVALID,
                    "Invalid device interrupt geometry");
    value->external_irq_count = (uint16_t)count;
    value->irq_priority_bits = (uint8_t)bits;
    const cJSON* flash = field(variant, "flash");
    if (!json_number(resolver, flash, "origin", &value->flash_origin) ||
        !json_number(resolver, flash, "size", &value->flash_bytes))
        return false;
    const cJSON* memory = find(field(variant, "memory"), "id", "sram");
    if (!json_number(resolver, memory, "origin", &value->sram_origin) ||
        !json_number(resolver, memory, "size", &value->sram_bytes))
        return false;
    if (!authored_string(resolver, assembly->authored, "clock", value->clock,
                         sizeof(value->clock), NULL) ||
        !authored_string(resolver, assembly->authored, "optimization",
                         value->optimization, sizeof(value->optimization),
                         "Os"))
        return false;
    if (!same(value->clock, "hse8-pll168"))
        return fail(resolver, NXCONFIG_UNSUPPORTED,
                    "Clock profile has no maintained C system implementation");
    if (!same(value->optimization, "Os") && !same(value->optimization, "O2") &&
        !same(value->optimization, "O0") && !same(value->optimization, "Og"))
        return fail(resolver, NXCONFIG_INVALID,
                    "Unsupported compiler optimization");
    const cJSON* profile =
        field(field(assembly->soc, "clock_profiles"), value->clock);
    uint32_t hse, board_hse;
    if (!json_number(resolver, profile, "core_hz", &value->core_hz) ||
        !json_number(resolver, profile, "apb1_hz", &value->apb1_hz) ||
        !json_number(resolver, profile, "apb2_hz", &value->apb2_hz) ||
        !json_number(resolver, profile, "hse_hz", &hse) ||
        !json_number(resolver, field(assembly->board, "clocks"), "hse_hz",
                     &board_hse))
        return false;
    if (hse != board_hse)
        return fail(resolver, NXCONFIG_CONFLICT,
                    "Board oscillator disagrees with clock profile");
    static const char* const allowed[] = {
        "main_stack_bytes", "static_ram_limit_bytes", "flash_load_limit_bytes",
        "libc_heap_bytes", NULL};
    toml_table_t* budgets = toml_table_in(assembly->authored, "memory");
    if (toml_key_exists(assembly->authored, "memory") && budgets == NULL)
        return fail(resolver, NXCONFIG_INVALID, "memory must be a table");
    uint32_t heap;
    if (!keys(resolver, budgets, allowed) ||
        !authored_uint(resolver, budgets, "main_stack_bytes",
                       &value->main_stack_bytes, 2048U, 256U,
                       value->sram_bytes) ||
        !authored_uint(resolver, budgets, "static_ram_limit_bytes",
                       &value->static_ram_limit_bytes, value->sram_bytes, 1U,
                       value->sram_bytes) ||
        !authored_uint(resolver, budgets, "flash_load_limit_bytes",
                       &value->flash_load_limit_bytes, value->flash_bytes, 1U,
                       value->flash_bytes) ||
        !authored_uint(resolver, budgets, "libc_heap_bytes", &heap, 0U, 0U, 0U))
        return false;
    if (value->main_stack_bytes > value->static_ram_limit_bytes ||
        (value->main_stack_bytes & 7U) != 0U)
        return fail(resolver, NXCONFIG_INVALID,
                    "Stack must be aligned and fit the RAM budget");
    return true;
}

static bool hardware(resolver_t* resolver, const cJSON* binding) {
    const cJSON* required = field(binding, "requires");
    const cJSON* item;
    toml_table_t* choices =
        toml_table_in(resolver->assembly->authored, "hardware");
    cJSON_ArrayForEach(item, required) {
        toml_datum_t value = choices != NULL
                                 ? toml_string_in(choices, item->string)
                                 : (toml_datum_t){0};
        bool matches = value.ok && same(value.u.s, string(item));
        if (value.ok)
            free(value.u.s);
        if (!matches)
            return fail(resolver, NXCONFIG_CONFLICT,
                        "Binding requires explicit matching hardware choice");
    }
    return true;
}

static bool pad(resolver_t* resolver, const cJSON* record,
                nxconfig_pin_t* output) {
    const char* name = text(record, "pin");
    uint32_t alternate;
    if (name == NULL || strlen(name) < 3U || strlen(name) > 4U ||
        name[0] != 'P' || name[1] < 'A' || name[1] > 'I' ||
        !isdigit((unsigned char)name[2]))
        return fail(resolver, NXCONFIG_INVALID, "Invalid exact pad name");
    char* end;
    unsigned long number = strtoul(name + 2, &end, 10);
    if (*end != '\0' || number > 15UL ||
        !contains(field(resolver->variant, "pins"), name) ||
        !json_number(resolver, record, "af", &alternate) || alternate > 15U)
        return fail(resolver, NXCONFIG_INVALID,
                    "Pad or alternate is absent from selected package");
    if (contains(field(resolver->assembly->board, "reserved_resources"), name))
        return fail(resolver, NXCONFIG_CONFLICT,
                    "Selected pad is reserved by board");
    *output = (nxconfig_pin_t){.port = (uint8_t)(name[1] - 'A'),
                               .number = (uint8_t)number,
                               .alternate = (uint8_t)alternate};
    return copy(resolver, output->name, sizeof(output->name), name);
}

static bool claim_pad(resolver_t* resolver, const nxconfig_pin_t* selected) {
    const nxconfig_assembly_t* assembly = resolver->assembly;
    for (size_t i = 0U; i < assembly->controller_count; ++i) {
        const nxconfig_controller_t* controller = &assembly->controllers[i];
        for (size_t j = 0U; j < controller->pin_count; ++j) {
            if (same(selected->name, controller->pins[j].name)) {
                char message[128];
                (void)snprintf(message, sizeof(message),
                               "Pad %s already has an owner", selected->name);
                return fail(resolver, NXCONFIG_CONFLICT, message);
            }
        }
    }
    for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
        if (same(selected->name, assembly->endpoints[i].chip_select.name)) {
            char message[128];
            (void)snprintf(message, sizeof(message),
                           "Pad %s already has a CS owner", selected->name);
            return fail(resolver, NXCONFIG_CONFLICT, message);
        }
    }
    return true;
}

static const cJSON* binding_route(resolver_t* resolver, const char* binding_id,
                                  const char* kind, const char* mode,
                                  const cJSON** binding_out,
                                  const cJSON** controller_out) {
    nxconfig_assembly_t* assembly = resolver->assembly;
    const cJSON* binding =
        find(field(assembly->board, "bindings"), "id", binding_id);
    const char* controller_id = text(binding, "controller");
    const cJSON* controller = field(field(assembly->soc, "controllers"),
                                    controller_id != NULL ? controller_id : "");
    const cJSON* route =
        find(field(assembly->routes, "routes"), "id", text(binding, "route"));
    if (!same(text(controller, "kind"), kind) ||
        !same(text(route, "kind"), kind) ||
        !same(text(route, "controller"), controller_id) ||
        !contains(field(route, "variants"), assembly->platform.part) ||
        !contains(field(controller, "modes"), mode) ||
        !contains(field(route, "modes"), mode)) {
        (void)fail(resolver, NXCONFIG_INVALID,
                   "Binding, controller, route, part or mode disagree");
        return NULL;
    }
    if (!hardware(resolver, binding))
        return NULL;
    *binding_out = binding;
    *controller_out = controller;
    return route;
}

static bool unique_controller(resolver_t* resolver, nxconfig_kind_t kind,
                              const char* id, const char* physical) {
    char normalized[64], existing[64];
    nxconfig_identifier(id, normalized, true);
    if (same(normalized, "COUNT"))
        return fail(resolver, NXCONFIG_CONFLICT,
                    "Identifier is reserved by the typed factory");
    for (size_t i = 0U; i < resolver->assembly->controller_count; ++i) {
        const nxconfig_controller_t* item = &resolver->assembly->controllers[i];
        nxconfig_identifier(item->id, existing, true);
        if (kind == item->kind && same(normalized, existing))
            return fail(resolver, NXCONFIG_CONFLICT,
                        "Generated identifier already exists");
        if (kind != NXCONFIG_GPIO && same(physical, item->controller))
            return fail(resolver, NXCONFIG_CONFLICT,
                        "Controller already has an owner");
    }
    return true;
}

static bool selected_pins(resolver_t* resolver, const cJSON* route,
                          const cJSON* controller,
                          nxconfig_controller_t* result) {
    const cJSON* records = field(route, "pins");
    int length = cJSON_GetArraySize(records);
    if (!cJSON_IsArray(records) || length <= 0 || length > 16)
        return fail(resolver, NXCONFIG_INVALID, "Invalid reviewed pad list");
    nxconfig_pin_t* pins = calloc((size_t)length, sizeof(*pins));
    if (pins == NULL)
        return fail(resolver, NXCONFIG_MEMORY, "Cannot allocate selected pads");
    result->pins = pins;
    result->pin_count = (size_t)length;
    for (int i = 0; i < length; ++i) {
        if (!pad(resolver, cJSON_GetArrayItem(records, i), &pins[i]) ||
            !contains(field(controller, "pins"), pins[i].name) ||
            !claim_pad(resolver, &pins[i]))
            return false;
        for (int j = 0; j < i; ++j) {
            if (same(pins[i].name, pins[j].name))
                return fail(resolver, NXCONFIG_CONFLICT, "Route repeats a pad");
        }
    }
    return true;
}

static bool gpio(resolver_t* resolver, toml_table_t* authored,
                 const cJSON* binding, nxconfig_controller_t* result) {
    static const char* const allowed[] = {
        "binding", "mode", "initial", "mask", "pull", "output_type", NULL};
    nxconfig_gpio_t* options = &result->options.gpio;
    if (!keys(resolver, authored, allowed))
        return false;
    uint32_t mask = 0U;
    for (size_t i = 0U; i < result->pin_count; ++i) {
        if (result->pins[i].port != result->pins[0].port)
            return fail(resolver, NXCONFIG_INVALID,
                        "GPIO binding spans register banks");
        mask |= UINT32_C(1) << result->pins[i].number;
    }
    uint32_t default_mask, default_initial;
    if (!json_number(resolver, binding, "mask", &default_mask) ||
        !json_number(resolver, binding, "initial", &default_initial))
        return false;
    if (default_mask != mask || (default_initial & ~mask) != 0U)
        return fail(resolver, NXCONFIG_INVALID,
                    "GPIO binding mask does not describe its route");
    if (!authored_uint(resolver, authored, "mask", &options->mask, mask, 1U,
                       65535U) ||
        !authored_uint(resolver, authored, "initial", &options->initial,
                       default_initial, 0U, 65535U))
        return false;
    if (options->mask != mask || (options->initial & ~mask) != 0U)
        return fail(resolver, NXCONFIG_INVALID,
                    "GPIO selection must exactly own its reviewed pads");
    options->output = same(result->mode, "output");
    const cJSON* electrical = field(binding, "electrical");
    const char* board_pull = text(electrical, "pull");
    const char* board_type = text(electrical, "output_type");
    char pull[16], type[24];
    if (!authored_string(resolver, authored, "pull", pull, sizeof(pull),
                         board_pull != NULL ? board_pull : "none") ||
        !authored_string(resolver, authored, "output_type", type, sizeof(type),
                         board_type != NULL ? board_type : "push-pull"))
        return false;
    if (!same(pull, "none") && !same(pull, "up") && !same(pull, "down"))
        return fail(resolver, NXCONFIG_INVALID, "Unknown GPIO pull");
    if (!same(type, "push-pull") && !same(type, "open-drain"))
        return fail(resolver, NXCONFIG_INVALID, "Unknown GPIO output type");
    options->pull = same(pull, "up") ? 1U : same(pull, "down") ? 2U : 0U;
    options->output_type = same(type, "open-drain") ? 1U : 0U;
    if (electrical != NULL &&
        ((text(electrical, "mode") != NULL &&
          !same(text(electrical, "mode"), result->mode)) ||
         (board_pull != NULL && !same(board_pull, pull)) ||
         (board_type != NULL && !same(board_type, type))))
        return fail(resolver, NXCONFIG_CONFLICT,
                    "GPIO electrical choice contradicts board wiring");
    return true;
}

static bool bus_clock(resolver_t* resolver, const cJSON* controller,
                      uint32_t* hz) {
    const char* domain = text(controller, "clock_domain");
    if (same(domain, "apb1"))
        *hz = resolver->assembly->platform.apb1_hz;
    else if (same(domain, "apb2"))
        *hz = resolver->assembly->platform.apb2_hz;
    else
        return fail(resolver, NXCONFIG_INVALID,
                    "Missing or unknown reviewed clock domain");
    return *hz != 0U ||
           fail(resolver, NXCONFIG_INVALID, "Zero controller clock");
}

static bool uart(resolver_t* resolver, toml_table_t* authored,
                 const cJSON* controller, nxconfig_controller_t* result) {
    static const char* const allowed[] = {"binding", "mode", "baud",
                                          "rx",      "irq",  NULL};
    static const char* const rx_allowed[] = {"format", "capacity", NULL};
    static const char* const irq_allowed[] = {"priority", "calls_os", NULL};
    nxconfig_uart_t* options = &result->options.uart;
    if (result->pin_count != 2U)
        return fail(resolver, NXCONFIG_INVALID,
                    "UART cold configuration requires exact TX and RX pads");
    toml_table_t* rx = toml_table_in(authored, "rx");
    toml_table_t* irq = toml_table_in(authored, "irq");
    if ((toml_key_exists(authored, "rx") && rx == NULL) ||
        (toml_key_exists(authored, "irq") && irq == NULL))
        return fail(resolver, NXCONFIG_INVALID,
                    "UART rx and irq must be tables");
    uint32_t priority;
    if (!keys(resolver, authored, allowed) || !keys(resolver, rx, rx_allowed) ||
        !keys(resolver, irq, irq_allowed) ||
        !authored_uint(resolver, authored, "baud", &options->baud, 115200U, 1U,
                       10500000U) ||
        !authored_uint(resolver, rx, "capacity", &options->rx_capacity, 64U, 1U,
                       65535U) ||
        !authored_uint(
            resolver, irq, "priority", &priority, 5U, 0U,
            (UINT32_C(1) << resolver->assembly->platform.irq_priority_bits) -
                1U) ||
        !bus_clock(resolver, controller, &options->clock_hz))
        return false;
    if (options->baud > options->clock_hz / 16U)
        return fail(resolver, NXCONFIG_INVALID,
                    "UART baud exceeds maintained oversampling range");
    char format[16];
    if (!authored_string(resolver, rx, "format", format, sizeof(format),
                         "bytes"))
        return false;
    if (!same(format, "bytes") && !same(format, "events"))
        return fail(resolver, NXCONFIG_UNSUPPORTED,
                    "UART RX profile has no selected C adapter");
    options->events = same(format, "events");
    options->priority = (uint8_t)priority;
    if (irq != NULL && toml_key_exists(irq, "calls_os")) {
        toml_datum_t calls_os = toml_bool_in(irq, "calls_os");
        if (!calls_os.ok || calls_os.u.b)
            return fail(resolver, NXCONFIG_UNSUPPORTED,
                        "baremetal UART cannot call an OS from IRQ");
    }
    const cJSON* vectors = field(controller, "irq");
    if (!cJSON_IsArray(vectors) || cJSON_GetArraySize(vectors) != 1 ||
        !copy(resolver, options->irq, sizeof(options->irq),
              string(cJSON_GetArrayItem(vectors, 0))))
        return fail(resolver, NXCONFIG_INVALID,
                    "UART requires one reviewed IRQ vector");
    if (!name_valid(options->irq))
        return fail(resolver, NXCONFIG_INVALID, "Invalid IRQ symbol");
    return true;
}

static bool spi(resolver_t* resolver, toml_table_t* authored,
                const cJSON* controller, nxconfig_controller_t* result) {
    static const char* const allowed[] = {"binding", "mode", "max_hz", NULL};
    nxconfig_spi_t* options = &result->options.spi;
    uint32_t limit;
    if (!keys(resolver, authored, allowed) ||
        !bus_clock(resolver, controller, &options->clock_hz) ||
        !json_number(resolver, controller, "max_hz", &limit) ||
        !authored_uint(resolver, authored, "max_hz", &options->max_hz, 1000000U,
                       1U, limit))
        return false;
    if (result->pin_count != 3U || options->max_hz > options->clock_hz / 2U)
        return fail(
            resolver, NXCONFIG_INVALID,
            "SPI short-poll requires three pads and legal prescaler budget");
    return true;
}

static bool controllers(resolver_t* resolver, const char* class_name,
                        nxconfig_kind_t kind) {
    nxconfig_assembly_t* assembly = resolver->assembly;
    toml_table_t* group = toml_table_in(assembly->authored, class_name);
    if (group == NULL)
        return !toml_key_exists(assembly->authored, class_name) ||
               fail(resolver, NXCONFIG_INVALID, "I/O category must be a table");
    for (int index = 0;; ++index) {
        const char* id = toml_key_in(group, index);
        if (id == NULL)
            return true;
        (void)snprintf(resolver->context, sizeof(resolver->context), "%s.%s",
                       class_name, id);
        toml_table_t* authored = toml_table_in(group, id);
        if (authored == NULL || !name_valid(id))
            return fail(resolver, NXCONFIG_INVALID,
                        "Expected named device table and ASCII identifier");
        nxconfig_controller_t result = {.kind = kind};
        if (!copy(resolver, result.id, sizeof(result.id), id) ||
            !authored_string(resolver, authored, "binding", result.binding,
                             sizeof(result.binding), NULL) ||
            !authored_string(resolver, authored, "mode", result.mode,
                             sizeof(result.mode), NULL))
            return false;
        if (kind == NXCONFIG_UART && same(result.mode, "irq"))
            (void)snprintf(result.mode, sizeof(result.mode), "irq-byte-event");
        if ((kind == NXCONFIG_GPIO && !same(result.mode, "input") &&
             !same(result.mode, "output")) ||
            (kind == NXCONFIG_UART && !same(result.mode, "irq-byte-event")) ||
            (kind == NXCONFIG_SPI && !same(result.mode, "short-poll")))
            return fail(resolver, NXCONFIG_UNSUPPORTED,
                        "Mode has not been implemented by this Host C path");
        const cJSON* binding;
        const cJSON* controller;
        const cJSON* route = binding_route(resolver, result.binding, class_name,
                                           result.mode, &binding, &controller);
        if (route == NULL ||
            !copy(resolver, result.controller, sizeof(result.controller),
                  text(binding, "controller")) ||
            !unique_controller(resolver, kind, result.id, result.controller) ||
            (kind != NXCONFIG_GPIO &&
             !copy(resolver, result.clock_domain, sizeof(result.clock_domain),
                   text(controller, "clock_domain"))))
            return false;
        if (!selected_pins(resolver, route, controller, &result)) {
            free((void*)result.pins);
            return false;
        }
        bool valid = kind == NXCONFIG_GPIO
                         ? gpio(resolver, authored, binding, &result)
                     : kind == NXCONFIG_UART
                         ? uart(resolver, authored, controller, &result)
                         : spi(resolver, authored, controller, &result);
        if (!valid) {
            free((void*)result.pins);
            return false;
        }
        size_t count = assembly->controller_count;
        nxconfig_controller_t* grown =
            realloc(assembly->controllers, (count + 1U) * sizeof(*grown));
        if (grown == NULL) {
            free((void*)result.pins);
            return fail(resolver, NXCONFIG_MEMORY,
                        "Cannot allocate typed controller record");
        }
        assembly->controllers = grown;
        assembly->controllers[count] = result;
        ++assembly->controller_count;
    }
}

static bool endpoints(resolver_t* resolver) {
    nxconfig_assembly_t* assembly = resolver->assembly;
    toml_table_t* group = toml_table_in(assembly->authored, "spi_device");
    if (group == NULL)
        return !toml_key_exists(assembly->authored, "spi_device") ||
               fail(resolver, NXCONFIG_INVALID, "spi_device must be a table");
    static const char* const allowed[] = {"controller", "driver", "cs_binding",
                                          "mode",       "max_hz", NULL};
    for (int index = 0;; ++index) {
        const char* id = toml_key_in(group, index);
        if (id == NULL)
            return true;
        (void)snprintf(resolver->context, sizeof(resolver->context),
                       "spi_device.%s", id);
        toml_table_t* authored = toml_table_in(group, id);
        nxconfig_spi_endpoint_t result = {0};
        char logical[64], driver[32], binding_id[64];
        if (!name_valid(id) || authored == NULL ||
            !keys(resolver, authored, allowed) ||
            !copy(resolver, result.id, sizeof(result.id), id) ||
            !authored_string(resolver, authored, "controller", logical,
                             sizeof(logical), NULL) ||
            !authored_string(resolver, authored, "driver", driver,
                             sizeof(driver), NULL) ||
            !authored_string(resolver, authored, "cs_binding", binding_id,
                             sizeof(binding_id), NULL))
            return fail(resolver, NXCONFIG_INVALID,
                        "Invalid SPI endpoint table");
        if (!same(driver, "spi-endpoint"))
            return fail(resolver, NXCONFIG_UNSUPPORTED,
                        "SPI endpoint driver is not implemented");
        const nxconfig_controller_t* owner = NULL;
        for (size_t i = 0U; i < assembly->controller_count; ++i) {
            if (assembly->controllers[i].kind == NXCONFIG_SPI &&
                same(assembly->controllers[i].id, logical)) {
                owner = &assembly->controllers[i];
                result.controller_index = i;
            }
        }
        if (owner == NULL)
            return fail(resolver, NXCONFIG_INVALID,
                        "SPI endpoint references no selected SPI controller");
        char normalized[64], previous[64];
        nxconfig_identifier(id, normalized, true);
        if (same(normalized, "COUNT"))
            return fail(resolver, NXCONFIG_CONFLICT,
                        "Endpoint identifier is reserved by the typed factory");
        for (size_t i = 0U; i < assembly->endpoint_count; ++i) {
            nxconfig_identifier(assembly->endpoints[i].id, previous, true);
            if (same(normalized, previous))
                return fail(resolver, NXCONFIG_CONFLICT,
                            "SPI endpoint generated identifier already exists");
        }
        uint32_t mode;
        if (!authored_uint(resolver, authored, "mode", &mode, 0U, 0U, 3U) ||
            !authored_uint(resolver, authored, "max_hz", &result.max_hz,
                           owner->options.spi.max_hz, 1U,
                           owner->options.spi.max_hz))
            return false;
        result.mode = (uint8_t)mode;
        const cJSON* binding;
        const cJSON* controller;
        const cJSON* route = binding_route(resolver, binding_id, "gpio",
                                           "output", &binding, &controller);
        (void)controller;
        const cJSON* pins = field(route, "pins");
        uint32_t initial, mask;
        if (route == NULL || cJSON_GetArraySize(pins) != 1 ||
            !pad(resolver, cJSON_GetArrayItem(pins, 0), &result.chip_select) ||
            !claim_pad(resolver, &result.chip_select) ||
            !json_number(resolver, binding, "initial", &initial) ||
            !json_number(resolver, binding, "mask", &mask))
            return false;
        if (mask != (UINT32_C(1) << result.chip_select.number) ||
            initial != mask)
            return fail(resolver, NXCONFIG_CONFLICT,
                        "SPI CS requires one reviewed inactive-high pad");
        size_t count = assembly->endpoint_count;
        nxconfig_spi_endpoint_t* grown =
            realloc(assembly->endpoints, (count + 1U) * sizeof(*grown));
        if (grown == NULL)
            return fail(resolver, NXCONFIG_MEMORY,
                        "Cannot allocate typed SPI endpoint");
        assembly->endpoints = grown;
        assembly->endpoints[count] = result;
        ++assembly->endpoint_count;
    }
}

static bool complete_resources(resolver_t* resolver) {
    const nxconfig_assembly_t* assembly = resolver->assembly;
    for (size_t i = 0U; i < assembly->controller_count; ++i) {
        if (assembly->controllers[i].kind != NXCONFIG_SPI)
            continue;
        bool addressed = false;
        for (size_t j = 0U; j < assembly->endpoint_count; ++j)
            addressed =
                addressed || assembly->endpoints[j].controller_index == i;
        if (!addressed) {
            (void)snprintf(resolver->context, sizeof(resolver->context),
                           "spi.%s", assembly->controllers[i].id);
            return fail(resolver, NXCONFIG_INVALID,
                        "Polling SPI requires an explicit endpoint and CS");
        }
    }
    return true;
}

nxconfig_result_t nxconfig_resolve(const char* source_root,
                                   const char* assembly_path,
                                   nxconfig_assembly_t** output,
                                   nxconfig_diagnostic_t* diagnostic) {
    if (diagnostic != NULL)
        *diagnostic = (nxconfig_diagnostic_t){0};
    if (output == NULL)
        return nxconfig_error(diagnostic, NXCONFIG_INVALID, assembly_path,
                              "input", "Missing owner output");
    *output = NULL;
    if (source_root == NULL || assembly_path == NULL)
        return nxconfig_error(diagnostic, NXCONFIG_INVALID, assembly_path,
                              "input", "Missing input path");
    nxconfig_assembly_t* assembly = calloc(1U, sizeof(*assembly));
    if (assembly == NULL)
        return nxconfig_error(diagnostic, NXCONFIG_MEMORY, assembly_path,
                              "input", "Cannot allocate owner");
    nxconfig_diagnostic_t local = {0};
    resolver_t resolver = {.assembly = assembly,
                           .diagnostic =
                               diagnostic != NULL ? diagnostic : &local,
                           .source = assembly_path,
                           .context = "assembly"};
    bool valid = load(&resolver, assembly_path, NULL);
    if (valid) {
        char error[256];
        assembly->authored =
            toml_parse(assembly->inputs[0].bytes, error, (int)sizeof(error));
        if (assembly->authored == NULL)
            valid = fail(&resolver, NXCONFIG_INVALID, error);
    }
    static const char* const allowed[] = {
        "schema", "board",        "board_package", "backend",
        "clock",  "optimization", "memory",        "hardware",
        "gpio",   "uart",         "spi",           "spi_device",
        NULL};
    if (valid) {
        uint32_t schema;
        char backend[32];
        valid = keys(&resolver, assembly->authored, allowed) &&
                authored_uint(&resolver, assembly->authored, "schema", &schema,
                              0U, 2U, 2U) &&
                authored_string(&resolver, assembly->authored, "backend",
                                backend, sizeof(backend), NULL);
        if (valid && schema != 2U)
            valid = fail(&resolver, NXCONFIG_INVALID, "schema=2 is required");
        if (valid && !same(backend, "baremetal"))
            valid = fail(&resolver, NXCONFIG_UNSUPPORTED,
                         "Host C slice currently implements baremetal only");
    }
    char path[4096];
    if (valid) {
        bool named = toml_key_exists(assembly->authored, "board");
        bool packaged = toml_key_exists(assembly->authored, "board_package");
        if (named == packaged)
            valid = fail(&resolver, NXCONFIG_INVALID,
                         "Select exactly one board or board_package");
        char selected[2048];
        if (valid)
            valid = authored_string(&resolver, assembly->authored,
                                    named ? "board" : "board_package", selected,
                                    sizeof(selected), NULL);
        if (valid && named && !name_valid(selected))
            valid =
                fail(&resolver, NXCONFIG_INVALID, "Invalid board identifier");
        if (valid) {
            int count;
            if (named)
                count = snprintf(path, sizeof(path), "%s/boards/%s/board.json",
                                 source_root, selected);
            else if (selected[0] == '/')
                count = snprintf(path, sizeof(path), "%s/board.json", selected);
            else {
                const char* input = assembly->inputs[0].path;
                const char* slash = strrchr(input, '/');
                count = snprintf(path, sizeof(path), "%.*s/%s/board.json",
                                 (int)(slash - input), input, selected);
            }
            valid = count > 0 && (size_t)count < sizeof(path);
            if (!valid)
                (void)fail(&resolver, NXCONFIG_INVALID, "Board path too long");
        }
        if (valid)
            valid = load(&resolver, path, &assembly->board);
    }
    const char* facts[] = {"soc.json", "routes.json", "devices.json"};
    cJSON** objects[] = {&assembly->soc, &assembly->routes, &assembly->devices};
    for (size_t i = 0U; valid && i < 3U; ++i) {
        int count = snprintf(path, sizeof(path), "%s/soc/stm32/stm32f4/%s",
                             source_root, facts[i]);
        valid = count > 0 && (size_t)count < sizeof(path) &&
                load(&resolver, path, objects[i]);
        if (!valid && resolver.diagnostic->code == NXCONFIG_OK)
            (void)fail(&resolver, NXCONFIG_INVALID, "Fact path too long");
    }
    if (valid)
        valid = platform(&resolver) && hardware_choices(&resolver) &&
                controllers(&resolver, "gpio", NXCONFIG_GPIO) &&
                controllers(&resolver, "uart", NXCONFIG_UART) &&
                controllers(&resolver, "spi", NXCONFIG_SPI) &&
                endpoints(&resolver) && complete_resources(&resolver);
    if (valid) {
        *output = assembly;
        return NXCONFIG_OK;
    }
    nxconfig_result_t result = resolver.diagnostic->code;
    if (result == NXCONFIG_OK)
        result =
            nxconfig_error(resolver.diagnostic, NXCONFIG_INVALID, assembly_path,
                           resolver.context, "Invalid selected configuration");
    nxconfig_destroy(assembly);
    return result;
}

void nxconfig_destroy(nxconfig_assembly_t* assembly) {
    if (assembly == NULL)
        return;
    for (size_t i = 0U; i < assembly->controller_count; ++i)
        free((void*)assembly->controllers[i].pins);
    free(assembly->controllers);
    free(assembly->endpoints);
    for (size_t i = 0U; i < assembly->input_count; ++i) {
        free(assembly->inputs[i].path);
        free(assembly->inputs[i].bytes);
    }
    toml_free(assembly->authored);
    cJSON_Delete(assembly->board);
    cJSON_Delete(assembly->soc);
    cJSON_Delete(assembly->routes);
    cJSON_Delete(assembly->devices);
    free(assembly);
}

const nxconfig_platform_t*
nxconfig_platform(const nxconfig_assembly_t* assembly) {
    return assembly != NULL ? &assembly->platform : NULL;
}

size_t nxconfig_controller_count(const nxconfig_assembly_t* assembly) {
    return assembly != NULL ? assembly->controller_count : 0U;
}

const nxconfig_controller_t*
nxconfig_controller(const nxconfig_assembly_t* assembly, size_t index) {
    return assembly != NULL && index < assembly->controller_count
               ? &assembly->controllers[index]
               : NULL;
}

size_t nxconfig_spi_endpoint_count(const nxconfig_assembly_t* assembly) {
    return assembly != NULL ? assembly->endpoint_count : 0U;
}

const nxconfig_spi_endpoint_t*
nxconfig_spi_endpoint(const nxconfig_assembly_t* assembly, size_t index) {
    return assembly != NULL && index < assembly->endpoint_count
               ? &assembly->endpoints[index]
               : NULL;
}
