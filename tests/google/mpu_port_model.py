#!/usr/bin/env python3
"""Compile exact reviewed port bodies with only register/ISA boundaries modeled.

The source authority is the production hash-checked derivation. No scheduling,
SVC admission or MPU permission algorithm is reproduced in this test harness.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "nexus_mpu_port", ROOT / "os/freertos/mpu/prepare_port.py")
PORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PORT)


def function(text: str, name: str) -> str:
    matches = list(re.finditer(
        r"^(?:static )?(?:void|uint32_t) " + name +
        r"\([^\n]+\)(?: /\*[^\n]*\*/)?\n\{\n.*?^\}\n",
        text, re.MULTILINE | re.DOTALL))
    if len(matches) != 1:
        raise ValueError("reviewed model function is not unique: " + name)
    return matches[0].group()


def generate(source: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        derived = Path(temporary) / "port.c"
        PORT.prepare(source, derived)
        text = derived.read_text()
    version7 = PORT.port_name(source) in {"ARM_CM3_MPU", "ARM_CM4_MPU"}
    dispatcher = "vSVCHandler_C" if version7 else "vPortSVCHandler_C"
    body = function(text, dispatcher)
    body = PORT.unique_replace(body, "( ( uint8_t * ) ulPC )[ -2 ]",
                               "nx_mpu_model_opcode(ulPC)")
    if version7:
        body = PORT.unique_replace(body, '__asm volatile ( "dsb" ::: "memory" );',
                                   "nx_mpu_model_barrier();")
        body = PORT.unique_replace(body, '__asm volatile ( "isb" );',
                                   "nx_mpu_model_barrier();")
    prefix = text[:text.index('#include "FreeRTOS.h"')]
    macros = []
    for name in (("portOFFSET_TO_PC",) + ((
                 "portEXPECTED_MPU_TYPE_VALUE", "portMPU_ENABLE",
                 "portMPU_BACKGROUND_ENABLE", "portMPU_REGION_VALID",
                 "portMPU_REGION_ENABLE", "portNVIC_MEM_FAULT_ENABLE") if version7 else ())):
        matches = re.findall(r"^#define " + name + r"\s+[^\n]+", text, re.MULTILINE)
        if len(matches) != 1:
            raise ValueError("reviewed model constant is not unique: " + name)
        macros.append(matches[0])
    model = prefix + '''
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "task.h"
#include "guard.h"
#include "mpu_port_model.h"
nx_freertos_mpu_start_t xNexusMpuStart;
void prvRestoreContextOfFirstTask(void);
void vRestoreContextOfFirstTask(void);
void vPortYield(void);
''' + "\n".join(macros) + '''
#undef portNVIC_INT_CTRL_REG
#define portNVIC_INT_CTRL_REG (*nx_mpu_model_register(NX_MPU_REG_PENDSV))
#define portMPU_TYPE_REG (*nx_mpu_model_register(NX_MPU_REG_TYPE))
#define portMPU_REGION_BASE_ADDRESS_REG (*nx_mpu_model_register(NX_MPU_REG_BASE))
#define portMPU_REGION_ATTRIBUTE_REG (*nx_mpu_model_register(NX_MPU_REG_ATTRIBUTES))
#define portMPU_CTRL_REG (*nx_mpu_model_register(NX_MPU_REG_CONTROL))
#define portNVIC_SYS_CTRL_STATE_REG (*nx_mpu_model_register(NX_MPU_REG_FAULT))
''' + body[:body.index("\n{")].split(" /*")[0] + ";\n" + body
    if version7:
        setup = function(text, "prvSetupMPU")
        setup = re.sub(r"\( uint32_t \) (__\w+__)",
                       r"nx_mpu_model_link_address(\1)", setup)
        model += (function(text, "prvGetMPURegionSizeSetting") + setup +
                  "void nx_mpu_model_setup(void) { prvSetupMPU(); }\n")
    model += ("void nx_mpu_model_dispatch(uint32_t *frame, uint32_t return_state) { " +
              dispatcher + "(frame, return_state); }\n")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(model)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    generate(args.source, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
