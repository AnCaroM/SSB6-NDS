"""Offline US moveset timeline; rendering and combat share original bones.

No C execution: a bounded interpreter handles the numeric attack/event subset.
Sound events and hit sounds map to the shared 1v1 list in fight_sounds.py;
effects/invulnerability are catalogued as unimplemented, not emulated.
"""
import re
import math
import json
from pathlib import Path
import numpy as np
from fight_sounds import BY_FGM,NONE,SMASH_RANDOM,hit_sound

# Stable order shared with src/moves.h. Clip IDs are resolved by resource ID.
SPECS=[
('JAB1',606,'Jab1'),('JAB2',607,'Jab2'),('JAB3',608,'Jab3'),('DASH',609,'DashAttack'),
('FTILT_HI',610,'FTiltHigh'),('FTILT',611,'FTilt'),('FTILT_LO',612,'FTiltLow'),
('UTILT',613,'UTilt'),('DTILT',614,'DTilt'),
('FSMASH_HI',615,'FSmashHigh'),('FSMASH_MHI',616,'FSmashMidHigh'),('FSMASH',617,'FSmash'),
('FSMASH_MLO',618,'FSmashMidLow'),('FSMASH_LO',619,'FSmashLow'),('USMASH',620,'USmash'),('DSMASH',621,'DSmash'),
('NAIR',622,'AttackAirN'),('FAIR',623,'AttackAirF'),('BAIR',624,'AttackAirB'),('UAIR',625,'AttackAirU'),('DAIR',626,'AttackAirD'),
('GRAB',561,'Catch'),('THROW_F',563,'ThrowF'),('THROW_B',564,'ThrowB'),
('FIRE',635,'FireballGround'),('FIRE_AIR',636,'FireballAir'),('UP',637,'SuperJumpPunchAir_0x16CC'),
('TORNADO',638,'MarioTornadoGround'),('TORNADO_AIR',639,'0x1884'),
('GETUP_D',553,'DownAttackD'),('GETUP_U',554,'DownAttackU'),
('LEDGE_START',580,'CliffAttackQuick1'),('LEDGE',581,'CliffAttackQuick2'),
('LEDGE_SLOW_START',582,'CliffAttackSlow1'),('LEDGE_SLOW',583,'CliffAttackSlow2'),
('GRAB_HOLD',562,None),
# Air up-special (Link's SpecialAirHi differs; Mario shares SuperJumpPunch).
('UP_AIR',637,'SuperJumpPunchAir_0x16CC')]

def us_source(text):
    result=[];stack=[];active=True
    for line in text.splitlines():
        if line.lstrip().startswith('#if'):
            condition='REGION_JP' not in line
            stack.append((active,condition));active=active and condition
        elif line.lstrip().startswith('#else'):
            parent,condition=stack[-1];active=parent and not condition
        elif line.lstrip().startswith('#endif'):
            active=stack.pop()[0]
        elif active:result.append(line)
    return '\n'.join(result)

def scripts(root):
    source=us_source((root/'upstream/ssb-decomp-re/src/relocData/202_MarioMainMotion.c').read_text())
    return {name:re.findall(r'(ft\w+)\(([^()]*)\)',body)
            for name,body in re.findall(r'(?:ftMotionCommand|u32)\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};',source,re.S)}

SOUND_OPS=('ftMotionPlayFGM','ftMotionPlayVoice','ftMotionPlayInterruptableVoice',
           'ftMotionCommandPlayFGMStoreInfo','ftMotionCommandPlaySmashVoice')

def sound_id(op,name):
    # PlaySmashVoice's argument is a placeholder; runtime picks attr smash_sfx.
    if op=='ftMotionCommandPlaySmashVoice':return SMASH_RANDOM
    return BY_FGM.get(name,NONE)

def timeline(table,name,limit=None):
    """Attack/flag/sound events; name is a script suffix (Mario) or a full symbol."""
    events=[];clock=0;budget=0
    def run(name,depth=0):
        nonlocal clock,budget
        if depth>8:raise ValueError('Moveset call depth')
        full=name if name in table else 'dMarioMainMotion_'+name
        code=table[full]
        i=0;loops=[]
        while i<len(code) and (limit is None or clock<=limit):
            budget+=1
            if budget>4096:raise ValueError('Moveset event budget')
            op,args=code[i];args=[a.strip() for a in args.split(',') if a.strip()];i+=1
            if op=='ftMotionCommandEnd':break
            if op=='ftMotionCommandWait':clock+=int(args[0],0)
            elif op=='ftMotionCommandWaitAsync':clock=max(clock,int(args[0],0))
            elif op=='ftMotionCommandLoopBegin':loops.append([i,int(args[0],0)])
            elif op=='ftMotionCommandLoopEnd':
                loops[-1][1]-=1
                if loops[-1][1]:i=loops[-1][0]
                else:loops.pop()
            elif op=='ftMotionCommandGoto':
                # Looping scripts jump back; only follow while inside the clip.
                if limit is None or clock<=limit:run(args[0],depth+1)
                break
            elif op in ('ftMotionCommandMakeAttackColl','ftMotionCommandMakeAttackCollScaled',
                        'ftMotionCommandClearAttackCollAll','ftMotionCommandClearAttackColl',
                        'ftMotionCommandRefreshAttackCollID','ftMotionCommandSetAttackCollDamage',
                        'ftMotionCommandSetAttackCollSize') or re.fullmatch('ftMotionCommandSetFlag[0-3]',op):
                events.append((clock,op,[int(a,0) for a in args]))
            elif op in SOUND_OPS:events.append((clock,'sound',[sound_id(op,args[0]),args[0]]))
        else:
            # Adjacent command arrays can intentionally fall through, notably
            # FireballGround into the shared spawn script at 0x1688.
            if limit is not None and clock>limit:return
            keys=list(table);pos=keys.index(full)
            if pos+1<len(keys):run(keys[pos+1],depth+1)
    if name:run(name)
    return events

