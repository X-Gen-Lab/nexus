/**
 * \file            cia402_private.h
 * \brief           Selected drive OD admission without widening generic nodes
 * \author          Nexus Team
 */
#ifndef NEXUS_CIA402_PRIVATE_H
#define NEXUS_CIA402_PRIVATE_H
#include "nexus/components/canopen.h"
/** \brief Only the fixed CiA 402 builder calls this private admission path. */
nx_result_t nx_canopen_init_drive(nx_canopen_t* node,
                                  const nx_canopen_config_t* config,
                                  void* workspace, size_t bytes);
#endif
