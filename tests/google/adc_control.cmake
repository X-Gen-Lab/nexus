# Optional integer layout tools do not enlarge existing ADC device state.
nexus_google_test(nexus_adc_format_test SOURCES adc_format_test.cpp
    LIBRARIES Nexus::IO)