def frames(table,name,ticks):
    events=timeline(table,name,ticks);active={};cache={};flags=0;epoch=0;cursor=0;output=[]
    def enable(aid,values):
        nonlocal epoch
        group=values[1]
        others=[h['strike'] for key,h in active.items() if h['v'][1]==group]
        if others:strike=others[0]
        else:epoch+=1;strike=epoch
        active[aid]={'v':list(values),'strike':strike};cache[aid]=list(values)
    for t in range(ticks):
        sounds=[]
        while cursor<len(events) and events[cursor][0]<=t:
            _,op,args=events[cursor];cursor+=1
            if op=='sound' and args[0]!=NONE:sounds.append(args[0])
            if op.startswith('ftMotionCommandMakeAttackColl'):enable(args[0],args)
            elif op=='ftMotionCommandClearAttackCollAll':active.clear()
            elif op=='ftMotionCommandClearAttackColl':active.pop(args[0],None)
            elif op=='ftMotionCommandRefreshAttackCollID':
                if args[0] not in cache:raise ValueError('Refresh missing attack')
                enable(args[0],cache[args[0]])
            elif op=='ftMotionCommandSetAttackCollDamage':
                if args[0] in active:active[args[0]]['v'][3]=args[1]
                if args[0] in cache:cache[args[0]][3]=args[1]
            elif op=='ftMotionCommandSetAttackCollSize':
                if args[0] in active:active[args[0]]['v'][6]=args[1]
                if args[0] in cache:cache[args[0]][6]=args[1]
            elif 'SetFlag' in op:
                bit=1<<int(op[-1]);flags=(flags|bit) if args[0] else (flags&~bit)
        if len(sounds)>2:raise ValueError('More than two sounds in one tick')
        output.append({'hits':[dict(v=list(h['v']),strike=h['strike']) for h in active.values()], 'flags':flags,
                       'sounds':sounds+[NONE]*(2-len(sounds))})
    if epoch>63:raise ValueError('Hit record mask exceeded')
    return output

def export(root,output,clips,bones):
    table=scripts(root);by_id={c['id']:(i,c) for i,c in enumerate(clips)}
    hits=[];allframes=[];descs=[];report=[]
    for key,fid,script in SPECS:
        clip,info=by_id[fid];sequence=frames(table,script,info['ticks']);first=len(allframes)
        for t,frame in enumerate(sequence):
            start=len(hits);world=bones[fid][t]
            for hit in frame['hits']:
                v=hit['v'];joint=v[2]
                p=np.array([*v[7:10],1.0])
                # Fighter IDs 4..28 are the model's 0..24. TopN=identity.
                if joint>=4:p=world[joint-4]@p
                elif joint!=0:raise ValueError('Unsupported attack joint')
                # Runtime render uses native model units; original encoded size
                # is diameter (ftMain), not radius. Project to the fight plane.
                x,y=round(p[2]),round(p[1]);radius=round(v[6]*.5)
                angle=v[10];a=math.radians(angle if angle!=361 else 0)
                hits.append([x,y,radius,v[3],angle,v[11],v[12],v[17],hit['strike'],v[13],round(math.cos(a)*4096),round(math.sin(a)*4096),
                             hit_sound(v[16],v[15])])
            spawn=world[12]@np.array([0,0,0,1.0])
            anchor=world[24]@np.array([0,0,0,1.0])
            allframes.append([start,len(hits)-start,frame['flags'],round(spawn[2]),round(spawn[1]),round(anchor[2]),round(anchor[1]),'{%d,%d}'%tuple(frame['sounds'])])
        descs.append([clip,info['ticks'],first]);report.append({'move':key,'resource':fid,'clip':clip,'ticks':info['ticks'],
                    'events':timeline(table,script),'hit_samples':sum(len(f['hits']) for f in sequence)})
    lines=['/* Generated from user ROM poses and pinned US source timelines. */','#include "moves.h"',
           'const MoveHit mario_move_hits[] = {']
    lines+=['{'+','.join(map(str,h))+'},' for h in hits];lines+=['};','const MoveFrame mario_move_frames[] = {']
    lines+=['{'+','.join(map(str,f))+'},' for f in allframes];lines+=['};','const MoveDesc mario_moves[MOVE_COUNT] = {']
    lines += [f'{{"{s[0]}",{d[0]},{d[1]},{d[2]}}},' for s,d in zip(SPECS,descs)]
    lines+=['};'];(output/'mario_moves.c').write_text('\n'.join(lines)+'\n')
    with (output/'mario_moves.c').open('a') as f:
        f.write('const unsigned mario_damage_ticks[17] = {'+','.join(
            str(by_id[fid][1]['ticks']) for fid in range(523,540))+'};\n')
    priority=[0,1,2,24,27,26,21,22,23,5,7,8,11,14,15,16,17,18,19,20,3]
    order=priority+[i for i in range(len(SPECS)) if i not in priority]
    with (output/'mario_moves.c').open('a') as f:
        f.write('const uint8_t mario_move_demo_order[MOVE_COUNT] = {'+','.join(map(str,order))+'};\n')
    (output/'moves-info.json').write_text(json.dumps(report,indent=2))
