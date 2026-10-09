#ifndef NATIVE_PROPERTY_SEED_H
#define NATIVE_PROPERTY_SEED_H

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <gtest/gtest.h>

/* Report a replayable seed in GTest XML; decimal and 0x-prefixed values are
 * accepted. A fixed default makes failures reproducible in local and CI runs. */
inline void native_property_seed(std::mt19937& rng) {
    uint32_t seed = 0x4e585553U;
    const char* configured = std::getenv("NEXUS_PROPERTY_SEED");
    if (configured != nullptr) {
        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(configured, &end, 0);
        if (configured[0] < '0' || configured[0] > '9' ||
            end == configured || *end != '\0' || errno == ERANGE ||
            parsed > std::numeric_limits<uint32_t>::max()) {
            ADD_FAILURE() << "NEXUS_PROPERTY_SEED must be a uint32_t decimal "
                             "or 0x-prefixed value; received: "
                          << configured;
        } else {
            seed = static_cast<uint32_t>(parsed);
        }
    }
    ::testing::Test::RecordProperty("property_seed", std::to_string(seed));
    rng.seed(seed);
}

#endif
