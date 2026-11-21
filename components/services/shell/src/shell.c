/**
 * \file            shell.c
 * \brief           Thin read-only execution port for pinned letter-shell
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "log_format_impl.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include "nexus/components/shell_storage.h"
#include <limits.h>
#include <stdarg.h>

extern void shellUp(Shell* shell);
extern void shellDown(Shell* shell);
extern void shellLeft(Shell* shell);
extern void shellRight(Shell* shell);
extern void shellTab(Shell* shell);
extern void shellBackspace(Shell* shell);
extern void shellDelete(Shell* shell);
extern void shellEnter(Shell* shell);
extern void shellHelp(int argc, char* argv[]);

static uint32_t entering;
static nx_shell_t* live_shell;
static nx_shell_t* active_shell;

/** \brief Upstream callback signatures require one serialized transient
 * context. */
static nx_result_t enter(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0U;
    return nx_atomic_u32_compare_exchange_acq_rel(&entering, &expected, 1U)
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

/** \brief Never leave a callback pointer referring to caller storage after
 * exit. */
static void leave(void) {
    active_shell = NULL;
    nx_atomic_u32_store_release(&entering, 0U);
}

/** \brief Convert complete synchronous transport writes to upstream byte
 * counts. */
static signed short output_bytes(char* bytes, unsigned short length) {
    if (active_shell == NULL || active_shell->result != NX_SUCCESS) {
        return 0;
    }
    if (length > SHRT_MAX) {
        active_shell->result = NX_ERROR_OVERFLOW;
        return 0;
    }
    active_shell->result = active_shell->io.write(
        active_shell->io.context, bytes, length, active_shell->deadline);
    return active_shell->result == NX_SUCCESS ? (signed short)length : 0;
}

/** \brief Command formatting uses fixed numeric formats and an 80-byte version.
 */
static void print(const char* format, ...) {
    char bytes[160];
    va_list arguments;
    va_start(arguments, format);
    size_t length = 0U;
    nx_result_t rendered =
        nx_log_render(bytes, sizeof(bytes), &length, format, arguments);
    va_end(arguments);
    if (rendered != NX_SUCCESS) {
        if (active_shell != NULL) {
            active_shell->result = rendered;
        }
        return;
    }
    (void)output_bytes(bytes, (unsigned short)length);
}

/** \brief No default command accepts a modifying argument or address. */
static bool no_arguments(int argc) {
    if (argc != 1) {
        print("unexpected arguments\r\n");
        return false;
    }
    return true;
}

/** \brief Preserve upstream completion and help over the selected static table.
 */
static int command_help(int argc, char* argv[]) {
    shellHelp(argc, argv);
    return 0;
}

/** \brief Immutable product version is data, never a printf format. */
static int command_version(int argc, char* argv[]) {
    (void)argv;
    if (no_arguments(argc)) {
        const nx_shell_diagnostics_t* diagnostics = active_shell->diagnostics;
        print("%.80s\r\n", diagnostics != NULL && diagnostics->version != NULL
                               ? diagnostics->version
                               : "version=unknown");
    }
    return 0;
}

/** \brief Read stable known groups without sampling live I/O or granting
 * recovery. */
static int command_health(int argc, char* argv[]) {
    (void)argv;
    if (!no_arguments(argc)) {
        return 0;
    }
    const nx_shell_diagnostics_t* diagnostics = active_shell->diagnostics;
    const nx_health_snapshot_t* health =
        diagnostics != NULL ? diagnostics->health : NULL;
    if (health == NULL || health->known == 0U) {
        print("health=unknown\r\n");
        return 0;
    }
    print("health findings=0x%08x\r\n", (unsigned)health->findings);
    if ((health->known & NX_HEALTH_PROGRESS_KNOWN) != 0U) {
        print("progress generation=%llu age_us=%llu\r\n",
              (unsigned long long)health->progress_generation,
              (unsigned long long)health->progress_age_us);
    } else {
        print("progress=unknown\r\n");
    }
    if ((health->known & NX_HEALTH_IO_KNOWN) != 0U) {
        print("io findings=0x%08x unknown=0x%08x\r\n",
              (unsigned)health->io_findings,
              (unsigned)health->io_unknown_facts);
    } else {
        print("io=unknown\r\n");
    }
    return 0;
}

/** \brief A missing core validity flag never turns zero into an observed PC. */
static int command_fault(int argc, char* argv[]) {
    (void)argv;
    if (!no_arguments(argc)) {
        return 0;
    }
    const nx_shell_diagnostics_t* diagnostics = active_shell->diagnostics;
    const nx_reset_record_t* reset =
        diagnostics != NULL ? diagnostics->reset : NULL;
    if (reset == NULL) {
        print("fault=unknown\r\n");
    } else if (!reset->has_fault) {
        print("fault=none\r\n");
    } else {
        const nx_fault_capture_t* fault = &reset->fault;
        print("fault sequence=%u domain=%u reason=%u flags=0x%08x\r\n",
              (unsigned)fault->sequence, (unsigned)fault->domain,
              (unsigned)fault->reason, (unsigned)fault->flags);
        if ((fault->flags & NX_FAULT_CORE_VALID) != 0U) {
            print("pc=0x%08x lr=0x%08x\r\n", (unsigned)fault->core[6],
                  (unsigned)fault->core[5]);
        } else {
            print("core=unknown\r\n");
        }
    }
    return 0;
}

/** \brief This command reports existing reset bits and never resets hardware.
 */
static int command_reset(int argc, char* argv[]) {
    (void)argv;
    if (no_arguments(argc)) {
        const nx_shell_diagnostics_t* diagnostics = active_shell->diagnostics;
        if (diagnostics != NULL && diagnostics->reset != NULL) {
            print("reset=0x%08x\r\n",
                  (unsigned)diagnostics->reset->reset_causes);
        } else {
            print("reset=unknown\r\n");
        }
    }
    return 0;
}

/** \brief Stack paint and ring loss are copied facts with independent unknowns.
 */
static int command_resources(int argc, char* argv[]) {
    (void)argv;
    if (!no_arguments(argc)) {
        return 0;
    }
    const nx_shell_diagnostics_t* diagnostics = active_shell->diagnostics;
    const nx_health_resources_t* resources =
        diagnostics != NULL ? diagnostics->resources : NULL;
    if (resources == NULL || resources->known == 0U) {
        print("resources=unknown\r\n");
        return 0;
    }
    if ((resources->known & NX_HEALTH_STACK_KNOWN) != 0U) {
        print("stack capacity=%zu untouched=%zu touched=%zu\r\n",
              resources->stack.capacity_bytes, resources->stack.untouched_bytes,
              resources->stack.touched_bytes);
    } else {
        print("stack=unknown\r\n");
    }
    if ((resources->known & NX_HEALTH_RING_KNOWN) != 0U) {
        print("ring dropped=%u\r\n", (unsigned)resources->ring_dropped);
    } else {
        print("ring=unknown\r\n");
    }
    return 0;
}

#define COMMAND_ATTRIBUTES                                                     \
    (SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN)
#define KEY_ATTRIBUTES                                                         \
    (SHELL_CMD_TYPE(SHELL_TYPE_KEY) | SHELL_CMD_ENABLE_UNCHECKED)

/** \brief Actual upstream table without variables, exec, sh or maintenance
 * APIs. */
const ShellCommand shellCommandList[] = {
    {.attr.value = SHELL_CMD_TYPE(SHELL_TYPE_USER),
     .data.user = {"nexus", "", "read-only diagnostics"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"help", command_help, "show read-only command help"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"version", command_version, "firmware version"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"health", command_health, "copied health facts"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"fault", command_fault, "previous raw crash facts"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"reset", command_reset, "latched reset causes"}},
    {.attr.value = COMMAND_ATTRIBUTES,
     .data.cmd = {"resources", command_resources,
                  "copied stack and ring facts"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x0a000000, shellEnter, "enter"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x0d000000, shellEnter, "enter"}},
    {.attr.value = KEY_ATTRIBUTES, .data.key = {0x09000000, shellTab, "tab"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x08000000, shellBackspace, "backspace"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x7f000000, shellDelete, "delete"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x1b5b4100, shellUp, "history up"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x1b5b4200, shellDown, "history down"}},
    {.attr.value = KEY_ATTRIBUTES,
     .data.key = {0x1b5b4300, shellRight, "right"}},
    {.attr.value = KEY_ATTRIBUTES, .data.key = {0x1b5b4400, shellLeft, "left"}},
};
const unsigned short shellCommandCount =
    (unsigned short)(sizeof(shellCommandList) / sizeof(shellCommandList[0]));

/** \brief Validate scratch provenance before upstream initialization writes. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Limit the only application-controlled string consumed by commands. */
static bool valid_version(const nx_shell_diagnostics_t* diagnostics) {
    if (diagnostics == NULL || diagnostics->version == NULL) {
        return true;
    }
    for (size_t i = 0U; i <= 80U; ++i) {
        if (diagnostics->version[i] == '\0') {
            return true;
        }
    }
    return false;
}

/** \brief Bind the real parser without publishing a second backend instance. */
nx_result_t nx_shell_init(nx_shell_t* shell, nx_shell_io_t io, char* buffer,
                          size_t capacity,
                          const nx_shell_diagnostics_t* diagnostics,
                          nx_time_us_t deadline) {
    if (shell == NULL || io.read == NULL || io.write == NULL ||
        buffer == NULL || capacity < 64U || capacity > UINT16_MAX ||
        !valid_version(diagnostics) ||
        overlaps(shell, sizeof(*shell), buffer, capacity) ||
        (diagnostics != NULL &&
         (overlaps(diagnostics, sizeof(*diagnostics), shell, sizeof(*shell)) ||
          overlaps(diagnostics, sizeof(*diagnostics), buffer, capacity)))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (live_shell != NULL) {
        leave();
        return NX_ERROR_BUSY;
    }
    *shell = (nx_shell_t){0};
    shell->io = io;
    shell->diagnostics = diagnostics;
    shell->deadline = deadline;
    shell->result = NX_SUCCESS;
    shell->engine.write = output_bytes;
    active_shell = shell;
    shellInit(&shell->engine, buffer, (unsigned short)capacity);
    result = shell->result;
    if (result == NX_SUCCESS) {
        shell->initialized = true;
        live_shell = shell;
    } else {
        shellRemove(&shell->engine);
    }
    leave();
    return result;
}

/** \brief Read one byte and invoke the unchanged parser in the same task. */
nx_result_t nx_shell_step(nx_shell_t* shell, nx_time_us_t deadline) {
    if (shell == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!shell->initialized || live_shell != shell) {
        leave();
        return NX_ERROR_STATE;
    }
    active_shell = shell;
    shell->deadline = deadline;
    shell->result = NX_SUCCESS;
    uint8_t byte = 0U;
    size_t count = 0U;
    result = shell->io.read(shell->io.context, &byte, 1U, &count);
    if (result == NX_SUCCESS) {
        if (count > 1U) {
            result = NX_ERROR_IO;
        } else if (count == 0U) {
            result = NX_ERROR_EMPTY;
        } else {
            shellHandler(&shell->engine, (char)byte);
            result = shell->result;
        }
    }
    leave();
    return result;
}

/** \brief Remove upstream references only after its serialized entry has
 * exited. */
nx_result_t nx_shell_stop(nx_shell_t* shell) {
    if (shell == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (shell->initialized) {
        if (live_shell != shell) {
            leave();
            return NX_ERROR_STATE;
        }
        shellRemove(&shell->engine);
        shell->initialized = false;
        live_shell = NULL;
    }
    leave();
    return NX_SUCCESS;
}
