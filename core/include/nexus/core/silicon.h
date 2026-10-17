/**
 * \file            silicon.h
 * \brief           Explicit cold observed-silicon software admission
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_SILICON_H
#define NEXUS_CORE_SILICON_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Observed silicon facts, without package or Board claims.
 * \note            Device group and Flash density cannot identify an exact
 *                  orderable part or package. Assembly and physical inspection
 *                  supply those independent facts. A missing revision has a
 *                  separate flag; numerical zero can be a real revision ID.
 */
typedef struct {
    uint32_t device_id;
    uint32_t revision_id;
    uint32_t flash_bytes;
    bool revision_known;
} nx_silicon_identity_t;

/**
 * \brief           Software review state, never physical qualification.
 * \note            Only REVIEWED and BLOCKED are valid authored rule values.
 */
typedef enum {
    NX_SILICON_REVISION_NOT_OBSERVED = 0,
    NX_SILICON_REVISION_REVIEWED,
    NX_SILICON_REVISION_BLOCKED,
    NX_SILICON_REVISION_UNREVIEWED,
    NX_SILICON_REVISION_WORKAROUND_MISSING,
    NX_SILICON_REVISION_DEVICE_MISMATCH,
    NX_SILICON_REVISION_DENSITY_MISMATCH
} nx_silicon_revision_state_t;

/** \brief One reviewed exact revision and its mode-specific workarounds. */
typedef struct {
    uint32_t revision_id;
    nx_silicon_revision_state_t disposition;
    uint32_t required_workarounds;
} nx_silicon_revision_rule_t;

/**
 * \brief           Caller-selected immutable software review policy.
 * \note            Rules have strictly increasing revision IDs. Nonempty
 *                  rules require a nonzero SHA256 identifying their reviewed
 *                  source record. This helper does not authenticate that
 *                  record or infer that required workarounds were implemented;
 *                  reviewed build/configuration facts supply the applied mask.
 *                  allow_unreviewed permits an explicitly observed unknown
 *                  revision only. It cannot override blocked revisions, a
 *                  missing observation, mismatched density/group or required
 *                  workarounds. Empty rules describe no reviewed revisions.
 *                  No hardware test status is established by this policy.
 */
typedef struct {
    uint32_t device_id;
    uint32_t flash_bytes;
    const nx_silicon_revision_rule_t* rules;
    size_t rule_count;
    uint32_t applied_workarounds;
    uint8_t review_sha256[32];
    bool allow_unreviewed;
} nx_silicon_admission_policy_t;

/**
 * \brief           Explicit cold source; no discovery, effects or retained
 * loan. \note            read executes once in caller context and copies facts
 * into distinct temporary output. The caller establishes legal MMIO, privilege,
 * security and cold Thread context first. A source must be bounded,
 * allocation-free, read-only, and return its actual access result. It must not
 * enable clocks, repair option bytes or retain output/context references.
 */
typedef struct {
    const void* context;
    nx_result_t (*read)(const void* context, nx_silicon_identity_t* out);
} nx_silicon_source_t;

/** \brief Copied comparison facts; admitted is software permission only. */
typedef struct {
    nx_silicon_identity_t identity;
    nx_silicon_revision_state_t revision;
    bool admitted;
} nx_silicon_admission_t;

/**
 * \brief           Read one explicit source before acquiring I/O resources.
 * \param[in]       source: Stable source and context, no concurrent teardown.
 * \param[in]       policy: Readonly policy/rules, stable during this call.
 * \param[out]      out: Distinct aligned output; never aliases source context,
 *                  complete policy/rules or MMIO. No reference is retained.
 * \return          SUCCESS for admitted software policy, UNSUPPORTED for
 *                  unknown/missing revision, PERMISSION for blocked/missing
 *                  workarounds, IO for group/density mismatch, INVALID for
 *                  malformed inputs; source errors propagate unchanged.
 * \note            Cold caller-serialized Thread context, O(rule_count) policy
 *                  validation. Malformed inputs reject before read. Source
 *                  failure leaves output untouched. A completed comparison
 *                  copies its rejection reason too; that never authorizes
 *                  startup. No reset, scheduling, clocks, IRQs or factory
 *                  operations are performed. Default platform behavior does
 *                  not invoke this optional explicit preflight.
 */
nx_result_t nx_silicon_preflight(const nx_silicon_source_t* source,
                                 const nx_silicon_admission_policy_t* policy,
                                 nx_silicon_admission_t* out);

#ifdef __cplusplus
}
#endif
#endif
