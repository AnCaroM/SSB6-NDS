# Estudio: efectos de sonido originales

Estudio previo a convertir sonidos de la ROM. El usuario aprobó las vistas
previas; la integración está en [Prueba14](sonido-prueba14.md). Prueba12 usó efectos sintetizados propios que el usuario
rechazó; Prueba13 los retiró. Este documento evalúa usar los originales.

## Dónde están

Referencia `ssb-decomp-re` en la revisión de `upstream.lock.json`
(`MUSIC_AND_SFX_DISCOVERIES.md`, `src/gm/gmsound.h`, `src/ft/ftmain.c`).

| Archivo (US) | ROM | Contenido |
|---|---|---|
| `B1_sounds2.ctl` | 0xC6B650 | Banco de efectos: 1 instrumento, 322 sonidos, 44,1 kHz |
| `B1_sounds2.tbl` | 0xC7B1F0 | 2,86 MiB de muestras VADPCM (compresión N64) |
| `fgm.ucd` | 0xF5A9C0 | 695 guiones de voz, indexados por `gmFGMVoiceID` |
| `fgm.tbl` | 0xF57BF0 | 464 articulaciones: muestra, tono y envolvente de volumen |
| `fgm.unk` | 0xF573D0 | 100 osciladores (vibrato/trémolo) |

Cadena: `nSYAudioFGM…` → guion `fgm.ucd` (notas, volumen, `fork_voice`
para capas) → articulación `fgm.tbl` (`trigger N`, tono en cents, pasos de
volumen) → `B1_sounds2` sonido `N` → onda `N`. Se comprobó que el sonido `N`
apunta a la onda `N` en los 322 casos. La documentación de la referencia
indica que el enlace `trigger` → banco aún no está confirmado allí; aquí se
deduce por consistencia (las voces de Mario son ondas contiguas 173..184;
aterrizaje, pasos y carrera comparten la onda 1). **Falta confirmarlo de oído.**

Los golpes eligen su sonido con `dFTMainHitCollisionFGMs[tipo][nivel]`
(`ftmain.c`): Punch/Kick/Coin/Burn… × S/M/L. Cada hitbox original lleva
`fgm_kind`/`fgm_level` en sus eventos de ataque, que `tools/mario_moves.py`
todavía no convierte. Saltos, voces y golpes al aire salen de eventos de
sonido en los guiones de animación.

## Selección para Mario 1v1

| Uso | ID original | Ondas | Duración aprox. |
|---|---|---|---|
| Golpe débil/medio/fuerte | PunchS/M/L | 0 + 11 (capas) | 1,0–1,15 s |
| Patada | KickS/M/L | 0 + 16 | ~1,0 s |
| Impacto de bola de fuego | BurnS/M/L | 27 | ~0,7 s |
| Impacto de arriba+B | MarioSpecialHiCoin | 30 + 22 | 0,1 s |
| Lanzar bola de fuego | MarioSpecialN | 19 | 0,05 s |
| Golpe al aire | LightSwingS/M/L | 71, 18 | 0,1 s |
| Agarre | Catch | 11 | 0,4 s |
| Aterrizaje | MarioLanding | 1 | 0,1 s |
| KO | DeadUpStar, DeadExplodeL | 40, 0 | 0,3–2,2 s |
| Voces de Mario | VoiceMarioJump/JumpAerial/Smash1..3/Damage/Dead/SpecialLw | 173..184 | 0,16–0,8 s |

## Coste estimado en DS

Las 21 ondas suman unos 5,8 s a 44,1 kHz.

| Formato | 16 kHz | 22 kHz |
|---|---|---|
| PCM 8 bits | ~91 KiB | ~125 KiB |
| IMA-ADPCM 4 bits | ~45 KiB | ~63 KiB |

Las vistas previas con tono aplicado y capas mezcladas ocupan ~200 KiB en
PCM8 16 kHz porque la explosión de KO dura 2,2 s; se puede recortar. El ARM9
estático de Prueba13 ocupa ~1,58 MiB; 50–120 KiB más caben en el presupuesto
de 2,5 MiB, pero conviene medir RAM real en DS antes de más crecimiento.

En DS: `soundPlaySample` de libnds acepta PCM8/PCM16/IMA-ADPCM en los 16
canales de hardware. `platform/nds/fight_audio.c` (Prueba12) ya limita 4
canales con prioridad y está probado con hardware simulado; puede reutilizarse.
maxmod 2.1.0 también está instalado si hiciera falta mezcla.

## Conversión propuesta (fuera de la DS)

1. Extraer ondas con el decodificador VADPCM de la referencia
   (`tools/audio_codec.py`, `tools/extract_ctl_bank.py`).
2. Aplicar offline el tono (cents de nota + articulación) remuestreando, y
   la envolvente de volumen; mezclar capas `fork_voice` en una sola muestra.
3. Guardar IMA-ADPCM o PCM8 a 16 kHz en `build/audio/`, nunca en el repo.
4. Convertir `fgm_kind/fgm_level` por hitbox y los eventos de sonido de los
   guiones de animación; el combate sólo emite eventos (ya existe
   `FightSoundEvent` en `versus.h`) y se prueba en PC.

## Incertidumbres

- Tono de reproducción: se supone 44,1 kHz a 0 cents. Las voces llevan
  ~−1200 cents (una octava abajo); escuchar las vistas previas lo confirma.
- Semántica exacta de notas repetidas, `set_unk1E/1F` y temporizadores de los
  guiones: no se ha leído el intérprete completo (`n_env.c`). Las vistas
  previas sólo usan la primera nota de cada capa, sin envolvente.
- Calidad a 16 kHz / 4 bits en el altavoz de la DS: probar en hardware.

## Vistas previas locales

`build/audio-estudio/` (ignorado por git, generado de la ROM del usuario):
`original-44khz/` son las ondas decodificadas sin tono; `version-ds-16khz/`
aplica tono y capas a 16 kHz 8 bits, como sonaría en DS.
