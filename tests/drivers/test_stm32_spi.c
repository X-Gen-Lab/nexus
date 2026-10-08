/** Host executable contracts using production STM32 driver + fake ST HAL.
 * Does not validate timing, register/electrical behavior or a physical board. */
#include "stm32_spi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osal/osal_mutex.h"
#include "osal/osal_sem.h"

SPI_TypeDef fake_spi;
static DMA_Stream_TypeDef tx_stream, rx_stream;
static DMA_HandleTypeDef tx_dma, rx_dma;
static uint32_t now, primask, ipsr, complete_after;
static SPI_HandleTypeDef* live;
static stm32_spi_impl_t* live_bus;
static bool dma_error, duplicate, start_fail, abort_fail, cancel_on_pump, missing_dma;
static bool board_missing;
static unsigned starts, aborts, callbacks, callback_result, give_isr, give_task;
static unsigned lock_delay, lock_timeout, transfer_timeout, cs_active;
static uint8_t trace_tx[16];
static unsigned trace_mode[16], trace_cs[16], trace_speed[16], traces;

uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1; }
void __set_PRIMASK(uint32_t value) { primask = value; }
uint32_t __get_IPSR(void) { return ipsr; }
void __DMB(void) {}
void __DSB(void) {}
uint32_t HAL_GetTick(void) { return now; }
static void pump(void) {
    ++now;
    if (!live) return;
    if (cancel_on_pump) {
        cancel_on_pump = false;
        ipsr = 16;
        assert(live_bus->active->base.cancel(&live_bus->active->base) == NX_OK);
        ipsr = 0;
    }
    if (live && complete_after && --complete_after == 0) {
        SPI_HandleTypeDef* h = live;
        if (h->pRxBuffPtr && h->RxXferCount)
            memcpy(h->pRxBuffPtr, h->pTxBuffPtr, h->RxXferCount);
        tx_stream.CR &= ~DMA_SxCR_EN; rx_stream.CR &= ~DMA_SxCR_EN;
        tx_dma.State = rx_dma.State = HAL_DMA_STATE_READY;
        live = NULL;
        ipsr = 16;
        if (dma_error) HAL_SPI_ErrorCallback(h);
        else HAL_SPI_TxRxCpltCallback(h);
        if (duplicate) { HAL_SPI_TxCpltCallback(h); HAL_SPI_ErrorCallback(h); }
        ipsr = 0;
    }
}
void __NOP(void) { pump(); }
nx_status_t stm32_spi_board_prepare(stm32_spi_impl_t* b) {
    if (board_missing) return NX_ERR_NOT_SUPPORTED;
    live_bus = b;
    tx_dma = (DMA_HandleTypeDef){.Instance=&tx_stream, .Parent=&b->hspi};
    rx_dma = (DMA_HandleTypeDef){.Instance=&rx_stream, .Parent=&b->hspi};
    if (!missing_dma) { b->hspi.hdmatx=&tx_dma; b->hspi.hdmarx=&rx_dma; }
    return NX_OK;
}
nx_status_t stm32_spi_board_select(stm32_spi_impl_t* b, uint8_t cs, bool active) {
    (void)b;
    if (active) cs_active = cs;
    return NX_OK;
}
uint32_t stm32_spi_board_clock_hz(stm32_spi_impl_t* b) { (void)b; return 16000000; }
void stm32_spi_board_release(stm32_spi_impl_t* b) { (void)b; }
bool stm32_spi_board_dma_buffer_valid(const void* data, size_t length, bool write) {
    (void)write;
    return data && length;
}
NX_NORETURN void stm32_spi_dma_failstop(stm32_spi_impl_t* b) { (void)b; abort(); }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef* h) { h->Instance->enabled = true; return HAL_OK; }
HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef* h) { h->Instance->enabled = false; return HAL_OK; }
static void trace(SPI_HandleTypeDef* h, uint8_t* data) {
    assert(traces < 16);
    trace_tx[traces] = *data;
    trace_mode[traces] = h->Init.CLKPolarity * 2 + h->Init.CLKPhase;
    trace_cs[traces] = cs_active;
    trace_speed[traces] = h->Init.BaudRatePrescaler;
    ++traces;
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef* h, uint8_t* data,
                                   uint16_t len, uint32_t timeout) {
    (void)len; ++starts; trace(h, data); transfer_timeout = timeout;
    if (timeout < 2) { now += timeout; return HAL_TIMEOUT; }
    now += 2; return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* h, uint8_t* tx,
                                          uint8_t* rx, uint16_t len, uint32_t timeout) {
    HAL_StatusTypeDef r = HAL_SPI_Transmit(h, tx, len, timeout);
    if (r == HAL_OK) memcpy(rx, tx, len);
    return r;
}
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef* h, uint8_t* tx, uint16_t len) {
    ++starts; trace(h, tx); live = h;
    h->pTxBuffPtr = tx; h->TxXferCount = len;
    tx_stream.CR = DMA_SxCR_EN | DMA_IT_TC | DMA_IT_TE;
    tx_dma.State = HAL_DMA_STATE_BUSY;
    h->Instance->CR2 |= SPI_CR2_TXDMAEN;
    return start_fail ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef* h, uint8_t* tx,
                                              uint8_t* rx, uint16_t len) {
    h->pRxBuffPtr=rx; h->RxXferCount=len;
    rx_stream.CR=DMA_SxCR_EN | DMA_IT_TC; rx_dma.State=HAL_DMA_STATE_BUSY;
    h->Instance->CR2 |= SPI_CR2_RXDMAEN;
    return HAL_SPI_Transmit_DMA(h, tx, len);
}
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef* h) {
    if (abort_fail) return HAL_ERROR;
    h->Instance->CR &= ~DMA_SxCR_EN; h->State=HAL_DMA_STATE_READY;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef* h) {
    ++aborts;
    if (abort_fail) return HAL_ERROR;
    if (h->hdmatx) (void)HAL_DMA_Abort(h->hdmatx);
    if (h->hdmarx) (void)HAL_DMA_Abort(h->hdmarx);
    h->Instance->CR2=0; live=NULL;
    return HAL_OK;
}

