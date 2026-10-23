"""Stable 1v1 sound list shared by extract_fight_audio.py and mario_moves.py.

Order must match FightSound in src/versus.h (checked by tests/test_fight_audio.py).
Each entry: (enum suffix, gmFGMVoiceID name, channel priority, max seconds).
"""
SOUNDS=[
    ('SWING_S','nSYAudioFGMLightSwingS',0,.5),
    ('SWING_M','nSYAudioFGMLightSwingM',0,.5),
    ('SWING_L','nSYAudioFGMLightSwingL',0,.5),
    ('SWING_LW','nSYAudioFGMLightSwingLw1',0,.5),
    ('SWING_HEAVY','nSYAudioFGMHeavySwing1',0,.5),
    ('SWING_TORNADO','nSYAudioFGMMarioUnkSwing1',0,.3),
    ('SWING_DAIR','nSYAudioFGMMarioUnkSwing2',0,.3),
    ('PUNCH_S','nSYAudioFGMPunchS',3,.8),
    ('PUNCH_M','nSYAudioFGMPunchM',3,.8),
    ('PUNCH_L','nSYAudioFGMPunchL',3,1.),
    ('KICK_S','nSYAudioFGMKickS',3,.8),
    ('KICK_M','nSYAudioFGMKickM',3,.8),
    ('KICK_L','nSYAudioFGMKickL',3,1.),
    ('COIN','nSYAudioFGMMarioSpecialHiCoin',3,.5),
    ('BURN_S','nSYAudioFGMBurnS',3,.6),
    ('BURN_M','nSYAudioFGMBurnM',3,.6),
    ('BURN_L','nSYAudioFGMBurnL',3,.8),
    ('CATCH','nSYAudioFGMCatch',1,.5),
    ('LAND','nSYAudioFGMMarioLanding',0,.3),
    ('FIRE','nSYAudioFGMMarioSpecialN',1,.3),
    ('UP_JUMP','nSYAudioFGMMarioSpecialHiJump',1,.6),
    ('DOWN_BOUNCE','nSYAudioFGMMarioDownBounce',1,.5),
    ('VOICE_JUMP','nSYAudioVoiceMarioJump',2,.5),
    ('VOICE_JUMP_AIR','nSYAudioVoiceMarioJumpAerial',2,.6),
    ('VOICE_SMASH1','nSYAudioVoiceMarioSmash1',2,.8),
    ('VOICE_SMASH2','nSYAudioVoiceMarioSmash2',2,.8),
    ('VOICE_SMASH3','nSYAudioVoiceMarioSmash3',2,.8),
    ('VOICE_TORNADO','nSYAudioVoiceMarioSpecialLw',2,.8),
    ('VOICE_DAMAGE','nSYAudioVoiceMarioDamage',2,.6),
    ('VOICE_DEAD','nSYAudioVoiceMarioDead',4,.6),
    ('DEAD_SLAM','nSYAudioFGMMarioDeadSlam',4,.8),
    ('DEAD_EXPLODE','nSYAudioFGMDeadExplodeL',4,1.2),
]
NONE=255
SMASH_RANDOM=254  # PlaySmashVoice; runtime picks VOICE_SMASH1..3
# dFTMainHitCollisionFGMs (src/ft/ftmain.c) rows used by Mario: kind -> S/M/L.
HIT_KINDS={0:('PUNCH_S','PUNCH_M','PUNCH_L'),1:('KICK_S','KICK_M','KICK_L'),
           2:('COIN','COIN','COIN'),3:('BURN_S','BURN_M','BURN_L')}
INDEX={key:i for i,(key,*_) in enumerate(SOUNDS)}
BY_FGM={fgm:i for i,(_,fgm,*_) in enumerate(SOUNDS)}

def hit_sound(kind,level):
    row=HIT_KINDS.get(kind)
    return INDEX[row[min(level,2)]] if row else NONE
