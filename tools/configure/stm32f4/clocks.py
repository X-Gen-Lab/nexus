"""Selected STM32F4 RCC calculations; no runtime clock policy."""
from collections.abc import Mapping

from tools.configure import clocks, usb_otg
from tools.configure.providers.common import fail, integer


DEFAULT = dict(source="hse", input_hz=8000000, pll_m=8, pll_n=336,
               pll_p=2, pll_q=7, ahb_div=1, apb1_div=4, apb2_div=2,
               adc_div=4, supply_mv=3300, css=False)
AHB = {1: 0, 2: 8, 4: 9, 8: 10, 16: 11, 64: 12, 128: 13,
       256: 14, 512: 15}
APB = {1: 0, 2: 4, 4: 5, 8: 6, 16: 7}
SDIO375_PROFILE = "hse8-pll150-sdio375"
SDIO375 = {**DEFAULT, "pll_n": 300, "pll_q": 4}

USB_CLOCKS = {
    ("usb_device", "device-fs"): ("OTG_FS", True, 14200000),
    ("usb_host", "host-fs"): ("OTG_FS", True, 14200000),
    ("usb_device", "device-hs-fs"): ("OTG_HS", True, 30000000),
    ("usb_host", "host-hs-fs"): ("OTG_HS", True, 30000000),
    ("usb_device", "device-hs-ulpi"): ("OTG_HS", False, 30000000),
    ("usb_host", "host-hs-ulpi"): ("OTG_HS", False, 30000000),
}


def _divide(numerator, denominator, label):
    result, remainder = divmod(numerator, denominator)
    if remainder or result == 0:
        fail("F407 clock must have exact integer Hz: " + label)
    return result


def _parameters(name, profile):
    if name == SDIO375_PROFILE:
        expected = dict(source="hse", input_hz=8000000, core_hz=150000000,
                        apb1_hz=37500000, apb2_hz=75000000)
        if clocks.normalize(profile) != expected:
            fail("F407 SDIO 37.5 MHz requires its exact reviewed HSE8 tuple")
        return dict(SDIO375)
    if name == "hse8-pll168":
        expected = dict(source="hse", input_hz=8000000, core_hz=168000000,
                        apb1_hz=42000000, apb2_hz=84000000)
        if clocks.normalize(profile) != expected:
            fail("F407 fixed clock profile differs from its implementation")
        return dict(DEFAULT)
    if name != "custom" or not isinstance(profile, Mapping):
        fail("F407 clock requires a reviewed profile or explicit custom tuple")
    optional = {"sysclk_source", "pll48", "pll_sai"}
    if set(DEFAULT) - set(profile) or set(profile) - set(DEFAULT) - optional:
        fail("F407 clock parameters require one exact oscillator/PLL tuple")
    result = dict(profile)
    if result["source"] not in {"hse", "hsi"}:
        fail("F407 clock source must be hse or hsi")
    integer(result["input_hz"], 4000000, 26000000, "F407 oscillator Hz")
    if result["source"] == "hsi" and result["input_hz"] != 16000000:
        fail("F407 HSI nominal source is exactly 16 MHz")
    for key, low, high in (("pll_m", 2, 63), ("pll_n", 192, 432),
                           ("pll_q", 2, 15), ("supply_mv", 2700, 3600)):
        integer(result[key], low, high, "F407 " + key)
    for key, values in (("pll_p", (2, 4, 6, 8)), ("ahb_div", AHB),
                        ("apb1_div", APB), ("apb2_div", APB),
                        ("adc_div", (2, 4, 6, 8))):
        if type(result[key]) is not int or result[key] not in values:
            fail("F407 unsupported divider: " + key)
    if type(result["css"]) is not bool or (result["css"] and
                                             result["source"] != "hse"):
        fail("F407 CSS requires an explicit external HSE source")
    return result