typedef struct { unsigned count; } sem_t;
osal_status_t osal_mutex_create(osal_mutex_handle_t* out) { *out = malloc(1); return *out ? OSAL_OK : OSAL_ERROR_NO_MEMORY; }
osal_status_t osal_mutex_delete(osal_mutex_handle_t h) { free(h); return OSAL_OK; }
osal_status_t osal_mutex_lock(osal_mutex_handle_t h, uint32_t timeout) {
    assert(h); lock_timeout=timeout;
    if (lock_delay > timeout) { now+=timeout; return OSAL_ERROR_TIMEOUT; }
    now+=lock_delay; return OSAL_OK;
}
osal_status_t osal_mutex_unlock(osal_mutex_handle_t h) { assert(h); return OSAL_OK; }
osal_status_t osal_sem_create(uint32_t initial, uint32_t max, osal_sem_handle_t* out) {
    assert(max == 1); sem_t* s=malloc(sizeof(*s)); s->count=initial; *out=s; return OSAL_OK;
}
osal_status_t osal_sem_delete(osal_sem_handle_t h) { free(h); return OSAL_OK; }
osal_status_t osal_sem_take(osal_sem_handle_t h, uint32_t timeout) {
    sem_t* s=h;
    while (!s->count && timeout) { pump(); --timeout; }
    if (!s->count) return OSAL_ERROR_TIMEOUT;
    --s->count; return OSAL_OK;
}
osal_status_t osal_sem_give(osal_sem_handle_t h) {
    assert(ipsr == 0); ++give_task; ((sem_t*)h)->count=1; return OSAL_OK;
}
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t h) {
    assert(ipsr != 0); ++give_isr; ((sem_t*)h)->count=1; return OSAL_OK;
}

