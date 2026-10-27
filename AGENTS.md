# Project constraints

- Target Nintendo DS original / DS Lite. The user owns a DS and DSPico and
  reported that Prueba01 runs very well on that hardware. Preserve that working
  binary when packaging later builds; new builds are not yet hardware-verified.
- Keep the DS limits central to every implementation choice: ARM9 about 67 MHz,
  ARM7 about 33 MHz, 4 MiB shared main RAM, no ARM9 hardware floating-point unit.
  Do not rely on DSi RAM or clock speed.
- Hardware geometry storage is capped at 2048 polygons and 6144 vertices per
  frame. Reserve headroom for clipping, stage, effects and UI. Initial project
  budget: <=1400 submitted polygons and <=4000 submitted vertices per frame.
- VRAM totals 656 KiB in specialized banks, not an interchangeable texture heap.
  Garden builds use bank A (128 KiB) for textures. Dream Land builds explicitly
  use A+B (256 KiB) for textures, E for palettes, and C for the sub-screen console.
  Keep texture allocation explicit.
- Prefer shared model/texture/animation data, fixed-point or bounded integer
  runtime work, offline asset conversion, and no per-frame allocation. Avoid
  per-frame trigonometry and floating-point skinning. Current animation sampling
  trades RAM for CPU: measure growth before adding many clips or fighters.
- Goal is 60 Hz with frame work within roughly 16.7 ms, not an unverified promise.
  Check ARM9 ELF text/data/BSS, texture bytes, geometry submissions and host tests
  for each substantive build. ELF size excludes stack, heap, ARM7 and other RAM.
  Emulator [60/60] does not establish hardware frame time or peak RAM.
- Maintain an explicit distinction between original-game code/data, converted
  assets, and provisional integration. No claim that the full game is ported.
- REA has been studied, not executed. No REA MCP tools are connected in this
  session. Its local Windows Ghidra P0 documentation excludes MIPS targets;
  raw N64 ROM support is not established. Do not claim to have used REA merely
  because its README or skill was read. Use source tools for repository analysis.
- See docs/limites-ds.md for budgets and docs/prueba-ds-dspico.md for hardware use.
- The user requested an original stage: prefer ROM-derived Dream Land for the
  new stage build. Identify the garden as a provisional test map, not original.
- User tested both Prueba03 camera views on DS: full-stage view is too distant;
  close view is comfortable. Prefer automatic fighter framing, smooth tracking
  and separation-based zoom; preserve previous packaged builds. See
  docs/camara-prueba04.md for source provenance and DS-specific adjustments.
- User approved Prueba04 camera in emulator; preserve its parameters while
  extending stage collisions. Prueba05 adds a reduced island wall/ceiling solver
  and ledge hold/climb/drop, with a single shared original CliffWait pose.
- Prueba06 adds one air jump, original JumpAerialF/B sampled every two ticks
  and full eight-tick CliffCatch. Keep the camera parameters and preserved
  packaged builds. Scene air steering/instant turning and climb remain local
  approximations, not exact N64 physics. Recovery demo shares host-tested input.
- Prueba07 maps L to recovery-only Super Jump Punch, original TransN curve and
  poses, followed by special fall/25-tick landing. Multi-hit damage, effects,
  sound and invulnerability are pending. Preserve camera and older packages.
- Prueba08 adds Mario's attack catalog and original offline bone hit positions.
  Poses use 189 unique positions plus an index map (960 draw vertices retained).
  B is now special, up+B recovery, L grab, Y+X smash; START+SELECT resets.
  Preserve camera parameters and all previously delivered binaries. Target hurt
  capsule, damage integration and grab/recovery scheduling remain reduced.
- Prueba09 fixes hidden-joint traversal: source XROTN macro bit30 inserts TransN;
  source TRANSN macro bit31 inserts XRotN (macro names disagree with bitfields).
  Catch bit28 adds joint28. Decode helpers before assigning common skeleton clips.
  Retain exact non-loop endpoints and integer interpolation of sampled poses.
  Dash/run/brake and CapturePulled now have separate original clips. Physical
  validation and complete capture rotation/throw pairing remain pending.
- User confirmed Prueba09 animations look correct. Prueba10 is a separate
  Mario-vs-CPU 3-stock build (--demo versus --compact-poses). Both sides share
  assets, run physics before contact, then commit damage. Host tests include
  trades, grabs, immunity, wins, CPU recovery and bounded self-play. Do not claim
  its hardware performance yet. New format quantizes poses per clip/axis with
  <=2.1 world-unit axis error and integer decode; keep full-pose build option.
  CPU/GPU profiling must be measured on DS; Timer0 is reserved by this build,
  system timers2/3 remain owned by Calico. Future four-player mode is a study,
  not implemented or performance-proven. See docs/optimizacion-y-escalado.md.
- Prueba11 adds a title screen, mode select (MODO DE PRUEBA, CPU vs CPU demo,
  controls) and colored HUD over the unchanged Prueba10 match. Menu flow is
  SDK-free in src/menu.c and host-tested. No new VRAM banks or textures; the
  title emblem is 26 untextured polygons. Compiles for DS; not yet emulated or
  hardware-tested. Consume transition-held buttons until release to prevent
  confirmation/retry A from jumping. Shared controls text is checked against
  the 32-column console layout. See docs/menu-prueba11.md.
