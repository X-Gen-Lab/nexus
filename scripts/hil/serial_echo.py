#!/usr/bin/env python3
"""Observe an actual UART echo challenge; default mode never opens a port.

This observation alone does not prove a board/image identity. A reviewed lab
adapter must associate it with physical identification and Flash readback.
"""
from __future__ import annotations
import argparse
import secrets
import sys
import time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import EvidenceError,atomic_json

def observe(port: str,baudrate: int,timeout_s: float,*,execute: bool=False,
            serial_factory=None,model: bool=False) -> dict:
    report={'schema_version':1,'kind':'uart_model' if model else 'uart_observation',
            'hardware_verified':False,'status':'fail','port':port,'baudrate':baudrate}
    try:
        if not isinstance(port,str) or not port or type(baudrate) is not int or baudrate<=0:
            raise EvidenceError('serial path and positive baudrate required')
        if type(timeout_s) not in (int,float) or not 0<timeout_s<=30:
            raise EvidenceError('UART timeout must be in (0,30] seconds')
        if serial_factory is not None and not model:
            raise EvidenceError('injected serial transports require model mode')
        if model and serial_factory is None:
            raise EvidenceError('model mode requires an injected serial transport')
        if not execute and not model:
            report.update(status='ready',dry_run=True)
            return report
        if serial_factory is None:
            try:
                import serial
            except ImportError as error:
                raise EvidenceError('pyserial is unavailable; install the reviewed lab dependency') from error
            serial_factory=serial.Serial
        challenge=b'NEXUS-HIL:'+secrets.token_hex(12).encode()+b'\r\n'
        received=bytearray()
        deadline=time.monotonic()+timeout_s
        kwargs={'port':port,'baudrate':baudrate,'bytesize':8,'parity':'N',
                'stopbits':1,'timeout':0.05,'write_timeout':min(timeout_s,1)}
        if sys.platform!='win32': kwargs['exclusive']=True
        with serial_factory(**kwargs) as stream:
            stream.reset_input_buffer()
            sent=0
            while sent<len(challenge):
                if time.monotonic()>=deadline: raise EvidenceError('UART challenge write deadline expired')
                n=stream.write(challenge[sent:])
                if type(n) is not int or n<=0: raise EvidenceError('UART write did not progress')
                sent+=n
            while challenge not in received:
                if time.monotonic()>=deadline: raise EvidenceError('UART echo challenge timed out')
                received.extend(stream.read(256))
                if len(received)>8192: raise EvidenceError('UART observation exceeds bounded transcript')
        report.update(status='pass',hardware_verified=not model,
                      challenge_hex=challenge.hex(),rx_hex=bytes(received).hex(),
                      tests=[{'id':'uart_echo','status':'pass'}])
    except (EvidenceError,OSError,ValueError) as error:
        report['failure']=str(error)
    return report

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--baudrate',type=int,default=115200)
    parser.add_argument('--timeout-s',type=float,default=5)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    result=observe(args.port,args.baudrate,args.timeout_s,execute=args.execute)
    atomic_json(args.report,result)
    print(f"UART {result['status']}; hardware_verified={result['hardware_verified']}")
    return 0 if result['status'] in ('pass','ready') else 1
if __name__=='__main__': raise SystemExit(main())
