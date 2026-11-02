/**
 * \file            layout.c
 * \brief           ABI layout symbols, not linked into the resource firmware
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/tmp117_i2c.h"
unsigned char layout_device[sizeof(nx_tmp117_t)];
unsigned char layout_sample[sizeof(nx_tmp117_sample_t)];
unsigned char layout_port[sizeof(nx_tmp117_transport_port_t)];
unsigned char layout_timestamp[sizeof(nx_io_timestamp_t)];
