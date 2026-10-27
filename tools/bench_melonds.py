"""Emulator timing of the CPU-vs-CPU benchmark ROM (build_nds.py --autoplay).

Runs build/nds/ssb-versus-auto.nds in melonDS with its GDB stub enabled for
the run, waits, then reads bench_report from ARM9 memory with devkitARM gdb.
melonDS timing is an approximation of the DS (no claim of hardware frame
time); use it to compare builds, and confirm on a real DS.

    python tools/bench_melonds.py --seconds 60
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
CONFIG=Path.home()/'Library/Preferences/melonDS/melonDS.toml'
MELON=Path.home()/'Applications/melonDS.app/Contents/MacOS/melonDS'

def set_gdb(text,enabled):
    return re.sub(r'(\[Instance0\.Gdb\]\nEnabled = )(true|false)',lambda m:m[1]+('true' if enabled else 'false'),text)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--seconds',type=int,default=60)
    parser.add_argument('--gdb',default='/opt/devkitpro/devkitARM/bin/arm-none-eabi-gdb')
    args=parser.parse_args()
    elf=ROOT/'build/nds/ssb-versus-auto.elf';nds=elf.with_suffix('.nds')
    original=CONFIG.read_text()
    if '[Instance0.Gdb]\nEnabled =' not in original:raise SystemExit('melonDS config has no Gdb section')
    CONFIG.write_text(set_gdb(original,True))
    try:
        emulator=subprocess.Popen([str(MELON),str(nds)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        time.sleep(args.seconds)
        out=subprocess.run([args.gdb,'-q','-batch','-nx',str(elf),'-ex','target remote localhost:3333',
                            '-ex','p/u bench_report','-ex','detach'],capture_output=True,text=True,timeout=60).stdout
        emulator.kill()
    finally:
        CONFIG.write_text(original)
    found=re.search(r'\$1 = \{(.*)\}',out)
    if not found:raise SystemExit('No bench_report read:\n'+out)
    values=[]
    for item in found[1].split(','):  # gdb compresses runs: "0 <repeats 33 times>"
        rep=re.fullmatch(r'\s*(\d+) <repeats (\d+) times>\s*',item)
        values+=[int(rep[1])]*int(rep[2]) if rep else [int(item)]
    count=min(values[0],16)
    windows=[dict(avg_us=values[2+i*3],peak_us=values[3+i*3],sim_peak_us=values[4+i*3]) for i in range(count)]
    report=dict(rom=nds.name,seconds=args.seconds,windows=count,over_16_7ms=values[1],
                avg_us=round(sum(w['avg_us'] for w in windows)/max(1,count)),
                peak_us=max((w['peak_us'] for w in windows),default=0),
                sim_peak_us=max((w['sim_peak_us'] for w in windows),default=0),
                last_window=dict(stage_avg_us=values[50],fighters_avg_us=values[51],hud_avg_us=values[52],hud_peak_us=values[53]),
                per_window=windows,
                note='melonDS emulated timing, not DS hardware')
    (ROOT/'build/nds/bench-melonds.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!='per_window'},indent=2))

if __name__=='__main__':main()
