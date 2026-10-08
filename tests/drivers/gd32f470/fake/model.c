#include "model.h"
#include "arch/nx_arch.h"
#include "board.h"
#include <assert.h>
#include <string.h>
uint32_t fake_mask,fake_isr,fake_millis,fake_clock_step,fake_usart_interrupts;
uint32_t fake_uart_flags,fake_reset_count,fake_spi_flags,fake_spi_writes,fake_uart_writes;
unsigned fake_board_safe_inits;
uint8_t fake_uart_rx;
bool fake_de,fake_cs,fake_uart_shift,fake_spi_shift,fake_spi_stall,fake_spi_hold_busy;
void (*fake_spi_hook)(void);
spi_parameter_struct fake_spi_config;
uint32_t fake_timer_count;
bool fake_timer_pending;
timer_parameter_struct fake_timer_config;
nx_arch_irq_state_t nx_arch_irq_save(void) { nx_arch_irq_state_t old={fake_mask};fake_mask=1;return old; }
void nx_arch_irq_restore(nx_arch_irq_state_t old) {fake_mask=old.value;}
bool nx_arch_irq_is_masked(void) {return fake_mask!=0;}
bool nx_arch_in_isr(void) {return fake_isr!=0;}
void nx_arch_dmb(void) {}
void nx_arch_dsb(void) {}
void nx_arch_isb(void) {}
void rcu_periph_clock_enable(uint32_t x) {(void)x;}
void rcu_periph_clock_sleep_enable(uint32_t x) {(void)x;}
void rcu_periph_reset_enable(uint32_t x) {
    fake_reset_count++;
    if(x==RCU_USART0RST){fake_uart_shift=false;fake_uart_flags=0;fake_usart_interrupts=0;}
    if(x==RCU_SPI4RST){fake_spi_shift=false;fake_spi_flags=SPI_STAT_TBE;}
}
void rcu_periph_reset_disable(uint32_t x) {(void)x;}
void NVIC_DisableIRQ(int x) {(void)x;}
void NVIC_EnableIRQ(int x) {(void)x;}
void NVIC_ClearPendingIRQ(int x) {(void)x;}
void NVIC_SetPriority(int x,uint32_t p) {(void)x;(void)p;}
#define NOOP2(f) void f(uint32_t x,uint32_t y){(void)x;(void)y;}
NOOP2(usart_baudrate_set)
NOOP2(usart_word_length_set)
NOOP2(usart_stop_bit_set)
NOOP2(usart_parity_config)
NOOP2(usart_receive_config)
NOOP2(usart_transmit_config)
void usart_interrupt_enable(uint32_t x,uint32_t y){(void)x;fake_usart_interrupts|=y;}
void usart_interrupt_disable(uint32_t x,uint32_t y){(void)x;fake_usart_interrupts&=~y;}
void usart_enable(uint32_t x){(void)x;}
void usart_disable(uint32_t x){(void)x;fake_uart_shift=false;}
void usart_deinit(uint32_t x){(void)x;fake_uart_flags=0;fake_usart_interrupts=0;}
void usart_flag_clear(uint32_t x,uint32_t y){(void)x;fake_uart_flags&=~y;}
uint16_t usart_data_receive(uint32_t x){(void)x;fake_uart_flags&=~(USART_STAT0_RBNE|15u);return fake_uart_rx;}
void usart_data_transmit(uint32_t x,uint16_t y){(void)x;(void)y;fake_uart_writes++;fake_uart_shift=true;fake_uart_flags&=~USART_STAT0_TC;}
nx_status_t nx_gd32_board_safe_init(void){
    assert(!fake_uart_shift&&!fake_spi_shift);
    ++fake_board_safe_inits;fake_de=false;fake_cs=false;return NX_OK;
}
nx_status_t nx_gd32_board_uart_pins(bool x){(void)x;return NX_OK;}
void nx_gd32_board_rs485_de(bool x){if(!x)assert(!fake_uart_shift);fake_de=x;}
nx_status_t nx_gd32_board_spi_pins(bool x){(void)x;return NX_OK;}
nx_status_t nx_gd32_board_spi_cs(uint8_t x,bool active){assert(x==0);if(!active)assert(!fake_spi_shift);fake_cs=active;return NX_OK;}
void spi_struct_para_init(spi_parameter_struct* p){memset(p,0,sizeof(*p));}
void spi_init(uint32_t x,spi_parameter_struct* p){(void)x;fake_spi_config=*p;}
void spi_enable(uint32_t x){(void)x;}
void spi_disable(uint32_t x){(void)x;fake_spi_shift=false;}
uint32_t fake_spi_stat(void){
    fake_millis+=fake_clock_step;
    fake_timer_count+=fake_clock_step*1000u;
    if(fake_spi_hook)fake_spi_hook();
    return fake_spi_stall ? SPI_STAT_TRANS : fake_spi_flags;
}
static uint8_t last_spi_byte;
void spi_i2s_data_transmit(uint32_t x,uint16_t y){(void)x;assert(fake_cs);fake_spi_writes++;last_spi_byte=(uint8_t)y;fake_spi_shift=true;fake_spi_flags=SPI_STAT_TBE|SPI_STAT_RBNE|SPI_STAT_TRANS;}
uint16_t spi_i2s_data_receive(uint32_t x){(void)x;fake_spi_flags=SPI_STAT_TBE|(fake_spi_hold_busy?SPI_STAT_TRANS:0);fake_spi_shift=fake_spi_hold_busy;return (uint8_t)(last_spi_byte^0xFFu);}
void timer_deinit(uint32_t x){(void)x;fake_timer_count=0;fake_timer_pending=false;}
void timer_struct_para_init(timer_parameter_struct* p){memset(p,0,sizeof(*p));}
void timer_init(uint32_t x,timer_parameter_struct* p){(void)x;fake_timer_config=*p;}
void timer_counter_value_config(uint32_t x,uint32_t y){(void)x;fake_timer_count=y;}
void timer_interrupt_flag_clear(uint32_t x,uint32_t y){(void)x;(void)y;fake_timer_pending=false;}
void timer_interrupt_enable(uint32_t x,uint32_t y){(void)x;(void)y;}
void timer_enable(uint32_t x){(void)x;}
uint32_t timer_counter_read(uint32_t x){(void)x;return fake_timer_count;}
FlagStatus timer_interrupt_flag_get(uint32_t x,uint32_t y){(void)x;(void)y;return fake_timer_pending?SET:RESET;}
