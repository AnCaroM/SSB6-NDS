"""Convert a fighter (model, textures, bone-pose clips) for the generic DS renderer.

Table driven: the motion list comes from the fighter's FTMotionDesc table
(fighter_motions.py), hidden-joint insertion from its FTANIM flags, and the
model from its FTCommonPartContainer hi-detail JointTree. Mario's legacy
pipeline (convert_mario_model.py) is kept for the existing 1v1; running this
tool with --fighter mario checks that both pipelines agree.

Output (build/fighters/<name>/): <name>_model.c with a FighterModel, a JSON
report and an OBJ of the bind pose. Generated from the user ROM; never publish.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import struct
import subprocess
import sys
import numpy as np
from prepare_rom import EXPECTED_SHA1
from convert_mario_model import Model
from mario_animation import decode_script, evaluate
from mario_moves import SPECS, frames as move_frames
from fight_sounds import INDEX as SOUND
from mario_skeleton import quantize, local_matrix, _float_matrix, TRANSLATE_SCALE
import fighter_motions as fm
import fight_sounds as fs

ROOT=Path(__file__).resolve().parents[1]
FILE_TABLE=0x1ac870;FILE_BASE=0x1b2c6c
LOCAL_DIVISOR=64
WORLD_UNIT=1024  # versus_main.c WORLD_UNIT: model unit -> 1/1024 GL unit
FIRST_SLOT=1     # slot 0 holds the caller's pushed matrix
MAX_ERROR=2.1
# Ledge phases are drawn relative to the ledge with their authored TransN
# (convert_mario_model/mario_animation: 572, 573, 580, 582 for Mario).
LEDGE_PREFIX={'CliffCatch','CliffWait','CliffAttackQuick1','CliffAttackSlow1'}
SINGLE_FRAME={'CliffWait'}  # one hanging pose, as in the Mario 1v1
COMMON_CLIPS=['Wait','WalkMiddle','Squat','SquatWait','SquatRv','LightThrowF','LightThrowAirF','Dash','Run','RunBrake','KneeBend','JumpF','JumpB','JumpAerialF',
    'JumpAerialB','Fall','LandingLight','LandingHeavy','DamageHi1','DamageHi2','DamageHi3','DamageN1',
    'DamageN2','DamageN3','DamageLw1','DamageLw2','DamageLw3','DamageAir1','DamageAir2','DamageAir3',
    'DamageFlyHi','DamageFlyN','DamageFlyLw','DamageFlyTop','DamageFlyRoll','DamageFall','FallSpecial',
    'CliffCatch','CliffWait','CliffAttackQuick1','CliffAttackQuick2','CliffAttackSlow1','CliffAttackSlow2',
    'DownAttackD','DownAttackU','Catch','CatchPull','ThrowF','ThrowB','CapturePulled','Attack11','Attack12',
    'AttackDash','AttackS3','AttackHi3','AttackLw3','AttackS4','AttackHi4','AttackLw4','AttackAirN',
    'AttackAirF','AttackAirB','AttackAirHi','AttackAirLw',
    # Prueba19: shield, rolls, techs, knockdown and shield break (src/fighter.h FighterActionId).
    'GuardOn','GuardOff','EscapeF','EscapeB','Passive','PassiveStandF','PassiveStandB',
    'DownBounceD','DownBounceU','DownStandD','DownStandU','DownForwardD','DownForwardU','DownBackD','DownBackU',
    'ShieldBreakFly','ShieldBreakFall','ShieldBreakDownD','ShieldBreakDownU','ShieldBreakStandD','ShieldBreakStandU','FuraFura']
# FighterActionId order -> motion (GUARD and DOWN_WAIT_* hold GuardOn/DownBounce's last frame).
ACTION_MOTIONS=[('NONE','Wait'),('GUARD_ON','GuardOn'),('GUARD','GuardOn'),('GUARD_OFF','GuardOff'),
    ('ESCAPE_F','EscapeF'),('ESCAPE_B','EscapeB'),('PASSIVE','Passive'),('PASSIVE_F','PassiveStandF'),('PASSIVE_B','PassiveStandB'),
    ('DOWN_BOUNCE_D','DownBounceD'),('DOWN_BOUNCE_U','DownBounceU'),('DOWN_WAIT_D','DownBounceD'),('DOWN_WAIT_U','DownBounceU'),
    ('DOWN_STAND_D','DownStandD'),('DOWN_STAND_U','DownStandU'),('DOWN_FORWARD_D','DownForwardD'),('DOWN_FORWARD_U','DownForwardU'),
    ('DOWN_BACK_D','DownBackD'),('DOWN_BACK_U','DownBackU'),('SHIELD_BREAK_FLY','ShieldBreakFly'),('SHIELD_BREAK_FALL','ShieldBreakFall'),
    ('SHIELD_BREAK_DOWN_D','ShieldBreakDownD'),('SHIELD_BREAK_DOWN_U','ShieldBreakDownU'),
    ('SHIELD_BREAK_STAND_D','ShieldBreakStandD'),('SHIELD_BREAK_STAND_U','ShieldBreakStandU'),('FURAFURA','FuraFura')]
# Move slots (src/moves.h order) -> motion, with fallbacks for fighters
# lacking angled variants (Link has one forward tilt and one forward smash).
SLOT_MOTIONS={'JAB1':['Attack11'],'JAB2':['Attack12'],'JAB3':['Attack13','Attack100Start'],'DASH':['AttackDash'],
    'FTILT_HI':['AttackS3Hi','AttackS3'],'FTILT':['AttackS3'],'FTILT_LO':['AttackS3Lw','AttackS3'],
    'UTILT':['AttackHi3'],'DTILT':['AttackLw3'],'FSMASH_HI':['AttackS4Hi','AttackS4'],
    'FSMASH_MHI':['AttackS4HiS','AttackS4'],'FSMASH':['AttackS4'],'FSMASH_MLO':['AttackS4LwS','AttackS4'],
    'FSMASH_LO':['AttackS4Lw','AttackS4'],'USMASH':['AttackHi4'],'DSMASH':['AttackLw4'],
    'NAIR':['AttackAirN'],'FAIR':['AttackAirF'],'BAIR':['AttackAirB'],'UAIR':['AttackAirHi'],'DAIR':['AttackAirLw'],
    'GRAB':['Catch'],'THROW_F':['ThrowF'],'THROW_B':['ThrowB'],'FIRE':['SpecialN','SpecialNStart'],
    'FIRE_AIR':['SpecialAirN','SpecialAirNStart'],'UP':['SpecialHi'],'TORNADO':['SpecialLw','SpecialLwStart'],
    'TORNADO_AIR':['SpecialAirLw','SpecialAirLwStart'],'GETUP_D':['DownAttackD'],
    'GETUP_U':['DownAttackU'],'LEDGE_START':['CliffAttackQuick1'],'LEDGE':['CliffAttackQuick2'],
    'LEDGE_SLOW_START':['CliffAttackSlow1'],'LEDGE_SLOW':['CliffAttackSlow2'],'GRAB_HOLD':['CatchPull'],
    'UP_AIR':['SpecialAirHi']}
FIGHTERS={
    # size: FTAttributes.size (US), the fighter DObj scale. The game scales the
    # model, its hit offsets and TransN movement by it; hit radii are not.
    # spawn/grab: tree joints for projectile spawn and captured-fighter anchor
    # (Mario: legacy mario_moves world[12]/world[24]; Link: itemlight 16, itemheavy 35).
    'mario':dict(Name='Mario',size=1.12,spawn=12,grab=24,kind='FIGHTER_MARIO',attr='fighter_attr_mario',
                 sounds=dict(jump='VOICE_JUMP',jump_air='VOICE_JUMP_AIR',land='LAND',damage='VOICE_DAMAGE',
                             dead='VOICE_DEAD',dead_slam='DEAD_SLAM',smash=['VOICE_SMASH1','VOICE_SMASH2','VOICE_SMASH3'],
                             down_bounce='DOWN_BOUNCE'),
                 model=296,main='203_MarioMain.c',motion='202_MarioMainMotion.c',base_parts={},itemlight=None,tree=0x2200,matanim=0x2680,table='dFTMarioMotionDescs',
                 special=['Attack13','SpecialN','SpecialAirN','SpecialHi','SpecialAirHi','SpecialLw','SpecialAirLw'],parts={}),
    # 324_LinkModel.c: dLinkModel_JointTree @0x3AE8; anim table +0x5B8 (225_LinkMain.c).
    # base_parts: ftManagerInitFighter (ftmanager.c:639-644) after tree defaults;
    # itemlight: joint_itemlight_id used by SetModelPartID(-2, ...).
    'link':dict(Name='Link',size=1.24,spawn=12,grab=31,kind='FIGHTER_LINK',attr='fighter_attr_link',
                sounds=dict(jump='LINK_JUMP',jump_air='LINK_JUMP_AIR',land='LINK_LAND',damage='LINK_DAMAGE',
                            dead='LINK_DEAD',dead_slam='LINK_DEAD_SLAM',smash=['LINK_SMASH1','LINK_SMASH2','LINK_SMASH3'],
                            down_bounce='LINK_DOWN_BOUNCE'),
                model=324,main='225_LinkMain.c',motion='224_LinkMainMotion.c',base_parts={21:-1,19:0},itemlight=16,tree=0x3AE8,matanim=0x3AE8+0x5B8,table='dFTLinkMotionDescs',
                special=['Attack13','Attack100Start','Attack100Loop','Attack100End','SpecialHi','SpecialHiEnd',
                         'SpecialAirHi','SpecialN','SpecialNGet','SpecialNEmpty','SpecialAirN','SpecialLw','SpecialAirLw'],
                parts={}),
    # 328_KirbyModel.c: dKirbyModel_JointTree @0x1448, anim table gap_0x1920
    # (229_KirbyMain.c commonparts); itemlight 17, itemheavy 30 (tree 13/26).
    'kirby':dict(Name='Kirby',size=0.91,spawn=13,grab=26,kind='FIGHTER_KIRBY',attr='fighter_attr_kirby',
                 sounds=dict(jump='KIRBY_JUMP',jump_air='KIRBY_JUMP_AIR',land='KIRBY_LAND',damage='KIRBY_DAMAGE',
                             dead='KIRBY_DEAD',dead_slam='KIRBY_DEAD_SLAM',smash=['KIRBY_SMASH1','KIRBY_SMASH2','KIRBY_SMASH3'],
                             down_bounce='KIRBY_DOWN_BOUNCE'),
                 model=328,main='229_KirbyMain.c',motion='228_KirbyMainMotion.c',base_parts={},itemlight=17,tree=0x1448,matanim=0x1920,
                 table='dFTKirbyMotionDescs',
                 special=['Attack100Start','Attack100Loop','Attack100End','JumpAerialF1','JumpAerialF2','JumpAerialF3',
                          'JumpAerialF4','JumpAerialF5','SpecialHi','SpecialHiLanding','SpecialAirHi','SpecialAirHiFall',
                          'SpecialLwStart','SpecialLwHold','SpecialLwEnd','SpecialAirLwStart','SpecialAirLwHold','SpecialAirLwFall',
                          'SpecialAirLwLanding','SpecialNStart','SpecialNLoop','SpecialNEnd','SpecialNEat','SpecialNThrow','SpecialNWait',
                          'SpecialAirNStart','SpecialAirNLoop','SpecialAirNEnd'],
                 parts={}),
}

def rom_file(rom,fid,decoder,scratch):
    flags,intern,_,_,decoded=struct.unpack_from('>I4H',rom,FILE_TABLE+fid*12)
    stop=struct.unpack_from('>I',rom,FILE_TABLE+(fid+1)*12)[0]&0x7fffffff
    start=FILE_BASE+(flags&0x7fffffff);end=FILE_BASE+stop
    if flags>>31:
        packed=scratch/f'{fid:04d}.vpk0';out=scratch/f'{fid:04d}.bin'
        packed.write_bytes(rom[start:end]);subprocess.run([str(decoder),str(packed),str(out)],check=True)
        data=out.read_bytes()
    else:data=rom[start:start+decoded*4]
    if len(data)!=decoded*4:raise ValueError(f'File {fid} length mismatch')
    return data,intern

def chain(data,word):
    pointers={}
    while word!=0xffff:
        if word*4 in pointers:raise ValueError('Cyclic relocations')
        raw=struct.unpack_from('>I',data,word*4)[0];pointers[word*4]=(raw&0xffff)*4;word=raw>>16
    return pointers

def hidden_parts(Name):
    """Root fighter joint of each FTHiddenPart; bit (31-k) of FTANIM flags creates part k."""
    text=fm.us_source((fm.UPSTREAM/f'src/relocData/{FIGHTERS[Name.lower()]["main"]}').read_text())
    body=re.search(r'FTHiddenPart d'+Name+r'Main_hiddenparts\[\d+\]\s*=\s*\{(.*?)\n\};',text,re.S)[1]
    return [int(row.split(',')[0],0) for row in re.findall(r'\{([^{}]*)\}',body)]

def animated_joints(flags,roots,count):
    """Animation tables list the DObjs that exist for this motion, in tree order
    (ftmain hidden parts 0..2 are XRotN/TransN/YRotN at the front; later parts
    are optional tree joints such as Mario's joint28, absent unless enabled)."""
    # Part 2 (YRotN, bit 29: GuardOn/GuardOff) is created but animated at run
    # time from the attributes' shield_anim_joints, not by a table in the file.
    aux=[k for k in range(min(2,len(roots))) if flags&(1<<(31-k))]
    enabled={roots[k] for k in range(3,len(roots)) if flags&(1<<(31-k))}
    optional=set(roots[3:])
    return aux,[j for j in range(count) if j+4 not in optional or j+4 in enabled]

def row_from_matrix(m):
    """Channel row (tx,ty,tz,rx,ry,rz,sx,sy,sz) of a T*Rz*Ry*Rx*S matrix
    (mario_skeleton._float_matrix convention)."""
    r=m[:3,:3];s=np.linalg.norm(r,axis=0);r=r/s
    ry=math.asin(max(-1.0,min(1.0,-r[2,0])));rx=math.atan2(r[2,1],r[2,2]);rz=math.atan2(r[1,0],r[0,0])
    return [float(m[0,3]),float(m[1,3]),float(m[2,3]),rx,ry,rz,float(s[0]),float(s[1]),float(s[2])]

def sample(data,word,depths,defaults,flags,ledge,single,roots):
    """Local channels [frame][joint+prefix][9] for one clip (mario_animation semantics)."""
    pointers=chain(data,word)
    aux,existing=animated_joints(flags,roots,len(depths))
    xrotn=bool(flags&fm.INSERT_XROTN)
    tables=[]
    for slot in range(len(aux)+len(existing)):
        ptr=pointers.get(slot*4)
        if ptr is None and struct.unpack_from('>I',data,slot*4)[0]:raise ValueError('Unresolved animation pointer')
        tables.append(decode_script(data,ptr) if ptr is not None else None)
    if not any(tables):raise ValueError('Empty animation')
    duration=max(s[1] for s in tables if s);looping=any(s[2] for s in tables if s)
    # Inserted joints come first, in tree order (TransN, XRotN, YRotN); with
    # several (GuardOn: XRotN+YRotN) their transforms compose into one prefix.
    auxiliaries=[tables.pop(0) for _ in aux]
    scripts=[None]*len(depths)
    for joint,script in zip(existing,tables):scripts[joint]=script
    if not 1<=duration<=400:raise ValueError('Unexpected clip duration')
    ticks=duration if looping else duration+1
    fields={0:3,1:4,2:5,4:0,5:1,6:2,7:6,8:7,9:8}
    frames=[]
    for frame in range(1 if single else ticks):
        prefixes=[]
        for auxiliary in auxiliaries:
            prefix=[0,0,0,0,0,0,1,1,1]
            if auxiliary is not None:
                for track,field in fields.items():
                    value=evaluate(auxiliary[0][track],min(frame,auxiliary[1]))
                    if value is not None:prefix[field]=value
            prefixes.append(prefix)
        if len(prefixes)>1:
            composed=np.eye(4)
            for prefix in prefixes:composed=composed@_float_matrix(prefix)
            prefix=row_from_matrix(composed)
            if np.abs(_float_matrix(prefix)-composed).max()>1e-6:raise ValueError('Inserted joints do not compose')
        else:prefix=prefixes[0] if prefixes else [0,0,0,0,0,0,1,1,1]
        rows=[]
        for joint,values in enumerate(defaults):
            values=list(values)
            if scripts[joint] is not None:
                tracks,end,_=scripts[joint]
                for track,field in fields.items():
                    value=evaluate(tracks[track],min(frame,end))
                    if value is not None:values[field]=value
            rows.append(values)
        frames.append(rows+[prefix])
    # Root role of the inserted joint: 1 = drawn (XRotN, ledge-relative TransN),
    # 2 = TransN that the game applies as movement (drawn only in the viewer).
    root=0 if not aux else 1 if xrotn or ledge else 2
    return frames,ticks,looping,root

def world(rows,parents,make):
    prefix=make(rows[len(parents)]);out=[]
    for joint,parent in enumerate(parents):out.append((out[parent] if parent>=0 else prefix)@make(rows[joint]))
    return out

def plan_joints(parents,has_geometry):
    needed=list(has_geometry)
    for joint in reversed(range(len(parents))):
        if needed[joint] and parents[joint]>=0:needed[parents[joint]]=True
    order=[j for j in range(len(parents)) if needed[j]]
    if [j for j in order if parents[j]<0]!=[order[0]]:raise ValueError('Expected a single root joint')
    after={a:b for a,b in zip(order,order[1:])}
    store=[has_geometry[j] or any(parents[c]==j and needed[c] and after.get(j)!=c for c in order) for j in range(len(parents))]
    # A child evaluated right after its parent continues from the current matrix.
    slots={};plan=[]
    for j in range(len(parents)):
        if store[j]:slots[j]=FIRST_SLOT+len(slots)
    if FIRST_SLOT+len(slots)>31:raise ValueError(f'{len(slots)} matrix slots exceed the DS stack')
    previous=None
    for j in range(len(parents)):
        if not needed[j]:plan.append((-1,-1,1));continue
        p=parents[j];restore=-1 if p==previous or (p<0 and previous is None) else slots[p]
        plan.append((restore,slots.get(j,-1),0));previous=j
    return plan,slots

def script_tables(Name):
    text=fm.us_source((fm.UPSTREAM/f'src/relocData/{FIGHTERS[Name.lower()]["motion"]}').read_text())
    return {n:re.findall(r'(ft\w+)\(([^()]*)\)',body)
            for n,body in re.findall(r'(?:ftMotionCommand|u32)\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};',text,re.S)}

PART_OPS=('ftMotionCommandSetModelPartID','ftMotionCommandResetModelPartAll','ftMotionCommandHideModelPartAll')
HIT_STATUS_OP='ftMotionCommandSetHitStatusAll'

def part_events(tables,symbol,limit):
    """Model-part commands of a motion script (see script_events)."""
    return script_events(tables,symbol,limit,PART_OPS)

def script_events(tables,symbol,limit,ops):
    """(clock, op, args) of the listed commands in a motion script (fall-through,
    Goto, Subroutine/Return and counted loops as in mario_moves.timeline)."""
    events=[];clock=0;budget=0;keys=list(tables)
    def run(name,depth=0):
        nonlocal clock,budget
        if depth>8:raise ValueError('Motion script depth')
        code=list(tables[name]);i=0;loops=[]
        while clock<=limit:
            if i>=len(code):
                # Arrays without End fall through into the next one (FireballGround).
                following=keys.index(name)+1
                if following>=len(keys):return 'end'
                name=keys[following];code=list(tables[name]);i=0;continue
            budget+=1
            if budget>8192:return 'end'  # loop without waits (e.g. while held): no more part changes
            op,args=code[i];args=[a.strip() for a in args.split(',') if a.strip()];i+=1
            if op=='ftMotionCommandEnd':return 'end'
            if op=='ftMotionCommandReturn':return 'return'
            if op=='ftMotionCommandWait':clock+=int(args[0],0)
            elif op=='ftMotionCommandWaitAsync':clock=max(clock,int(args[0],0))
            elif op=='ftMotionCommandLoopBegin':loops.append([i,int(args[0],0)])
            elif op=='ftMotionCommandLoopEnd':
                loops[-1][1]-=1
                if loops[-1][1]:i=loops[-1][0]
                else:loops.pop()
            elif op=='ftMotionCommandGoto':
                if args[0] not in tables:return 'end'
                name=args[0];code=list(tables[name]);i=0;loops=[]  # tail jump (looping scripts)
            elif op=='ftMotionCommandSubroutine' and args[0] in tables:
                if run(args[0],depth+1)=='end':return 'end'
            elif op in ops:events.append((clock,op,[int(a,0) for a in args]))
        return 'end'
    if symbol in tables:run(symbol)
    return events

def export_actions(clips,float_frames,parents,motions,tables,size):
    """FighterAction table (src/fighter.h order): clip, length, intangible
    window (SetHitStatusAll 3..1), facing flip (SetFlag1), tick-0 sounds and,
    for movement-TransN clips, the per-tick displacement the game applies."""
    position={c['motion']:i for i,c in enumerate(clips)};unique={c['index']:c for c in clips if 'same' not in c}
    lines=[];rows=[];shield=(0,0)
    for action,motion in ACTION_MOTIONS:
        entry=clips[position[motion]];index=entry.get('index',entry.get('same'));src=unique[index]
        values,root=float_frames[index];ticks=src['ticks']
        # DownBack* enter DownForwardU's array a few words in; every path there
        # is a Goto to DownForwardD, so the base symbol has the same events.
        script=re.sub(r'\s*\+\s*0x[0-9A-Fa-f]+$','',motions[motion]['script'] or '')
        sequence=move_frames(tables,script,ticks) if script in tables else None
        sounds=sequence[0]['sounds'] if sequence else [fs.NONE,fs.NONE]
        turn=next((t for t,f in enumerate(sequence) if f['flags']&2),255) if sequence else 255
        status=[(c,a[0]) for c,_,a in script_events(tables,script,ticks,(HIT_STATUS_OP,)) if a]
        on=[c for c,st in status if st==3];off=[c for c,st in status if st==1 and on and c>=on[0]]
        intangible=(min(on[0],ticks),min(off[0] if off else ticks,ticks)) if on else (0,0)
        ref='NULL'
        if root==2 and not action.startswith('DOWN_WAIT'):
            z=[round(float(values[t][len(parents)][2])*size) for t in range(len(values))]
            dx=[z[t]-(z[t-1] if t else 0) for t in range(len(z))]
            lines.append(f'static const int16_t dx_{action.lower()}[{len(dx)}]={{'+','.join(map(str,dx))+'};');ref=f'dx_{action.lower()}'
        if action=='GUARD':
            # Shield centre: the hip (model joint 0, child of YRotN) at the end
            # of GuardOn; YRotN's own runtime pose is not in the animation file.
            hip=world(values[len(values)-1],parents,_float_matrix)[0]
            shield=(round(float(hip[2,3])*size),round(float(hip[1,3])*size))
        rows.append(f'{{{position[motion]},{ticks},{intangible[0]},{intangible[1]},{turn},{{{sounds[0]},{sounds[1]}}},{ref}}}, /* {action} */')
    lines.append('static const FighterAction actions[ACTION_COUNT]={');lines+=rows;lines.append('};')
    return lines,shield

def model_parts(Name):
    """{fighter joint: [hi display-list symbol per part id]} from FTModelPartContainer."""
    text=fm.us_source((fm.UPSTREAM/f'src/relocData/{FIGHTERS[Name.lower()]["main"]}').read_text())
    descs={n:[re.sub(r'\(\w+\*\)|&','',e.split(',')[0]).strip() for e in re.findall(r'\{([^{}]*)\}',body)]
           for n,body in re.findall(r'FTModelPart (d\w+)\[\d+\]\s*=\s*\{(.*?)\n\};',text,re.S)}
    body=re.search(r'FTModelPartDesc \*d'+Name+r'Main_modelparts_container\[\d+\]\s*=\s*\{(.*?)\};',text,re.S)[1]
    out={}
    for index,item in enumerate(x.strip() for x in body.split(',') if x.strip()):
        if item!='NULL':out[index+4]=descs[re.sub(r'\(\w+\*\)','',item).strip()][0::2]
    return out

def dl_address(symbol):
    m=re.fullmatch(r'd(\w+?)_(?:Joint_0x([0-9A-F]+)_DisplayList|gap_0x([0-9A-F]+)_sub_0x([0-9A-F]+))',symbol)
    if not m:raise ValueError('Unrecognized display list '+symbol)
    return m[1],int(m[2],16) if m[2] else int(m[3],16)+int(m[4],16)

def throw_hits(cfg,motions):
    """FTThrowHitDesc row 0 named by SetThrow in the ThrowF/ThrowB scripts."""
    text=fm.us_source((fm.UPSTREAM/f'src/relocData/{cfg["motion"]}').read_text());out=[]
    for motion in ('ThrowF','ThrowB'):
        body=re.search(r'(?:ftMotionCommand|u32)\s+'+motions[motion]['script']+r'\[\]\s*=\s*\{(.*?)\n\};',text,re.S)[1]
        desc=re.search(r'SetThrow\(\(u32\)(\w+)\)',body)[1]
        row=re.search(r'FTThrowHitDesc '+desc+r'\[\]\s*=\s*\{\s*\{([^}]*)\}',text)[1]
        status,damage,angle,scale,weight,base,_=(int(x,0) for x in row.split(','))
        a=math.radians(angle)
        out.append([0,0,0,damage,angle,scale,weight,base,0,3,round(math.cos(a)*4096),round(math.sin(a)*4096),255])
    return out

def export_moves(name,cfg,clips,float_frames,parents,motions,tables):
    """Move slots for combat.c: per-tick hitboxes/flags/sounds from the motion
    scripts (mario_moves.frames) with hit centres from the bone pose; same
    layout as the legacy mario_moves.c."""
    position={c['motion']:i for i,c in enumerate(clips)}
    unique={c['index']:c for c in clips if 'same' not in c}
    hits=[];allframes=[];descs=[];identity=[0,0,0,0,0,0,1,1,1];size=cfg['size']
    for slot,_,_ in SPECS:
        motion=next(m for m in SLOT_MOTIONS[slot] if motions.get(m,{}).get('file'))
        entry=clips[position[motion]];index=entry.get('index',entry.get('same'));src=unique[index]
        values,root=float_frames[index];ticks=src['ticks']
        script=None if slot=='GRAB_HOLD' else motions[motion]['script']
        sequence=move_frames(tables,script,ticks)
        first=len(allframes)
        for t,frame in enumerate(sequence):
            rows=values[min(t,len(values)-1)].copy()
            if root==2:rows[len(parents)]=identity  # movement TransN: gameplay moves the fighter
            w=world(rows,parents,_float_matrix);start=len(hits)
            for hit in frame['hits']:
                v=hit['v'];joint=v[2];pt=np.array([*v[7:10],1.0])
                if joint>=4:pt=w[joint-4]@pt
                elif joint!=0:raise ValueError('Unsupported attack joint')
                angle=v[10];a=math.radians(angle if angle!=361 else 0)
                hits.append([round(pt[2]*size),round(pt[1]*size),round(v[6]*.5),v[3],angle,v[11],v[12],v[17],hit['strike'],v[13],
                             round(math.cos(a)*4096),round(math.sin(a)*4096),fs.hit_sound(v[16],v[15])])
            spawn=w[cfg['spawn']]@np.array([0,0,0,1.0]);anchor=w[cfg['grab']]@np.array([0,0,0,1.0])
            allframes.append([start,len(hits)-start,frame['flags'],round(spawn[2]*size),round(spawn[1]*size),
                              round(anchor[2]*size),round(anchor[1]*size),'{%d,%d}'%tuple(frame['sounds'])])
        descs.append(f'{{"{slot}",{position[motion]},{ticks},{first}}}')
    damage=['DamageHi1','DamageHi2','DamageHi3','DamageN1','DamageN2','DamageN3','DamageLw1','DamageLw2','DamageLw3',
            'DamageAir1','DamageAir2','DamageAir3','DamageFlyHi','DamageFlyN','DamageFlyLw','DamageFlyTop','DamageFlyRoll']
    tick_of=lambda m:unique[clips[position[m]].get('index',clips[position[m]].get('same'))]['ticks']
    clip=lambda m:position[m]
    clipmap=[clip(m) for m in ['Wait','WalkMiddle','JumpF','Fall','LandingLight','CliffWait','CliffCatch','JumpAerialF',
             'JumpAerialB','FallSpecial','Dash','Run','RunBrake','CapturePulled','DamageFall','Squat','SquatWait','SquatRv',
             'LightThrowF','LightThrowAirF']]
    snd=cfg['sounds'];sounds=[SOUND[snd[k]] for k in ('jump','jump_air','land','damage','dead','dead_slam')]
    smash='{'+','.join(str(SOUND[x]) for x in snd['smash'])+'}'
    throws=throw_hits(cfg,motions)
    lines=['static const MoveHit move_hits[]={']+['{'+','.join(map(str,h))+'},' for h in hits]+['};']
    lines+=['static const MoveFrame move_frames[]={']+['{'+','.join(map(str,f))+'},' for f in allframes]+['};']
    lines+=['static const MoveDesc move_descs[MOVE_COUNT]={'+','.join(descs)+'};']
    lines+=['static const unsigned damage_ticks[17]={'+','.join(str(tick_of(m)) for m in damage)+'};']
    actions,shield=export_actions(clips,float_frames,parents,motions,tables,cfg['size']);lines+=actions
    lines+=[f'const Fighter fighter_{name}={{"{cfg["Name"]}",{cfg["kind"]},&{cfg["attr"]},move_hits,move_frames,move_descs,damage_ticks,',
            '    {'+','.join(map(str,clipmap))+',{'+','.join(str(clip(m)) for m in damage)+'}},',
            '    {'+','.join(map(str,sounds))+','+smash+','+str(SOUND[snd['down_bounce']])+'},',
            '    {'+','.join('{'+','.join(map(str,h))+'}' for h in throws)+'},',
            f'    actions,{shield[0]},{shield[1]}}};']
    print(f'{cfg["Name"]}: shield centre ({shield[0]},{shield[1]})')
    return lines

def convert(name,check_mario=False):
    cfg=FIGHTERS[name];Name=cfg['Name']
    rom=(ROOT/'build/rom/baserom.us.z64').read_bytes()
    if hashlib.sha1(rom).hexdigest()!=EXPECTED_SHA1:raise ValueError('Expected verified US ROM')
    out=ROOT/'build/fighters'/name;out.mkdir(parents=True,exist_ok=True)
    scratch=out/'rom';scratch.mkdir(exist_ok=True)
    decoder=ROOT/'build/assets-tools'/('vpk_decode.exe' if sys.platform=='win32' else 'vpk_decode')
    data,word=rom_file(rom,cfg['model'],decoder,scratch)
    model=Model(data,word,cfg['tree'],cfg['matanim'],cfg['parts']);model.convert()
    depths=[j['depth'] for j in model.joints];parents=[];last={}
    for d in depths:parents.append(last[d-1] if d else -1);last[d]=len(parents)-1
    defaults=[list(struct.unpack_from('>9f',data,cfg['tree']+8+i*44)) for i in range(len(depths))]
    # Model parts: tree triangles of a joint with a part desc are its part 0;
    # other part ids used by base state or the motion scripts are converted
    # under that joint (materials of the tree joint drawing the same list).
    descs=model_parts(Name);ids=fm.file_ids();tables=script_tables(Name);motions=fm.motions(Name,cfg['table'])
    base={j:(0 if model.joints[j-4]['display_list'] is not None else -1) for j in descs};base.update(cfg['base_parts'])
    tree_dl={j['display_list']:j['index'] for j in model.joints if j['display_list'] is not None}
    wanted=COMMON_CLIPS+cfg['special']
    for options in SLOT_MOTIONS.values():
        for motion in options:
            if motions.get(motion,{}).get('file') and motion not in wanted:wanted.append(motion)
    used={(j,i) for j,i in base.items() if i>=0}
    for motion in wanted:
        for _,op,args in part_events(tables,motions[motion]['script'],10000):
            if op.endswith('SetModelPartID'):
                j=cfg['itemlight'] if args[0]==-2 else args[0]
                if j in descs and args[1]>=0:used.add((j,args[1]))
    parts=sorted(used);part_index={p:i for i,p in enumerate(parts)}
    if len(parts)>32:raise ValueError('More than 32 model parts')
    tags=[part_index.get((joint+4,0),255) if joint+4 in descs else 255 for joint,_ in model.triangles]
    for j,i in parts:
        file_name,offset=dl_address(descs[j][i])
        if file_name==Name+'Model' and offset==model.joints[j-4]['display_list'] and i==0:continue
        start=len(model.triangles)
        if file_name==Name+'Model':
            model.display_list(offset,model.world[j-4],j-4,tree_dl.get(offset,j-4))
        else:
            extra_data,extra_word=rom_file(rom,ids[file_name],decoder,scratch)
            extra=Model(extra_data,extra_word,cfg['tree'],cfg['matanim'])
            extra.colors=lambda joint:[]  # self-contained list: its own image/tile commands
            extra.display_list(offset,model.world[j-4],j-4)
            shift=len(model.textures);model.textures+=extra.textures
            model.triangles+=extra.triangles
            model.triangle_textures+=[t+shift if t>=0 else t for t in extra.triangle_textures]
        tags+=[part_index[(j,i)]]*(len(model.triangles)-start)
    base_mask=sum(1<<part_index[(j,i)] for j,i in base.items() if i>=0)
    sources=[v[3] for _,tri in model.triangles for v in tri]
    unique={};local_map=[];locals_=[]
    for joint,xyz in sources:
        key=(joint,*xyz)
        if key not in unique:unique[key]=len(locals_);locals_.append((joint,xyz))
        local_map.append(unique[key])
    geometry=[any(j==k for j,_ in locals_) for k in range(len(depths))]
    plan,slots=plan_joints(parents,geometry)
    local=np.array([[*xyz,1.0] for _,xyz in locals_]);ljoint=np.array([j for j,_ in locals_])
    # Skeleton units: model/divisor so joint-local vertices fit v16 (Link's
    # hookshot chain reaches 750 model units, beyond the 64 used by Mario).
    divisor=LOCAL_DIVISOR
    while np.abs(local[:,:3]).max()*4096/divisor>32767:divisor*=2
    fixed=np.rint(local[:,:3]*4096/divisor)
    restored=np.c_[fixed*divisor/4096,np.ones(len(local))]
    roots=hidden_parts(Name);clips=[];seen={};worst=0.0;total=0;float_frames={}
    lines=['/* Generated from the user ROM; do not distribute. */','#include <stddef.h>','#include "fighter_model.h"','#include "fighter.h"']
    for motion in wanted:
        info=motions[motion]
        if info['file'] is None:raise ValueError(f'{Name} {motion} has no animation')
        key=(info['file'],info['flags']&(fm.INSERT_TRANSN|fm.INSERT_XROTN|fm.CATCH_JOINT),motion in LEDGE_PREFIX,motion in SINGLE_FRAME)
        if key in seen:clips.append(dict(motion=motion,same=seen[key]));continue
        anim,aword=rom_file(rom,info['file'],decoder,scratch)
        frames,ticks,looping,root=sample(anim,aword,depths,defaults,info['flags'],motion in LEDGE_PREFIX,motion in SINGLE_FRAME,roots)
        values=np.array(frames,dtype=np.float64);q=quantize(values);flat=q.reshape(len(q),-1)
        varying=[c for c in range(flat.shape[1]) if (flat[:,c]!=flat[0,c]).any()]
        error=0.0
        for f in range(len(q)):
            exact=world(values[f],parents,_float_matrix);approx=world(q[f],parents,local_matrix)
            for j in range(len(parents)):
                sel=ljoint==j
                if sel.any():
                    error=max(error,float(np.abs((local[sel]@exact[j].T)[:,:3]-(restored[sel]@approx[j].T)[:,:3]).max()))
        if error>MAX_ERROR:raise ValueError(f'{Name} {motion}: skeleton error {error:.3f}')
        worst=max(worst,error);index=len([c for c in clips if 'same' not in c])
        lines.append(f'static const int16_t base_{index}[{flat.shape[1]}]={{'+','.join(map(str,flat[0]))+'};')
        if varying:
            lines.append(f'static const uint16_t channels_{index}[]={{'+','.join(map(str,varying))+'};')
            lines.append(f'static const int16_t frames_{index}[]={{'+','.join(map(str,flat[:,varying].flatten()))+'};')
        state=dict(base);changes=[];events=part_events(tables,info['script'],ticks)
        for tick in range(ticks):
            for clock,op,args in events:
                if clock!=tick:continue
                if op.endswith('ResetModelPartAll'):state=dict(base)
                elif op.endswith('HideModelPartAll'):state={j:-1 for j in descs}
                else:
                    j=cfg['itemlight'] if args[0]==-2 else args[0]
                    if j in descs:state[j]=args[1]
            mask=sum(1<<part_index[(j,i)] for j,i in state.items() if i>=0)
            if not changes or changes[-1][1]!=mask:changes.append((tick,mask))
        if events:lines.append(f'static const uint32_t parts_{index}[]={{'+','.join(f'{t},{m}' for t,m in changes)+'};')
        size=flat.shape[1]*2+len(varying)*(2+2*len(q))+8*len(changes)*bool(events);total+=size
        seen[key]=index
        float_frames[index]=(values,root)
        clips.append(dict(motion=motion,index=index,file=info['file'],flags=info['flags'],ticks=ticks,stored=len(q),
                          loop=looping,varying=len(varying),bytes=size,max_world_error=error,root=root,
                          part_changes=len(changes) if events else 0,
                          channels=q if check_mario else None))
    entries=[]
    for c in clips:
        src=next(x for x in clips if x.get('index')==(c['same'] if 'same' in c else c['index']) and 'same' not in x)
        i=src['index'];ref=lambda n:f'{n}_{i}' if src['varying'] else 'NULL'
        parts_ref=f'parts_{i}' if src['part_changes'] else 'NULL'
        entries.append(f'{{{ref("frames")},{ref("channels")},base_{i},{parts_ref},{src["ticks"]},{src["stored"]},'
                       f'{src["varying"]},{int(src["loop"])},{src["part_changes"]},{src["root"]}}}, /* {c["motion"]} */')
    lines.append('static const FighterClip clips[]={');lines+=entries;lines.append('};')
    lines.append('static const char *const clip_names[]={'+','.join(f'"{c["motion"]}"' for c in clips)+'};')
    # Draw vertices: color/uv per expanded vertex; position via local_map.
    tri_vertices=[v for _,tri in model.triangles for v in tri]
    lines.append('static const FighterVertex vertices[]={'+','.join(
        '{%d,%d,%d}'%(((rgb[0]>>3)|((rgb[1]>>3)<<5)|((rgb[2]>>3)<<10)),round(uv[0]*16),round(uv[1]*16))
        for _,rgb,uv,_ in tri_vertices)+'};')
    lines.append('static const int16_t triangle_textures[]={'+','.join(map(str,model.triangle_textures))+'};')
    for i,t in enumerate(model.textures):
        lines.append(f'static const uint16_t texture_{i}[] __attribute__((aligned(4)))={{'+','.join(map(str,t['pixels']))+'};')
    lines.append('static const FighterTexture textures[]={'+','.join(
        f'{{texture_{i},{t["padded_width"]},{t["padded_height"]},{t["param"]}}}' for i,t in enumerate(model.textures))+'};')
    lines.append('static const uint16_t local_map[]={'+','.join(map(str,local_map))+'};')
    lines.append('static const FighterLocalVertex locals[]={'+','.join(
        '{%d,%d,%d,%d}'%(*map(int,xyz),slots[j]) for xyz,j in zip(fixed,ljoint))+'};')
    lines.append('static const FighterJoint joints[]={'+','.join('{%d,%d,%d,0}'%p for p in plan)+'};')
    lines.append('static const int8_t parents[]={'+','.join(map(str,parents))+'};')
    lines.append('static const uint8_t triangle_parts[]={'+','.join(map(str,tags))+'};')
    scale=round(divisor/WORLD_UNIT*4096*cfg['size'])
    lines.append(f'const FighterModel {name}_model={{"{Name}",vertices,{len(tri_vertices)},triangle_textures,'
                 f'textures,{len(model.textures)},local_map,locals,clips,{len(clips)},joints,parents,{len(depths)},'
                 f'{len(slots)},{scale},{{0,0,0}},triangle_parts,{base_mask}u,{512//divisor}}};')
    lines.append(f'const char *const *{name}_clip_names=clip_names;')
    if name=='mario':
        # Fireball 16x16 CI4 sprite of MarioSpecial3 (file 297), as mario_animation.py.
        raw,_=rom_file(rom,297,decoder,scratch);palette=struct.unpack_from('>16H',raw,0x30);pixels=[]
        for byte in raw[0x58:0x58+128]:
            for i in (byte>>4,byte&15):
                c=palette[i];pixels.append((c>>11&31)|((c>>6&31)<<5)|((c>>1&31)<<10)|(32768 if c&1 else 0))
        lines.append('const uint16_t mario_fireball_pixels[256] __attribute__((aligned(4)))={'+','.join(map(str,pixels))+'};')
    lines+=export_moves(name,cfg,clips,float_frames,parents,motions,tables)
    (out/f'{name}_model.c').write_text('\n'.join(lines)+'\n')
    texture_bytes=sum(t['padded_width']*t['padded_height']*2 for t in model.textures)
    report=dict(fighter=Name,model_file=cfg['model'],joints=len(depths),slots=len(slots),triangles=len(model.triangles),
                draw_vertices=len(tri_vertices),local_vertices=len(locals_),textures=len(model.textures),
                texture_bytes=texture_bytes,pose_bytes=total,max_world_error=worst,
                parts=[dict(joint=j,id=i,display_list=descs[j][i],base=base.get(j)==i) for j,i in parts],
                clips=[{k:v for k,v in c.items() if k!='channels'} for c in clips])
    (out/f'{name}-info.json').write_text(json.dumps(report,indent=2))
    header=[f'#ifndef SSB_{name.upper()}_MODEL_H',f'#define SSB_{name.upper()}_MODEL_H','#include "fighter_model.h"',
            '#include "fighter.h"',f'extern const FighterModel {name}_model;',f'extern const char *const *{name}_clip_names;',
            f'extern const Fighter fighter_{name};']
    if name=='mario':header.append('extern const uint16_t mario_fireball_pixels[256];')
    header+=[f'#define {name.upper()}_CLIP_{c["motion"].upper()} {i}' for i,c in enumerate(clips)]
    # Model parts by fighter joint and part id (e.g. LINK_PART_11_1: boomerang in hand).
    header+=[f'#define {name.upper()}_PART_{j}_{i} {k}' for k,(j,i) in enumerate(parts)]
    (out/f'{name}_model.h').write_text('\n'.join(header+['#endif'])+'\n')
    print(f'{Name}: {len(depths)} joints ({len(slots)} slots), {len(model.triangles)} triangles, '
          f'{texture_bytes} texture bytes, {len(clips)} clips, {total} pose bytes, max error {worst:.3f}')
    return report,clips

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--fighter',choices=sorted(FIGHTERS),required=True)
    args=parser.parse_args();convert(args.fighter)

if __name__=='__main__':main()
