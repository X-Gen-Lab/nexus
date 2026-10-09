/** Production UART software contracts; fake HAL is not board/HIL evidence. */
#include "stm32_uart.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_runtime.h"
#include "stm32_uart_callbacks.h"
#include "hal/base/nx_device.h"
#include "hal/provider/nx_device_provider.h"
#include "hal/resource/nx_isr_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

USART_TypeDef fake_usart[6];
static uint32_t now, key, basepri, faultmask, ipsr;
static bool abort_failure, rx_failure, disconnect_failure, deinit_failure, init_failure;
static unsigned callbacks, aborted, rx_armed;
static nx_isr_func_t irq_handler[80];
static void* irq_context[80];
static UART_HandleTypeDef* active_tx;
static stm32_uart_impl_t* callback_driver;
static const uint8_t* callback_data;
static bool probe_callback, probe_close;
uint32_t __get_PRIMASK(void) { return key; }
void __disable_irq(void) { key = 1; }
void __set_PRIMASK(uint32_t saved) { key = saved; }
uint32_t __get_IPSR(void) { return ipsr; }
void __DMB(void) {}
void __DSB(void) {}
nx_arch_irq_state_t nx_arch_irq_save(void) { nx_arch_irq_state_t old={key}; key=1; return old; }
void nx_arch_irq_restore(nx_arch_irq_state_t old) { key=old.value; }
bool nx_arch_in_isr(void) {return ipsr!=0;}
bool nx_arch_irq_is_masked(void) {return key || basepri || faultmask;}
void nx_arch_dmb(void) {}
void nx_arch_dsb(void) {}
uint32_t HAL_GetTick(void) { return now; }
void __NOP(void) { assert(!key); now++; }
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { (void)irq; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { (void)irq; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { (void)irq; }
static nx_status_t connect_irq(nx_isr_manager_t* self,uint32_t irq,nx_isr_func_t fn,void* context,uint8_t priority) {
    (void)self; assert(priority == 5 && irq < 80 && !irq_handler[irq]);
    irq_handler[irq] = fn; irq_context[irq] = context; return NX_OK;
}
static nx_status_t disconnect_irq(nx_isr_manager_t* self,uint32_t irq) {
    (void)self; if (disconnect_failure) return NX_ERR_IO;
    irq_handler[irq] = NULL; irq_context[irq] = NULL; return NX_OK;
}
static nx_isr_manager_t manager = {connect_irq,disconnect_irq};
nx_isr_manager_t* nx_isr_manager_get(void) { return &manager; }
nx_status_t stm32_uart_board_prepare(uint8_t index) { return index < 3 ? NX_OK : NX_ERR_NOT_SUPPORTED; }
nx_status_t stm32_uart_board_release(uint8_t index) { return index < 3 ? NX_OK : NX_ERR_NOT_SUPPORTED; }
uint64_t stm32_uart_board_timestamp_us(void) { return (uint64_t)now * 1000; }
uint32_t stm32_uart_board_timestamp_resolution_us(void) { return 1000; }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef* handle) {
    if (init_failure) return HAL_ERROR;
    handle->Instance->CR1 = USART_CR1_UE; handle->Instance->SR = UART_FLAG_TC;
    handle->gState = handle->RxState = HAL_UART_STATE_READY; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef* handle) {
    if (deinit_failure) return HAL_ERROR;
    handle->Instance->CR1 = 0; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef* handle,uint8_t* data,uint16_t length) {
    if (handle->gState != HAL_UART_STATE_READY) return HAL_BUSY;
    handle->pTxBuffPtr=data; handle->TxXferCount=handle->TxXferSize=length;
    handle->gState=HAL_UART_STATE_BUSY_TX; handle->Instance->SR &= ~UART_FLAG_TC;
    handle->Instance->CR1 |= USART_CR1_TXEIE; active_tx=handle; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef* handle,uint8_t* data,uint16_t length) {
    if (rx_failure) return HAL_ERROR;
    assert(length == 1); handle->pRxBuffPtr=data; handle->RxXferCount=length;
    handle->RxState=HAL_UART_STATE_BUSY_RX; rx_armed++; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef* handle) {
    aborted++; if (abort_failure) return HAL_ERROR;
    handle->Instance->CR1 &= ~(USART_CR1_TXEIE|USART_CR1_TCIE);
    handle->pTxBuffPtr=NULL; handle->gState=HAL_UART_STATE_READY;
    if (active_tx == handle) active_tx=NULL;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef* handle) {
    handle->pRxBuffPtr=NULL; handle->RxState=HAL_UART_STATE_READY; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Abort(UART_HandleTypeDef* handle) {
    if (probe_close && handle == &callback_driver->huart) {
        nx_uart_operations_t* ops=&callback_driver->operations;
        nx_uart_ticket_t ticket; nx_uart_result_t result;
        assert(callback_driver->closing && !key);
        assert(ops->submit(ops,callback_data,3,10,&ticket)==NX_ERR_BUSY);
        assert(ops->cancel(ops,callback_driver->ticket)==NX_ERR_BUSY);
        assert(ops->poll(ops,callback_driver->ticket,&result)==NX_ERR_BUSY);
        assert(callback_driver->lifecycle.deinit(&callback_driver->lifecycle)==NX_ERR_BUSY);
    }
    HAL_StatusTypeDef status=HAL_UART_AbortTransmit(handle);
    if (status == HAL_OK) status=HAL_UART_AbortReceive(handle);
    return status;
}
uint32_t HAL_UART_GetError(UART_HandleTypeDef* handle) { return handle->ErrorCode; }
void HAL_UART_IRQHandler(UART_HandleTypeDef* handle) {
    assert(ipsr && handle->pRxBuffPtr);
    uint8_t* start=handle->pRxBuffPtr;
    *handle->pRxBuffPtr++=(uint8_t)handle->Instance->DR;
    handle->RxXferCount=0; handle->RxState=HAL_UART_STATE_READY;
    HAL_UART_RxCpltCallback(handle);
    /* HAL advances RX pointer before callback; driver must use rx_byte. */
    assert(handle->pRxBuffPtr == start);
}
static void completed(UART_HandleTypeDef* handle, bool tc) {
    handle->TxXferCount=0;
    if (tc) { handle->Instance->SR |= UART_FLAG_TC; handle->gState=HAL_UART_STATE_READY; }
    ipsr=16; HAL_UART_TxCpltCallback(handle); ipsr=0;
}
static void tx_callback(void* context) {
    assert(!ipsr && !nx_arch_irq_is_masked() && context == &callbacks); callbacks++;
    if (probe_callback) {
        nx_uart_operations_t* ops=&callback_driver->operations;
        nx_uart_ticket_t ticket; nx_uart_result_t result={0};
        assert(callback_driver->callback_active);
        assert(ops->cancel(ops,callback_driver->ticket)==NX_ERR_BUSY);
        assert(ops->poll(ops,callback_driver->ticket,&result)==NX_ERR_BUSY && !result.settled);
        assert(ops->submit(ops,callback_data,3,10,&ticket)==NX_ERR_BUSY && !ticket.sequence);
        assert(callback_driver->lifecycle.deinit(&callback_driver->lifecycle)==NX_ERR_BUSY);
        assert(callback_driver->lifecycle.suspend(&callback_driver->lifecycle)==NX_ERR_BUSY);
        assert(callback_driver->state->initialized && callback_driver->callback_active);
    }
}
extern const nx_device_t STM32_UART0,STM32_UART1,STM32_UART2;
static nx_uart_t* bind(const nx_device_t* descriptor) {
    void* api = NULL;
    assert(descriptor->construct(descriptor, &api) == NX_OK && api);
    return api;
}
int main(void) {
    nx_uart_t* ports[3]={bind(&STM32_UART0),bind(&STM32_UART1),bind(&STM32_UART2)};
    stm32_uart_impl_t* driver[3];
    for (unsigned i=0;i<3;i++) {
        driver[i]=NX_CONTAINER_OF(ports[i],stm32_uart_impl_t,base);
        assert(driver[i]->huart.Instance == &fake_usart[i]);
        assert(ports[i]->get_lifecycle(ports[i])->init(ports[i]->get_lifecycle(ports[i])) == NX_OK);
        assert(irq_handler[37+i] && irq_context[37+i] == driver[i]);
    }
    assert(rx_armed == 3);
    stm32_uart_impl_t* impl=driver[1]; nx_uart_operations_t* ops=ports[1]->get_operations(ports[1]);
    assert(stm32_uart_register_tx_callback(impl,tx_callback,&callbacks)==NX_OK);
    uint8_t data[3]={1,2,3}; nx_uart_ticket_t ticket; nx_uart_result_t result;
    assert(ops->submit(ops,data,sizeof(data),10,&ticket)==NX_OK);
    assert(impl->huart.pTxBuffPtr == data);
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_ERR_BUSY);
    completed(&impl->huart,false);
    assert(ops->poll(ops,ticket,&result)==NX_OK && !result.settled && !callbacks);
    completed(&impl->huart,true);
    assert(callbacks == 0);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.settled && result.wire_idle && result.status==NX_OK);
    assert(result.transferred==3 && callbacks==1);
    completed(&impl->huart,true);
    assert(ops->poll(ops,ticket,&result)==NX_OK && callbacks==1);
    /* Completion IRQ precedes cancel, but its deferred notification must not
     * survive a successful settlement response. */
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK);
    completed(&impl->huart,true);
    assert(impl->notification_pending);
    assert(ops->cancel(ops,ticket)==NX_OK && !impl->notification_pending);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.settled && result.status==NX_OK && callbacks==1);
    /* Model the task callback dispatch window directly: reentrant cancellation
     * or close cannot release a lease while the callback is executing. */
    callback_driver=impl; callback_data=data; probe_callback=true;
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK);
    completed(&impl->huart,true);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.settled && callbacks==2 && !impl->callback_active);
    probe_callback=false;
    assert(ops->cancel(ops,ticket)==NX_OK);
    nx_uart_ticket_t old=ticket;
    assert(ops->submit(ops,data,3,3,&ticket)==NX_OK);
    now+=3;
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.settled && result.status==NX_ERR_TIMEOUT);
    assert(!impl->huart.pTxBuffPtr && !result.wire_idle);
    assert(ops->poll(ops,old,&result)==NX_ERR_INVALID_STATE);
    impl->huart.Instance->SR |= UART_FLAG_TC;
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK);
    assert(ops->cancel(ops,ticket)==NX_OK);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.status==NX_ERR_CANCELLED && result.settled);
    impl->huart.Instance->SR |= UART_FLAG_TC;
    assert(ops->submit(ops,data,UINT16_MAX+1UL,10,&ticket)==NX_ERR_INVALID_PARAM);
    ipsr=16; assert(ops->submit(ops,data,3,10,&ticket)==NX_ERR_INVALID_STATE); ipsr=0;
    /* A task critical section is not an ISR, but also stops IRQ/tick progress.
     * Reject it before borrowing or entering a synchronous wait. */
    nx_tx_sync_t* tx=ports[1]->get_tx_sync(ports[1]);
    nx_rx_sync_t* rx=ports[1]->get_rx_sync(ports[1]);
    nx_tx_async_t* tx_async=ports[1]->get_tx_async(ports[1]);
    nx_rx_async_t* rx_async=ports[1]->get_rx_async(ports[1]);
    size_t length=sizeof(data); nx_uart_rx_event_t event;
    unsigned before_abort=aborted;
    key=1;
    assert(tx->send(tx,data,3,10)==NX_ERR_INVALID_STATE && !impl->state->tx_busy);
    assert(rx->receive(rx,data,&length,10)==NX_ERR_INVALID_STATE && length==3);
    assert(rx->receive_all(rx,data,&length,10)==NX_ERR_INVALID_STATE && length==3);
    assert(ops->submit(ops,data,3,10,&ticket)==NX_ERR_INVALID_STATE && !ticket.sequence);
    assert(tx_async->send(tx_async,data,3)==NX_ERR_INVALID_STATE);
    assert(tx_async->get_state(tx_async)==NX_ERR_INVALID_STATE);
    assert(rx_async->receive(rx_async,data,&length)==NX_ERR_INVALID_STATE);
    assert(ops->receive_event(ops,&event)==NX_ERR_INVALID_STATE);
    assert(aborted==before_abort && !impl->huart.pTxBuffPtr && key==1);
    key=0;
    uint32_t* masks[]={&basepri,&faultmask};
    for(unsigned i=0;i<2;i++) {
        *masks[i]=1; length=sizeof(data);
        assert(tx->send(tx,data,3,10)==NX_ERR_INVALID_STATE);
        assert(rx->receive_all(rx,data,&length,10)==NX_ERR_INVALID_STATE && length==3);
        assert(ops->submit(ops,data,3,10,&ticket)==NX_ERR_INVALID_STATE && !ticket.sequence);
        assert(!impl->state->tx_busy && !impl->huart.pTxBuffPtr && aborted==before_abort && *masks[i]==1);
        *masks[i]=0;
    }
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK);
    key=1;
    assert(ops->poll(ops,ticket,&result)==NX_ERR_INVALID_STATE);
    assert(ops->cancel(ops,ticket)==NX_ERR_INVALID_STATE);
    assert(impl->state->tx_busy && impl->huart.pTxBuffPtr==data && aborted==before_abort);
    key=0;
    assert(ops->cancel(ops,ticket)==NX_OK);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.settled);
    impl->huart.Instance->SR |= UART_FLAG_TC;
    /* Internal copy locking must not reject an ordinary legacy async caller. */
    assert(tx_async->send(tx_async,data,3)==NX_OK && impl->huart.pTxBuffPtr!=data);
    assert(memcmp(impl->huart.pTxBuffPtr,data,3)==0 && !key);
    assert(ops->cancel(ops,impl->ticket)==NX_OK);
    assert(tx_async->get_state(tx_async)==NX_ERR_CANCELLED);
    /* IRQ RX is routed to the selected instance, with acquisition timestamp. */
    assert(stm32_uart_register_rx_callback(impl,tx_callback,&callbacks)==NX_OK);
    probe_callback=true;
    now=42; fake_usart[1].DR=0xA5; ipsr=16; irq_handler[38](irq_context[38]); ipsr=0;
    now=99;
    assert(ops->receive_event(ops,&event)==NX_OK && event.has_data && event.data==0xA5);
    assert(!impl->callback_active);
    probe_callback=false;
    assert(stm32_uart_register_rx_callback(impl,NULL,&callbacks)==NX_OK);
    assert(event.timestamp_us==42000 && event.resolution_us==1000);
    assert(ports[0]->get_operations(ports[0])->receive_event(ports[0]->get_operations(ports[0]),&event)==NX_ERR_NO_DATA);
    impl->huart.ErrorCode=HAL_UART_ERROR_ORE; impl->huart.RxState=HAL_UART_STATE_READY;
    ipsr=16; HAL_UART_ErrorCallback(&impl->huart); ipsr=0;
    assert(ops->receive_event(ops,&event)==NX_OK && !event.has_data && event.status==NX_ERR_OVERRUN);
    for (unsigned i=0;i<6;i++) { now=100+i; fake_usart[1].DR=i; ipsr=16; irq_handler[38](irq_context[38]); ipsr=0; }
    for (unsigned i=0;i<4;i++) assert(ops->receive_event(ops,&event)==NX_OK && event.has_data && event.data==i);
    assert(ops->receive_event(ops,&event)==NX_OK && !event.has_data && event.status==NX_ERR_OVERRUN && event.raw_error==2 && event.timestamp_us==104000);
    UART_HandleTypeDef foreign={0}; HAL_UART_ErrorCallback(&foreign); HAL_UART_RxCpltCallback(&foreign); HAL_UART_TxCpltCallback(&foreign);
    /* Preserve nested interrupt mask and safe oversized DROP_OLD writes. */
    uint8_t ring_storage[4],readback[4],input[9]={0,1,2,3,4,5,6,7,8}; stm32_uart_buffer_t ring;
    stm32_uart_buffer_init(&ring,ring_storage,4,UART_OVERFLOW_DROP_OLD); key=1;
    assert(stm32_uart_buffer_write_safe(&ring,input,9)==4 && key==1);
    assert(stm32_uart_buffer_read_safe(&ring,readback,4)==4 && key==1 && memcmp(readback,input+5,4)==0); key=0;
    /* Abort failure is an observable quarantined fault with proven IT settlement. */
    for (unsigned i=0;i<6;i++) { now=200+i; fake_usart[1].DR=i; ipsr=16; irq_handler[38](irq_context[38]); ipsr=0; }
    now=206;
    impl->huart.Instance->SR |= UART_FLAG_TC;
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK); abort_failure=true;
    assert(ops->cancel(ops,ticket)==NX_ERR_HARDWARE);
    assert(ops->poll(ops,ticket,&result)==NX_OK && result.status==NX_ERR_HARDWARE && result.settled && result.wire_idle);
    assert(!impl->huart.pTxBuffPtr && !(impl->huart.Instance->CR1&USART_CR1_UE));
    unsigned before_rx=rx_armed;
    impl->huart.RxState=HAL_UART_STATE_READY;
    ipsr=16; HAL_UART_RxCpltCallback(&impl->huart); HAL_UART_ErrorCallback(&impl->huart); ipsr=0;
    assert(rx_armed==before_rx);
    for (unsigned i=0;i<4;i++) assert(ops->receive_event(ops,&event)==NX_OK && event.has_data && event.data==i);
    assert(ops->receive_event(ops,&event)==NX_OK && !event.has_data && event.status==NX_ERR_OVERRUN && event.raw_error==2);
    assert(ops->receive_event(ops,&event)==NX_OK && !event.has_data && event.status==NX_ERR_HARDWARE && event.timestamp_us==206000);
    assert(ops->receive_event(ops,&event)==NX_ERR_HARDWARE);
    assert(ops->submit(ops,data,3,10,&old)==NX_ERR_INVALID_STATE); abort_failure=false;
    /* Teardown failure retains live owner and can be retried. */
    disconnect_failure=true; assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_ERR_IO && impl->state->initialized);
    disconnect_failure=false; deinit_failure=true;
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_ERR_HARDWARE && impl->state->initialized);
    deinit_failure=false; assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_OK);
    uint64_t previous=ticket.sequence;
    assert(impl->lifecycle.init(&impl->lifecycle)==NX_OK);
    assert(ops->poll(ops,ticket,&result)==NX_ERR_INVALID_STATE);
    assert(!impl->callbacks.tx_complete_cb && !impl->notification_pending);
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK && ticket.sequence>previous);
    completed(&impl->huart,true); assert(ops->poll(ops,ticket,&result)==NX_OK && result.status==NX_OK);
    /* Close also suppresses a successful IRQ's unconsumed notification, and
     * reserves teardown before HAL calls can yield to another task. */
    assert(stm32_uart_register_tx_callback(impl,tx_callback,&callbacks)==NX_OK);
    assert(ops->submit(ops,data,3,10,&ticket)==NX_OK); completed(&impl->huart,true);
    assert(impl->notification_pending); unsigned before_callback=callbacks;
    probe_close=true;
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_OK);
    probe_close=false;
    assert(!impl->notification_pending && !impl->ticket_active && !impl->callbacks.tx_complete_cb);
    assert(impl->lifecycle.init(&impl->lifecycle)==NX_OK);
    assert(ops->poll(ops,ticket,&result)==NX_ERR_INVALID_STATE && callbacks==before_callback);
    for (unsigned i=0;i<3;i++) assert(driver[i]->lifecycle.deinit(&driver[i]->lifecycle)==NX_OK);
    assert(aborted>=6);
    /* Failed open with failed cleanup retains partial hardware ownership. */
    init_failure=deinit_failure=true;
    assert(impl->lifecycle.init(&impl->lifecycle)==NX_ERR_HARDWARE && impl->state->initialized && impl->faulted);
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_ERR_HARDWARE);
    init_failure=deinit_failure=false;
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_OK && !impl->state->initialized);
    rx_failure=true;
    assert(impl->lifecycle.init(&impl->lifecycle)==NX_ERR_HARDWARE && !impl->state->initialized);
    rx_failure=false;
    assert(impl->lifecycle.init(&impl->lifecycle)==NX_OK);
    assert(impl->lifecycle.deinit(&impl->lifecycle)==NX_OK);
    puts("STM32 UART binding, IRQ RX/error, TC, cancellation, deadline, ownership and failure contracts passed");
    return 0;
}
