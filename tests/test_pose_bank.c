#include "mario_mesh.h"
#include "pose_sample.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(sizeof(MarioPose)==1 && mario_pose_vertex_count==189 && mario_clip_count==50);
    FILE *out=fopen("build/scene/poses-decoded.bin","wb");assert(out);
    for(unsigned clip=0;clip<mario_clip_count;++clip) {
        const MarioClip *c=&mario_clips[clip];
        for(unsigned t=0;t<c->ticks*3;++t) {
            PoseSample p=pose_sample(t,c->ticks,c->frame_step,c->frames,c->loop);
            assert(p.a<c->frames && p.b<c->frames);
            if(p.part)assert(p.span && p.part<=p.span);
        }
        for(unsigned f=0;f<c->frames;++f)for(unsigned i=0;i<mario_pose_vertex_count*3;++i) {
            int16_t value=pose_decode(c->positions[f*mario_pose_vertex_count*3+i],c->low[i%3],c->span[i%3]);
            unsigned bits=(uint16_t)value;fputc(bits&255,out);fputc(bits>>8,out);
        }
    }
    fclose(out);puts("PASS: compiled 8-bit pose bank, every clip sample, playback bounds and integer decode");
    return 0;
}
