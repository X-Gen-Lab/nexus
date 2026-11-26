# Explicit arch behavior targets and private model requirements.
# Author: Nexus Team
include("${nexus_google_root}/arch/arm/cortex_m/cycle_probe.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_cortex.cmake")

include("${nexus_google_root}/arch/native/arch_native.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_cache.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_cache_control.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_mpu.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_security.cmake")

include("${nexus_google_root}/arch/arm/cortex_m/arch_nonsecure.cmake")

include("${nexus_google_root}/arch/native/arch_native_mechanism.cmake")
