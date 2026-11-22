/**
 * \file            application.h
 * \brief           Public-only robotics and smart component SDK consumer
 * \author          Nexus Team
 */
#ifndef NEXUS_SDK_SMART_APPLICATION_H
#define NEXUS_SDK_SMART_APPLICATION_H
/**
 * \brief Execute one known dense layer and public adapter admission checks.
 * \return Zero on success; nonzero identifies a mismatched software contract.
 * \note No hardware is accessed and no device, GUI or protocol is started.
 */
int smart_component_application(void);
#endif
