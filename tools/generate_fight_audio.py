"""Own provisional effects: offline PCM synthesis, no game audio/voices."""
import hashlib
import json
import math
from pathlib import Path
import random
import wave

ROOT=Path(__file__).resolve().parents[1]
RATE=16000
NAMES=['swing','hit','heavy','fire','jump','land','ko']
DURATIONS=[.12,.16,.27,.25,.16,.10,.65]

def samples(kind):
    rng=random.Random(6412+kind)
    count=int(DURATIONS[kind]*RATE)//4*4
    phase=0;filtered=0;result=[]
    for i in range(count):
        t=i/RATE;u=i/(count-1)
        noise=rng.uniform(-1,1);filtered=.65*filtered+.35*noise
        freq=[700,160,95,1000,260,90,480][kind]
        freq*=1+([-.7,-.8,-.85,-.8,2,-.5,-.9][kind])*u
        phase+=2*math.pi*freq/RATE
        tone=math.sin(phase)
        blend=[.9,.55,.45,.75,.12,.8,.35][kind]
        envelope=(1-u)**[2,3,2,2,2,3,1.5][kind]*min(1,i/24)
        value=(blend*filtered+(1-blend)*tone)*envelope
        result.append(round(value*110))
    return result

def main():
    out=ROOT/'build/audio';out.mkdir(parents=True,exist_ok=True)
    header=['#include <stdint.h>','#define FIGHT_AUDIO_RATE 16000',
            'typedef struct {const int8_t *data;unsigned bytes,ticks,priority,volume;} FightSample;',
            'extern const FightSample fight_samples[7];']
    (out/'fight_samples.h').write_text('\n'.join(header)+'\n')
    lines=['#include "fight_samples.h"'];report=[]
    for kind,name in enumerate(NAMES):
        pcm=samples(kind);raw=bytes(x%256 for x in pcm)
        lines.append(f'static const int8_t pcm_{name}[] __attribute__((aligned(32)))={{'+','.join(map(str,pcm))+'};')
        with wave.open(str(out/(name+'.wav')),'wb') as f:
            f.setparams((1,1,RATE,0,'NONE','not compressed'))
            f.writeframes(bytes(x+128 for x in pcm))
        report.append(dict(name=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
    lines.append('const FightSample fight_samples[7]={')
    for i,name in enumerate(NAMES):
        lines.append(f'{{pcm_{name},sizeof pcm_{name},{math.ceil(DURATIONS[i]*60)+2},{[0,2,3,1,0,0,4][i]},{[35,60,64,40,35,30,60][i]}}},')
    lines.append('};')
    (out/'fight_samples.c').write_text('\n'.join(lines)+'\n')
    (out/'audio-info.json').write_text(json.dumps(dict(provenance='Own synthesized provisional effects; no original audio',
        rate=RATE,format='signed PCM8 mono',channel_limit=4,bytes=sum(x['bytes'] for x in report),samples=report),indent=2))
    print('Generated provisional PCM8 effects:',sum(x['bytes'] for x in report),'bytes')
if __name__=='__main__':main()
