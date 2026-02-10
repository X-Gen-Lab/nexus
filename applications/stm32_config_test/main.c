/**
 * \file            main.c
 * \brief           STM32 configuration test application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-08
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This application tests that Kconfig settings are properly
 *                  applied to the STM32 platform. It demonstrates LED blinking
 *                  using SysTick and displays configuration values via UART.
 */

#include "config_display.h"
#include "nexus_config.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>


/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/* LED GPIO configuration for STM32F407 Discovery */
#define LED_PORT           GPIOD
#define LED_PIN            GPIO_PIN_12 /* Green LED */
#define LED_CLOCK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()

/* LED blink period in milliseconds */
#define LED_BLINK_PERIOD_MS 500

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

static volatile uint32_t g_tick_count = 0;
static volatile uint8_t g_led_state = 0;

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize LED GPIO
 * \details         Configures LED pin as output for blinking
 */
static void led_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clock */
    LED_CLOCK_ENABLE();

    /* Configure GPIO pin */
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    /* Initial state: LED off */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

/**
 * \brief           Toggle LED state
 * \details         Switches LED between on and off states
 */
static void led_toggle(void) {
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    g_led_state = !g_led_state;
}

/**
 * \brief           System Clock Configuration
 * \details         Configures system clock to use HSE and PLL
 * \note            This is a minimal configuration for testing
 */
static void system_clock_config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Initialize the RCC Oscillators */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        /* Clock configuration error */
        while (1) {
        }
    }

    /* Initialize the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        /* Clock configuration error */
        while (1) {
        }
    }
}

/**
 * \brief           Configure NVIC priority grouping
 * \details         Uses Kconfig setting or default value
 */
static void nvic_config(void) {
#ifdef NX_CONFIG_STM32_NVIC_PRIORITY_GROUP
    HAL_NVIC_SetPriorityGrouping(NX_CONFIG_STM32_NVIC_PRIORITY_GROUP);
#else
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
#endif

#ifdef NX_CONFIG_STM32_SYSTICK_PRIORITY
    HAL_NVIC_SetPriority(SysTick_IRQn, NX_CONFIG_STM32_SYSTICK_PRIORITY, 0);
#else
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
#endif

#ifdef NX_CONFIG_STM32_PENDSV_PRIORITY
    HAL_NVIC_SetPriority(PendSV_IRQn, NX_CONFIG_STM32_PENDSV_PRIORITY, 0);
#else
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
#endif
}

/*---------------------------------------------------------------------------*/
/* Interrupt Handlers                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SysTick interrupt callback
 * \details         Called every 1ms by SysTick timer
 */
void HAL_SYSTICK_Callback(void) {
    g_tick_count++;

    /* Toggle LED every LED_BLINK_PERIOD_MS milliseconds */
    if (g_tick_count % LED_BLINK_PERIOD_MS == 0) {
        led_toggle();
    }
}

/*---------------------------------------------------------------------------*/
/* Main Function                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main application entry point
 * \return          Should never return
 */
int main(void) {
    /* Initialize HAL library */
    HAL_Init();

    /* Configure system clock */
    system_clock_config();

    /* Configure NVIC priorities */
    nvic_config();

    /* Initialize LED */
    led_init();

    /* Display configuration information */
    printf("\n\n");
    config_display_print_all();

    printf("[INFO] Application started\n");
    printf("[INFO] LED will blink every %d ms\n", LED_BLINK_PERIOD_MS);
    printf("[INFO] Press reset to restart\n\n");

    /* Main loop */
    while (1) {
        /* LED is controlled by SysTick interrupt */

        /* Print summary every 5 seconds */
        if (g_tick_count % 5000 == 0 && g_tick_count > 0) {
            config_display_print_summary();
        }

        /* Small delay to reduce CPU usage */
        HAL_Delay(100);
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Error Handler                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error handler
 * \details         Called when a fatal error occurs
 */
void Error_Handler(void) {
    /* Disable interrupts */
    __disable_irq();

    /* Fast blink to indicate error */
    while (1) {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        for (volatile uint32_t i = 0; i < 1000000; i++) {
        }
    }
}

#ifdef USE_FULL_ASSERT
/**
 * \brief           Assert failed handler
 * \param[in]       file: Source file name
 * \param[in]       line: Line number where assert failed
 */
void assert_failed(uint8_t* file, uint32_t line) {
    printf("Assert failed: file %s, line %lu\n", file, (unsigned long)line);
    Error_Handler();
}
#endif
