#ifndef FIGHT_AUDIO_H
#define FIGHT_AUDIO_H
#include "versus.h"
void fight_audio_init(void);
void fight_audio_tick(void);
void fight_audio_events(const Versus *match);
void fight_audio_stop(void);
#endif
