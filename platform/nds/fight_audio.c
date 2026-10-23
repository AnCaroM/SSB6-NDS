/* Four bounded hardware PCM channels, no software mixing or allocation. */
#include <nds.h>
#include "fight_audio.h"
#include "fight_samples.h"
static struct {int id;unsigned left,priority;} slots[4];
void fight_audio_stop(void)
{
    for(unsigned i=0;i<4;++i) {
        soundKill(slots[i].id);slots[i].id=-1;slots[i].left=0;
    }
}
void fight_audio_init(void)
{
    soundEnable();
    for(unsigned i=0;i<4;++i)slots[i].id=-1;
    for(unsigned i=0;i<SOUND_COUNT;++i)
        DC_FlushRange(fight_samples[i].data,fight_samples[i].bytes);
}
void fight_audio_tick(void)
{
    for(unsigned i=0;i<4;++i)if(slots[i].left && !--slots[i].left) {
        soundKill(slots[i].id);slots[i].id=-1;
    }
}
void fight_audio_events(const Versus *match)
{
    for(unsigned e=0;e<match->sound_count;++e) {
        const FightSoundEvent *event=&match->sounds[e];
        if(event->sound>=SOUND_COUNT)continue;
        const FightSample *sample=&fight_samples[event->sound];
        unsigned choice=0;
        for(unsigned i=0;i<4;++i) {
            if(!slots[i].left) {choice=i;break;}
            if(slots[i].priority<slots[choice].priority ||
               (slots[i].priority==slots[choice].priority && slots[i].left<slots[choice].left))choice=i;
        }
        if(slots[choice].left && slots[choice].priority>sample->priority)continue;
        soundKill(slots[choice].id);
        float x=match->fighter[event->fighter].player.x;
        int pan=64+(int)(x/100);if(pan<32)pan=32;if(pan>96)pan=96;
        int id=soundPlaySample(sample->data,SoundFormat_8Bit,sample->bytes,
                              FIGHT_AUDIO_RATE,sample->volume,(unsigned)pan,false,0);
        /* Hardware can reuse an already completed channel before our tick
         * estimate expires. Transfer ownership so its old slot cannot stop it. */
        if(id>=0)for(unsigned i=0;i<4;++i)if(i!=choice && slots[i].id==id) {
            slots[i].id=-1;slots[i].left=0;
        }
        slots[choice].id=id;slots[choice].left=id<0?0:sample->ticks;
        slots[choice].priority=sample->priority;
    }
}
