/**
 * \file            native_test_helpers.c
 * \brief           Native platform test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_test_helpers.h"
#include "hal/base/nx_device.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Device Access Helpers                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get device state information
 */
nx_status_t native_get_device_state(const char* device_name, bool* initialized,
                                    bool* suspended) {
    const nx_device_t* dev = nx_device_find(device_name);
    if (dev == NULL) {
        return NX_ERR_NOT_FOUND;
    }

    if (dev->state == NULL) {
        return NX_ERR_INVALID_STATE;
    }

    if (initialized != NULL) {
        *initialized = dev->state->initialized;
    }

    if (suspended != NULL) {
        *suspended = false; /* Native platform doesn't support suspend */
    }

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Test Utilities                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all native platform state
 */
void native_reset_all(void) {
    /* TODO: Implement device reset logic */
}

/*---------------------------------------------------------------------------*/
/* Manual Device Registration Support                                        */
/*---------------------------------------------------------------------------*/

#if NX_DEVICE_MANUAL_REGISTRATION

/* Forward declarations of device descriptors */
/* These will be defined by NX_DEVICE_REGISTER in device helper files */

/**
 * \brief           Setup test devices for MSVC
 * \details         Registers all native platform devices manually for MSVC.
 *                  Devices are registered in a deterministic order:
 *                  UART, SPI, I2C, GPIO, ADC, ADC_BUFFER, DAC, Timer,
 *                  RTC, Flash, CRC, USB, Watchdog, SDIO, Option Bytes.
 *
 *                  Registration Order:
 *                  1. UART devices (UART0-UART7)
 *                  2. SPI devices (SPI0-SPI7)
 *                  3. I2C devices (I2C0-I2C7)
 *                  4. GPIO devices (PA0-PH15, by port then pin)
 *                  5. ADC devices (ADC0-ADC3)
 *                  6. ADC_BUFFER devices (ADC_BUFFER0-ADC_BUFFER3)
 *                  7. DAC devices (DAC0-DAC1)
 *                  8. Timer devices (TIMER0-TIMER15)
 *                  9. RTC device (RTC0)
 *                  10. Flash device (INTERNAL_FLASH0)
 *                  11. CRC device (CRC0)
 *                  12. USB device (USB0)
 *                  13. Watchdog device (WATCHDOG0)
 *                  14. SDIO device (SDIO0)
 *                  15. Option Bytes device (OPTION_BYTES0)
 *
 *                  Error Handling:
 *                  - Registration continues even if individual devices fail
 *                  - This ensures maximum test coverage
 *                  - Tests for failed devices will fail at device lookup
 *
 *                  CONFIG Macro Usage:
 *                  - Only devices with NX_CONFIG_INSTANCE_NX_* defined are
 * registered
 *                  - This matches the Kconfig-based device enablement
 *                  - Ensures consistency with GCC/Clang linker section behavior
 * \note            Uses CONFIG macros to only register enabled devices
 */
void native_test_setup_devices(void) {
    /* Clear any existing devices to ensure clean state */
    nx_device_clear_all();

    /*-----------------------------------------------------------------------*/
    /* UART Devices (UART0-UART7)                                            */
    /* Uses NX_CONFIG_INSTANCE_NX_UART_* to check if device is enabled      */
    /*-----------------------------------------------------------------------*/

    /* UART devices (UART0-UART7) */
#ifdef NX_CONFIG_INSTANCE_NX_UART_0
    extern const nx_device_t NX_UART0;
    nx_device_register(&NX_UART0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_1
    extern const nx_device_t NX_UART1;
    nx_device_register(&NX_UART1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_2
    extern const nx_device_t NX_UART2;
    nx_device_register(&NX_UART2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_3
    extern const nx_device_t NX_UART3;
    nx_device_register(&NX_UART3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_4
    extern const nx_device_t NX_UART4;
    nx_device_register(&NX_UART4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_5
    extern const nx_device_t NX_UART5;
    nx_device_register(&NX_UART5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_6
    extern const nx_device_t NX_UART6;
    nx_device_register(&NX_UART6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_UART_7
    extern const nx_device_t NX_UART7;
    nx_device_register(&NX_UART7);
#endif

    /*-----------------------------------------------------------------------*/
    /* SPI Devices (SPI0-SPI7)                                               */
    /* Uses NX_CONFIG_INSTANCE_NX_SPI_* to check if device is enabled       */
    /*-----------------------------------------------------------------------*/

    /* SPI devices (SPI0-SPI7) */
#ifdef NX_CONFIG_INSTANCE_NX_SPI_0
    extern const nx_device_t NX_SPI0;
    nx_device_register(&NX_SPI0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_1
    extern const nx_device_t NX_SPI1;
    nx_device_register(&NX_SPI1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_2
    extern const nx_device_t NX_SPI2;
    nx_device_register(&NX_SPI2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_3
    extern const nx_device_t NX_SPI3;
    nx_device_register(&NX_SPI3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_4
    extern const nx_device_t NX_SPI4;
    nx_device_register(&NX_SPI4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_5
    extern const nx_device_t NX_SPI5;
    nx_device_register(&NX_SPI5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_6
    extern const nx_device_t NX_SPI6;
    nx_device_register(&NX_SPI6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_SPI_7
    extern const nx_device_t NX_SPI7;
    nx_device_register(&NX_SPI7);
#endif

    /*-----------------------------------------------------------------------*/
    /* I2C Devices (I2C0-I2C7)                                               */
    /* Uses NX_CONFIG_INSTANCE_NX_I2C_* to check if device is enabled       */
    /*-----------------------------------------------------------------------*/

    /* I2C devices (I2C0-I2C7) */
#ifdef NX_CONFIG_INSTANCE_NX_I2C_0
    extern const nx_device_t NX_I2C0;
    nx_device_register(&NX_I2C0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_1
    extern const nx_device_t NX_I2C1;
    nx_device_register(&NX_I2C1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_2
    extern const nx_device_t NX_I2C2;
    nx_device_register(&NX_I2C2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_3
    extern const nx_device_t NX_I2C3;
    nx_device_register(&NX_I2C3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_4
    extern const nx_device_t NX_I2C4;
    nx_device_register(&NX_I2C4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_5
    extern const nx_device_t NX_I2C5;
    nx_device_register(&NX_I2C5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_6
    extern const nx_device_t NX_I2C6;
    nx_device_register(&NX_I2C6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_I2C_7
    extern const nx_device_t NX_I2C7;
    nx_device_register(&NX_I2C7);
#endif

    /*-----------------------------------------------------------------------*/
    /* GPIO Devices (PA0-PH15)                                               */
    /* Registered by port (A-H) then by pin (0-15)                          */
    /* Uses NX_CONFIG_INSTANCE_NX_GPIO*_PIN* to check if device is enabled  */
    /*-----------------------------------------------------------------------*/

    /* Port A (PA0-PA15) */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN0
    extern const nx_device_t NX_GPIOA0;
    nx_device_register(&NX_GPIOA0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN1
    extern const nx_device_t NX_GPIOA1;
    nx_device_register(&NX_GPIOA1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN2
    extern const nx_device_t NX_GPIOA2;
    nx_device_register(&NX_GPIOA2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN3
    extern const nx_device_t NX_GPIOA3;
    nx_device_register(&NX_GPIOA3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN4
    extern const nx_device_t NX_GPIOA4;
    nx_device_register(&NX_GPIOA4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN5
    extern const nx_device_t NX_GPIOA5;
    nx_device_register(&NX_GPIOA5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN6
    extern const nx_device_t NX_GPIOA6;
    nx_device_register(&NX_GPIOA6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN7
    extern const nx_device_t NX_GPIOA7;
    nx_device_register(&NX_GPIOA7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN8
    extern const nx_device_t NX_GPIOA8;
    nx_device_register(&NX_GPIOA8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN9
    extern const nx_device_t NX_GPIOA9;
    nx_device_register(&NX_GPIOA9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN10
    extern const nx_device_t NX_GPIOA10;
    nx_device_register(&NX_GPIOA10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN11
    extern const nx_device_t NX_GPIOA11;
    nx_device_register(&NX_GPIOA11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN12
    extern const nx_device_t NX_GPIOA12;
    nx_device_register(&NX_GPIOA12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN13
    extern const nx_device_t NX_GPIOA13;
    nx_device_register(&NX_GPIOA13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN14
    extern const nx_device_t NX_GPIOA14;
    nx_device_register(&NX_GPIOA14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN15
    extern const nx_device_t NX_GPIOA15;
    nx_device_register(&NX_GPIOA15);
#endif

    /* Port B */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN0
    extern const nx_device_t NX_GPIOB0;
    nx_device_register(&NX_GPIOB0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN1
    extern const nx_device_t NX_GPIOB1;
    nx_device_register(&NX_GPIOB1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN2
    extern const nx_device_t NX_GPIOB2;
    nx_device_register(&NX_GPIOB2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN3
    extern const nx_device_t NX_GPIOB3;
    nx_device_register(&NX_GPIOB3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN4
    extern const nx_device_t NX_GPIOB4;
    nx_device_register(&NX_GPIOB4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN5
    extern const nx_device_t NX_GPIOB5;
    nx_device_register(&NX_GPIOB5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN6
    extern const nx_device_t NX_GPIOB6;
    nx_device_register(&NX_GPIOB6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN7
    extern const nx_device_t NX_GPIOB7;
    nx_device_register(&NX_GPIOB7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN8
    extern const nx_device_t NX_GPIOB8;
    nx_device_register(&NX_GPIOB8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN9
    extern const nx_device_t NX_GPIOB9;
    nx_device_register(&NX_GPIOB9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN10
    extern const nx_device_t NX_GPIOB10;
    nx_device_register(&NX_GPIOB10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN11
    extern const nx_device_t NX_GPIOB11;
    nx_device_register(&NX_GPIOB11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN12
    extern const nx_device_t NX_GPIOB12;
    nx_device_register(&NX_GPIOB12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN13
    extern const nx_device_t NX_GPIOB13;
    nx_device_register(&NX_GPIOB13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN14
    extern const nx_device_t NX_GPIOB14;
    nx_device_register(&NX_GPIOB14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOB_PIN15
    extern const nx_device_t NX_GPIOB15;
    nx_device_register(&NX_GPIOB15);
#endif

/* Port C */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN0
    extern const nx_device_t NX_GPIOC0;
    nx_device_register(&NX_GPIOC0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN1
    extern const nx_device_t NX_GPIOC1;
    nx_device_register(&NX_GPIOC1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN2
    extern const nx_device_t NX_GPIOC2;
    nx_device_register(&NX_GPIOC2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN3
    extern const nx_device_t NX_GPIOC3;
    nx_device_register(&NX_GPIOC3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN4
    extern const nx_device_t NX_GPIOC4;
    nx_device_register(&NX_GPIOC4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN5
    extern const nx_device_t NX_GPIOC5;
    nx_device_register(&NX_GPIOC5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN6
    extern const nx_device_t NX_GPIOC6;
    nx_device_register(&NX_GPIOC6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN7
    extern const nx_device_t NX_GPIOC7;
    nx_device_register(&NX_GPIOC7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN8
    extern const nx_device_t NX_GPIOC8;
    nx_device_register(&NX_GPIOC8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN9
    extern const nx_device_t NX_GPIOC9;
    nx_device_register(&NX_GPIOC9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN10
    extern const nx_device_t NX_GPIOC10;
    nx_device_register(&NX_GPIOC10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN11
    extern const nx_device_t NX_GPIOC11;
    nx_device_register(&NX_GPIOC11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN12
    extern const nx_device_t NX_GPIOC12;
    nx_device_register(&NX_GPIOC12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN13
    extern const nx_device_t NX_GPIOC13;
    nx_device_register(&NX_GPIOC13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN14
    extern const nx_device_t NX_GPIOC14;
    nx_device_register(&NX_GPIOC14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOC_PIN15
    extern const nx_device_t NX_GPIOC15;
    nx_device_register(&NX_GPIOC15);
#endif

/* Port D */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN0
    extern const nx_device_t NX_GPIOD0;
    nx_device_register(&NX_GPIOD0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN1
    extern const nx_device_t NX_GPIOD1;
    nx_device_register(&NX_GPIOD1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN2
    extern const nx_device_t NX_GPIOD2;
    nx_device_register(&NX_GPIOD2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN3
    extern const nx_device_t NX_GPIOD3;
    nx_device_register(&NX_GPIOD3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN4
    extern const nx_device_t NX_GPIOD4;
    nx_device_register(&NX_GPIOD4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN5
    extern const nx_device_t NX_GPIOD5;
    nx_device_register(&NX_GPIOD5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN6
    extern const nx_device_t NX_GPIOD6;
    nx_device_register(&NX_GPIOD6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN7
    extern const nx_device_t NX_GPIOD7;
    nx_device_register(&NX_GPIOD7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN8
    extern const nx_device_t NX_GPIOD8;
    nx_device_register(&NX_GPIOD8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN9
    extern const nx_device_t NX_GPIOD9;
    nx_device_register(&NX_GPIOD9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN10
    extern const nx_device_t NX_GPIOD10;
    nx_device_register(&NX_GPIOD10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN11
    extern const nx_device_t NX_GPIOD11;
    nx_device_register(&NX_GPIOD11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN12
    extern const nx_device_t NX_GPIOD12;
    nx_device_register(&NX_GPIOD12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN13
    extern const nx_device_t NX_GPIOD13;
    nx_device_register(&NX_GPIOD13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN14
    extern const nx_device_t NX_GPIOD14;
    nx_device_register(&NX_GPIOD14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOD_PIN15
    extern const nx_device_t NX_GPIOD15;
    nx_device_register(&NX_GPIOD15);
#endif

/* Port E */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN0
    extern const nx_device_t NX_GPIOE0;
    nx_device_register(&NX_GPIOE0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN1
    extern const nx_device_t NX_GPIOE1;
    nx_device_register(&NX_GPIOE1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN2
    extern const nx_device_t NX_GPIOE2;
    nx_device_register(&NX_GPIOE2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN3
    extern const nx_device_t NX_GPIOE3;
    nx_device_register(&NX_GPIOE3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN4
    extern const nx_device_t NX_GPIOE4;
    nx_device_register(&NX_GPIOE4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN5
    extern const nx_device_t NX_GPIOE5;
    nx_device_register(&NX_GPIOE5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN6
    extern const nx_device_t NX_GPIOE6;
    nx_device_register(&NX_GPIOE6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN7
    extern const nx_device_t NX_GPIOE7;
    nx_device_register(&NX_GPIOE7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN8
    extern const nx_device_t NX_GPIOE8;
    nx_device_register(&NX_GPIOE8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN9
    extern const nx_device_t NX_GPIOE9;
    nx_device_register(&NX_GPIOE9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN10
    extern const nx_device_t NX_GPIOE10;
    nx_device_register(&NX_GPIOE10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN11
    extern const nx_device_t NX_GPIOE11;
    nx_device_register(&NX_GPIOE11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN12
    extern const nx_device_t NX_GPIOE12;
    nx_device_register(&NX_GPIOE12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN13
    extern const nx_device_t NX_GPIOE13;
    nx_device_register(&NX_GPIOE13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN14
    extern const nx_device_t NX_GPIOE14;
    nx_device_register(&NX_GPIOE14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOE_PIN15
    extern const nx_device_t NX_GPIOE15;
    nx_device_register(&NX_GPIOE15);
#endif

/* Port F */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN0
    extern const nx_device_t NX_GPIOF0;
    nx_device_register(&NX_GPIOF0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN1
    extern const nx_device_t NX_GPIOF1;
    nx_device_register(&NX_GPIOF1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN2
    extern const nx_device_t NX_GPIOF2;
    nx_device_register(&NX_GPIOF2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN3
    extern const nx_device_t NX_GPIOF3;
    nx_device_register(&NX_GPIOF3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN4
    extern const nx_device_t NX_GPIOF4;
    nx_device_register(&NX_GPIOF4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN5
    extern const nx_device_t NX_GPIOF5;
    nx_device_register(&NX_GPIOF5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN6
    extern const nx_device_t NX_GPIOF6;
    nx_device_register(&NX_GPIOF6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN7
    extern const nx_device_t NX_GPIOF7;
    nx_device_register(&NX_GPIOF7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN8
    extern const nx_device_t NX_GPIOF8;
    nx_device_register(&NX_GPIOF8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN9
    extern const nx_device_t NX_GPIOF9;
    nx_device_register(&NX_GPIOF9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN10
    extern const nx_device_t NX_GPIOF10;
    nx_device_register(&NX_GPIOF10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN11
    extern const nx_device_t NX_GPIOF11;
    nx_device_register(&NX_GPIOF11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN12
    extern const nx_device_t NX_GPIOF12;
    nx_device_register(&NX_GPIOF12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN13
    extern const nx_device_t NX_GPIOF13;
    nx_device_register(&NX_GPIOF13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN14
    extern const nx_device_t NX_GPIOF14;
    nx_device_register(&NX_GPIOF14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOF_PIN15
    extern const nx_device_t NX_GPIOF15;
    nx_device_register(&NX_GPIOF15);
#endif

/* Port G */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN0
    extern const nx_device_t NX_GPIOG0;
    nx_device_register(&NX_GPIOG0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN1
    extern const nx_device_t NX_GPIOG1;
    nx_device_register(&NX_GPIOG1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN2
    extern const nx_device_t NX_GPIOG2;
    nx_device_register(&NX_GPIOG2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN3
    extern const nx_device_t NX_GPIOG3;
    nx_device_register(&NX_GPIOG3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN4
    extern const nx_device_t NX_GPIOG4;
    nx_device_register(&NX_GPIOG4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN5
    extern const nx_device_t NX_GPIOG5;
    nx_device_register(&NX_GPIOG5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN6
    extern const nx_device_t NX_GPIOG6;
    nx_device_register(&NX_GPIOG6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN7
    extern const nx_device_t NX_GPIOG7;
    nx_device_register(&NX_GPIOG7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN8
    extern const nx_device_t NX_GPIOG8;
    nx_device_register(&NX_GPIOG8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN9
    extern const nx_device_t NX_GPIOG9;
    nx_device_register(&NX_GPIOG9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN10
    extern const nx_device_t NX_GPIOG10;
    nx_device_register(&NX_GPIOG10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN11
    extern const nx_device_t NX_GPIOG11;
    nx_device_register(&NX_GPIOG11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN12
    extern const nx_device_t NX_GPIOG12;
    nx_device_register(&NX_GPIOG12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN13
    extern const nx_device_t NX_GPIOG13;
    nx_device_register(&NX_GPIOG13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN14
    extern const nx_device_t NX_GPIOG14;
    nx_device_register(&NX_GPIOG14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOG_PIN15
    extern const nx_device_t NX_GPIOG15;
    nx_device_register(&NX_GPIOG15);
#endif

/* Port H */
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN0
    extern const nx_device_t NX_GPIOH0;
    nx_device_register(&NX_GPIOH0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN1
    extern const nx_device_t NX_GPIOH1;
    nx_device_register(&NX_GPIOH1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN2
    extern const nx_device_t NX_GPIOH2;
    nx_device_register(&NX_GPIOH2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN3
    extern const nx_device_t NX_GPIOH3;
    nx_device_register(&NX_GPIOH3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN4
    extern const nx_device_t NX_GPIOH4;
    nx_device_register(&NX_GPIOH4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN5
    extern const nx_device_t NX_GPIOH5;
    nx_device_register(&NX_GPIOH5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN6
    extern const nx_device_t NX_GPIOH6;
    nx_device_register(&NX_GPIOH6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN7
    extern const nx_device_t NX_GPIOH7;
    nx_device_register(&NX_GPIOH7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN8
    extern const nx_device_t NX_GPIOH8;
    nx_device_register(&NX_GPIOH8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN9
    extern const nx_device_t NX_GPIOH9;
    nx_device_register(&NX_GPIOH9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN10
    extern const nx_device_t NX_GPIOH10;
    nx_device_register(&NX_GPIOH10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN11
    extern const nx_device_t NX_GPIOH11;
    nx_device_register(&NX_GPIOH11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN12
    extern const nx_device_t NX_GPIOH12;
    nx_device_register(&NX_GPIOH12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN13
    extern const nx_device_t NX_GPIOH13;
    nx_device_register(&NX_GPIOH13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN14
    extern const nx_device_t NX_GPIOH14;
    nx_device_register(&NX_GPIOH14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_GPIOH_PIN15
    extern const nx_device_t NX_GPIOH15;
    nx_device_register(&NX_GPIOH15);
#endif

    /*-----------------------------------------------------------------------*/
    /* ADC Devices (ADC0-ADC3)                                               */
    /* Uses NX_CONFIG_INSTANCE_NX_ADC_* to check if device is enabled       */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_ADC_0
    extern const nx_device_t NX_ADC0;
    nx_device_register(&NX_ADC0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_1
    extern const nx_device_t NX_ADC1;
    nx_device_register(&NX_ADC1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_2
    extern const nx_device_t NX_ADC2;
    nx_device_register(&NX_ADC2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_3
    extern const nx_device_t NX_ADC3;
    nx_device_register(&NX_ADC3);
#endif

    /*-----------------------------------------------------------------------*/
    /* ADC_BUFFER Devices (ADC_BUFFER0-ADC_BUFFER3)                          */
    /* Uses NX_CONFIG_INSTANCE_NX_ADC_BUFFER_* to check if enabled          */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_0
    extern const nx_device_t NX_ADC_BUFFER0;
    nx_device_register(&NX_ADC_BUFFER0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_1
    extern const nx_device_t NX_ADC_BUFFER1;
    nx_device_register(&NX_ADC_BUFFER1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_2
    extern const nx_device_t NX_ADC_BUFFER2;
    nx_device_register(&NX_ADC_BUFFER2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_3
    extern const nx_device_t NX_ADC_BUFFER3;
    nx_device_register(&NX_ADC_BUFFER3);
#endif

    /*-----------------------------------------------------------------------*/
    /* DAC Devices (DAC0-DAC1)                                               */
    /* Uses NX_CONFIG_INSTANCE_NX_DAC_* to check if device is enabled       */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_DAC_0
    extern const nx_device_t NX_DAC0;
    nx_device_register(&NX_DAC0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_DAC_1
    extern const nx_device_t NX_DAC1;
    nx_device_register(&NX_DAC1);
#endif

    /*-----------------------------------------------------------------------*/
    /* Timer Devices (TIMER0-TIMER15)                                        */
    /* Uses NX_CONFIG_INSTANCE_NX_TIMER_* to check if device is enabled     */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_0
    extern const nx_device_t NX_TIMER0;
    nx_device_register(&NX_TIMER0);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_1
    extern const nx_device_t NX_TIMER1;
    nx_device_register(&NX_TIMER1);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_2
    extern const nx_device_t NX_TIMER2;
    nx_device_register(&NX_TIMER2);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_3
    extern const nx_device_t NX_TIMER3;
    nx_device_register(&NX_TIMER3);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_4
    extern const nx_device_t NX_TIMER4;
    nx_device_register(&NX_TIMER4);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_5
    extern const nx_device_t NX_TIMER5;
    nx_device_register(&NX_TIMER5);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_6
    extern const nx_device_t NX_TIMER6;
    nx_device_register(&NX_TIMER6);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_7
    extern const nx_device_t NX_TIMER7;
    nx_device_register(&NX_TIMER7);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_8
    extern const nx_device_t NX_TIMER8;
    nx_device_register(&NX_TIMER8);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_9
    extern const nx_device_t NX_TIMER9;
    nx_device_register(&NX_TIMER9);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_10
    extern const nx_device_t NX_TIMER10;
    nx_device_register(&NX_TIMER10);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_11
    extern const nx_device_t NX_TIMER11;
    nx_device_register(&NX_TIMER11);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_12
    extern const nx_device_t NX_TIMER12;
    nx_device_register(&NX_TIMER12);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_13
    extern const nx_device_t NX_TIMER13;
    nx_device_register(&NX_TIMER13);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_14
    extern const nx_device_t NX_TIMER14;
    nx_device_register(&NX_TIMER14);
#endif
#ifdef NX_CONFIG_INSTANCE_NX_TIMER_15
    extern const nx_device_t NX_TIMER15;
    nx_device_register(&NX_TIMER15);
#endif

    /*-----------------------------------------------------------------------*/
    /* RTC Device (RTC0)                                                     */
    /* Uses NX_CONFIG_INSTANCE_NX_RTC* to check if device is enabled        */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_RTC_0
    extern const nx_device_t NX_RTC0;
    nx_device_register(&NX_RTC0);
#endif

    /*-----------------------------------------------------------------------*/
    /* Flash Device (INTERNAL_FLASH0)                                        */
    /* Uses NX_CONFIG_INSTANCE_NX_INTERNAL_FLASH* to check if enabled       */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_INTERNAL_FLASH0
    extern const nx_device_t NX_INTERNAL_FLASH0;
    nx_device_register(&NX_INTERNAL_FLASH0);
#endif

    /*-----------------------------------------------------------------------*/
    /* CRC Device (CRC0)                                                     */
    /* Uses NX_CONFIG_INSTANCE_NX_CRC* to check if device is enabled        */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_CRC_0
    extern const nx_device_t NX_CRC0;
    nx_device_register(&NX_CRC0);
#endif

    /*-----------------------------------------------------------------------*/
    /* USB Device (USB0)                                                     */
    /* Uses NX_CONFIG_INSTANCE_NX_USB_* to check if device is enabled       */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_USB_0
    extern const nx_device_t NX_USB0;
    nx_device_register(&NX_USB0);
#endif

    /*-----------------------------------------------------------------------*/
    /* Watchdog Device (WATCHDOG0)                                           */
    /* Uses NX_CONFIG_INSTANCE_NX_WATCHDOG_* to check if device is enabled  */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_WATCHDOG_0
    extern const nx_device_t NX_WATCHDOG0;
    nx_device_register(&NX_WATCHDOG0);
#endif

    /*-----------------------------------------------------------------------*/
    /* SDIO Device (SDIO0)                                                   */
    /* Uses NX_CONFIG_INSTANCE_NX_SDIO_* to check if device is enabled      */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_SDIO_0
    extern const nx_device_t NX_SDIO0;
    nx_device_register(&NX_SDIO0);
#endif

    /*-----------------------------------------------------------------------*/
    /* Option Bytes Device (OPTION_BYTES0)                                   */
    /* Uses NX_CONFIG_INSTANCE_NX_OPTION_BYTES* to check if enabled         */
    /*-----------------------------------------------------------------------*/
#ifdef NX_CONFIG_INSTANCE_NX_OPTION_BYTES_0
    extern const nx_device_t NX_OPTION_BYTES0;
    nx_device_register(&NX_OPTION_BYTES0);
#endif
}

/**
 * \brief           Cleanup test devices for MSVC
 * \details         Clears all manually registered devices from the registry.
 *                  This ensures a clean state for subsequent test runs.
 *
 *                  Error Handling:
 *                  - This function cannot fail (void return)
 *                  - Clears the entire device registry unconditionally
 *                  - Safe to call multiple times
 * \note            Called automatically by test main after all tests complete
 */
void native_test_cleanup_devices(void) {
    nx_device_clear_all();
}

#endif
