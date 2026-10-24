"""Read a fighter's motion table (src/ft/ftdata.c) from the pinned reference.

Each motion ID (common FTCommonMotion + fighter-specific ftXMotion) maps to an
animation file (relocFileDescriptions name -> file ID), a motion script
symbol and FTANIM flags. Table-driven so every fighter shares one converter.
"""
import re
from pathlib import Path
from mario_moves import us_source

ROOT=Path(__file__).resolve().parents[1]
UPSTREAM=ROOT/'upstream/ssb-decomp-re'
FLAGS={'FTANIM_FLAG_TRANSN_JOINT':0x80000000,'FTANIM_FLAG_XROTN_JOINT':0x40000000,
       'FTANIM_FLAG_YROTN_JOINT':0x20000000,'FTANIM_FLAG_SUBMOTION_SCRIPT':0x10,
       'FTANIM_FLAG_ANIMJOINT':8,'FTANIM_FLAG_TRANSLATE_SCALES':4,'FTANIM_FLAG_SHIELDPOSE':2,
       'FTANIM_FLAG_ANIMLOCKS':1,'FTANIM_FLAG_NONE':0}
# ftMain hidden-joint insertion (AGENTS.md, Prueba09): bit30 (named XROTN)
# inserts TransN, bit31 (named TRANSN) inserts XRotN, bit28 enables joint28.
INSERT_TRANSN=0x40000000;INSERT_XROTN=0x80000000;CATCH_JOINT=0x10000000

def enum_values(text,name):
    body=re.search(r'typedef enum '+name+r'\s*\{(.*?)\}',text,re.S)[1]
    body=re.sub(r'/\*.*?\*/','',re.sub(r'//[^\n]*','',body),flags=re.S)
    values={};current=-1
    for item in body.split(','):
        item=item.strip()
        if not item:continue
        if '=' in item:
            key,expr=(x.strip() for x in item.split('=',1))
            current=values[expr] if expr in values else int(expr,0)
        else:key=item;current+=1
        values[key]=current
    return values

def file_ids():
    text=(UPSTREAM/'tools/relocFileDescriptions.us.txt').read_text()
    return {name:int(fid) for fid,name in re.findall(r'^-(\d+):\s*(\S+)',text,re.M)}

def motions(fighter,table):
    """{motion name: dict(index, file, script, flags)}; fighter like 'Link'."""
    common=enum_values((UPSTREAM/'src/ft/ftdef.h').read_text(),'FTCommonMotion')
    special=enum_values((UPSTREAM/f'src/ft/ftchar/ft{fighter.lower()}/ft{fighter.lower()}.h').read_text().replace(
        'nFTCommonMotionSpecialStart',str(common['nFTCommonMotionSpecialStart'])),f'ft{fighter}Motion')
    names={}  # index -> every enum alias (e.g. AttackAirStart and AttackAirN)
    for key,value in common.items():names.setdefault(value,[]).append(key.replace('nFTCommonMotion',''))
    for key,value in special.items():names[value]=[key.replace(f'nFT{fighter}Motion','')]
    source=us_source((UPSTREAM/'src/ft/ftdata.c').read_text())
    body=re.search(r'FTMotionDesc '+table+r'\[\]\s*=\s*\{(.*?)\n\};',source,re.S)[1]
    ids=file_ids();result={}
    # Entries are {anim, script, flags} triplets; some are written without braces.
    values=[v.strip() for v in re.sub(r'/\*.*?\*/','',body.replace('{',' ').replace('}',' '),flags=re.S).split(',') if v.strip()]
    if len(values)%3:raise ValueError('Motion table is not a list of triplets')
    for index in range(len(values)//3):
        anim,script,flags=values[index*3:index*3+3]
        match=re.fullmatch(r'&ll(\w+)FileID',anim)
        value=0
        for part in flags.split('|'):
            part=part.strip();value|=FLAGS[part] if part in FLAGS else int(part,0)
        entry=dict(index=index,file=ids[match[1]] if match else None,
                   script=None if script.startswith('0x8') or script=='0x00000000' else script,flags=value)
        for name in names.get(index,[f'Motion{index}']):result[name]=entry
    return result
