"""Emit read-only selected-resource admission before global clock effects.

This is a configuration-time projection, never a runtime ownership registry.
Shared GPIO gates do not establish a foreign writer. Unobservable watchdog state
and outside-assembly owners retain the explicit reset-like admission contract.
"""
import re

try:
    from .providers.common import fail, owned_claims
    from .providers import hardware
    from . import gpio_phase
except ImportError:
    from providers.common import fail, owned_claims
    from providers import hardware
    import gpio_phase


def _reject(condition, indent=4):
    space = " " * indent
    return [f"{space}if ({condition}) {{",
            f"{space}    result.primary = NX_ERROR_BUSY; return result;",
            f"{space}}}"]


def _clock(controller, family):
    """Name a maintained exclusive gate, not an inferred silicon topology."""
    stm = family == "stm32f407"
    if controller.startswith(("USART", "SPI", "I2C")):
        bus = hardware.maintained(family, controller).apb
    elif re.fullmatch(r"TIM[1349]", controller) and stm:
        bus = 2 if controller in {"TIM1", "TIM9"} else 1
    elif re.fullmatch(r"TIMER[0238]", controller) and not stm:
        bus = 2 if controller in {"TIMER0", "TIMER8"} else 1
    elif controller in ({"CAN1", "CAN2"} if stm else {"CAN0", "CAN1"}):
        bus = 1
    else:
        fail("No maintained cold clock assertion for " + controller)
    return ((f"RCC->APB{bus}ENR", f"RCC_APB{bus}ENR_{controller}EN") if stm
            else (f"RCU_APB{bus}EN", f"RCU_APB{bus}EN_{controller}EN"))


