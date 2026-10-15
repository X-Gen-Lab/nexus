/**
 * \file            stm32_official_model.h
 * \brief           Host equivalent of the CMSIS bit-position CPU primitive
 * \author          Nexus Team
 */
#ifndef NX_STM32_OFFICIAL_MODEL_H
#define NX_STM32_OFFICIAL_MODEL_H

#include "stm32f4xx.h"

/* Official LL GPIO uses CLZ(RBIT(pin)); the host has no Arm RBIT instruction.
 * Only the CPU primitive changes. Real LL inline bodies and register structs
 * execute unchanged, with nonzero single-pin masks checked by production code.
 */
#undef POSITION_VAL
#define POSITION_VAL(value) ((uint32_t)__builtin_ctz((uint32_t)(value)))

/* Host equivalents of the vendor's exclusive CPU primitives. No competing
 * actor writes modeled MMIO. Production LL keeps original CMSIS operations.
 */
#undef ATOMIC_SET_BIT
#undef ATOMIC_CLEAR_BIT
#undef ATOMIC_MODIFY_REG
#define ATOMIC_SET_BIT(reg, bits)          SET_BIT(reg, bits)
#define ATOMIC_CLEAR_BIT(reg, bits)        CLEAR_BIT(reg, bits)
#define ATOMIC_MODIFY_REG(reg, clear, set) MODIFY_REG(reg, clear, set)

#endif
