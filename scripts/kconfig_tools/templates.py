r"""
\file            templates.py
\brief           Peripheral template library
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Provides predefined peripheral templates for common
                 peripherals like UART, GPIO, SPI, I2C, ADC, DAC, CRC,
                 and Watchdog.
"""

from .naming_rules import ParameterConfig, ChoiceConfig, PeripheralTemplate


# ---------------------------------------------------------------------------
# UART Template
# ---------------------------------------------------------------------------

UART_TEMPLATE = PeripheralTemplate(
    name="UART",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="BAUDRATE",
            type="int",
            default=115200,
            help="Baud rate for UART (simulated)."
        ),
        ParameterConfig(
            name="DATA_BITS",
            type="int",
            default=8,
            range=(5, 9),
            help="Number of data bits for UART."
        ),
        ParameterConfig(
            name="STOP_BITS",
            type="int",
            default=1,
            range=(1, 2),
            help="Number of stop bits for UART."
        ),
        ParameterConfig(
            name="TX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of TX buffer for UART in bytes."
        ),
        ParameterConfig(
            name="RX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of RX buffer for UART in bytes."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="PARITY",
            options=["NONE", "ODD", "EVEN"],
            default="NONE",
            help="Parity configuration for UART.",
            values={"NONE": 0, "ODD": 1, "EVEN": 2}
        ),
        ChoiceConfig(
            name="MODE",
            options=["POLLING", "INTERRUPT", "DMA"],
            default="INTERRUPT",
            help="Transfer mode for UART.",
            values={"POLLING": 0, "INTERRUPT": 1, "DMA": 2}
        ),
    ],
    help_text="Enable UART peripheral support for the Native platform. "
              "This provides simulated UART functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# GPIO Template
# ---------------------------------------------------------------------------

GPIO_TEMPLATE = PeripheralTemplate(
    name="GPIO",
    platform="NATIVE",
    max_instances=3,  # GPIOA, GPIOB, GPIOC
    instance_type="alpha",
    parameters=[
        ParameterConfig(
            name="OUTPUT_VALUE",
            type="int",
            default=0,
            range=(0, 1),
            help="Initial output value for GPIO: 0 = Low, 1 = High"
        ),
    ],
    choices=[
        ChoiceConfig(
            name="MODE",
            options=["INPUT", "OUTPUT_PP", "OUTPUT_OD", "AF_PP", "AF_OD", "ANALOG"],
            default="OUTPUT_PP",
            help="Pin mode configuration.",
            values={
                "INPUT": 0,
                "OUTPUT_PP": 1,
                "OUTPUT_OD": 2,
                "AF_PP": 3,
                "AF_OD": 4,
                "ANALOG": 5
            }
        ),
        ChoiceConfig(
            name="PULL",
            options=["NONE", "UP", "DOWN"],
            default="NONE",
            help="Pull-up/pull-down configuration.",
            values={"NONE": 0, "UP": 1, "DOWN": 2}
        ),
        ChoiceConfig(
            name="SPEED",
            options=["LOW", "MEDIUM", "HIGH", "VERY_HIGH"],
            default="MEDIUM",
            help="GPIO speed configuration.",
            values={"LOW": 0, "MEDIUM": 1, "HIGH": 2, "VERY_HIGH": 3}
        ),
    ],
    help_text="Enable GPIO peripheral support for the Native platform. "
              "This provides simulated GPIO functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# SPI Template
# ---------------------------------------------------------------------------

SPI_TEMPLATE = PeripheralTemplate(
    name="SPI",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="MAX_SPEED",
            type="int",
            default=1000000,
            help="Maximum SPI speed for SPI in Hz (simulated)."
        ),
        ParameterConfig(
            name="TX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of TX buffer for SPI in bytes."
        ),
        ParameterConfig(
            name="RX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of RX buffer for SPI in bytes."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="CPOL",
            options=["LOW", "HIGH"],
            default="LOW",
            help="Clock polarity configuration.",
            values={"LOW": 0, "HIGH": 1}
        ),
        ChoiceConfig(
            name="CPHA",
            options=["1EDGE", "2EDGE"],
            default="1EDGE",
            help="Clock phase configuration.",
            values={"1EDGE": 0, "2EDGE": 1}
        ),
        ChoiceConfig(
            name="BIT_ORDER",
            options=["MSB_FIRST", "LSB_FIRST"],
            default="MSB_FIRST",
            help="Bit order configuration.",
            values={"MSB_FIRST": 0, "LSB_FIRST": 1}
        ),
    ],
    help_text="Enable SPI peripheral support for the Native platform. "
              "This provides simulated SPI functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# I2C Template
