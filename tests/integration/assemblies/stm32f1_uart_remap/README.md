# MB784 C.1 USART2 RS-232 and PHY management

The STM32F107VCT6 software reference uses the HSE25/PLL72 clock,
PD5/PD6 USART2 at 115200 baud and PC1/PA2 Clause 22 MDIO. Select RS-232
with JP16 and leave the ISP-control jumpers JP18 and JP19 open. The
CN6 DB9 connector carries RS-232 levels through the ST3241EBPR. The
ordinary MDIO-only reference can leave the USART wiring unspecified.

RM0008 Rev 21 §9.4.2 documents an undefined read value for the SWJ_CFG
bits in MAPR. The USART2 cold owner uses the documented 111 no-effect
encoding also used by ST LL, and checks the readable remap bit. An
existing remap is adopted. A mapping changed by Nexus is restored
after the USART and its interrupt are stopped. Only the owned field
is restored. The default PA2/PA3 path reads the mapping without
writing MAPR.

The model exercises apply, adopt, failed activation and retry after a
failed restore. The ARM fixture proves the UART and MDIO faces coexist
with disjoint pins and one USART2 interrupt owner. Board jumpers,
electrical behavior and serial timing require physical verification.

[RM0008 Rev 21](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) · [UM0600 Rev 5, Table 8, Table 20 and Figure 25](https://www.tme.eu/Document/aece8c709b198489a4f26cf104594d1a/CD00212441.pdf)
