# Primer núcleo de movimiento: laboratorio C

Este laboratorio adapta siete operaciones numéricas identificadas en la decompilación. Separa sus entradas y resultados de `FTStruct` para poder experimentar sin libultra ni recursos extraídos. Es una prueba de adaptación de funciones, no un luchador completo ni un port ejecutable en DS.

Origen: `VetriTheRetri/ssb-decomp-re`, revisión `572a747d8171137718fad472c5d7473dc7309ffd`. La atribución también figura en `src/motion.c`. No se ha establecido una licencia para publicar este código derivado; el trabajo actual es local.

## Procedencia

| Función del laboratorio | Función de referencia | Archivo upstream |
|---|---|---|
| `motion_ground_clamp` | `ftPhysicsClampGroundVel` | `src/ft/ftphysics.c` |
| `motion_ground_friction` | `ftPhysicsSetGroundVelFriction` | `src/ft/ftphysics.c` |
| `motion_walk_speed` | `ftPhysicsSetGroundVelAbsStickRange` | `src/ft/ftphysics.c` |
| `motion_gravity` | `ftPhysicsApplyGravityClampTVel` | `src/ft/ftphysics.c` |
| `motion_air_accel` | `ftPhysicsClampAirVelXStickRange` | `src/ft/ftphysics.c` |
| `motion_air_friction` | `ftPhysicsApplyAirVelXFriction` | `src/ft/ftphysics.c` |
| `motion_button_jump_y` | Componente vertical de `ftCommonJumpGetJumpForceButton` | `src/ft/ftcommon/ftcommonjump.c` |

El dominio del stick en estas funciones es de -80 a 80. Los límites, fricciones y aceleraciones deben ser valores finitos y no negativos. No se ofrece un parser de entradas externas. `motion_button_jump_y` conserva la conversión final de flotante a entero de la referencia, que afecta al impulso. `motion_walk_speed` devuelve la velocidad escalar de suelo; la orientación del luchador y la transferencia a velocidad mundial siguen fuera de esta función, como en upstream.

## Reproducir

Desde la raíz del proyecto en PowerShell:

```powershell
python tools/verify_motion.py
```

El script busca primero Zig en `.tools/zig/ziglang/zig.exe` (o `zig` en otros sistemas). Si no existe, busca `cc`, `clang` o `gcc` en PATH. La instalación local usada es `ziglang==0.15.2`, distribución de la herramienta Zig mediante PyPI:

```powershell
python -m pip install --target .tools/zig ziglang==0.15.2
```

El script dirige las cachés Zig a `.cache/`. Los directorios de herramientas, caché y resultados están excluidos en `.gitignore`. Esto no instala devkitARM ni configura un servidor MCP.

El verificador comprueba la revisión upstream y que los cuatro archivos de referencia no tengan diferencias respecto de HEAD, extrae los cuerpos originales de las siete funciones y toma sus constantes de las cabeceras originales. Compila esos cuerpos en una unidad C separada con tipos mínimos de prueba. Esos tipos proporcionan únicamente los campos utilizados: no reproducen el ABI ni el tamaño real de `FTStruct` o `FTAttributes`.

Compara los resultados con las funciones adaptadas, incluyendo el signo del cero, sin tolerancia numérica. Los casos recorren ambos sentidos, velocidades dentro y fuera de los límites, fricciones que cruzan cero, todos los valores enteros del stick entre -80 y 80, salto corto/largo y 600 aplicaciones consecutivas de gravedad. La aceleración aérea se prueba con un umbral proporcionado de 8: es un parámetro de prueba, no una afirmación sobre la configuración del estado aéreo completo.

Los resultados se guardan en:

- `build/host/compare_motion.exe`: comparación en Windows.
- `build/host/motion_lab.exe`: experimento numérico en Windows.
- `build/host/jump-experiment.csv`: integración vertical de dos impulsos.
- `build/host/verification.json`: revisión, compilador, funciones y resultado de comparación.