# ---------------------------------------------------------------------------

I2C_TEMPLATE = PeripheralTemplate(
    name="I2C",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="TX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of TX buffer for I2C in bytes."
        ),
        ParameterConfig(
            name="RX_BUFFER_SIZE",
            type="int",
            default=256,
            range=(16, 4096),
            help="Size of RX buffer for I2C in bytes."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="SPEED",
            options=["STANDARD", "FAST", "FAST_PLUS"],
            default="STANDARD",
            help="I2C speed mode configuration.",
            values={"STANDARD": 100000, "FAST": 400000, "FAST_PLUS": 1000000}
        ),
        ChoiceConfig(
            name="ADDR_MODE",
            options=["7BIT", "10BIT"],
            default="7BIT",
            help="I2C addressing mode configuration.",
            values={"7BIT": 0, "10BIT": 1}
        ),
    ],
    help_text="Enable I2C peripheral support for the Native platform. "
              "This provides simulated I2C functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# ADC Template
# ---------------------------------------------------------------------------

ADC_TEMPLATE = PeripheralTemplate(
    name="ADC",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="CHANNEL_COUNT",
            type="int",
            default=16,
            range=(1, 16),
            help="Number of ADC channels for ADC."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="RESOLUTION",
            options=["8BIT", "10BIT", "12BIT", "16BIT"],
            default="12BIT",
            help="ADC resolution configuration.",
            values={"8BIT": 8, "10BIT": 10, "12BIT": 12, "16BIT": 16}
        ),
        ChoiceConfig(
            name="SAMPLE_TIME",
            options=["FAST", "MEDIUM", "SLOW"],
            default="MEDIUM",
            help="ADC sampling time configuration.",
            values={"FAST": 0, "MEDIUM": 1, "SLOW": 2}
        ),
    ],
    help_text="Enable ADC peripheral support for the Native platform. "
              "This provides simulated ADC functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# DAC Template
# ---------------------------------------------------------------------------

DAC_TEMPLATE = PeripheralTemplate(
    name="DAC",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="CHANNEL_COUNT",
            type="int",
            default=2,
            range=(1, 4),
            help="Number of DAC channels for DAC."
        ),
        ParameterConfig(
            name="VREF_MV",
            type="int",
            default=3300,
            range=(1000, 5000),
            help="Reference voltage for DAC in millivolts."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="RESOLUTION",
            options=["8BIT", "10BIT", "12BIT", "16BIT"],
            default="12BIT",
            help="DAC resolution configuration.",
            values={"8BIT": 8, "10BIT": 10, "12BIT": 12, "16BIT": 16}
        ),
        ChoiceConfig(
            name="TRIGGER",
            options=["SOFTWARE", "TIMER", "EXTERNAL"],
            default="SOFTWARE",
            help="DAC trigger mode configuration.",
            values={"SOFTWARE": 0, "TIMER": 1, "EXTERNAL": 2}
        ),
    ],
    help_text="Enable DAC peripheral support for the Native platform. "
              "This provides simulated DAC functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# CRC Template
# ---------------------------------------------------------------------------

