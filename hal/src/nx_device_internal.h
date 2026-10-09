/** Private dispatch bridge. A pin keeps registered lifetime and hardware open.
 * Shared SPI pins permit a second task to request cancellation while a provider
 * is blocked. The SPI child pool separately serializes same-child actions. */
#ifndef NX_DEVICE_INTERNAL_H
#define NX_DEVICE_INTERNAL_H
#include "hal/provider/nx_device_provider.h"
nx_status_t nx_device_dispatch_pin(nx_device_ref_t ref, nx_device_class_t expected,
                                  bool serialize, void** api);
void nx_device_dispatch_unpin(nx_device_ref_t ref);
nx_lifecycle_t* nx_device_dispatch_lifecycle(const nx_device_t* dev, void* api);
nx_status_t nx_device_dispatch_recover_unknown_uart(nx_device_ref_t ref);
#endif
