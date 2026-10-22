#include "stage.h"

static const StagePlatform garden[] = {
    {-900, 900, 0},
    {-740, -220, 500},
    {220, 740, 500},
    {-260, 260, 1000}
};

/* Original horizontal ground lines from resource 104, vertices at 0x1D64,
 * links at 0x1E08, ground line IDs 0..3. Reordered to keep island at index 0.
 * Static walls/ceiling contour below uses the remaining line IDs 4..6. */
static const StagePlatform dreamland[] = {
    {-2318,2318,0}, {-1841,-951,904}, {951,1892,907}, {-570,570,1542}
};

static const StagePoint island[] = {
    {-2318,0},{2318,0},{2307,-124},{2290,-331},{2075,-834},
    {1972,-1072},{-1972,-1072},{-2075,-834},{-2290,-331},{-2307,-124}
};

const StagePoint *stage_island(StageId stage,unsigned *count)
{
    *count=stage==STAGE_DREAMLAND?sizeof island/sizeof *island:0;
    return island;
}

void stage_resolve_solid(StageId stage,float old_x,float old_y,
                         float *x,float *y,float *vx,float *vy)
{
    unsigned count;const StagePoint *points=stage_island(stage,&count);
    if(!count)return;
    /* Swept convex half-planes, expanded by Mario's original diamond:
     * top=320, center=190, bottom=0, width=150 (203_MarioMain).
     * At corners this conservative expansion is a reduced approximation.
     * Up to three contacts slide along a wall without per-frame allocations. */
    float px=old_x,py=old_y,dx=*x-old_x,dy=*y-old_y;
    for(int pass=0;pass<3;++pass) {
        float first=0,last=1,nx=0,ny=0;int face=-1,miss=0;
        for(unsigned i=0;i<count;++i) {
            StagePoint a=points[i],b=points[(i+1)%count];
            float n_x=a.y-b.y,n_y=b.x-a.x;
            float support=0,v=n_y*320;
            if(v<support)support=v;
            v=n_x*150+n_y*190;if(v<support)support=v;
            v=-n_x*150+n_y*190;if(v<support)support=v;
            float limit=n_x*a.x+n_y*a.y-support;
            float distance=n_x*px+n_y*py-limit,rate=n_x*dx+n_y*dy;
            /* Parallel/tangent motion on an exterior face is not penetration. */
            if(distance>=0 && rate>=0) {miss=1;break;}
            if(rate<0) {
                float t=-distance/rate;
                if(t>=first) {first=t;face=(int)i;nx=n_x;ny=n_y;}
            } else if(rate>0) {
                float t=-distance/rate;if(t<last)last=t;
            }
        }
        if(miss || face<0 || first>last || first>1 || last<0) {
            px+=dx;py+=dy;break;
        }
        if(first<0)first=0;
        px+=dx*first;py+=dy*first;
        dx*=1-first;dy*=1-first;
        float length2=nx*nx+ny*ny;
        float into=(dx*nx+dy*ny)/length2;
        if(into<0) {dx-=nx*into;dy-=ny*into;}
        into=(*vx*nx+*vy*ny)/length2;
        if(into<0) {*vx-=nx*into;*vy-=ny*into;}
        /* Tiny outward separation avoids repeated contacts from float error. */
        if(nx>0)px+=.001f;else if(nx<0)px-=.001f;
        if(ny>0 && nx!=0)py+=.001f;else if(ny<0)py-=.001f;
    }
    *x=px;*y=py;
}

const StagePlatform *stage_platforms(StageId stage, unsigned *count)
{
    if(stage==STAGE_DREAMLAND) {
        *count=sizeof dreamland/sizeof *dreamland;
        return dreamland;
    }
    *count = stage == STAGE_GARDEN ? sizeof garden / sizeof *garden : 1;
    return garden;
}

float stage_blast_bottom(StageId stage)
{
    /* GRPupupuMap MPGroundData.map_bound_bottom. */
    return stage==STAGE_DREAMLAND ? -3500 : -1800;
}
