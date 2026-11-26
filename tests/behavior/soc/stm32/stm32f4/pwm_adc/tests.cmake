# Explicit pwm_adc behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/pwm_adc.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/power_stage.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/adc_pair.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/single_shunt.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/power_stage_dc.cmake")
