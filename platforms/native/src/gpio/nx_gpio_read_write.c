/**
 * \file nx_gpio_read_write.c
 * \brief Compose both capabilities of the registered Native GPIO device.
 */

#include "nx_gpio_types.h"

extern void gpio_init_read(nx_gpio_read_t* read);
extern void gpio_init_write(nx_gpio_write_t* write);

/* Both interfaces are subobjects of nx_gpio_read_write_impl_t and share the
 * same state, lifecycle and power object. Never install an alternate layout. */
void gpio_init_read_write(nx_gpio_read_write_t* gpio) {
    if (!gpio) {
        return;
    }
    gpio_init_read(&gpio->read);
    gpio_init_write(&gpio->write);
}
