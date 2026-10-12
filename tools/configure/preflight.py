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
    for item in result.controllers:
        if item.kind not in {"gpio", "uart"}:
            fail("New SoC cold profile has no maintained controller admission")
        if item.kind == "uart":
            if item.controller != uart:
                fail("New SoC cold profile requires its exact reviewed UART")
            source += _reject(f"(NX_{hook}_READ(&RCC->{gate_register}) & "
                              f"RCC_{gate_register}_{uart}EN) != 0u")
            source += _reject(f"(NX_{hook}_READ(&RCC->{mux_register}) & {mux}) != 0u")
            source += _reject(f"!NX_{hook}_IRQ_AVAILABLE((IRQn_Type){uart}_IRQn)")
        for pin in item["pins"]:
            name = pin["pin"]
            pads.setdefault(name[1], set()).add(int(name[2:]))
    for port, numbers in sorted(pads.items()):
        mask = sum(1 << number for number in numbers)
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
        if any(item.kind == "uart" for item in result.controllers):
            source += _reject("(NX_G474_READ(&RCC->APB1ENR2) & "
                              "RCC_APB1ENR2_LPUART1EN) != 0u")
        for item in result.controllers:
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
            elif item.kind == "eth":
                clocks.add(("RCC->AHB1ENR", "RCC_AHB1ENR_ETHMACEN | "
                            "RCC_AHB1ENR_ETHMACTXEN | "
                            "RCC_AHB1ENR_ETHMACRXEN") if
                           family == "stm32f407" else
                           ("RCU_AHB1EN", "RCU_AHB1EN_ENETEN | "
                            "RCU_AHB1EN_ENETTXEN | RCU_AHB1EN_ENETRXEN"))
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
