/**
 * \file            runtime.c
 * \brief           C runtime hooks for the fixed STM32F1 image startup.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/** \brief Runtime initialization runs through the linker init array. */
void _init(void);
/** \brief Runtime finalization belongs to the ordinary C runtime. */
void _fini(void);

void _init(void) {
}

void _fini(void) {
}
