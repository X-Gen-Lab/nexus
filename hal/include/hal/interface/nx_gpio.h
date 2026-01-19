/**
 * \file            nx_gpio.h
 * \brief           GPIO device interface definition
 * \author          Nexus Team
 */

#ifndef NX_GPIO_H
#define NX_GPIO_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO mode enumeration
 */
typedef enum nx_gpio_mode_e {
    NX_GPIO_MODE_INPUT = 0, /**< Input mode */
    NX_GPIO_MODE_OUTPUT_PP, /**< Output push-pull */
    NX_GPIO_MODE_OUTPUT_OD, /**< Output open-drain */
    NX_GPIO_MODE_AF_PP,     /**< Alternate function push-pull */
    NX_GPIO_MODE_AF_OD,     /**< Alternate function open-drain */
    NX_GPIO_MODE_ANALOG,    /**< Analog mode */
} nx_gpio_mode_t;

/**
 * \brief           GPIO pull-up/pull-down enumeration
 */
typedef enum nx_gpio_pull_e {
    NX_GPIO_PULL_NONE = 0, /**< No pull-up/pull-down */
    NX_GPIO_PULL_UP,       /**< Pull-up */
    NX_GPIO_PULL_DOWN,     /**< Pull-down */
} nx_gpio_pull_t;

/**
 * \brief           GPIO speed enumeration
 */
typedef enum nx_gpio_speed_e {
    NX_GPIO_SPEED_LOW = 0,   /**< Low speed */
    NX_GPIO_SPEED_MEDIUM,    /**< Medium speed */
    NX_GPIO_SPEED_HIGH,      /**< High speed */
    NX_GPIO_SPEED_VERY_HIGH, /**< Very high speed */
} nx_gpio_speed_t;

/**
 * \brief           GPIO interrupt trigger type enumeration
 */
typedef enum nx_gpio_trigger_e {
    NX_GPIO_TRIGGER_RISING = 0, /**< Rising edge trigger */
    NX_GPIO_TRIGGER_FALLING,    /**< Falling edge trigger */
    NX_GPIO_TRIGGER_BOTH,       /**< Both edges trigger */
} nx_gpio_trigger_t;

/**
 * \brief           GPIO interrupt callback function type
 * \param[in]       user_data: User context pointer
 */
typedef void (*nx_gpio_callback_t)(void* user_data);

/*---------------------------------------------------------------------------*/
/* GPIO Read Interface                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read interface (input only)
 */
typedef struct nx_gpio_read_s nx_gpio_read_t;
struct nx_gpio_read_s {
    /**
     * \brief           Read GPIO pin state
     * \param[in]       self: Interface pointer
     * \return          Pin state (0 or 1)
     */
    uint8_t (*read)(nx_gpio_read_t* self);

    /**
     * \brief           Register external interrupt callback
     * \param[in]       self: Interface pointer
     * \param[in]       callback: Interrupt callback function
     * \param[in]       user_data: User context pointer
     * \param[in]       trigger: Interrupt trigger type
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*register_exti)(nx_gpio_read_t* self,
                                 nx_gpio_callback_t callback, void* user_data,
                                 nx_gpio_trigger_t trigger);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_gpio_read_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: Interface pointer
     * \return          Power management interface pointer
     */
    nx_power_t* (*get_power)(nx_gpio_read_t* self);
};

/*---------------------------------------------------------------------------*/
/* GPIO Write Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO write interface (output only)
 */
typedef struct nx_gpio_write_s nx_gpio_write_t;
struct nx_gpio_write_s {
    /**
     * \brief           Write GPIO pin state
     * \param[in]       self: Interface pointer
     * \param[in]       state: Pin state to write (0 or 1)
     */
    void (*write)(nx_gpio_write_t* self, uint8_t state);

    /**
     * \brief           Toggle GPIO pin state
     * \param[in]       self: Interface pointer
     */
    void (*toggle)(nx_gpio_write_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_gpio_write_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: Interface pointer
     * \return          Power management interface pointer
     */
    nx_power_t* (*get_power)(nx_gpio_write_t* self);
};

/*---------------------------------------------------------------------------*/
/* GPIO Read-Write Interface                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO read-write interface (bidirectional)
 */
typedef struct nx_gpio_read_write_s nx_gpio_read_write_t;
struct nx_gpio_read_write_s {
    nx_gpio_read_t read;   /**< Read interface */
    nx_gpio_write_t write; /**< Write interface */
};

/*---------------------------------------------------------------------------*/
/* Initialization Macros                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO read interface
 * \param[in]       p: Pointer to nx_gpio_read_t structure
 * \param[in]       _read: Read function pointer
 * \param[in]       _register_exti: Register EXTI function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_GPIO_READ(p, _read, _register_exti, _get_lifecycle,            \
                          _get_power)                                          \
    do {                                                                       \
        (p)->read = (_read);                                                   \
        (p)->register_exti = (_register_exti);                                 \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->read);                                                  \
        NX_ASSERT((p)->get_lifecycle);                                         \
    } while (0)

/**
 * \brief           Initialize GPIO write interface
 * \param[in]       p: Pointer to nx_gpio_write_t structure
 * \param[in]       _write: Write function pointer
 * \param[in]       _toggle: Toggle function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_GPIO_WRITE(p, _write, _toggle, _get_lifecycle, _get_power)     \
    do {                                                                       \
        (p)->write = (_write);                                                 \
        (p)->toggle = (_toggle);                                               \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->write && (p)->toggle);                                  \
        NX_ASSERT((p)->get_lifecycle);                                         \
    } while (0)

/**
 * \brief           Initialize GPIO read-write interface
 * \param[in]       p: Pointer to nx_gpio_read_write_t structure
 * \param[in]       _read: Read function pointer
 * \param[in]       _register_exti: Register EXTI function pointer
 * \param[in]       _write: Write function pointer
 * \param[in]       _toggle: Toggle function pointer
 * \param[in]       _get_lifecycle_r: Get lifecycle function pointer for read
 * \param[in]       _get_power_r: Get power function pointer for read
 * \param[in]       _get_lifecycle_w: Get lifecycle function pointer for write
 * \param[in]       _get_power_w: Get power function pointer for write
 */
#define NX_INIT_GPIO_READ_WRITE(p, _read, _register_exti, _write, _toggle,     \
                                _get_lifecycle_r, _get_power_r,                \
                                _get_lifecycle_w, _get_power_w)                \
    do {                                                                       \
        NX_INIT_GPIO_READ(&(p)->read, _read, _register_exti, _get_lifecycle_r, \
                          _get_power_r);                                       \
        NX_INIT_GPIO_WRITE(&(p)->write, _write, _toggle, _get_lifecycle_w,     \
                           _get_power_w);                                      \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Type Aliases for Backward Compatibility                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO interface type alias (for backward compatibility)
 * \note            New code should use nx_gpio_read_t, nx_gpio_write_t, or
 *                  nx_gpio_read_write_t explicitly
 */
typedef nx_gpio_read_write_t nx_gpio_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_H */
