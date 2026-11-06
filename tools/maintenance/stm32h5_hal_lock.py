#!/usr/bin/env python3
"""Fixed CubeH5 paired HAL/LL/I3C compile-reference source closure."""
from pathlib import Path
import sys

if __name__ == '__main__':
    sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.maintenance.stm32_ll_lock import Profile  # noqa: E402

PROFILE = Profile(
    'h5', 'stm32h563', '56e28dbebd720e3df2866dfaf438b525e9e8af04',
    '884b8dc78e41cbfca008363342b17f4a9e8641f7', 'v1.7.0',
    '4e039436d8561003a1d88b78a1520f3f81eadd24',
    ('i3c', 'bus', 'rcc', 'gpio', 'pwr', 'system'),
    extra_files=('Inc/stm32h5xx_hal.h', 'Inc/stm32h5xx_hal_def.h',
                 'Inc/stm32h5xx_hal_i3c.h',
                 'Inc/Legacy/stm32_hal_legacy.h',
                 'Src/stm32h5xx_hal_i3c.c'),
    api_scope='hal-ll-i3c-compile-reference')
DIRECTORY = PROFILE.directory
CMSIS_DIRECTORY = PROFILE.cmsis_directory
LOCK_NAME = PROFILE.lock_name
contract = PROFILE.contract
verify = PROFILE.verify

if __name__ == '__main__':
    raise SystemExit(PROFILE.main())
