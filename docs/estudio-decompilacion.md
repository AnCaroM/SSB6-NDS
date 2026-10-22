# Estudio inicial de la decompilación de Smash 64

Fecha: 20 de octubre de 2026. Objetivo: identificar una ruta de adaptación a Nintendo DS a partir del código existente.

## Revisión y alcance

- Origen: https://github.com/VetriTheRetri/ssb-decomp-re
- Revisión estudiada: `572a747d8171137718fad472c5d7473dc7309ffd`.
- Copia local: `../upstream/ssb-decomp-re`, clon superficial de la rama predeterminada.
- Se inspeccionaron fuentes, cabeceras, Makefile, instalador, configuración de submódulos e informe US. No se ejecutó el instalador, no se inicializaron submódulos ni se extrajeron datos de una ROM. No se compiló ni se ejecutó el juego.
- La copia upstream permanece sin cambios según `git status --short`.

Las conclusiones sobre dependencias se basan en lectura del código. Las propuestas para DS son decisiones de diseño pendientes de implementación y medición.

## Qué tenemos realmente

Es una decompilación orientada a reconstruir una ROM de N64 idéntica a la referencia. No contiene una plataforma de ejecución para DS.

El archivo [us_report.json](../upstream/ssb-decomp-re/us_report.json) declara 7.165 funciones coincidentes de 7.165, 1.475.232 bytes de código coincidentes y 100 % en sus medidas agregadas de código y datos. Es el informe guardado por upstream, no una comprobación reproducida aquí. [CLAUDE.md](../upstream/ssb-decomp-re/CLAUDE.md) conserva una descripción de avance de aproximadamente 95,9 %, por lo que esa cifra narrativa no debe utilizarse como estado actual. Tampoco debe interpretarse 100 % como ausencia de dependencias binarias o portabilidad automática.

No apareció un archivo cuyo nombre contenga LICENSE o COPYING en la búsqueda del árbol clonado. Antes de incorporar y distribuir código derivado habrá que aclarar las condiciones de reutilización con el proyecto; este estudio no determina permisos.

## Mapa del código

Conteos de archivos `.c` presentes en esta revisión; no son porcentajes de avance ni medidas de tamaño ejecutable.

| Carpeta | Archivos C | Responsabilidad observada y consecuencia para DS |
|---|---:|---|
| `src/ft` | 211 | Luchadores, estados comunes, física, IA, datos y dibujo. Principal objeto de estudio. |
| `src/mp` | 3 | Geometría y procesamiento de colisiones del escenario. El nombre no identifica una capa de red. |
| `src/gr` | 15 | Escenarios y su comportamiento. Separar colisiones de efectos y representación. |
| `src/gm` | 5 | Cámara, sonido y otros servicios comunes. Adaptar sus interfaces. |
| `src/if` | 2 | Interfaz de batalla y control de sus fases; también conduce actualizaciones de objetos. |
| `src/sys` | 24 | Objetos, animación, tareas, controladores, vídeo, audio, DMA y memoria. Mezcla código aprovechable con dependencias directas de N64. |
| `src/lb` | 6 | Funciones comunes y carga/reubicación de recursos. Revisar al cambiar formato de datos. |
| `src/libultra` | 126 | Servicios de la plataforma N64. Reemplazar los servicios utilizados por la adaptación. |
| `src/sc` | 48 | Escenas, partida VS, entrenamiento y modos individuales. No arrancar todos los modos en la primera prueba. |
| `src/wp` / `src/it` / `src/ef` | 19 / 51 / 4 | Proyectiles, objetos y efectos. Aumentan las dependencias del combate completo. |
| `src/relocData` | 2.040 | Descripciones C de datos: atributos, modelos, animaciones, scripts y otros recursos. Parte de los bloques se genera desde la ROM. |

## Arranque de una batalla

La referencia principal es [scvsbattle.c](../upstream/ssb-decomp-re/src/sc/sccommon/scvsbattle.c), especialmente `scVSBattleStartBattle` (línea 125).

La función configura archivos y cámaras, inicializa colisiones del escenario, objetos, efectos y gestores de luchadores/proyectiles. Después prepara un `FTDesc` por jugador y llama a `ftManagerMakeFighter`. También crea la interfaz y otros actores. No constituye un punto de entrada pequeño e independiente del sistema N64.

En [ftmanager.c](../upstream/ssb-decomp-re/src/ft/ftmanager.c), los atributos se obtienen desde el archivo principal del personaje (línea 694). La creación monta una jerarquía de `DObj`, y la posición de colisión queda enlazada a la traslación de uno de esos objetos (línea 847). Esto impide asumir que toda la física puede usar `FTStruct` sin mantener alguna representación de las articulaciones.