CRC_TEMPLATE = PeripheralTemplate(
    name="CRC",
    platform="NATIVE",
    max_instances=2,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="POLYNOMIAL",
            type="hex",
            default=0x04C11DB7,
            help="CRC polynomial value for CRC. "
                 "Default: 0x04C11DB7 for CRC-32, 0x1021 for CRC-16, 0x07 for CRC-8."
        ),
        ParameterConfig(
            name="INIT_VALUE",
            type="hex",
            default=0xFFFFFFFF,
            help="Initial CRC value for CRC. "
                 "Default: 0xFFFFFFFF for CRC-32, 0xFFFF for CRC-16, 0xFF for CRC-8."
        ),
        ParameterConfig(
            name="FINAL_XOR",
            type="hex",
            default=0xFFFFFFFF,
            help="Final XOR value applied to CRC result. "
                 "Default: 0xFFFFFFFF for CRC-32, 0x0000 for CRC-16, 0x00 for CRC-8."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="ALGORITHM",
            options=["CRC32", "CRC16", "CRC8"],
            default="CRC32",
            help="CRC algorithm selection.",
            values={"CRC32": 0, "CRC16": 1, "CRC8": 2}
        ),
        ChoiceConfig(
            name="INPUT_FORMAT",
            options=["BYTE", "HALFWORD", "WORD"],
            default="BYTE",
            help="CRC input data format.",
            values={"BYTE": 0, "HALFWORD": 1, "WORD": 2}
        ),
    ],
    help_text="Enable CRC peripheral support for the Native platform. "
              "This provides simulated CRC functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# Watchdog Template
# ---------------------------------------------------------------------------

WATCHDOG_TEMPLATE = PeripheralTemplate(
    name="WATCHDOG",
    platform="NATIVE",
    max_instances=2,
    instance_type="numeric",
    parameters=[
        ParameterConfig(
            name="DEFAULT_TIMEOUT_MS",
            type="int",
            default=5000,
            range=(100, 60000),
            help="Default timeout value for Watchdog in milliseconds. "
                 "The watchdog will trigger a reset if not fed within this time period."
        ),
        ParameterConfig(
            name="WINDOW_MIN_MS",
            type="int",
            default=1000,
            range=(0, 60000),
            help="Minimum time before watchdog can be fed in window mode."
        ),
    ],
    choices=[
        ChoiceConfig(
            name="WINDOW",
            options=["DISABLED", "ENABLED"],
            default="DISABLED",
            help="Watchdog window mode configuration.",
            values={"DISABLED": 0, "ENABLED": 1}
        ),
    ],
    help_text="Enable Watchdog Timer peripheral support for the Native platform. "
              "This provides simulated watchdog functionality for testing "
              "and development on PC."
)


# ---------------------------------------------------------------------------
# Template Registry
# ---------------------------------------------------------------------------

PERIPHERAL_TEMPLATES = {
    "UART": UART_TEMPLATE,
    "GPIO": GPIO_TEMPLATE,
    "SPI": SPI_TEMPLATE,
    "I2C": I2C_TEMPLATE,
    "ADC": ADC_TEMPLATE,
    "DAC": DAC_TEMPLATE,
    "CRC": CRC_TEMPLATE,
    "WATCHDOG": WATCHDOG_TEMPLATE,
}


def get_template(peripheral_name: str) -> PeripheralTemplate:
    r"""
    \brief           Get peripheral template by name
    \param[in]       peripheral_name: Peripheral name (e.g., "UART", "GPIO")
    \return          Peripheral template
    """
    peripheral_upper = peripheral_name.upper()
    if peripheral_upper not in PERIPHERAL_TEMPLATES:
        raise ValueError(
            f"Unknown peripheral '{peripheral_name}'. "
            f"Available templates: {list(PERIPHERAL_TEMPLATES.keys())}"
        )
    return PERIPHERAL_TEMPLATES[peripheral_upper]


def list_templates() -> list:
    r"""
    \brief           List all available peripheral templates
    \return          List of peripheral names
    """
    return list(PERIPHERAL_TEMPLATES.keys())
