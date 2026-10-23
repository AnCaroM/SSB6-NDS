/* Minimal hardware boundary for host tests, never a DS build include path. */
#include <stdbool.h>
enum {SoundFormat_8Bit};
void soundEnable(void);
void soundKill(int id);
void DC_FlushRange(const void *data,unsigned bytes);
int soundPlaySample(const void *data,int format,unsigned bytes,unsigned short freq,
                    unsigned char volume,unsigned char pan,bool loop,unsigned short point);
