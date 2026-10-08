#include "model.h"
#include "gd32f470_platform.h"
#include <assert.h>
#include <stdio.h>
#include "../../../soc/gd32f470zg/interrupt.c"
int main(void){
    int initialized=nx_gd32f470_timebase_init();
    assert(initialized==0&&fake_timer_config.prescaler==99&&fake_timer_config.period==UINT32_MAX);
    fake_timer_count=1234567;fake_mask=1;
    uint64_t timestamp=nx_gd32f470_timestamp_us();
    uint32_t millis=nx_gd32f470_millis();
    assert(timestamp==1234567&&fake_mask==1&&millis==1234);
    fake_timer_count=UINT32_MAX;
    timestamp=nx_gd32f470_timestamp_us();
    assert(timestamp==UINT32_MAX);
    fake_timer_count=7;fake_timer_pending=true;
    timestamp=nx_gd32f470_timestamp_us();
    assert(timestamp==UINT64_C(4294967303)&&fake_timer_pending);
    TIMER1_IRQHandler();
    timestamp=nx_gd32f470_timestamp_us();
    assert(timestamp==UINT64_C(4294967303)&&!fake_timer_pending&&fake_mask==1);
    fake_timer_count=42;
    timestamp=nx_gd32f470_timestamp_us();
    assert(timestamp==UINT64_C(4294967338));
    puts("GD32 1MHz TIMER1 pending overflow, epoch and nested interrupt mask passed");
    return 0;
}
