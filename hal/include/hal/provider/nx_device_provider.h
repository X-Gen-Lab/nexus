/** Implementor-only registry and binding contract. Ordinary applications use
 * hal/base/nx_device.h; no mutable provider state is exposed by that header.
 * Constructors bind static instances; they never start hardware. Lifetime,
 * settlement and callback contracts apply even to faulty provider recovery. */
#ifndef NX_DEVICE_PROVIDER_H
#define NX_DEVICE_PROVIDER_H
#include "hal/base/nx_device.h"
#ifdef __cplusplus
extern "C" {
#endif
/*---------------------------------------------------------------------------*/
/* Helper Macros                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Concatenate two tokens
 */
#define _NX_CONCAT(a, ...) a##__VA_ARGS__
#define NX_CONCAT(a, ...)  _NX_CONCAT(a, __VA_ARGS__)

/*---------------------------------------------------------------------------*/
/* Platform Detection                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Determine if manual device registration is needed
 * \details         Manual registration is used when:
 *                  - Native platform (for testing on host)
 *                  - MSVC compiler (no linker section support)
 *                  - Unknown compilers without linker section support
 */
#if defined(NEXUS_PLATFORM_NATIVE) || defined(_MSC_VER) ||                     \
    (!defined(__GNUC__) && !defined(__ARMCC_VERSION) &&                        \
     !defined(__ICCARM__) && !defined(__TI_ARM__) && !defined(__TASKING__) &&  \
     !defined(__CC_ARM))
#define NX_DEVICE_MANUAL_REGISTRATION 1
#else
#define NX_DEVICE_MANUAL_REGISTRATION 0
#endif

typedef enum nx_device_phase_e {
    NX_DEVICE_CLOSED = 0, NX_DEVICE_OPENING, NX_DEVICE_OPEN, NX_DEVICE_CLOSING,
    NX_DEVICE_RECOVERY_REQUIRED
} nx_device_phase_t;

typedef struct nx_device_config_state_s {
    uint8_t init_res; /**< Initialization result */
    bool initialized; /**< Initialization flag */
    void* api;        /**< Cached API pointer */
    bool initializing; /**< Initialization in progress; protected by port critical region */
    nx_device_phase_t phase;
    nx_status_t last_status;
    uintptr_t owner;
    uint64_t generation; /**< Never reset by close or registry reset. */
    uint32_t active_calls;
    uint64_t active_ticket; /**< UART buffer lease, zero when no request. */
    uint64_t last_ticket; /**< Most recent UART ticket issued to this owner. */
    bool uart_unknown_lease; /**< Provider admitted storage without a ticket. */
    uint32_t child_refs; /**< Live SPI children, including failed-open cleanup. */
} nx_device_config_state_t;
struct nx_device_s {
    const char* name;                                    /**< Device name */
    const void* config;                                  /**< Device config */
    struct nx_device_config_state_s* state;              /**< Device state */
    void* (*device_init)(const struct nx_device_s* dev); /**< Init fn */
    nx_device_class_t device_class;
    uint32_t capabilities;
    /** New constructors preserve failure reasons and clean up partial binding
     * on failure (hardware has not started). Legacy device_init is only
     * a migration construction hook, with NULL mapped to NX_ERR_GENERIC. */
    nx_status_t (*construct)(const struct nx_device_s* dev, void** api);
    nx_lifecycle_t* (*get_lifecycle)(void* api);
};
/* Registration type mapping: names are never used as a type proof. */
#define NX_DEVICE_CLASS_NX_GPIO NX_DEVICE_CLASS_GPIO
#define NX_DEVICE_CLASS_STM32_GPIO NX_DEVICE_CLASS_GPIO
#define NX_DEVICE_CLASS_GD32_GPIO NX_DEVICE_CLASS_GPIO
#define NX_DEVICE_CLASS_NX_GPIO_READ NX_DEVICE_CLASS_GPIO_READ
#define NX_DEVICE_CLASS_NX_GPIO_WRITE NX_DEVICE_CLASS_GPIO_WRITE
#define NX_DEVICE_CLASS_NX_UART NX_DEVICE_CLASS_UART
#define NX_DEVICE_CLASS_STM32_UART NX_DEVICE_CLASS_UART
#define NX_DEVICE_CLASS_GD32_UART NX_DEVICE_CLASS_UART
#define NX_DEVICE_CLASS_NX_SPI NX_DEVICE_CLASS_SPI
#define NX_DEVICE_CLASS_NX_I2C NX_DEVICE_CLASS_I2C
#define NX_DEVICE_CLASS_NX_TIMER NX_DEVICE_CLASS_TIMER
#define NX_DEVICE_CLASS_NX_ADC NX_DEVICE_CLASS_ADC
#define NX_DEVICE_CLASS_NX_ADC_BUFFER NX_DEVICE_CLASS_ADC_BUFFER
#define NX_DEVICE_CLASS_NX_DAC NX_DEVICE_CLASS_DAC
#define NX_DEVICE_CLASS_NX_INTERNAL_FLASH NX_DEVICE_CLASS_FLASH
#define NX_DEVICE_CLASS_NX_CAN NX_DEVICE_CLASS_CAN
#define NX_DEVICE_CLASS_NX_USB NX_DEVICE_CLASS_USB
#define NX_DEVICE_CLASS_NX_RTC NX_DEVICE_CLASS_RTC
#define NX_DEVICE_CLASS_NX_CRC NX_DEVICE_CLASS_CRC
#define NX_DEVICE_CLASS_NX_SDIO NX_DEVICE_CLASS_SDIO
#define NX_DEVICE_CLASS_NX_WATCHDOG NX_DEVICE_CLASS_WATCHDOG
#define NX_DEVICE_CLASS_NX_OPTION_BYTES NX_DEVICE_CLASS_OPTION_BYTES

/*---------------------------------------------------------------------------*/
/* Device Registration Macro                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register a device at compile time
 * \param[in]       device_type: Device type identifier (e.g., NX_UART)
 * \param[in]       index: Device index
 * \param[in]       device_name: Device name string
 * \param[in]       device_config: Pointer to device configuration
 * \param[in]       device_state: Pointer to device state
 * \param[in]       init: Device initialization function
 *
 * This macro registers a device in the .nx_device linker section.
 * The device will be automatically discovered at runtime.
 *
 * Example:
 * \code
 * NX_DEVICE_REGISTER(NX_UART, 0, "UART0", &uart0_config,
 *                    &uart0_state, uart0_init);
 * \endcode
 */
#if NX_DEVICE_MANUAL_REGISTRATION
/* Manual registration: Devices are not static */
#define NX_DEVICE_REGISTER(device_type, index, device_name, device_config,     \
                           device_state, init)                                 \
    const nx_device_t NX_CONCAT(device_type, index) = {                        \
        .name = device_name,                                                   \
        .config = device_config,                                               \
        .state = device_state,                                                 \
        .device_init = init,                                                   \
        .device_class = NX_CONCAT(NX_DEVICE_CLASS_, device_type),                \
    }
