# Optional token framing executes the same production AT parser.
# Author: Nexus Team
nexus_google_test(nexus_at_framing_test
    SOURCES "${nexus_google_root}/components/at_framing_test.cpp"
    LIBRARIES Nexus::ATClient
)
