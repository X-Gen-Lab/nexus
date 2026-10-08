#include "model.h"
#include "gd32f470_platform.h"
#include <assert.h>
#include <stdio.h>
#include "../../../soc/gd32f470zg/interrupt.c"
int main(void){
    assert(nx_gd32f470_timebase_init()==0&&fake_timer_config.prescaler==99&&fake_timer_config.period==UINT32_MAX);
    fake_timer_count=1234567;fake_mask=1;
    assert(nx_gd32f470_timestamp_us()==1234567&&fake_mask==1&&nx_gd32f470_millis()==1234);
    fake_timer_count=UINT32_MAX;assert(nx_gd32f470_timestamp_us()==UINT32_MAX);
    fake_timer_count=7;fake_timer_pending=true;
    assert(nx_gd32f470_timestamp_us()==UINT64_C(4294967303)&&fake_timer_pending);
    TIMER1_IRQHandler();
    assert(nx_gd32f470_timestamp_us()==UINT64_C(4294967303)&&!fake_timer_pending&&fake_mask==1);
    fake_timer_count=42;assert(nx_gd32f470_timestamp_us()==UINT64_C(4294967338));
    puts("GD32 1MHz TIMER1 pending overflow, epoch and nested interrupt mask passed");
    return 0;
}
