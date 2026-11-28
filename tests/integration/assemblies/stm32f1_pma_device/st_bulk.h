/**
 * \file            st_bulk.h
 * \brief           Private one-packet vendor-class echo fixture.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_PMA_ST_BULK_H
#define NEXUS_TEST_F1_PMA_ST_BULK_H

#include "usb_device_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

nx_result_t nx_test_pma_st_start(const nx_usb_device_profile_t* profile,
                                 const nx_usb_activation_port_t* activation);
nx_result_t nx_test_pma_st_step(void);
nx_result_t nx_test_pma_st_stop(void);

#ifdef __cplusplus
}
#endif

#endif
