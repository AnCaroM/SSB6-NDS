#ifndef SSB_STAGE_H
#define SSB_STAGE_H
/* Training surfaces and ROM-derived Dream Land geometry. */
typedef struct { float left, right, height; } StagePlatform;
typedef struct { float x,y; } StagePoint;
typedef enum { STAGE_FLAT, STAGE_GARDEN, STAGE_DREAMLAND } StageId;
const StagePlatform *stage_platforms(StageId stage, unsigned *count);
float stage_blast_bottom(StageId stage);
const StagePoint *stage_island(StageId stage,unsigned *count);
/* Sweeps Mario's collision diamond against the static convex island. */
void stage_resolve_solid(StageId stage,float old_x,float old_y,
                         float *x,float *y,float *vx,float *vy);
#endif
