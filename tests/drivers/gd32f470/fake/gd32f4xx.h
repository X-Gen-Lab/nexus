#ifndef NEXUS_TEST_GD32F4XX_H
#define NEXUS_TEST_GD32F4XX_H
/* Test-only controller model. Production always uses locked official SDK. */
#include <stdint.h>
typedef enum { RESET = 0, SET = 1 } FlagStatus;
#define USART0 0u
#define SPI4 4u
#define TIMER1 1u
#define USART0_IRQn 37
#define TIMER1_IRQn 28
#define RCU_USART0 1u
#define RCU_USART0RST 2u
#define RCU_SPI4 3u
#define RCU_SPI4RST 4u
#define RCU_TIMER1 5u
#define RCU_TIMER1_SLP 6u
#define USART_STAT0_PERR (1u << 0)
#define USART_STAT0_FERR (1u << 1)
#define USART_STAT0_NERR (1u << 2)
#define USART_STAT0_ORERR (1u << 3)
#define USART_STAT0_RBNE (1u << 5)
#define USART_STAT0_TC (1u << 6)
#define USART_STAT0_TBE (1u << 7)
#define USART_FLAG_TC USART_STAT0_TC
#define USART_INT_RBNE USART_STAT0_RBNE
#define USART_INT_ERR (1u << 8)
#define USART_INT_PERR USART_STAT0_PERR
#define USART_INT_TBE USART_STAT0_TBE
#define USART_INT_TC USART_STAT0_TC
#define USART_WL_8BIT 0u
#define USART_STB_1BIT 0u
#define USART_PM_NONE 0u
#define USART_RECEIVE_ENABLE 1u
#define USART_TRANSMIT_ENABLE 1u
extern uint32_t fake_uart_flags;
#define USART_STAT0(x) (fake_uart_flags)
void usart_baudrate_set(uint32_t,uint32_t);
void usart_word_length_set(uint32_t,uint32_t);
void usart_stop_bit_set(uint32_t,uint32_t);
void usart_parity_config(uint32_t,uint32_t);
void usart_receive_config(uint32_t,uint32_t);
void usart_transmit_config(uint32_t,uint32_t);
void usart_interrupt_enable(uint32_t,uint32_t);
void usart_interrupt_disable(uint32_t,uint32_t);
void usart_enable(uint32_t);
void usart_disable(uint32_t);
void usart_deinit(uint32_t);
void usart_flag_clear(uint32_t,uint32_t);
uint16_t usart_data_receive(uint32_t);
void usart_data_transmit(uint32_t,uint16_t);
void rcu_periph_clock_enable(uint32_t);
void rcu_periph_clock_sleep_enable(uint32_t);
void rcu_periph_reset_enable(uint32_t);
void rcu_periph_reset_disable(uint32_t);
void NVIC_DisableIRQ(int);
void NVIC_EnableIRQ(int);
void NVIC_ClearPendingIRQ(int);
void NVIC_SetPriority(int,uint32_t);
/* Only host boot fixtures bind these CPU/SysTick ports. */
#define SysTick_IRQn (-1)
#define PendSV_IRQn (-2)
extern uint32_t SystemCoreClock;
void NVIC_SetPriorityGrouping(uint32_t);
uint32_t SysTick_Config(uint32_t);
#define SPI_STAT_RBNE (1u << 0)
#define SPI_STAT_TBE (1u << 1)
#define SPI_STAT_CONFERR (1u << 5)
#define SPI_STAT_RXORERR (1u << 6)
#define SPI_STAT_TRANS (1u << 7)
#define SPI_STAT_FERR (1u << 8)
#define SPI_FLAG_TBE SPI_STAT_TBE
#define SPI_FLAG_RBNE SPI_STAT_RBNE
#define SPI_FLAG_TRANS SPI_STAT_TRANS
uint32_t fake_spi_stat(void);
#define SPI_STAT(x) fake_spi_stat()
#define SPI_MASTER 1u
#define SPI_TRANSMODE_FULLDUPLEX 0u
#define SPI_FRAMESIZE_8BIT 0u
#define SPI_NSS_SOFT 1u
#define SPI_ENDIAN_MSB 0u
#define SPI_ENDIAN_LSB 1u
#define SPI_CK_PL_LOW_PH_1EDGE 0u
#define SPI_CK_PL_LOW_PH_2EDGE 1u
#define SPI_CK_PL_HIGH_PH_1EDGE 2u
#define SPI_CK_PL_HIGH_PH_2EDGE 3u
typedef struct { uint32_t device_mode,trans_mode,frame_size,nss,endian,clock_polarity_phase,prescale; } spi_parameter_struct;
void spi_struct_para_init(spi_parameter_struct*);
void spi_init(uint32_t,spi_parameter_struct*);
void spi_enable(uint32_t);
void spi_disable(uint32_t);
void spi_i2s_data_transmit(uint32_t,uint16_t);
uint16_t spi_i2s_data_receive(uint32_t);
#define TIMER_COUNTER_EDGE 0u
#define TIMER_COUNTER_UP 0u
#define TIMER_CKDIV_DIV1 0u
#define TIMER_INT_FLAG_UP 1u
#define TIMER_INT_UP 1u
typedef struct { uint16_t prescaler,alignedmode,counterdirection;uint32_t period;uint16_t clockdivision,repetitioncounter; } timer_parameter_struct;
void timer_deinit(uint32_t);
void timer_struct_para_init(timer_parameter_struct*);
void timer_init(uint32_t,timer_parameter_struct*);
void timer_counter_value_config(uint32_t,uint32_t);
void timer_interrupt_flag_clear(uint32_t,uint32_t);
void timer_interrupt_enable(uint32_t,uint32_t);
void timer_enable(uint32_t);
uint32_t timer_counter_read(uint32_t);
FlagStatus timer_interrupt_flag_get(uint32_t,uint32_t);
#define __NOP() ((void)0)
#define FMC_FLAG_END (1u << 0)
#define FMC_FLAG_OPERR (1u << 1)
#define FMC_FLAG_WPERR (1u << 2)
#define FMC_FLAG_PGMERR (1u << 3)
#define FMC_FLAG_PGSERR (1u << 4)
#define FMC_FLAG_RDDERR (1u << 5)
typedef enum {FMC_READY,FMC_BUSY,FMC_OPERR} fmc_state_enum;
void fmc_unlock(void);
#define DBG_ID 0x12345678u
void fmc_lock(void);
void fmc_flag_clear(uint32_t);
fmc_state_enum fmc_halfword_program(uint32_t,uint16_t);
fmc_state_enum fmc_page_erase(uint32_t);
fmc_state_enum fmc_state_get(void);
#endif
