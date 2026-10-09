#ifndef NEXUS_GD32F470ZG_FLASH_H
#define NEXUS_GD32F470ZG_FLASH_H
#include "hal/interface/nx_flash.h"
/* Typed FLASH0 exposes the whole 1MiB device and F470-specific 4KiB blocks.
 * External layouts/regions define storage, with no default product partition.
 * Explicit unlock, task context and an unmasked maintenance window at 3.3V.
 * TIMER1 measures total deadlines; a started pulse settles before return.
 * Same-bank fetch stalls; physical timing/protection/power-fail HIL required.
 * The caller must exclude the executing image and vectors from writes; full
 * physical geometry does not grant permission to erase live executable code. */
#endif