static void terminal(void* context, nx_status_t result) {
    assert(context == &callbacks);
    assert(!ipsr && !(tx_stream.CR & DMA_SxCR_EN) && !(rx_stream.CR & DMA_SxCR_EN));
    ++callbacks; callback_result=result;
}
static void setup(stm32_spi_impl_t* b, bool dma) {
    now=primask=ipsr=0; complete_after=1; live=NULL; traces=0; starts=aborts=callbacks=0;
    dma_error=duplicate=start_fail=abort_fail=cancel_on_pump=missing_dma=board_missing=false;
    lock_delay=lock_timeout=transfer_timeout=give_isr=give_task=0;
    tx_stream=(DMA_Stream_TypeDef){0}; rx_stream=(DMA_Stream_TypeDef){0}; fake_spi=(SPI_TypeDef){0};
    stm32_spi_platform_config_t c={.spi_base=&fake_spi, .spi_index=1,
        .mode=SPI_MODE_MASTER, .direction=SPI_DIRECTION_2LINES,
        .data_size=SPI_DATASIZE_8BIT, .nss=SPI_NSS_SOFT,
        .baud_prescaler=SPI_BAUDRATEPRESCALER_16, .use_dma=dma};
    stm32_spi_construct(b,&c);
}
static nx_spi_device_t* device(stm32_spi_impl_t* b, uint8_t cs, uint32_t speed, uint8_t mode) {
    nx_spi_device_config_t c=NX_SPI_DEVICE_CONFIG_DEFAULT(cs,speed); c.mode=mode;
    static nx_spi_device_t handles[64];
    static unsigned next;
    assert(next < 64);
    nx_spi_device_t* d=&handles[next++];
    assert(b->base.open_device(&b->base,&c,d)==NX_OK); return d;
}
static void finish(stm32_spi_impl_t* b) { assert(b->lifecycle.deinit(&b->lifecycle)==NX_OK); }
static void test_configuration_and_lifecycle(void) {
    stm32_spi_impl_t b; setup(&b,false);
    assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* a=device(&b,1,1000000,0); nx_spi_device_t* z=device(&b,2,2000000,3);
    uint8_t tx=0x5a,rx=0;
    nx_spi_transaction_t t={&tx,&rx,1,10,terminal,&callbacks};
    assert(a->transfer(a,&t)==NX_OK && rx==tx);
    assert(z->transfer(z,&t)==NX_OK);
    assert(trace_cs[0]==1 && trace_cs[1]==2 && trace_mode[0]==0 && trace_mode[1]==3);
    assert(trace_speed[0]==16 && trace_speed[1]==8 && callbacks==2);
    assert(b.lifecycle.get_state(&b.lifecycle)==NX_DEV_STATE_RUNNING);
    assert(b.power.disable(&b.power)==NX_OK);
    assert(a->transfer(a,&t)==NX_ERR_SUSPENDED);
    assert(b.lifecycle.resume(&b.lifecycle)==NX_OK);
    ipsr=16; assert(a->transfer(a,&t)==NX_ERR_INVALID_STATE); ipsr=0;
    assert(b.base.close_device(&b.base,a)==NX_OK);
    assert(b.base.close_device(&b.base,a)==NX_ERR_INVALID_STATE);
    finish(&b);
}
static void test_dma_outcomes(void) {
    for (unsigned scenario=0;scenario<6;++scenario) {
        stm32_spi_impl_t b; setup(&b,true); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
        nx_spi_device_t* d=device(&b,1,1000000,0);
        uint8_t tx[2]={4,5},rx[2]={0}; nx_spi_transaction_t t={tx,rx,2,5,terminal,&callbacks};
        nx_status_t expected=NX_OK;
        if (scenario==0) duplicate=true;
        if (scenario==1) { dma_error=true; expected=NX_ERR_DMA_TRANSFER; }
        if (scenario==2) { complete_after=0; expected=NX_ERR_TIMEOUT; }
        if (scenario==3) { complete_after=0; cancel_on_pump=true; expected=NX_ERR_CANCELLED; }
        if (scenario==4) { start_fail=true; expected=NX_ERR_IO; }
        if (scenario==5) { complete_after=0; abort_fail=true; expected=NX_ERR_HARDWARE; }
        assert(d->transfer(d,&t)==expected);
        assert(callbacks==1 && callback_result==(unsigned)expected && aborts>0);
        assert(!(tx_stream.CR & DMA_SxCR_EN) && !(rx_stream.CR & DMA_SxCR_EN));
        assert(b.hspi.pTxBuffPtr==NULL && b.hspi.pRxBuffPtr==NULL);
        unsigned before=callbacks;
        HAL_SPI_TxCpltCallback(&b.hspi); HAL_SPI_ErrorCallback(&b.hspi);
        assert(callbacks==before && b.phase==STM32_SPI_IDLE);
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
        if (scenario<=1 || scenario==3) assert(give_isr>0);
#endif
        abort_fail=false; live=NULL; finish(&b);
    }
}
static void test_queue_cancel_deadline_and_pool(void) {
    stm32_spi_impl_t b; setup(&b,true); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* a=device(&b,1,1000000,0); nx_spi_device_t* z=device(&b,2,1000000,0);
    uint8_t tx=8; nx_spi_transaction_t t={&tx,NULL,1,5,terminal,&callbacks};
    assert(a->submit(a,&t)==NX_OK); assert(a->submit(a,&t)==NX_ERR_BUSY);
    assert(b.lifecycle.deinit(&b.lifecycle)==NX_ERR_BUSY);
    assert(b.base.close_device(&b.base,a)==NX_ERR_BUSY);
    assert(z->submit(z,&t)==NX_OK); assert(z->cancel(z)==NX_OK);
    now=6;
    assert(b.base.service(&b.base)==NX_ERR_TIMEOUT); assert(starts==0 && callbacks==1);
    assert(b.base.service(&b.base)==NX_ERR_CANCELLED); assert(starts==0 && callbacks==2);
    assert(b.base.service(&b.base)==NX_ERR_NO_DATA);
    nx_spi_device_t* pool[STM32_SPI_MAX_DEVICES-2];
    for(unsigned i=0;i<STM32_SPI_MAX_DEVICES-2;++i) pool[i]=device(&b,(uint8_t)(i+3),1000000,0);
    nx_spi_device_config_t cfg=NX_SPI_DEVICE_CONFIG_DEFAULT(0,1000000);
    nx_spi_device_t overflow={.token=1};
    assert(b.base.open_device(&b.base,&cfg,&overflow)==NX_ERR_NO_RESOURCE && !overflow.token);
    for(unsigned i=0;i<STM32_SPI_MAX_DEVICES-2;++i) assert(b.base.close_device(&b.base,pool[i])==NX_OK);
    finish(&b);
}
static unsigned received; static void receive_data(void* context,const uint8_t* rx,size_t len) {
    assert(context==&received && len==2 && rx[0]==0x11 && rx[1]==0x22); ++received;
}
static void test_legacy_copy_and_context(void) {
    stm32_spi_impl_t b; setup(&b,true); assert(b.lifecycle.init(&b.lifecycle)==NX_OK); received=0;
    nx_spi_device_config_t cfg=NX_SPI_DEVICE_CONFIG_DEFAULT(1,1000000);
    nx_tx_rx_async_t* a=b.base.get_tx_rx_async_handle(&b.base,cfg,receive_data,&received);
    uint8_t tx[2]={0x11,0x22}; assert(a->tx_rx(a,tx,2,10)==NX_OK);
    memset(tx,0,sizeof(tx)); assert(a->get_state(a)==NX_ERR_BUSY);
    assert(b.base.service(&b.base)==NX_OK);
    assert(received==1 && a->get_state(a)==NX_OK && trace_tx[0]==0x11);
    finish(&b);
}
static void test_budget_and_wrap(void) {
    stm32_spi_impl_t b; setup(&b,false); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* d=device(&b,1,1000000,0); uint8_t tx=3;
    nx_spi_transaction_t t={&tx,NULL,1,6,terminal,&callbacks};
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    lock_delay=5;
    assert(d->transfer(d,&t)==NX_ERR_TIMEOUT); assert(lock_timeout==6 && transfer_timeout==1);
    lock_delay=8; starts=0; assert(d->transfer(d,&t)==NX_ERR_TIMEOUT && !starts);
    lock_delay=0;
#endif
    now=UINT32_MAX-1; assert(d->transfer(d,&t)==NX_OK && now==0);
    t.length=UINT16_MAX+1U; assert(d->transfer(d,&t)==NX_ERR_INVALID_PARAM);
    t.length=1; t.timeout_ms=0; starts=0; assert(d->transfer(d,&t)==NX_ERR_TIMEOUT && !starts);
    finish(&b);
}
static void test_missing_board_and_dma(void) {
    stm32_spi_impl_t b; setup(&b,true); board_missing=true;
    assert(b.lifecycle.init(&b.lifecycle)==NX_ERR_NOT_SUPPORTED);
    board_missing=false; missing_dma=true;
    assert(b.lifecycle.init(&b.lifecycle)==NX_ERR_DMA_CONFIG);
    assert(b.lifecycle.get_state(&b.lifecycle)==NX_DEV_STATE_UNINITIALIZED);
}
static void test_seeded_storm(void) {
    stm32_spi_impl_t b; setup(&b,true); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* devices[2]={device(&b,0,1000000,0), device(&b,1,2000000,3)};
    uint32_t seed=0x2034f407;
    uint8_t tx[4]={1,2,3,4},rx[4];
    for(unsigned i=0;i<1000;++i) {
        seed=1664525U*seed+1013904223U;
        unsigned id=seed & 1U;
        traces=callbacks=0;
        complete_after=1+(seed % 3);
        dma_error=(seed & 8U)!=0;
        duplicate=(seed & 16U)!=0;
        cancel_on_pump=(seed & 32U)!=0;
        nx_status_t expected=cancel_on_pump ? NX_ERR_CANCELLED :
                              dma_error ? NX_ERR_DMA_TRANSFER : NX_OK;
        nx_spi_transaction_t t={tx,rx,sizeof(tx),5,terminal,&callbacks};
        assert(devices[id]->submit(devices[id],&t)==NX_OK);
        assert(b.base.service(&b.base)==expected);
        assert(callbacks==1 && callback_result==(unsigned)expected);
        assert(trace_cs[0]==id && trace_mode[0]==(id ? 3U : 0U));
        assert(!(tx_stream.CR & DMA_SxCR_EN) && !(rx_stream.CR & DMA_SxCR_EN));
        HAL_SPI_TxRxCpltCallback(&b.hspi);
        assert(callbacks==1 && b.base.service(&b.base)==NX_ERR_NO_DATA);
    }
    finish(&b);
}
typedef struct {
    stm32_spi_impl_t* bus;
    nx_spi_device_t* device;
    nx_spi_transaction_t transaction;
    unsigned calls;
} chain_context_t;
static void chained_terminal(void* context, nx_status_t result) {
    chain_context_t* c=context;
    assert(result==NX_OK);
    ++c->calls;
    assert(c->bus->base.service(&c->bus->base)==NX_ERR_BUSY);
    assert(c->bus->lifecycle.deinit(&c->bus->lifecycle)==NX_ERR_BUSY);
    if (c->calls==1) {
        assert(c->device->submit(c->device,&c->transaction)==NX_OK);
    } else {
        assert(c->bus->base.close_device(&c->bus->base,c->device)==NX_OK);
    }
}
static void test_callback_chaining(void) {
    stm32_spi_impl_t b; setup(&b,false); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    uint8_t tx=9;
    chain_context_t c={.bus=&b,.device=device(&b,0,1000000,0)};
    c.transaction=(nx_spi_transaction_t){&tx,NULL,1,10,chained_terminal,&c};
    assert(c.device->submit(c.device,&c.transaction)==NX_OK);
    assert(b.base.service(&b.base)==NX_OK && c.calls==1);
    assert(b.base.service(&b.base)==NX_OK && c.calls==2);
    assert(b.base.service(&b.base)==NX_ERR_NO_DATA);
    finish(&b);
}
static void test_stale_value_generation_and_exhaustion(void) {
    stm32_spi_impl_t b; setup(&b,false); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* current=device(&b,0,1000000,0);
    nx_spi_device_t stale=*current;
    assert(b.base.close_device(&b.base,current)==NX_OK);
    nx_spi_device_t* replacement=device(&b,1,1000000,0);
    assert(stale.token != replacement->token);
    uint8_t tx=0x44; nx_spi_transaction_t t={&tx,NULL,1,10,NULL,NULL};
    assert(stale.transfer(&stale,&t)==NX_ERR_INVALID_STATE);
    assert(stale.cancel(&stale)==NX_ERR_INVALID_STATE);
    assert(b.base.close_device(&b.base,&stale)==NX_ERR_INVALID_STATE);
    assert(replacement->transfer(replacement,&t)==NX_OK);
    nx_spi_device_t previous=*replacement;
    assert(b.lifecycle.deinit(&b.lifecycle)==NX_OK);
    assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* newer=device(&b,2,1000000,0);
    assert(newer->token!=previous.token);
    assert(previous.transfer(&previous,&t)==NX_ERR_INVALID_STATE);
    assert(b.base.close_device(&b.base,&previous)==NX_ERR_INVALID_STATE);
    assert(newer->transfer(newer,&t)==NX_OK);
    assert(b.base.close_device(&b.base,newer)==NX_OK);
    b.next_token=UINT64_MAX;
    nx_spi_device_config_t cfg=NX_SPI_DEVICE_CONFIG_DEFAULT(0,1000000);
    nx_spi_device_t out={.token=1};
    assert(b.base.open_device(&b.base,&cfg,&out)==NX_ERR_NO_RESOURCE && out.token==0);
    finish(&b);
}
typedef struct { stm32_spi_impl_t* bus; nx_spi_device_t* device; unsigned calls; } sync_context_t;
static void sync_terminal(void* context, nx_status_t status) {
    sync_context_t* c=context;
    assert(status==NX_OK); ++c->calls;
    assert(c->bus->lifecycle.deinit(&c->bus->lifecycle)==NX_ERR_BUSY);
    assert(c->bus->lifecycle.suspend(&c->bus->lifecycle)==NX_ERR_BUSY);
    assert(c->bus->base.close_device(&c->bus->base,c->device)==NX_OK);
}
static void test_sync_callback_lifecycle_pin(void) {
    stm32_spi_impl_t b; setup(&b,false); assert(b.lifecycle.init(&b.lifecycle)==NX_OK);
    nx_spi_device_t* d=device(&b,0,1000000,0);
    sync_context_t c={&b,d,0};
    uint8_t byte=0x11; nx_spi_transaction_t t={&byte,NULL,1,10,sync_terminal,&c};
    assert(d->transfer(d,&t)==NX_OK && c.calls==1);
    finish(&b);
}
int main(void) {
    test_configuration_and_lifecycle(); test_dma_outcomes();
    test_queue_cancel_deadline_and_pool(); test_legacy_copy_and_context();
    test_budget_and_wrap(); test_missing_board_and_dma(); test_seeded_storm(); test_callback_chaining(); test_stale_value_generation_and_exhaustion(); test_sync_callback_lifecycle_pin();
    printf("STM32 SPI driver contracts: 10 suites, DMA 6 fault/race outcomes + 1000 seeded operations passed (%s)\n",
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
        "OSAL"
#else
        "bare metal"
#endif
    );
    return 0;
}
