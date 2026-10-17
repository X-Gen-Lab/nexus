/**
 * \file            silicon.c
 * \brief           Cold explicit software revision admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/silicon.h"

/** \brief Checked spans reject wrapped bounds without unrelated ordering. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Reject ambiguous policy before any external identity read. */
static bool valid_policy(const nx_silicon_admission_policy_t* policy) {
    if (policy->flash_bytes == 0U ||
        policy->rule_count > SIZE_MAX / sizeof(*policy->rules) ||
        (policy->rule_count != 0U && policy->rules == NULL)) {
        return false;
    }
    if (policy->rule_count == 0U) {
        return policy->rules == NULL;
    }
    size_t bytes = policy->rule_count * sizeof(*policy->rules);
    if (overlaps(policy, sizeof(*policy), policy->rules, bytes)) {
        return false;
    }
    uint8_t digest_bits = 0U;
    for (size_t index = 0U; index != sizeof(policy->review_sha256); ++index) {
        digest_bits |= policy->review_sha256[index];
    }
    if (digest_bits == 0U) {
        return false;
    }
    for (size_t index = 0U; index < policy->rule_count; ++index) {
        const nx_silicon_revision_rule_t* rule = &policy->rules[index];
        if ((index != 0U &&
             rule->revision_id <= policy->rules[index - 1U].revision_id) ||
            (rule->disposition != NX_SILICON_REVISION_REVIEWED &&
             rule->disposition != NX_SILICON_REVISION_BLOCKED) ||
            (rule->disposition == NX_SILICON_REVISION_BLOCKED &&
             rule->required_workarounds != 0U)) {
            return false;
        }
    }
    return true;
}

/** \brief Exact IDs only; no revision-range or newer-silicon assumptions. */
static nx_result_t decide(const nx_silicon_admission_policy_t* policy,
                          nx_silicon_admission_t* result) {
    if (result->identity.device_id != policy->device_id) {
        result->revision = NX_SILICON_REVISION_DEVICE_MISMATCH;
        return NX_ERROR_IO;
    }
    if (result->identity.flash_bytes != policy->flash_bytes) {
        result->revision = NX_SILICON_REVISION_DENSITY_MISMATCH;
        return NX_ERROR_IO;
    }
    if (!result->identity.revision_known) {
        result->revision = NX_SILICON_REVISION_NOT_OBSERVED;
        return NX_ERROR_UNSUPPORTED;
    }
    result->revision = NX_SILICON_REVISION_UNREVIEWED;
    for (size_t index = 0U; index < policy->rule_count; ++index) {
        const nx_silicon_revision_rule_t* rule = &policy->rules[index];
        if (rule->revision_id != result->identity.revision_id) {
            continue;
        }
        if (rule->disposition == NX_SILICON_REVISION_BLOCKED) {
            result->revision = NX_SILICON_REVISION_BLOCKED;
            return NX_ERROR_PERMISSION;
        }
        if ((policy->applied_workarounds & rule->required_workarounds) !=
            rule->required_workarounds) {
            result->revision = NX_SILICON_REVISION_WORKAROUND_MISSING;
            return NX_ERROR_PERMISSION;
        }
        result->revision = NX_SILICON_REVISION_REVIEWED;
        result->admitted = true;
        return NX_SUCCESS;
    }
    result->admitted = policy->allow_unreviewed;
    return result->admitted ? NX_SUCCESS : NX_ERROR_UNSUPPORTED;
}

/** \brief A rejected comparison remains diagnostic rather than startup. */
nx_result_t nx_silicon_preflight(const nx_silicon_source_t* source,
                                 const nx_silicon_admission_policy_t* policy,
                                 nx_silicon_admission_t* out) {
    if (source == NULL || policy == NULL || out == NULL ||
        source->read == NULL ||
        overlaps(source, sizeof(*source), out, sizeof(*out)) ||
        overlaps(policy, sizeof(*policy), out, sizeof(*out)) ||
        !valid_policy(policy) ||
        (policy->rule_count != 0U &&
         (overlaps(policy->rules, policy->rule_count * sizeof(*policy->rules),
                   out, sizeof(*out)) ||
          overlaps(policy->rules, policy->rule_count * sizeof(*policy->rules),
                   source, sizeof(*source))))) {
        return NX_ERROR_INVALID;
    }
    nx_silicon_admission_t result = {0};
    nx_result_t read_result = source->read(source->context, &result.identity);
    if (read_result != NX_SUCCESS) {
        return read_result;
    }
    nx_result_t admitted = decide(policy, &result);
    *out = result;
    return admitted;
}
