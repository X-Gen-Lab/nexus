/**
 * \file            shell_storage.h
 * \brief           Exact pinned letter-shell composition storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SHELL_STORAGE_H
#define NEXUS_COMPONENTS_SHELL_STORAGE_H
#include "nexus/components/shell.h"
#include <shell.h>

/**
 * \brief           Caller-owned backend; include only where storage is
 * composed.
 *
 * \note            Nexus::ShellStorage exposes the exact selected
 * upstream profile, avoiding a guessed maximum-size opaque allocation. Fields
 * are private implementation state, not product APIs.
 */
struct nx_shell {
    Shell engine;
    nx_shell_io_t io;
    const nx_shell_diagnostics_t* diagnostics;
    nx_time_us_t deadline;
    nx_result_t result;
    bool initialized;
};
#endif /* NEXUS_COMPONENTS_SHELL_STORAGE_H */