En otros sistemas los ejecutables no llevan extensión `.exe`.

El experimento CSV aplica parámetros de Mario desde `203_MarioMain.c`: multiplicador de salto 0,7, base 26, gravedad 2,4 y velocidad terminal 44. Parte directamente del lanzamiento con stick horizontal neutro. La muestra del tic 0 es el estado inicial; cada siguiente muestra aplica gravedad y suma velocidad a posición. No aplica suelo: permite valores negativos de posición.

**Límite de la evidencia:** la igualdad con cuerpos C compilados para el mismo equipo verifica esta adaptación numérica. No prueba coincidencia con instrucciones MIPS, timing original, un salto completo de Mario, rendimiento ARM o funcionamiento en DS. La trayectoria del CSV es un experimento con un orden de integración declarado, no una traza capturada del juego.

## Resultado ejecutado

La primera ejecución en Windows, con Zig 0.15.2 y su compilador C, completó **16.498 comparaciones sin diferencias**. Se compilaron ambos ejecutables con `-O2 -Wall -Wextra -Werror -ffp-contract=off -fno-fast-math` y se generó el CSV de 91 muestras más cabecera. El primer intento dentro del sandbox falló por permisos de archivos temporales; el reintento autorizado fuera del sandbox terminó correctamente. No se compiló para ARM ni se ejecutó un emulador DS.

## Qué falta para mover un luchador

La ruta estudiada utiliza `GObj -> FTStruct -> FTAttributes`, estado de entrada anterior y actual, `DObj`/articulaciones, callbacks por estado y colisiones `MPCollData`.

- `Wait` y `Walk` consideran interrupciones por salto, giro, dash, ataques y otras acciones. El laboratorio no implementa esas prioridades.
- `KneeBend` avanza un contador con `DObj.anim_speed` y determina el salto corto al liberar un botón C durante la ventana de tres cuadros.
- `ftCommonJumpSetStatus` establece cinética aérea, elige JumpF/JumpB y calcula velocidades con los atributos. Aquí solo se aisló el cálculo vertical de fuerza por botón.
- `JumpF/JumpB` usa la finalización de animación para pasar a caída; alcanzar el ápice no es un sustituto verificado de esa condición.
- `ftPhysicsApplyAirVelDriftFastFall` añade detección y memoria de fastfall, reducción de velocidad excesiva y efectos de color. No puede sustituirse por llamar siempre a `motion_air_accel` y `motion_air_friction`.
- `ftMainProcPhysicsMap` considera hitlag, daños y desplazamiento de articulaciones. El suelo plano provisional aún no implementa este procedimiento.

La siguiente integración debe definir explícitamente qué partes del runtime se conservan y qué partes son provisionales. Hace falta devkitARM/libnds para construir un `.nds`, además de un emulador o consola para verificarlo.

## Papel de REA

Hasta este hito no se ha ejecutado REA. La lectura de fuentes y las comparaciones usan herramientas de archivos, Git y compilación C.

La guía local `C:\ruta\rea\.agents\skills\reverse-engineer-anything\SKILL.md` recomienda herramientas normales para análisis de un repositorio completo. REA sería pertinente si necesitamos evidencia que dependa del binario original y existe una ruta de análisis compatible.

La documentación local `docs/raw-binary.md` no incluye MIPS entre las arquitecturas raw admitidas y limita Windows Ghidra P0 a PE nativo y raw 6502. El soporte MIPS ELF descrito por separado no demuestra soporte de una ROM N64 ni compatibilidad con VR4300. Tampoco se ha validado un proveedor instalado.

Además, Node detectado es 22.14.0; el `package.json` local de REA requiere `^22.19.0 || ^24.11.0 || >=26.0.0` y `.nvmrc` fija 24.18.0 para desarrollo. No se ha cambiado Node ni instalado/configurado REA para intentar forzar esa ruta.

Referencias de herramientas: https://pypi.org/project/ziglang/ y https://ziglang.org/learn/overview/.
