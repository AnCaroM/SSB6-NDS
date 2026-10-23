"""Convert original US sound effects/voices from the user's ROM for the 1v1.

ROM B1_sounds2 (VADPCM, 44.1 kHz) is decoded with the pinned reference
decoder. Each gmFGMVoiceID is followed through fgm.ucd -> fgm.tbl ->
sound N. Offline approximation: the first note of every reached articulation
(including fork_voice layers) is resampled with its pitch in cents and mixed;
the articulation volume envelopes, LFOs and repeated-note timing are not
reproduced. Output: signed PCM8 mono, 16 kHz, in build/audio (never published).
"""
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import wave
import numpy as np
from prepare_rom import EXPECTED_SHA1
from mario_moves import us_source
from fight_sounds import SOUNDS

ROOT=Path(__file__).resolve().parents[1]
UPSTREAM=ROOT/'upstream/ssb-decomp-re'
REVISION='572a747d8171137718fad472c5d7473dc7309ffd'
sys.path.insert(0,str(UPSTREAM/'tools'))
import audio_codec  # noqa: E402  pinned reference VADPCM decoder
import decode_ctl  # noqa: E402
import extract_fgm  # noqa: E402

RATE=16000
SOURCE_RATE=44100
CTL=(0xC6B650,0xC7B1F0);TBL=(0xC7B1F0,0xF573D0)
UCD=(0xF5A9C0,0xF5F4E0);ATL=(0xF57BF0,0xF5A9C0)

def voice_names():
    text=us_source((UPSTREAM/'src/gm/gmsound.h').read_text())
    body=re.search(r'typedef enum\s+gmFGMVoiceID\s*\{(.*?)\}\s*gmFGMVoiceID',text,re.S)[1]
    body=re.sub(r'/\*.*?\*/','',re.sub(r'//[^\n]*','',body),flags=re.S)
    names=[x.split('=')[0].strip() for x in body.split(',')]
    return [x for x in names if x]

