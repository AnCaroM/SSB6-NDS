# BattleShip como referencia para el port a DS

Fecha: 20 de octubre de 2026.

## Resultado

**Sí es útil:** aporta una capa de portabilidad concreta sobre el mismo juego, conversiones de recursos, correcciones de representación de datos y herramientas para localizar divergencias. Lo incorporamos como segunda referencia técnica. No se sustituye todavía la base del proyecto por BattleShip completo.

La estrategia propuesta es conservar la decompilación como referencia del comportamiento y estudiar las soluciones de BattleShip al integrar servicios de plataforma, datos y validación. Cada solución debe evaluarse con el presupuesto de memoria, instrucciones y gráficos de DS.

## Versiones examinadas

| Árbol | Revisión |
|---|---|
| BattleShip | `aeb8779bad36d76d949745dc3c2e3909be214f41` |
| Submódulo `decomp` fijado por BattleShip | `df3170da4932ae5702db007625a2145bd9ce026f` |
| Decompilación original de nuestro laboratorio | `572a747d8171137718fad472c5d7473dc7309ffd` |

Origen: https://github.com/JRickey/BattleShip. Copia local: `upstream/BattleShip`.

Se descargó también su fork de decompilación en `upstream/BattleShip/decomp` y se seleccionó la revisión exacta del gitlink. La cabeza actual de `port-patches` era distinta: estudiar solo esa rama habría mezclado versiones. Los submódulos libultraship y Torch no se descargaron ni se inspeccionaron internamente; sus revisiones fijadas son `3c1fdce9038c1d66e8f55f756eaefbfbdf0eba38` y `3aa9c9739af95659f19adc73afeb12df93957fc3`.

El estudio es estático. No se configuró ni compiló BattleShip, no se extrajo una ROM y no se ejecutó el port. Las descripciones de funciones disponibles no son pruebas de rendimiento o fidelidad realizadas por nosotros. No se ejecutó REA para este estudio de fuentes.

## Arquitectura observada

El [CMakeLists.txt](../upstream/BattleShip/CMakeLists.txt) construye una biblioteca de objetos C `ssb64_game` con las carpetas del juego. Agrega implementaciones de compatibilidad de `port/stubs` y una selección de libultra, mientras el ejecutable incorpora código C++ de plataforma. Declara C11 y C++20. Excluye los módulos de depuración `ovl8` y `db` de la selección normal.

El [README](../upstream/BattleShip/README.md) identifica libultraship como capa de gráficos/audio/entrada y Torch como extractor de datos desde la ROM. El código inspeccionado confirma llamadas hacia esas interfaces, pero no hemos auditado sus implementaciones.

| Sistema | Evidencia local | Utilidad para DS |
|---|---|---|
| Ejecución del motor | [gameloop.cpp](../upstream/BattleShip/port/gameloop.cpp), [n64_stubs.c](../upstream/BattleShip/port/stubs/n64_stubs.c) | Identificar qué servicios `os*`, colas y cambios de contexto requiere realmente el juego. |
| Recursos | [lbreloc_bridge.cpp](../upstream/BattleShip/port/bridge/lbreloc_bridge.cpp), `port/resource` | Estudiar dependencias entre archivos y cómo se resuelven referencias sin DMA de cartucho N64. |
| Orden de bytes | [lbreloc_byteswap.h](../upstream/BattleShip/port/bridge/lbreloc_byteswap.h) y su implementación | Catálogo de problemas reales: vértices, texturas, pares de u16, estructuras mixtas y scripts. |
| Estructuras serializadas | [fttypes.h del fork](../upstream/BattleShip/decomp/src/ft/fttypes.h) | Consultar `PORT_RESOLVE`, accesores y `_Static_assert` de tamaños/desplazamientos. |
| Matemáticas | CMake y [análisis de trigonometría](../upstream/BattleShip/docs/bugs/trig_host_libm_divergence_2026-07-31.md) | Evitar suponer que la biblioteca matemática del equipo reproduce la de N64. |
| Trazas | [state_trace.md](../upstream/BattleShip/docs/state_trace.md) | Comparar estado por tic y distinguir diferencias de entrada, física, articulaciones y cámara. |
| Opciones adicionales | `port/enhancements` | Identificar comportamientos opcionales que habría que desactivar en una referencia de fidelidad. |

## Qué conservar como ideas

### Datos con representación explícita

Los archivos de recursos conservan referencias y campos de 32 bits, aunque PC use punteros de 64 bits. BattleShip introduce referencias mediante tokens y accesores. Sus comprobaciones de tamaño son especialmente útiles para evitar que un cambio de compilador altere silenciosamente una estructura.

DS también obliga a comprobar el orden de bytes y los campos de bits, aunque una compilación ARM de 32 bits reduzca el problema de anchura de puntero. No decidir aún entre punteros reubicados, offsets o identificadores compactos: primero determinar qué datos vamos a cargar y con qué vida útil.

La cabecera del conversor describe una pasada general de palabras u32 y correcciones específicas posteriores. La implementación contiene además conjuntos y mapas para registrar conversiones. Es una fuente útil de casos que probar; no debe copiarse como un algoritmo de conversión universal ni suponerse barata para DS. Preferimos estudiar qué conversiones pueden hacerse fuera de la consola.

