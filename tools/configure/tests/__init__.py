"""Configuration regression tests discovered by the ordinary tool domain."""

# Complete modules that assert electrical or fitting facts of exact boards.
BOARD_FACT_CONTRACTS = {
    "resources/pins/test_qiming_board_facts.py": {
        "boards/stm32f407zg_qiming_core/board.json",
        "boards/stm32f407zg_qiming_v41/board.json",
    },
}
