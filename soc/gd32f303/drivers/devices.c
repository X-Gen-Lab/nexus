/**
 * \file            devices.c
 * \brief           Exact legacy device register review and physical geometry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include <stddef.h>
_Static_assert(offsetof(nx_legacy_adc_registers_t, control2) == 0x08U &&
                   offsetof(nx_legacy_adc_registers_t, sample2) == 0x10U &&
                   offsetof(nx_legacy_adc_registers_t, sequence3) == 0x34U &&
                   offsetof(nx_legacy_adc_registers_t, data) == 0x4cU &&
                   ADC_CTL1_ADCON == 1U && ADC_CTL1_CLB == 4U &&
                   ADC_CTL1_RSTCLB == 8U && ADC_CTL1_SWRCST == (1U << 22U),
               "Exact SDK3.0.3 GD303 ADC0 register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, status) ==
                       FMC_STAT0_REG_OFFSET &&
                   offsetof(nx_legacy_flash_registers_t, control) ==
                       FMC_CTL0_REG_OFFSET,
               "Exact GD303 FMC bank0 register ABI");
_Static_assert(FMC_STAT0_BUSY == 1U && FMC_STAT0_PGERR == 4U &&
                   FMC_STAT0_WPERR == 16U && FMC_CTL0_LK == 128U,
               "Exact GD303 FMC command/status bits");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, compare1) == 0x34U &&
                   TIMER_CHCTL0_CH0COMCTL == 0x70U &&
                   TIMER_CHCTL0_CH0COMSEN == 8U,
               "Exact GD303 TIMER CH0 ABI");
static const nx_flash_sector_t sectors[128] = {
    {0U, 2048U},      {2048U, 2048U},   {4096U, 2048U},   {6144U, 2048U},
    {8192U, 2048U},   {10240U, 2048U},  {12288U, 2048U},  {14336U, 2048U},
    {16384U, 2048U},  {18432U, 2048U},  {20480U, 2048U},  {22528U, 2048U},
    {24576U, 2048U},  {26624U, 2048U},  {28672U, 2048U},  {30720U, 2048U},
    {32768U, 2048U},  {34816U, 2048U},  {36864U, 2048U},  {38912U, 2048U},
    {40960U, 2048U},  {43008U, 2048U},  {45056U, 2048U},  {47104U, 2048U},
    {49152U, 2048U},  {51200U, 2048U},  {53248U, 2048U},  {55296U, 2048U},
    {57344U, 2048U},  {59392U, 2048U},  {61440U, 2048U},  {63488U, 2048U},
    {65536U, 2048U},  {67584U, 2048U},  {69632U, 2048U},  {71680U, 2048U},
    {73728U, 2048U},  {75776U, 2048U},  {77824U, 2048U},  {79872U, 2048U},
    {81920U, 2048U},  {83968U, 2048U},  {86016U, 2048U},  {88064U, 2048U},
    {90112U, 2048U},  {92160U, 2048U},  {94208U, 2048U},  {96256U, 2048U},
    {98304U, 2048U},  {100352U, 2048U}, {102400U, 2048U}, {104448U, 2048U},
    {106496U, 2048U}, {108544U, 2048U}, {110592U, 2048U}, {112640U, 2048U},
    {114688U, 2048U}, {116736U, 2048U}, {118784U, 2048U}, {120832U, 2048U},
    {122880U, 2048U}, {124928U, 2048U}, {126976U, 2048U}, {129024U, 2048U},
    {131072U, 2048U}, {133120U, 2048U}, {135168U, 2048U}, {137216U, 2048U},
    {139264U, 2048U}, {141312U, 2048U}, {143360U, 2048U}, {145408U, 2048U},
    {147456U, 2048U}, {149504U, 2048U}, {151552U, 2048U}, {153600U, 2048U},
    {155648U, 2048U}, {157696U, 2048U}, {159744U, 2048U}, {161792U, 2048U},
    {163840U, 2048U}, {165888U, 2048U}, {167936U, 2048U}, {169984U, 2048U},
    {172032U, 2048U}, {174080U, 2048U}, {176128U, 2048U}, {178176U, 2048U},
    {180224U, 2048U}, {182272U, 2048U}, {184320U, 2048U}, {186368U, 2048U},
    {188416U, 2048U}, {190464U, 2048U}, {192512U, 2048U}, {194560U, 2048U},
    {196608U, 2048U}, {198656U, 2048U}, {200704U, 2048U}, {202752U, 2048U},
    {204800U, 2048U}, {206848U, 2048U}, {208896U, 2048U}, {210944U, 2048U},
    {212992U, 2048U}, {215040U, 2048U}, {217088U, 2048U}, {219136U, 2048U},
    {221184U, 2048U}, {223232U, 2048U}, {225280U, 2048U}, {227328U, 2048U},
    {229376U, 2048U}, {231424U, 2048U}, {233472U, 2048U}, {235520U, 2048U},
    {237568U, 2048U}, {239616U, 2048U}, {241664U, 2048U}, {243712U, 2048U},
    {245760U, 2048U}, {247808U, 2048U}, {249856U, 2048U}, {251904U, 2048U},
    {253952U, 2048U}, {256000U, 2048U}, {258048U, 2048U}, {260096U, 2048U}};
const nx_flash_geometry_t g_nx_gd32f303_flash_geometry = {
    0x08000000U, 262144U, 2U, sectors, 128U, 0U};