def _pll_sai_output(source, m, n, output, divider, allowed_post):
    """One selected output has no authored frequency or second PLL source."""
    if (not isinstance(output, Mapping) or
            set(output) != {divider, "post_divider"}):
        fail("PLLSAI output requires exact divider/post_divider fields")
    value = integer(output[divider], 2, 15 if divider == "q" else 7,
                    "PLLSAI " + divider)
    post = output["post_divider"]
    if type(post) is not int or post not in allowed_post:
        fail("PLLSAI output has an unsupported post_divider")
    if source * n > 216000000 * m * value:
        fail("PLLSAI output exceeds 216 MHz")
    hz = _divide(source * n, m * value * post, "PLLSAI " + divider + " output")
    label = "kernel_hz" if divider == "q" else "pixel_hz"
    return {**output, label: hz}


def _pll_sai(source, parameters):
    """Resolve a single System PLL with independently selected Q/R outputs.

    These are common encoded limits. The selected DeviceView additionally
    binds the exact device's VCO minimum, output existence and shared versus
    dedicated M. Neither this tuple nor this leaf grants a device/mode review.
    """
    if (not isinstance(parameters, Mapping) or
            not {"m", "n"}.issubset(parameters) or
            set(parameters) - {"m", "n", "sai1", "ltdc"} or
            not set(parameters).intersection({"sai1", "ltdc"})):
        fail("PLLSAI requires m/n and at least one selected output")
    m = integer(parameters["m"], 2, 63, "PLLSAI m")
    n = integer(parameters["n"], 50, 432, "PLLSAI n")
    if not 1000000 * m <= source <= 2000000 * m:
        fail("PLLSAI nominal input must be in 1..2 MHz")
    if not 100000000 * m <= source * n <= 432000000 * m:
        fail("PLLSAI VCO must be in 100..432 MHz before device limits")
    q = (_pll_sai_output(source, m, n, parameters["sai1"], "q", range(1, 33))
         if "sai1" in parameters else None)
    r = (_pll_sai_output(source, m, n, parameters["ltdc"], "r", (2, 4, 8, 16))
         if "ltdc" in parameters else None)
    return dict(m=m, n=n, sai1=q, ltdc=r)