#else
/* Linker section: Devices are static and placed in .nx_device section */
#define NX_DEVICE_REGISTER(device_type, index, device_name, device_config,     \
                           device_state, init)                                 \
    NX_USED NX_SECTION(".nx_device")                                           \
        NX_ALIGNED(sizeof(void*)) static const nx_device_t                     \
        NX_CONCAT(device_type, index) = {                                      \
            .name = device_name,                                               \
            .config = device_config,                                           \
            .state = device_state,                                             \
            .device_init = init,                                               \
            .device_class = NX_CONCAT(NX_DEVICE_CLASS_, device_type),            \
    }
#endif

/** Status-preserving registration for new drivers. The constructor only binds
 * an instance and must not start hardware; get_lifecycle bridges that typed
 * instance into hardware open/close. Config/state/descriptors have static life. */
#if NX_DEVICE_MANUAL_REGISTRATION
#define NX_DEVICE_DESCRIPTOR_STORAGE
#else
#define NX_DEVICE_DESCRIPTOR_STORAGE NX_USED NX_SECTION(".nx_device") NX_ALIGNED(sizeof(void*)) static
#endif
#define NX_DEVICE_REGISTER_TYPED(device_type, index, device_name, device_config, \
                                  device_state, class_id, caps, constructor, life) \
    NX_DEVICE_DESCRIPTOR_STORAGE const nx_device_t NX_CONCAT(device_type, index) = { \
        .name = device_name, .config = device_config, .state = device_state,     \
        .device_class = class_id, .capabilities = caps,                         \
        .construct = constructor, .get_lifecycle = life                         \
    }