def _modern_selected(result):
    """Cold GPIO/byte-UART profiles keep family-specific banks and SDK hooks."""
    h7 = result.soc_family == "stm32h743"
    hook = "H743" if h7 else "U575"
    gate_register = "APB1LENR" if h7 else "APB2ENR"
    uart = "USART3" if h7 else "USART1"
    mux_register = "D2CCIP2R" if h7 else "CCIPR1"
    mux = "RCC_D2CCIP2R_USART28SEL" if h7 else "RCC_CCIPR1_USART1SEL"
    gpio_register = "AHB4ENR" if h7 else "AHB2ENR1"
    source = []
    pads = {}
    if h7 and result.dma_topology.lanes:
        # A shared reset is another owner's observable responsibility, even
        # when its clock is off and stream reads would look reset-like.
        source += _reject(
            "(NX_H743_READ(&RCC->AHB1RSTR) & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0u")
    for item in result.controllers:
        allowed = {"gpio", "uart", "spi", "pwm", "exti", "watchdog",
                   "adc", "i2c", "flash"}
        if not h7:
            allowed.add("lptim")
        if item.kind not in allowed:
            fail("New SoC cold profile has no maintained controller admission")
        if not h7 and item.kind == "lptim":
            source += _reject(
                "(NX_U575_READ(&RCC->APB1RSTR2) & RCC_APB1RSTR2_LPTIM2RST) != 0u || "
                "(NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_LPTIM2SEL) != 0u || "
                "!NX_U575_IRQ_AVAILABLE(LPTIM2_IRQn)")
            # A disabled gate cannot establish cold peripheral ownership.
            # The constructor repeats admission after acquiring only its gate.
            source.append("    if ((NX_U575_READ(&RCC->APB1ENR2) & RCC_APB1ENR2_LPTIM2EN) != 0u) {")
            conditions = [f"NX_U575_READ(&LPTIM2->{name}) != 0u" for name in
                          ("CR", "CFGR", "CFGR2", "DIER", "ISR", "CCMR1",
                           "RCR", "CNT", "CCR1", "CCR2")]
            conditions.append("NX_U575_READ(&LPTIM2->ARR) != 1u")
            source += _reject(" || ".join(conditions), indent=8)
            source.append("    }")
        if h7 and item.kind in {"spi", "pwm"}:
            register = "APB2ENR" if item.kind == "spi" else "APB1LENR"
            reset = "APB2RSTR" if item.kind == "spi" else "APB1LRSTR"
            source += _reject(
                f"(NX_H743_READ(&RCC->{register}) & RCC_{register}_{item.controller}EN) != 0u || "
                f"(NX_H743_READ(&RCC->{reset}) & RCC_{reset}_{item.controller}RST) != 0u")
            if item.kind == "spi":
                source += _reject(
                    "(NX_H743_READ(&RCC->D2CCIP1R) & RCC_D2CCIP1R_SPI45SEL) != 0u")
                if item.mode == "dma":
                    try:
                        from .providers.stm32h743.emission import dma_admission_conditions
                    except ImportError:
                        from providers.stm32h743.emission import dma_admission_conditions
                    for irq in ("DMA1_Stream2", "DMA1_Stream3", "SPI4"):
                        source += _reject(f"!NX_H743_IRQ_AVAILABLE({irq}_IRQn)")
                    source += [
                        "    if ((NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u) {"]
                    source += _reject(" || ".join(dma_admission_conditions(
                        result, spi_only=True)), indent=8)
                    source.append("    }")
        if h7 and item.kind == "exti":
            line = int(item.controller.removeprefix("EXTI"))
            source += _reject(
                "((NX_H743_READ(&EXTI->IMR1) | NX_H743_READ(&EXTI->EMR1) | "
                "NX_H743_READ(&EXTI->RTSR1) | NX_H743_READ(&EXTI->FTSR1) | "
                f"NX_H743_READ(&EXTI->PR1)) & {1 << line}u) != 0u || "
                f"!NX_H743_IRQ_AVAILABLE(EXTI{line}_IRQn)")
        if h7 and item.kind == "i2c":
            source += _reject(
                "(NX_H743_READ(&RCC->APB1LENR) & RCC_APB1LENR_I2C1EN) != 0u || "
                "(NX_H743_READ(&RCC->APB1LRSTR) & RCC_APB1LRSTR_I2C1RST) != 0u || "
                "(NX_H743_READ(&RCC->D2CCIP2R) & RCC_D2CCIP2R_I2C123SEL) != 0u")
        if h7 and item.kind == "adc":
            source += _reject(
                "(NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_ADC12EN) != 0u || "
                "(NX_H743_READ(&RCC->AHB1RSTR) & RCC_AHB1RSTR_ADC12RST) != 0u")
        if h7 and item.kind == "flash":
            source += _reject(
                "((NX_H743_READ(&FLASH->SR1) | NX_H743_READ(&FLASH->SR2)) & "
                "(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE)) != 0u || "
                "(NX_H743_READ(&FLASH->OPTSR_CUR) & FLASH_OPTSR_SWAP_BANK_OPT) != 0u")
        if item.kind == "uart":
            if not h7:
                uart = item.controller
                if uart not in {"USART1", "USART2"}:
                    fail("U575 cold profile requires a reviewed USART")
                gate_register = "APB2ENR" if uart == "USART1" else "APB1ENR1"
                mux = "RCC_CCIPR1_" + uart + "SEL"
            if item.controller != uart:
                fail("New SoC cold profile requires its exact reviewed UART")
            source += _reject(f"(NX_{hook}_READ(&RCC->{gate_register}) & "
                              f"RCC_{gate_register}_{uart}EN) != 0u")
            source += _reject(f"(NX_{hook}_READ(&RCC->{mux_register}) & {mux}) != 0u")
            source += _reject(f"!NX_{hook}_IRQ_AVAILABLE((IRQn_Type){uart}_IRQn)")
            if not h7 and item.get("mode") in {"dma-tx", "dma-chain-tx"}:
                source += _reject(
                    "!NX_U575_IRQ_AVAILABLE(GPDMA1_Channel0_IRQn)")
            if h7 and item.get("mode") in {"dma-tx", "rx-dma-idle", "rx-dma-continuous"}:
                lane = 0 if item.get("mode") == "dma-tx" else 1
                source += _reject(
                    f"!NX_H743_IRQ_AVAILABLE(DMA1_Stream{lane}_IRQn)")
                source.append(
                    "    if ((NX_H743_READ(&RCC->AHB1ENR) & "
                    "RCC_AHB1ENR_DMA1EN) != 0u) {")
                source += _reject(
                    f"NX_H743_READ(&DMA1_Stream{lane}->CR) != 0u || "
                    f"NX_H743_READ(&DMAMUX1_Channel{lane}->CCR) != 0u", indent=8)
                source.append("    }")
        elif not h7 and item.kind in {"spi", "pwm", "i2c", "adc"}:
            register = ("APB2ENR" if item.controller == "SPI1" else
                        "AHB3ENR" if item.controller == "ADC4" else "APB1ENR1")
            source += _reject(
                f"(NX_U575_READ(&RCC->{register}) & "
                f"RCC_{register}_{item.controller}EN) != 0u")
            if item.kind in {"spi", "i2c"}:
                source += _reject(f"(NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_{item.controller}SEL) != 0u")
                if item.kind == "i2c":
                    source += _reject("(NX_U575_READ(&RCC->CR) & (RCC_CR_HSION | RCC_CR_HSIKERON | RCC_CR_HSIRDY)) != 0u")
                    source += _reject("(NX_U575_READ(&RCC->ICSCR3) & RCC_ICSCR3_HSITRIM) != RCC_ICSCR3_HSITRIM_4")
            elif item.kind == "adc":
                source += _reject("(NX_U575_READ(&RCC->CCIPR3) & RCC_CCIPR3_ADCDACSEL) != 0u || (NX_U575_READ(&RCC->AHB2ENR1) & RCC_AHB2ENR1_ADC12EN) != 0u || (NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0u || (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_ASV) == 0u")
        elif not h7 and item.kind == "flash":
            source += _reject("!NX_U575_IRQ_AVAILABLE(FLASH_IRQn)")
        elif not h7 and item.kind == "exti":
            line = int(item.controller[-1])
            source += _reject(f"!NX_U575_IRQ_AVAILABLE((IRQn_Type)EXTI{line}_IRQn)")
            source += _reject(f"((NX_U575_READ(&EXTI->IMR1) | NX_U575_READ(&EXTI->EMR1) | NX_U575_READ(&EXTI->RTSR1) | NX_U575_READ(&EXTI->FTSR1) | NX_U575_READ(&EXTI->RPR1) | NX_U575_READ(&EXTI->FPR1)) & {1 << line}u) != 0u")
        for pin in item["pins"]:
            name = pin["pin"]
            pads.setdefault(name[1], set()).add(int(name[2:]))
    if not h7 and "claims" in result:
        owners = {item.id for item in result.controllers}
        owners.update(item["id"] for item in result["devices"])
        for claim in owned_claims(result, owners):
            if claim["key"].startswith("pin:"):
                pin = claim["key"][4:]
                pads.setdefault(pin[1], set()).add(int(pin[2:]))
    if h7:
        for item in result.devices:
            binding = result["board_bindings"].get(item.get("cs_binding"))
            if binding is not None:
                for pin in result["routes"][binding["route"]]["pins"]:
                    name = pin["pin"]
                    pads.setdefault(name[1], set()).add(int(name[2:]))
    for port, numbers in sorted(pads.items()):
        mask = sum(1 << number for number in numbers)
        if not h7 and port == "G" and mask & ~3:
            source += _reject(
                "(NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0u || "
                "(NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_IO2SV) == 0u")
        source += [f"    if ((NX_{hook}_READ(&RCC->{gpio_register}) & "
                   f"RCC_{gpio_register}_GPIO{port}EN) != 0u) {{",
                   f"        uint32_t mode = NX_{hook}_READ(&GPIO{port}->MODER);"]
        for number in sorted(numbers):
            bits = f"((mode >> {number * 2}u) & 3u)"
            source += _reject(f"{bits} != 0u && {bits} != 3u", indent=8)
        source += _reject(f"(NX_{hook}_READ(&GPIO{port}->LCKR) & {mask}u) != 0u",
                          indent=8)
        source.append("    }")
    return source


