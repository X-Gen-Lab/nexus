"""Maintained authoring decisions; CMake owns component source dependencies."""
COMPONENTS = frozenset({
    "owner-observer", "bus-owner", "spi-owner", "spi-async-owner",
    "uart-owner", "bmp280", "bmp280-spi", "log", "storage",
    "flash-storage", "modbus-rtu",
})