def plan(profile_name, profile):
    """Calculate all consumer domains once, rejecting fractional frequencies.

    Voltage range is deliberately limited to the reviewed 2.7--3.6 V Flash
    table. Frequencies are nominal, never oscillator accuracy or HIL bounds.
    PLLI2S shares the selected source and M; its N/R belong to the audio owner.
    """
    p = _parameters(profile_name, profile)
    p.setdefault("sysclk_source", "pll")
    p.setdefault("pll48", False)
    if (type(p["sysclk_source"]) is not str or
            p["sysclk_source"] not in {"pll", "input"}):
        fail("F407 SYSCLK source must be pll or input")
    if type(p["pll48"]) is not bool:
        fail("F407 explicit PLL48 ownership must be boolean")
    p["pll_enabled"] = p["sysclk_source"] == "pll" or p["pll48"]
    source, m, n = p["input_hz"], p["pll_m"], p["pll_n"]
    auxiliary = (_pll_sai(source, p.pop("pll_sai"))
                 if "pll_sai" in p else None)
    if not 1000000 * m <= source <= 2000000 * m:
        fail("F407 PLL input must be in 1..2 MHz")
    if not 192000000 * m <= source * n <= 432000000 * m:
        fail("F407 PLL VCO must be in 192..432 MHz")
    pll_sysclk = _divide(source * n, m * p["pll_p"], "PLL P")
    sysclk = pll_sysclk if p["sysclk_source"] == "pll" else source
    core = _divide(sysclk, p["ahb_div"], "HCLK")
    apb1 = _divide(core, p["apb1_div"], "PCLK1")
    apb2 = _divide(core, p["apb2_div"], "PCLK2")
    adc = _divide(apb2, p["adc_div"], "ADC")
    if (pll_sysclk > 168000000 or core > 168000000 or
            apb1 > 50000000 or apb2 > 100000000):
        fail("STM32F4 clock exceeds the reviewed family calculation limits")
    if adc > 36000000:
        fail("F407 ADC clock exceeds 36 MHz")
    tim1 = apb1 * (1 if p["apb1_div"] == 1 else 2)
    tim2 = apb2 * (1 if p["apb2_div"] == 1 else 2)
    if tim1 % 1000000:
        fail("F407 TIM2 timebase requires an exact 1 MHz prescaler")
    pll48 = _divide(source * n, m * p["pll_q"], "PLL48")
    if p["pll48"] and pll48 != 48000000:
        fail("F407 explicit PLL48 owner requires exactly 48 MHz")
    if not p["pll_enabled"]:
        pll48 = 0
    p["sw"] = (2 if p["sysclk_source"] == "pll" else
               1 if p["source"] == "hse" else 0)
    p.update(flash_wait_states=(core - 1) // 30000000,
             pllcfgr=(m | n << 6 | (p["pll_p"] // 2 - 1) << 16 |
                      (1 << 22 if p["source"] == "hse" else 0) |
                      p["pll_q"] << 24),
             cfgr=(AHB[p["ahb_div"]] << 4 | APB[p["apb1_div"]] << 10 |
                   APB[p["apb2_div"]] << 13))
    domains = dict(sysclk=sysclk, core=core, apb1=apb1, apb2=apb2,
                   pll_sysclk=pll_sysclk if p["pll_enabled"] else 0,
                   apb1_timer=tim1, apb2_timer=tim2, adc=adc, pll48=pll48)
    domain = lambda label, hz: dict(source=label, nominal_hz=hz,
                                   minimum_hz=0, maximum_hz=0)
    return dict(profile=profile_name, input=domain(p["source"], source),
                domains={key: domain(key, hz) for key, hz in domains.items()},
                parameters=p, pll_sai=auxiliary,
                reviewed_revision_ids=(),
                provenance=("Selected STM32F4 static RCC plan; exact document/cut "
                            "scope is bound by the selected device review"),
                physical_qualified=False)


def normalize(profile):
    """Keep the generic input projection free of duplicated PLL choices."""
    if "pll_m" not in profile:
        return clocks.normalize(profile)
    selected = plan("custom", profile)
    return projection(selected)


def projection(selected):
    domains = selected["domains"]
    return dict(source=selected["input"]["source"],
                input_hz=selected["input"]["nominal_hz"],
                **{key + "_hz": domains[key]["nominal_hz"]
                   for key in ("core", "apb1", "apb2")})


def controller_hz(result, controller):
    """Derive a selected peripheral clock from the one resolved plan."""
    if controller in {"USART1", "USART6", "SPI1", "ADC1", "ADC2", "ADC3",
                      "SAI1", "SAI2"}:
        return result.clock["apb2_hz"]
    if controller.startswith("TIM"):
        return result.clock_plan.domains[timer_domain(controller)].nominal_hz
    if (controller in {"ETH", "CRC", "FSMC", "FMC", "DCMI"} or
            controller.startswith("GPIO")):
        return result.clock["core_hz"]
    if controller == "OTG_HS":
        item = next(item for item in result.controllers
                    if item.controller == controller)
        if usb_otg.device_mode(item.mode) in {"host-hs-ulpi", "device-hs-ulpi"}:
            return item.options.ulpi_clock_hz
        return result.clock_plan.domains["pll48"].nominal_hz
    if controller in {"OTG_FS", "SDIO", "RNG"}:
        return result.clock_plan.domains["pll48"].nominal_hz
    if controller in {"USART2", "USART3", "UART4", "UART5", "SPI2", "SPI3",
                       "I2C1", "I2C2", "I2C3", "CAN1", "CAN2", "DAC", "WWDG"}:
        return result.clock["apb1_hz"]
    return None


def timer_domain(controller):
    """Select a timer's actual bus domain without duplicating its frequency."""
    bus = 2 if controller in {"TIM1", "TIM8", "TIM9", "TIM10", "TIM11"} else 1
    return f"apb{bus}_timer"


def header_lines(result):
    """Emit constants, never hardware algorithms or a second runtime owner."""
    selected = result.clock_plan
    p = selected.parameters
    auxiliary = selected.pll_sai
    lines = ["#define NEXUS_CLOCK_SDIO_HS375 " +
             str(int(selected.profile == SDIO375_PROFILE)) + "u"]
    for protocol, macro in (("sd-memory", "SD"), ("mmc", "MMC")):
        enabled = any(item.kind == "block" and
                      item.options.protocol == protocol and
                      item.options.speed == "high-speed"
                      for item in result.controllers)
        lines.append(f"#define NEXUS_STM32_SDIO_{macro}_HS {int(enabled)}u")
    lines += [f"#define NEXUS_{key.upper()}_HZ {value.nominal_hz}u"
             for key, value in selected.domains.items() if key != "core"]
    for field in ("pll_m", "pll_n", "pll_p", "pll_q"):
        macro = field.replace('_', '').upper()
        lines.append(f"#define NEXUS_CLOCK_{macro} {p[field]}u")
    for key in ("pllcfgr", "cfgr", "ahb_div", "adc_div", "css", "sw",
                "pll_enabled"):
        lines.append(f"#define NEXUS_CLOCK_{key.upper()} {int(p[key])}u")
    lines.append(f"#define NEXUS_CLOCK_PLLSAI_ENABLED {int(auxiliary is not None)}u")
    for name in ("sai1", "ltdc"):
        enabled = auxiliary is not None and getattr(auxiliary, name) is not None
        lines.append(f"#define NEXUS_CLOCK_PLLSAI_{name.upper()}_ENABLED {int(enabled)}u")
    sai2_master = any(item.kind == "audio" and item.controller == "SAI2" and
                      item.mode.startswith("master-")
                      for item in result.controllers)
    lines.append("#define NEXUS_CLOCK_PLLSAI_SAI2_ENABLED " +
                 str(int(sai2_master)) + "u")
    if auxiliary is not None:
        for macro, value in (("M", auxiliary.m), ("N", auxiliary.n)):
            lines.append(f"#define NEXUS_CLOCK_PLLSAI_{macro} {value}u")
        if auxiliary.sai1 is not None:
            q = auxiliary.sai1
            for macro, value in (("Q", q.q), ("DIVQ", q.post_divider),
                                 ("SAI1_HZ", q.kernel_hz)):
                lines.append(f"#define NEXUS_CLOCK_PLLSAI_{macro} {value}u")
        if auxiliary.ltdc is not None:
            r = auxiliary.ltdc
            for macro, value in (("R", r.r), ("DIVR", r.post_divider),
                                 ("LTDC_HZ", r.pixel_hz)):
                lines.append(f"#define NEXUS_CLOCK_PLLSAI_{macro} {value}u")
    lines += [f"#define NEXUS_FLASH_WAIT_STATES {p['flash_wait_states']}u",
              "#ifndef NEXUS_CLOCK_SOURCE_HSE",
              f"#define NEXUS_CLOCK_SOURCE_HSE {int(p['source'] == 'hse')}u",
              "#endif"]
    return lines


def _sdio_divider(selected, width, minimum_divisor):
    """Require a legal even wire divider from the selected static domains."""
    kernel = selected["domains"]["pll48"]["nominal_hz"]
    peripheral = selected["domains"]["apb2"]["nominal_hz"]
    sdio375 = selected["profile"] == SDIO375_PROFILE
    if (kernel != (75000000 if sdio375 else 48000000) or
            peripheral <= 0 or type(width) is not int or
            width not in {1, 4, 8}):
        fail("F407 SDIO requires exact PLL48, PCLK2 and bus width")
    # RM0090 31.2 is non-strict; ES0182 2.4.6 must be strictly satisfied.
    bus_minimum = (3 * kernel + 8 * peripheral - 1) // (8 * peripheral)
    fifo_minimum = 3 * width * (kernel + peripheral) // (32 * peripheral) + 1
    divisor = max(2 if sdio375 else 4, minimum_divisor,
                  bus_minimum, fifo_minimum)
    divisor = (divisor + 1) & ~1
    if divisor > 256:
        fail("F407 RM0090/ES0182 SDIO has no legal divider for PCLK2")
    return divisor - 2


def validate_consumers(selected, controllers):
    """Reject impossible selected consumers before generating an image."""
    domain = lambda name: selected["domains"][name]["nominal_hz"]
    auxiliary = selected.get("pll_sai")
    sai_master = any(item.kind == "audio" and item.controller in {"SAI1", "SAI2"} and
                     item.mode.startswith("master-") for item in controllers)
    ltdc = any(item.kind == "display" and item.controller == "LTDC" and
               item.mode == "rgb-scanout" for item in controllers)
    if auxiliary is not None and auxiliary["sai1"] is not None and not sai_master:
        fail("PLLSAI Q requires its explicitly selected SAI1 master or SAI2 master consumer")
    if sai_master and (auxiliary is None or auxiliary["sai1"] is None):
        fail("SAI1 master or SAI2 master requires the one declared System PLLSAI Q plan")
    if auxiliary is not None and auxiliary["ltdc"] is not None and not ltdc:
        fail("PLLSAI R requires its explicitly selected LTDC consumer")
    if ltdc and (auxiliary is None or auxiliary["ltdc"] is None):
        fail("LTDC requires the one declared System PLLSAI R plan")
    sdio375 = selected["profile"] == SDIO375_PROFILE
    if sdio375 and not any(item.kind == "block" for item in controllers):
        fail("F407 SDIO 37.5 MHz profile requires its selected memory host")
    for item in controllers:
        if sdio375 and item.kind in {"usb_device", "usb_host", "entropy"}:
            fail("F407 SDIO 75 MHz workaround forbids every USB and RNG owner")
        if item.kind in {"eth", "eth_frames"} and domain("core") < 25000000:
            fail("F407 Ethernet requires an AHB clock of at least 25 MHz")
        if item.kind == "entropy" and domain("pll48") != 48000000:
            fail("F407 RNG requires selected nominal PLL48=48MHz")
        kernel48 = item.kind == "block" and not sdio375
        if item.kind in {"usb_device", "usb_host"}:
            policy = USB_CLOCKS.get((item.kind, usb_otg.device_mode(item.mode)))
            if policy is None or item.controller != policy[0]:
                fail("F407 USB clock requires its exact engine/PHY mode")
            if domain("core") <= policy[2]:
                fail("F407 USB AHB frequency is below its engine limit")
            kernel48 = policy[1]
            if kernel48 and selected["input"]["source"] != "hse":
                fail("F407 internal USB PHY requires an external HSE crystal")
        if kernel48 and domain("pll48") != 48000000:
            fail("F407 selected USB/SDIO requires exact nominal PLL48")
        if item.kind == "block":
            high_speed = getattr(item.options, "speed", "default") == "high-speed"
            if high_speed != sdio375:
                fail("SDIO high-speed requires its exact F407 37.5 MHz profile")
            if sdio375 and (item.options.bus_width != 4 or
                    item.options.protocol not in {"sd-memory", "mmc"}):
                fail("F407 SDIO high-speed admits only four-bit SDR memory")
            _sdio_divider(selected, item.options.bus_width, 2 if sdio375 else 4)
            _sdio_divider(selected, 1, domain("pll48") // 400000 + 1)
        if (item.kind in {"adc", "adc_group", "pwm_adc"} and
                domain("adc") < 600000):
            fail("F407 selected ADC clock must be at least 0.6 MHz")
        apb = (2 if item.controller in {"USART1", "USART6", "SPI1"} else 1)
        hz = domain(f"apb{apb}")
        if item.kind == "uart":
            divisor = (hz + item.options.baud // 2) // item.options.baud
            if not 16 <= divisor <= 65535:
                fail("F407 UART baud has no admitted selected APB divider")
        elif item.kind in {"i2c", "i2c_target"} and (
                hz % 1000000 or hz < (4000000
                    if item.options.max_hz > 100000 else 2000000)):
            fail("F407 I2C requires an integer 2..42 MHz selected APB clock")
        elif item.kind in {"pwm", "timer_capture", "advanced_pwm"}:
            hz = domain(timer_domain(item.controller))
            if (hz % item.options.tick_hz or
                    not 1 <= hz // item.options.tick_hz <= 65536):
                fail("F407 timer tick must divide its selected bus clock")
        elif item.kind == "can":
            timing = item.options.timing
            if (item.options.bitrate * timing.prescaler *
                    (1 + timing.segment1 + timing.segment2) != hz):
                fail("F407 CAN bitrate differs from the selected APB clock")
