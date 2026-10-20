#!/usr/bin/env python3
"""Fixed STM32U575 inline LL source/CMSIS compatibility profile."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.maintenance.stm32_ll_lock import PROFILES  # noqa: E402

PROFILE = PROFILES["u5"]
DIRECTORY = PROFILE.directory
CMSIS_DIRECTORY = PROFILE.cmsis_directory
LOCK_NAME = PROFILE.lock_name
contract = PROFILE.contract
verify = PROFILE.verify

if __name__ == "__main__":
    raise SystemExit(PROFILE.main())
