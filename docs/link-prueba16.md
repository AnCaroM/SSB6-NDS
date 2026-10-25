# Prueba16: visor de Link

Segunda entrega para añadir a Link. Prueba16 convierte desde la ROM del
usuario el modelo, las texturas y 72 animaciones de Link, y los muestra en un
visor nuevo del menú (**VISOR DE LINK**). Link todavía no combate. El 1v1 de
Mario, la cámara, el menú y el sonido no cambian. **Aún no se ha visto en
emulador ni en DS.**

## Visor

| Botón | Acción |
|---|---|
| Izquierda / derecha | Animación anterior / siguiente |
| A | Pausa |
| R (en pausa) | Avanzar un tic |
| Y | Cámara lenta (1/3) |
| X | Girar a Link |
| B | Volver al menú |

La pantalla inferior muestra el nombre del movimiento original (tabla de
movimientos del juego), el tic y si la animación se repite.

## Conversión genérica

`tools/convert_fighter.py --fighter link` sirve para cualquier luchador:

- Lista de animaciones desde la tabla `FTMotionDesc` del luchador
  (`src/ft/ftdata.c`), con los ID de archivo de `relocFileDescriptions` y las
  banderas FTANIM (`tools/fighter_motions.py`). Algunas entradas de la tabla
  de Link están escritas sin llaves; el lector lo tiene en cuenta.
- Articulaciones insertadas/opcionales desde `FTHiddenPart` del archivo Main:
  las tablas de animación sólo listan las articulaciones que existen en cada
  movimiento (TransN/XRotN delante; partes opcionales como el gancho de Link o
  la articulación 28 de Mario sólo cuando su bandera las crea).
- Modelo: `dLinkModel_JointTree` @0x3AE8 y su tabla de materiales +0x5B8
  (`225_LinkMain.c`, contenedor de partes comunes). La apariencia por defecto
  es la del árbol: espada y escudo en la mano; vaina, escudo a la espalda y
  gancho ocultos, como al crear al luchador en el juego.
- Huesos: mismo formato que Prueba15; el plan de evaluación decide qué
  articulaciones guardan matriz (19 ranuras de la pila para Link, 15 para
  Mario) y omite las que no tienen geometría ni hijos.

Comprobaciones: el mismo conversor aplicado a Mario reproduce **exactamente**
los canales del banco de Prueba15 en 62 clips comunes; error máximo de Link
0,31 unidades frente a la evaluación en coma flotante; una prueba en C
confirma que el plan con ranuras da las mismas matrices que la jerarquía
completa en las 2488 poses de Link.

## Partes del modelo y bordes (revisión tras la primera prueba)

- **Partes que cambian durante la animación.** Los comandos
  `SetModelPartID`/`ResetModelPartAll`/`HideModelPartAll` de los guiones
  originales (`224_LinkMainMotion.c`, `WaitAsync` absoluto, saltos y
  subrutinas) se convierten en una lista (tic, máscara) por clip. Partes de
  Link: espada en mano, boomerang en mano (archivo `LinkBoomerangModel`),
  gancho (3 piezas), escudo en mano, vaina con espada, escudo a la espalda.
  Ejemplos: bomba en el aire guarda el escudo en la espalda en el tic 28;
  el boomerang cambia la espada por la vaina y el boomerang en 9/10/26/38.
- **Movimiento de TransN.** El hueso insertado TransN se guarda siempre. En
  los clips donde el juego lo usa como desplazamiento (subir del borde,
  ataque en carrera) sólo se dibuja si se pide (`FIGHTER_DRAW_ROOT_MOTION`);
  el visor lo usa en los clips de borde y los muestra colgando del borde
  derecho de Dream Land, mirando al escenario.
- Herramientas de revisión sin DS: `tools/render_fighter.py` (hojas de
  contacto) y `tools/raster_fighter.py` (rasterizado 256×192 con texturas,
  profundidad, partes y alfa de 1 bit a partir de los datos generados).

## Coste

| | Prueba15 | Prueba16 |
|---|---|---|
| ARM9 estático | 823.276 B | 1.166.056 B |
| Texturas (A+B) | 166.144 B | 235.520 B de 262.144 |
| Huesos de Link | — | 248.664 B |

Link: 32 articulaciones (24 ranuras de matriz), 371 triángulos con partes,
texturas 69.376 B. El gancho obliga a usar unidades de modelo/128 (error
máximo 0,68 unidades).

## Pendiente (no en esta entrega)

- Caras (parpadeo, daño), estela de la espada, la bomba como objeto y el
  boomerang/gancho lanzados (son objetos, no partes del modelo).
- Combate de Link: hitboxes de espada, atributos, boomerang, bomba, ataque
  giratorio, sonidos y voces.
- Selección de personaje.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --skeletal-poses`
