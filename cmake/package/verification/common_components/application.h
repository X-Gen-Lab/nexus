/**
 * \file            application.h
 * \brief           Public-only common component SDK consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_SDK_COMPONENT_APPLICATION_H
#define NEXUS_SDK_COMPONENT_APPLICATION_H

/**
 * \brief           Exercise pure arithmetic and public adapter admission.
 * \return          Zero on success, nonzero for a mismatched SDK contract.
 * \note            Synchronous task context, no device I/O or retained borrow.
 *                  Caller owns platform startup and composition lifetime.
 */
int common_component_application(void);

#endif
