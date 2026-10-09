/* Production USART0 code, faulting IRQ model; no electrical timing claim. */
#include "model.h"
#include "gd32f470_platform.h"
#include <assert.h>
#include <stdio.h>
// NOLINTNEXTLINE(bugprone-suspicious-include): deliberate same-TU production fault fixture; retain private factory/state checks.
#include "../../../soc/gd32f470/controllers/uart.c"
uint32_t nx_gd32f470_millis(void){return fake_millis;}
uint64_t nx_gd32f470_timestamp_us(void){return (uint64_t)fake_millis*1000u+123u;}
static void irq(uint32_t flags){fake_uart_flags=flags;if(flags&USART_STAT0_TC)fake_uart_shift=false;fake_isr=1;USART0_IRQHandler();fake_isr=0;}
int main(void){
    nx_uart_t* api=create_uart(NULL);nx_lifecycle_t* life=api->get_lifecycle(api);
    assert(!fake_usart_interrupts); /* discovery cannot start a controller */
    assert(life->init(life)==NX_OK);
    nx_uart_operations_t* ops=api->get_operations(api);
    uint8_t bytes[]={1,2};nx_uart_ticket_t ticket;nx_uart_result_t result;
    assert(ops->submit(ops,bytes,2,5,&ticket)==NX_OK&&fake_de);
    assert(life->deinit(life)==NX_ERR_BUSY);
    irq(USART_STAT0_TBE|USART_STAT0_TC); /* sampled TC predates DATA write */
    assert(ops->poll(ops,ticket,&result)==NX_OK&&!result.settled&&result.transferred==1);
    irq(USART_STAT0_TBE|USART_STAT0_TC);
    assert(ops->poll(ops,ticket,&result)==NX_OK&&!result.settled&&result.transferred==2);
    irq(USART_STAT0_TC);
    assert(ops->poll(ops,ticket,&result)==NX_OK&&result.settled&&result.wire_idle&&result.status==NX_OK&&!fake_de);
    nx_uart_ticket_t previous=ticket;
    assert(ops->submit(ops,bytes,2,5,&ticket)==NX_OK);
    assert(ops->cancel(ops,previous)==NX_ERR_INVALID_STATE);
    irq(USART_STAT0_TBE);
    uint32_t writes=fake_uart_writes;
    assert(ops->cancel(ops,ticket)==NX_OK&&!fake_uart_shift&&!fake_de);
    assert(ops->poll(ops,ticket,&result)==NX_OK&&result.settled&&result.status==NX_ERR_CANCELLED);
    irq(USART_STAT0_TBE|USART_STAT0_TC);
    assert(fake_uart_writes==writes);
    nx_uart_rx_event_t event;
    assert(ops->receive_event(ops,&event)==NX_OK&&event.status==NX_ERR_IO&&!event.has_data&&event.raw_error==RX_CONTROLLER_RESET);
    fake_millis=UINT32_MAX-2u;
    assert(ops->submit(ops,bytes,2,5,&ticket)==NX_OK);
    fake_millis=3u;
    assert(ops->poll(ops,ticket,&result)==NX_OK&&result.settled&&result.status==NX_ERR_TIMEOUT);
    assert(ops->receive_event(ops,&event)==NX_OK&&event.status==NX_ERR_IO);
    for(unsigned i=0;i<NX_CONFIG_GD32_UART_RX_BUFFER_SIZE+2u;i++){fake_uart_rx=(uint8_t)i;irq(USART_STAT0_RBNE);}
    for(unsigned i=0;i<NX_CONFIG_GD32_UART_RX_BUFFER_SIZE;i++){
        assert(ops->receive_event(ops,&event)==NX_OK&&event.data==i&&event.has_data&&event.status==NX_OK&&event.resolution_us==1u);
    }
    assert(ops->receive_event(ops,&event)==NX_OK&&event.status==NX_ERR_FULL&&event.raw_error==2);
    fake_uart_rx=99;irq(USART_STAT0_RBNE|USART_STAT0_ORERR);
    assert(ops->receive_event(ops,&event)==NX_OK&&event.status==NX_ERR_IO&&event.has_data&&event.data==99);
    fake_mask=1;
    assert(api->get_tx_sync(api)->send(api->get_tx_sync(api),bytes,2,5)==NX_ERR_INVALID_STATE&&fake_mask==1);
    fake_mask=0;
    assert(life->deinit(life)==NX_OK&&life->init(life)==NX_OK);
    assert(ops->submit(ops,bytes,2,5,&ticket)==NX_OK&&ticket.sequence>previous.sequence);
    assert(ops->cancel(ops,ticket)==NX_OK);
    uart.sequence=UINT64_MAX;
    assert(ops->submit(ops,bytes,2,5,&ticket)==NX_ERR_FULL);
    fake_isr=1;assert(life->deinit(life)==NX_ERR_INVALID_STATE);fake_isr=0;
    puts("GD32 USART0 TC, cancellation settlement, stale tickets, deadlines, RX loss and bounded ring passed");
    return 0;
}
