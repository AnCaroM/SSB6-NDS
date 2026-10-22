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
