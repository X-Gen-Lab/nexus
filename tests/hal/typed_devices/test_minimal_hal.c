/** GPIO-only consumer: no platform, support, OSAL or other I/O facade linked. */
#include "hal/nx_hal.h"
#if defined(NX_DEVICE_REGISTER) || defined(NX_DEVICE_MANUAL_REGISTRATION)
#error "Consumer umbrella exposed provider registration"
#endif
int main(void) {
    const nx_device_t* descriptor = NULL;
    if (nx_device_discover("absent", NX_DEVICE_CLASS_GPIO, &descriptor) != NX_ERR_NOT_FOUND || descriptor)
        return 1;
    nx_device_ref_t invalid = {0};
    invalid.device_class = NX_DEVICE_CLASS_GPIO;
    if (nx_device_gpio_write(invalid, 1) != NX_ERR_NOT_FOUND) return 2;
    return 0;
}
