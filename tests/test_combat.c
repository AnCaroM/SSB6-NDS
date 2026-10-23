#include "combat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    CombatScene s;
    combat_init(&s);
    combat_step(&s,(SceneInput){0,0},1,0);
    assert(s.attack_frame == 0 && s.percent == 0);
    combat_step(&s,(SceneInput){0,0},1,0);
    assert(s.percent == 0); /* Startup cannot hit. */
    combat_step(&s,(SceneInput){0,0},1,0);
    assert(s.percent == 2 && s.hits == 1 && s.hitstop == 5);
    float x = s.target.x;
    for (int i=0;i<5;++i) combat_step(&s,(SceneInput){0,0},1,0);
    assert(s.target.x == x && s.attack_frame == 2); /* Frozen during hitstop. */
    for (int i=0;i<50;++i) combat_step(&s,(SceneInput){0,0},1,0);
    assert(s.hits == 1 && s.percent == 2 && s.target.x > x); /* No multihit or held auto-repeat. */
    combat_init(&s);
    s.target.x = -600;
    for (int i=0;i<30;++i) combat_step(&s,(SceneInput){0,0},i==0,0);
    assert(s.hits == 0); /* Target behind attack. */
    s.target.x = 1500;
    for (int i=0;i<30;++i) combat_step(&s,(SceneInput){0,0},i==0,0);
    assert(s.hits == 0); /* Target out of range. */
    combat_init(&s); s.target.y = 1000;
    for (int i=0;i<10;++i) combat_step(&s,(SceneInput){0,0},i==0,0);
    assert(s.hits == 0); /* A fighter overhead is not hit by the ground proxy. */
    assert(combat_jab_knockback(100) > combat_jab_knockback(0));
    assert(fabsf(combat_jab_knockback(0)-17.28f) < .0001f);
    combat_init(&s); s.target.state = SCENE_AIR; s.target.y = -1801;
    s.percent=50;
    combat_step(&s,(SceneInput){0,0},0,0);
    assert(s.knockouts == 1 && s.percent == 0 && s.target.x == 180);
    CombatScene replay;
    combat_init(&s); combat_init(&replay);
    for (int t=0;t<500;++t) {
        SceneInput input = {t%100<30 ? 24 : 0,t%150<8};
        combat_step(&s,input,t%35==0,0); combat_step(&replay,input,t%35==0,0);
        assert(s.percent == replay.percent && s.hits == replay.hits && s.player.x == replay.player.x && s.target.x == replay.target.x);
        assert(isfinite(s.target.x) && isfinite(s.target.y));
    }
    puts("PASS: jab startup, active window, hitstop, one hit, range/facing, drift, percent scaling, KO and replay");
    return 0;
}
