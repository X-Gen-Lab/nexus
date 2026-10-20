/**
 * \file            u575_lptim_factory_model.h
 * \brief           External addresses for the generated LPTIM2 assembly
 * \author          Nexus Team
 */
#ifndef NX_U575_LPTIM_FACTORY_MODEL_H
#define NX_U575_LPTIM_FACTORY_MODEL_H
#include "u575_factory_model.h"
#include "u575_lptim_model.h"
#undef LPTIM2
#define LPTIM2 (&g_u575_lptim_model.registers)
#endif