- Prueba12 adds ROM-derived damage reactions (resources523..539; FlyRoll
  converted but reserved), source hitlag/angle thresholds and independent damage
  impulse in 1v1. Preserve normal gravity, source air decay1.7 and flat ground
  traction*.25. Landing recovery6 is provisional; full tumble/tech/DI are absent.
  Seven own synthesized PCM8 effects share a 27360-byte bank, max4 hardware
  channels. They are not original game sounds or voices. Host mock tests cover
  priority and channel reuse; audio/feel/performance still need physical DS.
  Preserve camera/menu behavior and all old packages. See docs/impactos-prueba12.md.
- User disliked Prueba12 sound/impact animation. Prueba13 removes synthesized
  audio from the versus binary. Sample all SPECS attacks and damage523..540 at
  every original tick: vertex interpolation across two ticks loses fast joint
  rotations. Keep compact decode error<=2.1 and shared bank<=1500KiB. Damage
  starts at authored recoil tick1, can finish after stun without extending input
  lock, continues strong air launches with DamageFall540, exits via a short
  recovery blend. Separate --animation-review ROM scripts six attacks mirrored
  at one-third speed for melonDS inspection; never package it as the playable
  release. See docs/animaciones-prueba13.md for validation and limits.
- User approved the original-sound previews. Prueba14 converts 32 ROM sounds
  (B1_sounds2 via fgm.ucd/fgm.tbl, first note per articulation, pitch-resampled,
  fork layers mixed; no envelopes/LFOs) to PCM8 16 kHz in build/audio, never
  published. Hit sounds come from hitbox fgm_kind/level, swings/voices from motion
  script events; smash voice rotates instead of random. Keep fight_sound.h and
  tools/fight_sounds.py in the same order. Not yet heard on DS. See
  docs/sonido-prueba14.md and docs/estudio-sonido.md.
- Prueba15 (step 1 of adding Link) renders Mario from a bone-pose bank:
  per-tick local joint channels (only varying ones stored), rebuilt with DS
  matrix commands into stack slots 3..29 and rigid per-vertex glRestoreMatrix.
  232563 bytes vs 1.25 MiB; ARM9 static 823276. Max 0.35 units offline, 1.33
  in host fixed-point emulation (bound 2.1). Build with --skeletal-poses; keep
  compact/16-bit options. Not yet seen in emulator or DS. Next: generalize the
  converter for Link's model/animations. See docs/huesos-prueba15.md.
- Prueba16 (step 2 of Link) adds tools/convert_fighter.py: table-driven
  (FTMotionDesc in ftdata.c, some entries unbraced; FTHiddenPart decides which
  joints each animation table lists). It reproduces Mario's Prueba15 bone bank
  exactly. Link: tree @0x3AE8, 32 joints, 19 matrix slots, 72 clips, 248176
  pose bytes, 49920 texture bytes, shown in a menu viewer via
  platform/nds/fighter_render.c. Model-part swaps from motion scripts are per-clip
  (tick, mask) lists; movement-only TransN (root 2) is drawn only on request
  (viewer ledge clips). Faces, items and Link combat are pending. Not yet seen in emulator/DS.
  See docs/link-prueba16.md.
- Prueba17 (step 3 of Link): src/fighter.h holds per-fighter data; physics,
  combat and versus read it (MotionScene.attr, CombatScene.fighter). Attributes
  are copied FTAttributes (test_fighter_attr.py). convert_fighter.py emits moves,
  clip map, sounds and throws; for Mario they equal the legacy mario_moves.c and
  all recorded Mario traces stay byte-identical (check them on every change).
  The 1v1 links only generic models; legacy Mario remains for old demos via
  src/fighter_mario_legacy.c. Link: spin attack (ground/air), sword hits and
  sounds; boomerang/bomb pending (Prueba18). Not yet seen in emulator/DS.
  See docs/link-prueba17.md.
- Prueba18: crouch (SCENE_SQUAT, MotionScene.squat_held set by combat) for both
  fighters; Link boomerang (spawn flag0 tick26, decay/return/homing by fixed-angle
  rotation, catch, 9/8 dmg) and bomb item (pull tick29, held, LightThrow table x.6,
  release tick8, fuse 300, contact/explosion) in combat.c, reduced: no self-damage,
  pickup, reflection or map collision for the boomerang; provisional bomb drawing.
  Mario traces stay byte-identical. See docs/link-prueba18.md.
- Prueba19: shield, rolls, techs, knockdown and shield break for both fighters
  as CombatScene.action (fighter.h FighterActionId; FighterAction table emitted
  by convert_fighter.py: clip, intangible window from SetHitStatusAll, flag1
  turn, tick-0 sounds, per-tick TransN dx for movement clips). FTANIM bit 29
  (YRotN, GuardOn/GuardOff) creates a joint animated at run time from
  shield_anim_joints: no table in the file, so only bits 31/30 insert aux
  tables (the sampler can compose several prefixes). Shield 55, -1/16 ticks up, +1/10 down,
  radius (.65h/55+.35)*shield_size at YRotN; blocked hit = setoff (dmg*1.62+4
  ticks, x2 push) or break (vy shield_break_vel_y, FuraFura 400-pct+90, mash -4).
  Landing while stunned/tumbling: tech if R within 20 ticks else DownBounce.
  R is now the shield; the map toggles by touch. The island solid is the exact
  hull of the island swept by the body diamond (no hovering past ledges);
  recovery/special traces re-baselined, versus.csv changes (CPU shields),
  other Mario traces stay byte-identical. The menu's Link viewer is replaced by
  an explore mode (Versus.solo: one fighter, no contact, endless lives). Not
  yet seen on DS. See docs/escudo-prueba19.md.
