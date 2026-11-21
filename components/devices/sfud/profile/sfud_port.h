/**
 * \file            sfud_port.h
 * \brief           Private declaration for the selected upstream port ABI
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_PRIVATE_SFUD_PORT_H
#define NEXUS_PRIVATE_SFUD_PORT_H
#include <sfud.h>
/** \brief Discovery is outside this selected static-profile adapter. */
sfud_err sfud_spi_port_init(sfud_flash* flash);
#endif
