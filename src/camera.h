#ifndef SSB_CAMERA_H
#define SSB_CAMERA_H
#include <stdint.h>

/* World coordinates in sixteenths; no allocation or float math in the solver. */
typedef struct { int32_t x, y; int facing; } CameraFighter;
typedef struct { int32_t x, y, height; int ready; } FightCamera;
void camera_reset(FightCamera *camera);
void camera_step(FightCamera *camera, const CameraFighter *fighters, unsigned count);
#endif