def waves(rom):
    ctl=rom[CTL[0]:CTL[1]];tbl=rom[TBL[0]:TBL[1]]
    structs=decode_ctl.walk(ctl);by={s['offset']:s for s in structs}
    inst=[s for s in structs if s['kind']=='ALInstrument'][0]
    ordered=sorted((s for s in structs if s['kind']=='ALWaveTable'),key=lambda s:s['base'])
    def decode(sound):
        w=by[by[inst['soundArray_offs'][sound]]['wavetable_off']]
        if w['type']!=0:raise ValueError('Expected VADPCM wave')
        book=by[w['book_off']];data=tbl[w['base']:w['base']+w['length']//9*9]
        pcm=audio_codec.adpcm_decode(data,book['entries'],book['order'],book['npredictors'])
        return np.array(pcm,dtype=np.float32)/32768
    return decode,len(inst['soundArray_offs']),{w['offset']:i for i,w in enumerate(ordered)}

def layers(ucd,tbl,index,seen=None):
    """(sound, cents, volume) for the first note of each articulation reached."""
    seen=set() if seen is None else seen
    if index in seen:return []
    seen.add(index);art=None;volume=127;done=set();out=[]
    for op in ucd[index]['program']:
        if op[0]=='set_articulation':art=op[1]
        elif op[0]=='set_volume':volume=op[1]
        elif op[0]=='note' and op[1] and art is not None and art not in done:
            done.add(art);program=tbl[art]['program']
            pitch=([o[1] for o in program if o[0]=='pitch' and abs(o[1])<=1200]+[0])[0]
            out+=[(o[1],pitch+op[1]*100-1300,volume) for o in program if o[0]=='trigger']
        elif op[0]=='fork_voice':out+=layers(ucd,tbl,op[1],seen)
    return out

def render(decode,parts,limit):
    mixed=[]
    for sound,cents,_ in parts:
        x=decode(sound);rate=SOURCE_RATE*2**(cents/1200)
        t=np.arange(0,len(x)/rate,1/RATE)
        mixed.append(np.interp(t*rate,np.arange(len(x)),x))
    y=np.zeros(max(len(m) for m in mixed))
    for m in mixed:y[:len(m)]+=m
    y=y[:int(limit*RATE)]
    fade=min(len(y),int(.04*RATE));y[len(y)-fade:]*=np.linspace(1,0,fade)
    y=y/max(1e-6,float(np.abs(y).max()))*.9
    pcm=np.round(y*127).astype(np.int8)
    return np.concatenate([pcm,np.zeros(-len(pcm)%4,dtype=np.int8)])

def main():
    rom=(ROOT/'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest()!=EXPECTED_SHA1:raise ValueError('Expected verified US ROM')
    revision=subprocess.check_output(['git','-C',str(UPSTREAM),'rev-parse','HEAD'],text=True).strip()
    if revision!=REVISION:raise ValueError('Unexpected ssb-decomp-re revision: '+revision)
    ucd=extract_fgm.decode_fgm_ucd(rom[UCD[0]:UCD[1]])['entries']
    tbl=extract_fgm.decode_fgm_tbl(rom[ATL[0]:ATL[1]])['entries']
    names=voice_names()
    # Trailing fgm.ucd entries (forked sub-voices) have no enum name.
    if len(names)>len(ucd):raise ValueError('gmFGMVoiceID exceeds fgm.ucd count')
    decode,sound_count,_=waves(rom)
    out=ROOT/'build/audio';out.mkdir(parents=True,exist_ok=True)
    lines=['/* Generated from the user ROM; original Nintendo/HAL audio, never publish. */',
           '#include "fight_samples.h"']
    table=[];report=[]
    for key,fgm,priority,limit in SOUNDS:
        parts=layers(ucd,tbl,names.index(fgm))
        if not parts or any(p[0]>=sound_count for p in parts):raise ValueError('Unresolved '+fgm)
        pcm=render(decode,parts,limit)
        volume=min(127,max(p[2] for p in parts)*127//255)
        ticks=math.ceil(len(pcm)/RATE*60)+2
        lines.append(f'static const int8_t pcm_{key.lower()}[] __attribute__((aligned(32)))={{'
                     +','.join(map(str,pcm.tolist()))+'};')
        table.append(f'{{pcm_{key.lower()},sizeof pcm_{key.lower()},{ticks},{priority},{volume}}},')
        with wave.open(str(out/(key.lower()+'.wav')),'wb') as f:
            f.setnchannels(1);f.setsampwidth(1);f.setframerate(RATE)
            f.writeframes((pcm.astype(np.int16)+128).astype(np.uint8).tobytes())
        report.append({'sound':key,'fgm':fgm,'layers':[{'rom_sound':p[0],'cents':p[1],'ucd_volume':p[2]} for p in parts],
                       'bytes':len(pcm),'volume':volume,'priority':priority,
                       'sha256':hashlib.sha256(pcm.tobytes()).hexdigest()})
    lines+=[f'const FightSample fight_samples[{len(SOUNDS)}]={{',*table,'};']
    (out/'fight_samples.c').write_text('\n'.join(lines)+'\n')
    (out/'fight_samples.h').write_text('\n'.join([
        '#include <stdint.h>',f'#define FIGHT_AUDIO_RATE {RATE}',
        'typedef struct {const int8_t *data;unsigned bytes,ticks,priority,volume;} FightSample;',
        f'extern const FightSample fight_samples[{len(SOUNDS)}];'])+'\n')
    total=sum(r['bytes'] for r in report)
    (out/'audio-info.json').write_text(json.dumps({
        'provenance':'Original US ROM B1_sounds2 via fgm.ucd/fgm.tbl; local build only, not distributable',
        'upstream_revision':REVISION,'rate':RATE,'format':'signed PCM8 mono','channel_limit':4,
        'approximation':'first note per articulation, pitch resampled, layers mixed; no envelopes/LFOs',
        'bytes':total,'samples':report},indent=2))
    print(f'Converted {len(SOUNDS)} original sounds: {total} bytes PCM8 {RATE} Hz')

if __name__=='__main__':main()
