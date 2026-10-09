/** Typed GPIO facade; no OSAL or vendor dependency. */
#include "nx_device_internal.h"
#include "hal/interface/nx_gpio.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>
static nx_status_t pin(nx_device_ref_t ref, nx_device_class_t expected, void** api) {
    return nx_device_dispatch_pin(ref, expected, true, api);
}
static void unpin(nx_device_ref_t ref) { nx_device_dispatch_unpin(ref); }
static nx_status_t gpio_pin(nx_device_ref_t ref, void** api) {
    if (ref.device_class != NX_DEVICE_CLASS_GPIO && ref.device_class != NX_DEVICE_CLASS_GPIO_READ &&
        ref.device_class != NX_DEVICE_CLASS_GPIO_WRITE) return NX_ERR_TYPE_MISMATCH;
    nx_status_t status = pin(ref, ref.device_class, api);
    if (status != NX_OK) return status;
    nx_lifecycle_t* life = nx_device_dispatch_lifecycle(ref.descriptor, *api);
    nx_device_state_t hardware = life && life->get_state ? life->get_state(life) : NX_DEV_STATE_ERROR;
    if (hardware != NX_DEV_STATE_RUNNING) {
        status = hardware == NX_DEV_STATE_SUSPENDED ? NX_ERR_SUSPENDED :
            hardware == NX_DEV_STATE_UNINITIALIZED ? NX_ERR_NOT_INIT : NX_ERR_NOT_READY;
        unpin(ref);
    }
    return status;
}
nx_status_t nx_device_gpio_read(nx_device_ref_t ref, uint8_t* value) {
    if (!value) return NX_ERR_NULL_PTR;
    *value = 0;
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_WRITE) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_read_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->read : api;
    if (gpio->read) *value = gpio->read(gpio); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_gpio_write(nx_device_ref_t ref, uint8_t value) {
    if (value > 1) return NX_ERR_INVALID_PARAM;
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_READ) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_write_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->write : api;
    if (gpio->write) gpio->write(gpio, value); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_gpio_toggle(nx_device_ref_t ref) {
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_READ) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_write_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->write : api;
    if (gpio->toggle) gpio->toggle(gpio); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
