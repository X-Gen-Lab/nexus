/** Production validation must see the same effective chip features as callers. */
#include "system/stm32_chip_validation.h"
#include "nexus_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(stm32_chip_validate_config()==0);
    assert(strcmp(stm32_chip_get_series_name(),"STM32F4")==0);
    assert(strcmp(stm32_chip_get_variant_name(),NX_CONFIG_STM32_CHIP_NAME)==0);
    assert(stm32_chip_has_feature(STM32_FEATURE_FPU));
    assert(stm32_chip_has_feature(STM32_FEATURE_MPU));
    assert(!stm32_chip_has_feature(STM32_FEATURE_DCACHE));
    assert(!stm32_chip_has_feature(STM32_FEATURE_ICACHE));
    assert(!stm32_chip_has_feature(STM32_FEATURE_TRUSTZONE));
    puts("STM32F407 production chip features and effective configuration passed");
    return 0;
}