/*---------------------------------------------------------------------------*/
/* Instance Traversal Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Traverse all enabled instances of a device type
 * \param[in]       fn: Function macro to call for each instance
 * \param[in]       device_type: Device type identifier
 *
 * This macro expands to call the provided function macro for each
 * enabled instance of the device type. The instance list is generated
 * by Kconfig using NX_DEFINE_INSTANCE_* macros.
 *
 * Example:
 * \code
 * #define UART_REGISTER(index) \
 *     NX_DEVICE_REGISTER(NX_UART, index, "UART" #index, ...)
 *
 * NX_TRAVERSE_EACH_INSTANCE(UART_REGISTER, NX_UART);
 * \endcode
 */
#define NX_TRAVERSE_EACH_INSTANCE(fn, device_type)                             \
    NX_CONCAT(NX_DEFINE_INSTANCE_, device_type)(fn)

/**
 * \brief           Initialize device and cache the API pointer
 * \param[in]       dev: Device descriptor
 * \return          Device API pointer, NULL on failure
 * \note            This function caches the API pointer after first init
 *                  Subsequent calls return the cached pointer
 */
/** Legacy construction entrance, not an owning reference. New products use
 * nx_device_open and typed actions. Task context only. Returns NULL while another caller initializes this
 * descriptor; the caller may retry. Device initialization never runs while
 * holding the port critical region. Descriptors and cached API are static. */
void* nx_device_init(const nx_device_t* dev);

/**
 * \brief           Legacy untyped construction (find + init), for migration only
 * \param[in]       name: Device name
 * \return          Device API pointer, NULL if not found or init failed
 * \note            This is a convenience function combining find and init
 */
void* nx_device_get(const char* name);

/** Migration factory entry: class checked, construction only, no hardware
 * ownership. It rejects a descriptor currently claimed by the typed API.
 * New products must use references; previously escaped legacy pointers cannot
 * acquire generation protection retroactively. */
void* nx_device_get_checked(const char* name, nx_device_class_t expected);

/*---------------------------------------------------------------------------*/
/* Manual Registration (for MSVC, native platform, and testing)              */
/*---------------------------------------------------------------------------*/

#if NX_DEVICE_MANUAL_REGISTRATION

/**
 * \brief           Manually register a device
 * \param[in]       dev: Device descriptor pointer
 * \return          NX_OK on success, error code otherwise
 * \note            Available on platforms without linker section support
 *                  Task context; registry metadata is protected by Arch
 */
nx_status_t nx_device_register(const nx_device_t* dev);

/**
 * \brief           Clear all manually registered devices
 * \note            Useful for test cleanup
 *                  Leaves registry intact while a typed owner/construction/shutdown is active;
 *                  use nx_device_registry_reset() for an explicit status
 */
void nx_device_clear_all(void);

/** Startup/test registry reset; BUSY if any device has a typed owner or is
 * constructing. No hardware shutdown and no generation counter reset. */
nx_status_t nx_device_registry_reset(void);

#endif

#ifdef __cplusplus
}
#endif
#endif
