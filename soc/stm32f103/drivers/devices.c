/**
 * \file            devices.c
 * \brief           Exact legacy device register review and physical geometry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_provider.h"
#include <stddef.h>
_Static_assert(offsetof(nx_legacy_adc_registers_t, control2) ==
                   offsetof(ADC_TypeDef, CR2),
               "Exact F103 CR2 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, sample2) ==
                   offsetof(ADC_TypeDef, SMPR2),
               "Exact F103 SMPR2 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, sequence3) ==
                   offsetof(ADC_TypeDef, SQR3),
               "Exact F103 SQR3 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, data) ==
                   offsetof(ADC_TypeDef, DR),
               "Exact F103 DR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, key) ==
                   offsetof(FLASH_TypeDef, KEYR),
               "Exact F103 KEYR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, status) ==
                   offsetof(FLASH_TypeDef, SR),
               "Exact F103 SR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, control) ==
                   offsetof(FLASH_TypeDef, CR),
               "Exact F103 CR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, address) ==
                   offsetof(FLASH_TypeDef, AR),
               "Exact F103 AR register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, channel_mode1) ==
                   offsetof(TIM_TypeDef, CCMR1),
               "Exact F103 CCMR1 register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, prescaler) ==
                   offsetof(TIM_TypeDef, PSC),
               "Exact F103 PSC register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, reload) ==
                   offsetof(TIM_TypeDef, ARR),
               "Exact F103 ARR register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, compare1) ==
                   offsetof(TIM_TypeDef, CCR1),
               "Exact F103 CCR1 register ABI");
_Static_assert(ADC_CR2_ADON == 1U && ADC_CR2_CAL == 4U &&
                   ADC_CR2_RSTCAL == 8U && ADC_CR2_SWSTART == (1U << 22U),
               "Exact F103 ADC control bits");
_Static_assert(FLASH_SR_BSY == 1U && FLASH_SR_PGERR == 4U &&
                   FLASH_SR_WRPRTERR == 16U && FLASH_CR_LOCK == 128U,
               "Exact F103 Flash command/status bits");
static const nx_flash_sector_t sectors[128] = {
    {0U, 1024U},      {1024U, 1024U},   {2048U, 1024U},   {3072U, 1024U},
    {4096U, 1024U},   {5120U, 1024U},   {6144U, 1024U},   {7168U, 1024U},
    {8192U, 1024U},   {9216U, 1024U},   {10240U, 1024U},  {11264U, 1024U},
    {12288U, 1024U},  {13312U, 1024U},  {14336U, 1024U},  {15360U, 1024U},
    {16384U, 1024U},  {17408U, 1024U},  {18432U, 1024U},  {19456U, 1024U},
    {20480U, 1024U},  {21504U, 1024U},  {22528U, 1024U},  {23552U, 1024U},
    {24576U, 1024U},  {25600U, 1024U},  {26624U, 1024U},  {27648U, 1024U},
    {28672U, 1024U},  {29696U, 1024U},  {30720U, 1024U},  {31744U, 1024U},
    {32768U, 1024U},  {33792U, 1024U},  {34816U, 1024U},  {35840U, 1024U},
    {36864U, 1024U},  {37888U, 1024U},  {38912U, 1024U},  {39936U, 1024U},
    {40960U, 1024U},  {41984U, 1024U},  {43008U, 1024U},  {44032U, 1024U},
    {45056U, 1024U},  {46080U, 1024U},  {47104U, 1024U},  {48128U, 1024U},
    {49152U, 1024U},  {50176U, 1024U},  {51200U, 1024U},  {52224U, 1024U},
    {53248U, 1024U},  {54272U, 1024U},  {55296U, 1024U},  {56320U, 1024U},
    {57344U, 1024U},  {58368U, 1024U},  {59392U, 1024U},  {60416U, 1024U},
    {61440U, 1024U},  {62464U, 1024U},  {63488U, 1024U},  {64512U, 1024U},
    {65536U, 1024U},  {66560U, 1024U},  {67584U, 1024U},  {68608U, 1024U},
    {69632U, 1024U},  {70656U, 1024U},  {71680U, 1024U},  {72704U, 1024U},
    {73728U, 1024U},  {74752U, 1024U},  {75776U, 1024U},  {76800U, 1024U},
    {77824U, 1024U},  {78848U, 1024U},  {79872U, 1024U},  {80896U, 1024U},
    {81920U, 1024U},  {82944U, 1024U},  {83968U, 1024U},  {84992U, 1024U},
    {86016U, 1024U},  {87040U, 1024U},  {88064U, 1024U},  {89088U, 1024U},
    {90112U, 1024U},  {91136U, 1024U},  {92160U, 1024U},  {93184U, 1024U},
    {94208U, 1024U},  {95232U, 1024U},  {96256U, 1024U},  {97280U, 1024U},
    {98304U, 1024U},  {99328U, 1024U},  {100352U, 1024U}, {101376U, 1024U},
    {102400U, 1024U}, {103424U, 1024U}, {104448U, 1024U}, {105472U, 1024U},
    {106496U, 1024U}, {107520U, 1024U}, {108544U, 1024U}, {109568U, 1024U},
    {110592U, 1024U}, {111616U, 1024U}, {112640U, 1024U}, {113664U, 1024U},
    {114688U, 1024U}, {115712U, 1024U}, {116736U, 1024U}, {117760U, 1024U},
    {118784U, 1024U}, {119808U, 1024U}, {120832U, 1024U}, {121856U, 1024U},
    {122880U, 1024U}, {123904U, 1024U}, {124928U, 1024U}, {125952U, 1024U},
    {126976U, 1024U}, {128000U, 1024U}, {129024U, 1024U}, {130048U, 1024U}};
const nx_flash_geometry_t g_nx_stm32f103_flash_geometry = {
    0x08000000U, 131072U, 2U, sectors, 128U, 0U};
