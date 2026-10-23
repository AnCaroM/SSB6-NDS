# Prueba14: sonidos originales

Prueba14 añade efectos y voces originales de Super Smash Bros. 64 al 1v1 de
Prueba13. Combate, animaciones, cámara y menú no cambian. Las versiones
anteriores se conservan. **Aún no se ha escuchado en DS.**

Los sonidos se convierten durante la compilación desde la ROM propia del
usuario (`build/audio/`). No se incluyen en el repositorio y el `.nds`
resultante contiene audio original: es sólo para uso local.

## Qué suena

| Momento | Sonido original | Procedencia |
|---|---|---|
| Golpes al aire | LightSwingS/M/L, LightSwingLw1, MarioUnkSwing1/2 | guion de animación de cada ataque, en su tic |
| Golpe conectado | PunchS/M/L, KickS/M/L, Coin | `fgm_kind`/`fgm_level` de cada hitbox → `dFTMainHitCollisionFGMs` |
| Impacto de bola de fuego | BurnS | atributos de MarioSpecial1 (`sfx` 28) |
| Lanzar bola / arriba+B / lanzamiento atrás | MarioSpecialN, MarioSpecialHiJump, MarioDownBounce | guiones Fireball, SpecialHi, ThrowB |
| Agarre | Catch | guion Catch |
| Salto / salto aéreo | VoiceMarioJump / VoiceMarioJumpAerial | guiones Jump / JumpAerial |
| Aterrizaje | MarioLanding | guiones LandingAirX |
| Smash, jab3, ataques de borde | VoiceMarioSmash1..3 | `PlaySmashVoice` → `attr->smash_sfx[]` |
| Tornado, lanzamiento atrás | VoiceMarioSpecialLw | guiones |
| Daño fuerte | VoiceMarioDamage | `ftCommonDamage`: aturdimiento ≥ 80 |
| KO | VoiceMarioDead, MarioDeadSlam, DeadExplodeL | `ftCommonDead`: `attr->dead_fgm_ids` + explosión |

Referencia: `ssb-decomp-re` revisión `572a747d8171137718fad472c5d7473dc7309ffd`
(`202_MarioMainMotion.c`, `203_MarioMain.c`, `204_MarioSpecial1.c`,
`ftmain.c`, `ftcommondamage.c`, `ftcommondead.c`). Más detalle del formato en
[estudio-sonido.md](estudio-sonido.md).

## Conversión

`tools/extract_fight_audio.py` decodifica VADPCM con el decodificador de la
referencia y sigue cada ID por `fgm.ucd` → `fgm.tbl` → banco `B1_sounds2`.
Para cada articulación toma la primera nota, aplica su tono (cents) al
remuestrear a 16 kHz y mezcla las capas `fork_voice`. Resultado: PCM 8 bits
mono, 32 sonidos, 225.972 bytes. Es la misma aproximación que el usuario
aprobó al escuchar las vistas previas.

**Aproximaciones locales:** no se reproducen las envolventes de volumen, los
osciladores (vibrato/trémolo) ni la repetición de notas de los guiones; cada
sonido se normaliza y luego se escala con el volumen del guion original y se
recorta a su duración máxima con un fundido de 40 ms. La voz de smash original
es aleatoria; aquí rota Smash1→2→3. El KO no distingue el KO estrella.
Faltan pasos, carrera, escudo, esquivas, multitud, locutor y música.

## Reproducción en DS

`platform/nds/fight_audio.c` (de Prueba12) reproduce PCM8 con
`soundPlaySample`, máximo 4 canales de hardware, prioridad
golpes/KO > voces > agarres/fuego > golpes al aire/aterrizaje. Pausar,
reiniciar, cambiar de modo o volver al menú detiene los sonidos. Sin mezcla
por software ni asignaciones por cuadro.

## Coste

ARM9 estático 1.895.660 bytes (1,81 MiB; antes 1,58 MiB) de un presupuesto de
2,5 MiB; audio 225.972 bytes. Texturas y geometría sin cambios
(944 polígonos / 2881 vértices). El ELF no mide el pico real de RAM.

## Validación

- Pruebas en PC: el combate emite el golpe al aire del jab en su tic, el
  sonido de la hitbox al conectar, voces de salto y salto aéreo, aterrizaje,
  los tres sonidos del KO y la rotación de las tres voces de smash.
- El enum de C (`src/fight_sound.h`) coincide con `tools/fight_sounds.py`;
  la tabla de golpes coincide con la original; el remuestreo por cents se
  comprueba con un tono sintético.
- Canales/prioridades/propiedad de canal reutilizado con hardware simulado.

Pendiente: escuchar en melonDS y en DS, comprobar volumen relativo y que no
haya cortes con 4 canales; medir Trabajo/Pico.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --compact-poses`
