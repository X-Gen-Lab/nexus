# Real framing geometry plus exact optional DMA2D provider contracts.
nexus_google_test(nexus_frame_stream_test SOURCES "${nexus_google_root}/io/frame_stream_test.cpp"
    LIBRARIES Nexus::IO Nexus::Arch
)
