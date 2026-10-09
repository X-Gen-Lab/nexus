#!/usr/bin/env python3
"""Use the maintained explicit-preset prerequisite check."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).with_name('setup.py')), run_name='__main__')
