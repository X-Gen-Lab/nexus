/**
 * \file            runtime.c
 * \brief           C runtime hooks; constructors use the linker init arrays
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/** \brief Runtime initialization is owned by the ordinary C startup path. */
void _init(void);
/** \brief Runtime finalization is owned by the ordinary C startup path. */
void _fini(void);

void _init(void) {
}

void _fini(void) {
}
