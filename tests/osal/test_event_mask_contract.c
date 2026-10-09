#include "event_mask_contract.h"
#include "resource_usage_contract.h"
#include <stdio.h>
int main(void) {
    assert(osal_init() == OSAL_OK);
    test_event_advertised_bits();
    test_resource_usage_pool();
    assert(osal_deinit() == OSAL_OK);
    puts("Every advertised event bit is usable and reserved bits fail");
    return 0;
}
