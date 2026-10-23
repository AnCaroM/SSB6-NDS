#include "animation_review.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    Versus v={0};
    for(unsigned sequence=0;sequence<12;++sequence) {
        int hit=0,reaction=0;
        for(unsigned frame=0;frame<144*3;++frame) {
            animation_review_step(&v,sequence*144*3+frame);
            hit|=v.fighter[0].hits!=0;reaction|=versus_damage_pose(&v,1)>=0;
            assert(v.fighter[0].attack_facing==(sequence<6?1:-1));
        }
        assert(hit && reaction); /* Every scripted attack contacts both ways. */
    }
    puts("PASS: six emulator review scenarios contact and react in both directions");
}