La misma escena elige detalle alto para menos de tres participantes y bajo para más participantes. Es un mecanismo existente que conviene investigar después; no demuestra que cualquiera de esos modelos quepa o rinda bien en DS.

## Actualización: preservar el orden

Ruta observada en batalla VS:

```text
sys/taskman.c: espera del tic y task_update
  -> syTaskmanCommonTaskUpdate: lectura de control y scene_update
  -> scVSBattleFuncUpdate
  -> ifCommonBattleUpdateInterfaceAll: decide según fase de batalla
  -> gcRunAll cuando corresponde actualizar los objetos
  -> callbacks de cada proceso por prioridad
```

Referencias: [taskman.c](../upstream/ssb-decomp-re/src/sys/taskman.c), [ifcommon.c](../upstream/ssb-decomp-re/src/if/ifcommon.c) desde línea 3167 y [objman.c](../upstream/ssb-decomp-re/src/sys/objman.c) desde línea 2203.

`gcRunAll` recorre primero los callbacks generales de objetos y después las colas de procesos desde la prioridad mayor a la menor. [ftmanager.c](../upstream/ssb-decomp-re/src/ft/ftmanager.c), líneas 858–863, registra:

| Prioridad | Callback |
|---:|---|
| 5 | `ftMainProcUpdateInterrupt` |
| 4 | `ftMainProcPhysicsMapDefault` |
| 3 | `ftMainProcPhysicsMapCapture` |
| 2 | `ftMainProcSearchCatch` |
| 1 | `ftMainProcSearchHitAll` |
| 0 | `ftMainProcParams` |

La prioridad se aplica a las colas globales. No reemplazarlo sin comprobar por un bucle que ejecute todas las fases del jugador 1 y después todas las del jugador 2: puede cambiar interacciones entre luchadores. Además existen procesos de tipo función y de tipo hilo; estos últimos usan servicios `os*` de N64 en `gcRunGObjProcess`.

## Estados y movimiento

[fttypes.h](../upstream/ssb-decomp-re/src/ft/fttypes.h) define `FTStatusDesc`, `FTAttributes` y `FTStruct`. Los estados contienen callbacks de actualización, interrupción, física y colisión con el mapa. [ftcommonstatus.h](../upstream/ssb-decomp-re/src/ft/ftcommon/ftcommonstatus.h), desde línea 453, vincula `JumpF` con:

- `ftAnimEndSetFall`: transición al finalizar animación.
- `ftCommonJumpProcInterrupt`: acciones que pueden interrumpir.
- `ftPhysicsApplyAirVelDriftFastFall`: física aérea.
- `mpCommonProcFighterCliffFloorCeil`: colisiones con escenario.

`ftMainSetStatus`, en [ftmain.c](../upstream/ssb-decomp-re/src/ft/ftmain.c), línea 4365, hace bastante más que cambiar un número: gestiona colisiones de ataque, partes del modelo, animaciones y callbacks. Conviene delimitar explícitamente qué semántica conserva una primera adaptación.

La preparación del salto está en [ftcommonkneebend.c](../upstream/ssb-decomp-re/src/ft/ftcommon/ftcommonkneebend.c): utiliza avance de animación y liberación del botón para el salto corto. [ftcommonjump.c](../upstream/ssb-decomp-re/src/ft/ftcommon/ftcommonjump.c) calcula el impulso y selecciona salto hacia delante o atrás. [ftphysics.c](../upstream/ssb-decomp-re/src/ft/ftphysics.c) contiene gravedad, fricción, límites de velocidad y control aéreo.

El controlador lógico `SYController` está definido en [controller.h](../upstream/ssb-decomp-re/src/sys/controller.h). `ftMainProcUpdateInterrupt` procesa botones mantenidos, pulsación, liberación, valores previos y temporizadores del stick. El máximo lógico del stick está definido como 80 en `include/macros.h`. Un adaptador de cruceta debe establecer valores e historial coherentes; no basta con mover la posición directamente.

## Mario como primer caso

Los atributos ya están escritos en [203_MarioMain.c](../upstream/ssb-decomp-re/src/relocData/203_MarioMain.c), desde línea 230. Valores leídos, expresados en las unidades del juego:

| Campo | Valor |
|---|---:|
| `traction` | 1,5 |
| `dash_speed` / `run_speed` | 54 / 44 |
| `kneebend_anim_length` | 3 |
| `jump_vel_x` | 0,35 |
| `jump_height_mul` / `jump_height_base` | 0,7 / 26 |
| `air_accel` / `air_speed_max_x` | 0,025 / 30 |
| `gravity` | 2,4 |
| `tvel_base` / `tvel_fast` | 44 / 70 |
| `jumps_max` | 2 |

No convertir estos números directamente a píxeles o segundos: primero hay que preservar las unidades y el orden de actualización. Los datos visuales y otros miembros de `FTAttributes` siguen enlazados a recursos del personaje.

