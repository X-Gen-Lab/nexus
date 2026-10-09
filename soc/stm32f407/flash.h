#ifndef NEXUS_STM32F407_FLASH_H
#define NEXUS_STM32F407_FLASH_H
#include "hal/interface/nx_flash.h"
/* The SoC registers typed FLASH0 for the whole xE/xG device. Applications
 * borrow HAL regions from their own layout; no storage/linker partition is
 * required. Explicit unlock, task context, unmasked maintenance window,
 * stable SYSCLK and VDD 2.7..3.6V. A started pulse cannot be aborted; deadlines
 * return after settlement. DWT must run and each pulse must be shorter than
 * its 32-bit wrap. This is not control-loop or physical power-fail acceptance.
 * The caller must exclude the executing image and vectors from erase/program;
 * full-chip geometry is a physical fact, not permission to alter live code. */
#endif
