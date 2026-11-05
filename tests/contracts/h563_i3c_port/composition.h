/**
 * \file            composition.h
 * \brief           Generic I3C boundary of the link-only private composition
 * \author          Nexus Team
 */
#ifndef NEXUS_H563_REFERENCE_COMPOSITION_H
#define NEXUS_H563_REFERENCE_COMPOSITION_H
#include "nexus/components/i3c_controller_storage.h"
/**
 * \brief Link the real private transport without asserting hardware readiness.
 * \param[out] controller: Zeroed static controller surviving every port face.
 * \return UNSUPPORTED without trusted hardware/reset proof; cold validation
 *         may return INVALID, STATE or BUSY before that proof.
 * \note Link-only reference, never a boot recipe or physical validation.
 */
nx_result_t nx_h563_reference_compose(nx_i3c_controller_t* controller);
#endif /* NEXUS_H563_REFERENCE_COMPOSITION_H */