## Animación y combate no son independientes

`ftMainParseMotionEvent`, línea 152 de `ftmain.c`, interpreta eventos como `MakeAttackColl`, `SetAttackCollOffset`, `SetAttackCollDamage` y `SetAttackCollSize`. Suprimir el intérprete de movimientos eliminaría parte del comportamiento de ataques aunque el personaje se dibuje correctamente.

Asimismo, `ftMainProcPhysicsMap` usa la traslación de la articulación `TopN` y contiene condiciones de hitlag. Se debe mantener la separación entre avance de simulación, animación y dibujo sin asumir que toda animación es meramente visual.

## Dependencias que habrá que adaptar

| Área | Evidencia | Trabajo previsto |
|---|---|---|
| Gráficos | `ftdisplaymain.c` emite `gSP*` y `gDP*`; `objdisplay.c` y `rdp.c` forman parte del sistema gráfico. | Implementar representación y conversión de recursos para DS. Evaluar después cuánto del árbol de objetos conservar. |
| Audio | `sys/audio.c` usa `n_alAudioFrame`, DMA, colas y `osAiSetNextBuffer`. | Crear una interfaz de sonido para el port; elegir implementación DS al integrar audio. |
| Recursos | `lbreloc.c` carga con `syDmaReadRom` y `syDmaReadVpk0`, y resuelve referencias. | Extraer y convertir fuera de la consola; definir carga y referencias explícitas para DS. |
| Escenas | `scmanager.c` usa `syDmaLoadOverlay`. | Elegir una estrategia de memoria y carga para las escenas adaptadas. No trasladar direcciones enlazadas de N64. |
| ABI y datos | Makefile usa big-endian, VR4300 y ABI de 32 bits; hay punteros y campos de bits en estructuras. | Decodificar datos serializados de forma explícita. No reinterpretar bloques binarios como estructuras ARM sin comprobar diseño y orden de bytes. |
| Matemáticas | Física usa `f32`; salto usa `sqrtf`; otras rutas usan trigonometría. | Conservar inicialmente semántica y medir coste en destino antes de decidir cambios de representación numérica. |

## Construcción de la referencia

El [Makefile](../upstream/ssb-decomp-re/Makefile) rechaza expresamente Windows nativo y sugiere WSL. Usa Bash, Python, herramientas MIPS e IDO; el instalador también prepara una herramienta IRIX 4 empaquetada para Linux x86-64. US es la versión predeterminada; el propio Makefile conserva advertencias de trabajo pendiente para JP.

El README requiere `baserom.us.z64` para extraer y publica SHA-1 `e2929e10fccc0aa84e5776227e798abc07cedabf`. `make init` realiza extracción y construcción. No ejecutarlo hasta disponer de la referencia y preparar el entorno correspondiente.

Hay recursos descritos en C, pero no todos los datos necesarios están presentes: por ejemplo, `296_MarioModel.c` incluye bloques `.vtx.inc.c` generados; `include/reloc_data.h` selecciona cabeceras por versión también generadas. `RELOC_DATA=1` reconstruye datos mediante la cadena de herramientas de upstream; no convierte su formato a DS. `NON_MATCHING=1` tampoco es una opción de portabilidad.

## Primera adaptación propuesta

**Decisión de diseño:** usar Mario y un suelo plano provisional como primera prueba de movimiento. No cargar la escena VS completa todavía.

1. Preparar un ejecutable mínimo DS y una forma de registrar el estado por tic.
2. Aislar unas pocas operaciones numéricas identificadas: `ftPhysicsClampGroundVel`, `ftPhysicsSetGroundVelFriction` y `ftPhysicsApplyGravityClampTVel`. Documentar qué se adapta y su revisión de origen.
3. Comparar casos de signo, fricción al cruzar cero y saturación de velocidad contra la implementación de referencia. Estos casos verifican la adaptación de funciones, no toda la fidelidad del juego.
4. Seguir el ciclo `Wait -> Walk -> KneeBend -> Jump -> Fall -> Landing` conservando sus condiciones, parámetros y temporización. Determinar el subconjunto necesario de objetos, animaciones y colisiones antes de escribir sustitutos.
5. Mantener una representación visual provisional mientras se compara entrada, estado, posición y velocidad por tic. Para afirmar equivalencia con el original hará falta una referencia ejecutable o trazas de éste; comparar dos implementaciones propias no basta.
6. Medir memoria y tiempo por tic en DS antes de extender a dos luchadores y al primer ataque.

No hay todavía estimación fiable de rendimiento, fidelidad visual, alcance completo ni calendario. El siguiente trabajo técnico es cerrar el subconjunto de tipos y llamadas para la prueba de movimiento, en paralelo con la preparación del entorno DS cuando corresponda.