### Comparación de estado, además de entradas

El formato de trazas documentado separa RNG, batalla, luchadores, objetos, proyectiles, escenario y otras categorías. Evita utilizar direcciones de memoria como identidad; representa luchadores por jugador y otros objetos mediante claves estables. Los flotantes se consideran por patrón de bits.

Esto mejora nuestro plan: cuando exista el runtime del luchador, registrar por tic entrada lógica, estado, posición, velocidad, RNG y variables relevantes. Para comparar con BattleShip deberán coincidir escenario, región, configuración, semilla y secuencia de entradas. Una coincidencia con PC por sí sola no establece equivalencia con N64.

La documentación distingue categorías incluidas en el resultado agregado y categorías diagnósticas, así como áreas aún no cubiertas. No presentar un PASS de una selección de campos como validación de todo el juego.

### Matemáticas y correcciones con alcance acotado

BattleShip compila las implementaciones de seno/coseno de libultra y establece opciones para evitar contracción de operaciones y determinadas sustituciones del compilador. Es una advertencia concreta para la futura física de daños y las animaciones; nuestro laboratorio actual solo compara siete funciones en el mismo host.

El fork también contiene defensas ante articulaciones nulas en `ftPhysicsApplyGroundVelTransN` y `ftPhysicsGetAirVelTransN`. Sus comentarios describen rutas de recuperación por problemas de inicialización. Deben tratarse como correcciones con un contexto concreto, no importarse como comportamiento original deseado.

## Qué exige rediseño para DS

1. **Referencias en memoria.** [RelocPointerTable.cpp](../upstream/BattleShip/port/resource/RelocPointerTable.cpp) fija `INITIAL_CAPACITY = 256 * 1024`. Cada entrada contiene un puntero y una generación u32. Bajo un diseño de 32 bits con entrada de 8 bytes eso serían 2 MiB solo para la tabla inicial; es una estimación estructural, no un tamaño medido con devkitARM. El comentario de PC indica aproximadamente 4 MB. Debemos dimensionar cualquier tabla según el subconjunto real y considerar offsets o referencias más sencillas.
2. **Pilas y corrutinas.** `gameloop.cpp` solicita una pila de 1 MiB para la corrutina del juego. `n64_stubs.c` define 256 KiB para hilos de servicio y 64 KiB para procesos GObj. No adoptar esas reservas por defecto. Hay que medir profundidad de pila y delimitar qué procesos pueden mantenerse como callbacks.
3. **Backend ARM existente.** `coroutine_armv7.S` está dirigido a ARMv7-A y guarda registros VFP; `coroutine_android.cpp` utiliza servicios Android/POSIX. Que exista un backend ARM no lo convierte en un backend DS. El ejemplo oficial de devkitPro configura ARM9 con `-march=armv5te -mtune=arm946e-s`.
4. **Gráficos.** `port_drain_pending_display_list` termina llamando a `Fast3dWindow::DrawAndRunGraphicsCommands`, con opciones de interpolación. Sigue pendiente una implementación para el sistema gráfico de DS y una estrategia de conversión de modelos/materiales; cambiar la ventana no resuelve ese trabajo.
5. **Dependencias y contenido adicional.** GUI, mods, texturas de alta resolución, actualizador y otras opciones de escritorio amplían memoria y dependencias. Seleccionar únicamente servicios necesarios para la primera prueba.

## Comparación con el núcleo que ya tenemos

Se compararon los cuerpos C exactos de las siete funciones de referencia del laboratorio entre los dos árboles fijados. **7 de 7 son idénticos**. Los hashes y revisiones están en [battleship-motion-comparison.json](battleship-motion-comparison.json).

Reproducir desde la raíz:

```powershell
python tools/compare_battleship.py
```

El script lee los archivos de los commits mediante `git show`, comprueba las tres revisiones y el gitlink de BattleShip. No compara todo el motor, las constantes externas ni las estructuras; tampoco prueba igualdad de comportamiento de los dos ejecutables. No se ha cambiado el núcleo C ni ha sido necesario repetir las 16.498 comparaciones numéricas por esta revisión de documentación.

## Decisión para el siguiente hito

Continuar con la prueba pequeña de movimiento, incorporando ahora estas referencias:

- Usar las estructuras y accesores del fork para planificar la carga de atributos y la jerarquía mínima del luchador.
- Definir un registro por tic antes de integrar más comportamiento.
- Consultar las conversiones y auditorías de BattleShip al introducir cada tipo de recurso.
- Conservar el presupuesto DS como criterio de diseño para referencias, pilas y gráficos.
- Si después compilamos BattleShip con una ROM de referencia, usarlo como entorno de comparación de estados con opciones que alteran la jugabilidad desactivadas.

La [licencia de BattleShip](../upstream/BattleShip/LICENSE) declara MIT para su capa específica de portabilidad y herramientas, con exclusiones expresas para la decompilación, material derivado enumerado y recursos del juego. Registrar procedencia y revisar el alcance por archivo antes de incorporar código.

Referencia de la configuración ARM9 oficial: https://github.com/devkitPro/nds-examples/blob/master/templates/arm9/Makefile.
