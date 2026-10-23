#include "fight_audio.h"
#include "nds.h"
#include <assert.h>
#include <stdio.h>
static int active[16],plays,flushes,last_id=-1;
static unsigned live(void)
{unsigned n=0;for(unsigned i=0;i<16;++i)n+=active[i]!=0;return n;}
void soundEnable(void) {}
void soundKill(int id) {if(id>=0) {assert(id<16);active[id]=0;}}
void DC_FlushRange(const void *data,unsigned bytes)
{assert(data && bytes && !(bytes&3));++flushes;}
int soundPlaySample(const void *data,int format,unsigned bytes,unsigned short freq,
                    unsigned char volume,unsigned char pan,bool loop,unsigned short point)
{
    assert(data && format==SoundFormat_8Bit && bytes && freq==16000);
    assert(volume<=127 && pan>=32 && pan<=96 && !loop && !point);
    for(int i=0;i<16;++i)if(!active[i]) {active[i]=1;++plays;last_id=i;return i;}
    return -1;
}
static void event(Versus *v,unsigned sound)
{v->sound_count=1;v->sounds[0]=(FightSoundEvent){sound,0};fight_audio_events(v);}
int main(void)
{
    Versus v={0};fight_audio_init();assert(flushes==SOUND_COUNT);
    for(int i=0;i<12;++i)event(&v,SOUND_SWING);
    assert(live()==4 && plays==12);fight_audio_stop();assert(!live());
    for(int i=0;i<4;++i)event(&v,SOUND_KO);
    int before=plays;event(&v,SOUND_SWING);assert(plays==before && live()==4);
    fight_audio_stop();
    event(&v,SOUND_LAND);
    for(int t=0;t<7;++t)fight_audio_tick();
    active[last_id]=0; /* Sample ended in hardware before its slot estimate. */
    event(&v,SOUND_KO);int ko=last_id;
    for(int t=0;t<3;++t)fight_audio_tick();
    assert(active[ko] && live()==1); /* Old slot must not kill reused channel. */
    for(int t=0;t<45;++t)fight_audio_tick();assert(!live());
    event(&v,SOUND_COUNT);assert(!live());
    puts("PASS: bounded audio channels, priority, expired-channel ownership, PCM parameters and cleanup (mock hardware)");
}
