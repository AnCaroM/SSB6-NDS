# Prueba20: optimizaciones y Kirby

**Aún no se ha visto en DS real.** Incluye todo lo de la Prueba19 (escudo,
rodar, explorar el mapa).

## Optimizaciones

Medidas en melonDS con la ROM de prueba (`build_nds.py --autoplay`: CPU
contra CPU, Link contra Link, el caso más pesado) y `tools/bench_melonds.py`,
que lee los tiempos de la memoria del emulador. Son tiempos del emulador, no
del hardware; sirven para comparar versiones.

| Por cuadro | Antes | Ahora |
|---|---:|---:|
| Dibujar los dos personajes | 5,0 ms | 1,7 ms |
| Media total | 6,1 ms | 2,7 ms |
| Pico (sin el arranque) | 8–9 ms | 5 ms |

- **Listas de dibujo precalculadas.** Los vértices, colores y coordenadas de
  textura de un personaje no cambian: solo las matrices de los huesos. Cada
  pieza del modelo se guarda como lista de comandos del motor 3D y se envía
  por DMA; el procesador solo calcula los huesos.
- **Texturas por combate.** Solo los dos personajes en pantalla ocupan la
  memoria de vídeo; los demás se descargan. El presupuesto comprueba el peor
  par junto con Dream Land.
- **Audio.** Probé IMA-ADPCM (la mitad de memoria) y lo descarté: medido
  contra el original, varios sonidos quedaban con mucho ruido (relación
  señal/ruido de 12 dB frente a 35 dB del formato actual). Sigue en PCM8.

## Tamaño de los personajes

El juego escala cada luchador por `FTAttributes.size` (Mario 1,12, Link 1,24,
Kirby 0,91), que no se aplicaba. Ahora escala el modelo, las posiciones de
los golpes, los puntos de proyectil y agarre, el desplazamiento al rodar y
el escudo; el radio de los golpes no cambia (como en el original). Mario y
Link se ven más grandes que antes y en proporción correcta entre ellos.

## Kirby

Modelo y 112 animaciones convertidos de la ROM (27 huesos, 370 triángulos,
4 KiB de texturas). Física de sus atributos (`229_KirbyMain.c`), escudo,
rodar, agarres, lanzamientos y todos los ataques normales con sus golpes y
sonidos originales.

- **Seis saltos.** El primero en el aire como los demás; los siguientes con
  las velocidades 60/52/47/40 de `dFTKirbyJumpAerialFVelocities`, cada uno a
  partir del tic 28 del anterior (`flag1` de sus guiones) y con su animación
  `JumpAerialF1..F5`.
- **Arriba+B, Corte final.** La subida y la caída salen del `TransN` de la
  animación (×0,8 en el aire, como el original); sin gravedad, con deriva a
  media aceleración. Al aterrizar, `SpecialHiLanding` lanza una onda por el
  suelo: 100 por tic durante 20 tics, 6 %, ángulo 361, escala 50, base 70.
- **Abajo+B, Piedra.** En el tic de `flag1` se convierte en piedra (la pieza
  del modelo de piedra): aguanta 38 puntos de daño sin sufrir daño ni salir
  despedido; en el aire cae a 140 por tic con un golpe de 20 %. B la deshace
  a partir de los 18 tics, o sola a los 160.
- **B, Inhalar.** Mientras se mantiene B (mínimo 40 tics) atrae y atrapa al
  rival aunque se esté cubriendo; queda dentro (invisible). X o B lo escupen
  como estrella: 10 % y sale despedido a 120, frenando 4 por tic. El rival
  escapa a los 500 tics, o antes aporreando botones (6 por pulsación), y sale
  con el golpe de escape del original (ángulo 80).

## Simplificaciones de Kirby

- No copia habilidades: abajo/B con alguien dentro también lo escupe.
- Dentro de Kirby no se puede caminar ni saltar; solo girar.
- El lanzamiento hacia delante no salta: suelta al terminar la animación (el
  original suelta al caer, con un golpe al aterrizar que no está).
- La onda del Corte final y la estrella son dibujos provisionales; la onda no
  sigue pendientes ni choca con paredes.
- `Attack100` (golpes rápidos) solo hace su inicio como tercer golpe.
- Sin caras intercambiables (`SetTexturePartID`), como Mario y Link.

## Otros arreglos

- Una caída detenida justo encima de la plataforma por el sólido de la isla
  cuenta como aterrizaje (si no, una caída sin gravedad flotaba).
- La CPU ya no se queda girando debajo de un rival en una plataforma.
- Un lanzamiento que termina sin soltar al rival lo suelta.

## Coste

ARM9 estático 2.061.012 B (antes 1.540.292; presupuesto 2.621.440). Audio
489.612 B (82 sonidos). Texturas en VRAM, peor par: 235.520 de 262.144 B.
Geometría sin cambios (Kirby 370 triángulos, como Link). Pruebas en PC: 17
suites (nueva `tests/test_kirby.c`). Los recorridos de Mario siguen
idénticos salvo `versus.csv` (la CPU cambió).