def selected(result):
    """Project exact selected leases; base preflight owns the reserved timer."""
    family = result.soc_family
    if family in {"stm32h743", "stm32u575"}:
        return _modern_selected(result)
    if family not in {"stm32f407", "gd32f470", "stm32g474"}:
        fail("No maintained selected cold preflight: " + family)
    source = []
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    if family == "stm32g474":
        for item in result.controllers:
            if item.kind in {"uart", "spi", "pwm", "i2c"}:
                controller = item.controller
                register = ("APB1ENR2" if controller == "LPUART1" else
                            "APB2ENR" if controller in {"USART1", "SPI1"} else
                            "APB1ENR1")
                source += _reject(
                    f"(NX_G474_READ(&RCC->{register}) & "
                    f"RCC_{register}_{controller}EN) != 0u")
                if item.kind == "pwm":
                    source += _reject(
                        f"(NX_G474_READ(&RCC->APB1RSTR1) & "
                        f"RCC_APB1RSTR1_{controller}RST) != 0u")
                if item.kind == "uart":
                    source += _reject(
                        f"(NX_G474_READ(&RCC->CCIPR) & "
                        f"RCC_CCIPR_{controller}SEL) != 0u")
                if item.kind == "i2c":
                    source += _reject(
                        "(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_I2C1SEL) != 0u")
            if item.kind == "can_fd":
                source += _reject(
                    "(NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_FDCANEN) != 0u || "
                    "(NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_FDCANRST) != 0u || "
                    "(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_FDCANSEL) != 0u")
            if item.kind in {"adc", "pwm_adc", "adc_group"}:
                source += _reject(
                    "(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) != 0u")
                source += _reject(
                    "(NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC12EN) != 0u")
                source += _reject(
                    "(NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC345EN) != 0u")
                if item.mode == "trigger-dma":
                    source += _reject(
                        "(NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM6EN) != 0u")
                if item.kind == "adc_group":
                    source += _reject("!NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn)")
                    if item.options.trigger == "tim6-trgo":
                        source += _reject(
                            "(NX_G474_READ(&RCC->APB1ENR1) & "
                            "RCC_APB1ENR1_TIM6EN) != 0u || "
                            "(NX_G474_READ(&RCC->APB1RSTR1) & "
                            "RCC_APB1RSTR1_TIM6RST) != 0u")
                    if item.mode == "regular-sequence-dma":
                        source += _reject(
                            "!NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn)")
                        source += [
                            "    if ((NX_G474_READ(&RCC->AHB1ENR) & "
                            "RCC_AHB1ENR_DMA1EN) != 0u) {"]
                        source += _reject(
                            "NX_G474_READ(&DMA1_Channel1->CCR) != 0u || "
                            "NX_G474_READ(&DMA1_Channel1->CNDTR) != 0u || "
                            "NX_G474_READ(&DMA1_Channel1->CPAR) != 0u || "
                            "NX_G474_READ(&DMA1_Channel1->CMAR) != 0u",
                            indent=8)
                        source += ["    }"]
                if item.kind == "pwm_adc":
                    source += _reject(
                        "(NX_G474_READ(&RCC->AHB1ENR) & "
                        "(RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN)) != 0u || "
                        "(NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM15EN) != 0u")
        for item in result.controllers:
            if item.kind == "flash":
                source += _reject(
                    "(NX_G474_READ(&FLASH->ECCR) & FLASH_ECCR_ECCIE) != 0u")
            if item.kind == "exti":
                line = int(item.controller.removeprefix("EXTI"))
                source += _reject(
                    "((NX_G474_READ(&EXTI->IMR1) | "
                    "NX_G474_READ(&EXTI->EMR1) | "
                    "NX_G474_READ(&EXTI->RTSR1) | "
                    "NX_G474_READ(&EXTI->FTSR1) | "
                    f"NX_G474_READ(&EXTI->PR1)) & {1 << line}u) != 0u")
    else:
        clocks = set()
        for item in result.controllers:
            if item.kind in {"uart", "spi", "i2c", "pwm", "timer_capture",
                             "advanced_pwm", "can"}:
                clocks.add(_clock(item.controller, family))
            elif item.kind == "adc":
                stm = family == "stm32f407"
                register = "RCC->APB2ENR" if stm else "RCU_APB2EN"
                names = ("ADC1", "ADC2", "ADC3") if stm else (
                    "ADC0", "ADC1", "ADC2")
                prefix = "RCC_APB2ENR_" if stm else "RCU_APB2EN_"
                clocks.add((register, " | ".join(prefix + name + "EN"
                                                for name in names)))
            elif item.kind == "usb_device":
                clocks.add(("RCC->AHB2ENR", "RCC_AHB2ENR_OTGFSEN") if
                           family == "stm32f407" else
                           ("RCU_AHB2EN", "RCU_AHB2EN_USBFSEN"))
            elif item.kind in {"eth", "eth_frames"}:
                clocks.add(("RCC->AHB1ENR", "RCC_AHB1ENR_ETHMACEN | "
                            "RCC_AHB1ENR_ETHMACTXEN | "
                            "RCC_AHB1ENR_ETHMACRXEN") if
                           family == "stm32f407" else
                           ("RCU_AHB1EN", "RCU_AHB1EN_ENETEN | "
                            "RCU_AHB1EN_ENETTXEN | RCU_AHB1EN_ENETRXEN"))
                if item.kind == "eth_frames":
                    clocks.add(("RCC->AHB1ENR", "RCC_AHB1ENR_ETHMACPTPEN") if
                               family == "stm32f407" else
                               ("RCU_AHB1EN", "RCU_AHB1EN_ENETPTPEN"))
            elif item.kind == "exti":
                line = int(item["pins"][0]["pin"][2:])
                registers = ("EXTI->IMR | EXTI->EMR | EXTI->PR" if
                             family == "stm32f407" else
                             "EXTI_INTEN | EXTI_EVEN | EXTI_PD")
                source += _reject(f"(({registers}) & {1 << line}u) != 0u")
        for item in claims:
            timer = item["key"].removeprefix("controller:")
            if re.fullmatch(r"TIM[1349]|TIMER[0238]", timer):
                clocks.add(_clock(timer, family))
        if result.get("can_domain") is not None:
            clocks.add(_clock("CAN1" if family == "stm32f407" else "CAN0",
                              family))
        for register, mask in sorted(clocks):
            source += _reject(f"({register} & ({mask})) != 0u")
    for key in sorted({item["key"] for item in claims
                       if item["key"].startswith("irq:")}):
        vector = key.removeprefix("irq:") + "_IRQn"
        if family == "stm32g474":
            source += _reject(
                f"!NX_G474_IRQ_AVAILABLE((IRQn_Type){vector})")
            continue
        bank = f"(uint32_t){vector} / 32u"
        mask = f"(1u << ((uint32_t){vector} % 32u))"
        source += _reject(f"((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | "
                          f"NVIC->IABR[{bank}]) & {mask}) != 0u")
    for key in sorted({item["key"] for item in claims
                       if item["key"].startswith("dma:")}):
        match = re.fullmatch(r"dma:(DMA[0-9]+):stream([0-7])", key)
        if match is None:
            fail("No maintained selected DMA lane admission: " + key)
        dma, lane = match.groups()
        if family == "stm32g474":
            source += _reject(
                f"(NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u && "
                f"NX_G474_READ(&{dma}_Channel{lane}->CCR) != 0u")
            source += _reject(
                "(NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMAMUX1EN) != 0u && "
                "NX_G474_READ(&DMAMUX1_Channel0->CCR) != 0u")
            continue
        control = (f"{dma}_Stream{lane}->CR" if family == "stm32f407" else
                   f"DMA_CHCTL({dma}, DMA_CH{lane})")
        source += _reject(f"({control} & 1u) != 0u")
    return source + gpio_phase.preflight(result)


def emit(result):
    """Finish base and selected rejection before the system transaction."""
    calls = {
        "stm32f407": "nx_stm32_system_preflight(&g_nx_stm32_system)",
        "gd32f470": "nx_gd32_soc_preflight()",
        "stm32g474": "nx_stm32g474_system_preflight(&g_nx_stm32g474_system)",
        "stm32h743": "nx_stm32h743_system_preflight(&g_nx_stm32h743_system)",
        "stm32u575": "nx_stm32u575_system_preflight(&g_nx_stm32u575_system)",
    }
    call = calls.get(result.soc_family)
    if call is None:
        fail("No maintained base cold preflight: " + result.soc_family)
    return [f"    result.primary = {call};",
            "    if (result.primary != NX_SUCCESS) { return result; }",
            *selected(result)]
