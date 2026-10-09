# Dependency inputs

The maintained build uses explicit dependency targets. It does not search arbitrary
SDK environment paths, infer a toolchain from Kconfig, or download during configure.

Initialize the pinned inputs from the repository root:

```sh
git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
```

Native tests use the pinned GoogleTest checkout. Native authenticated storage uses
the system OpenSSL 3 package. The F407 build uses pinned CMSIS, STM32F4 device/HAL,
and FreeRTOS sources through `platforms/stm32/CMakeLists.txt` and
`osal/CMakeLists.txt`. Missing required files fail configuration. MCU cryptography
requires an explicit product provider; it does not link the Native OpenSSL backend.

Dependency revisions are part of the release evidence. The former generic vendor
resolver was removed with the retired build modules.
