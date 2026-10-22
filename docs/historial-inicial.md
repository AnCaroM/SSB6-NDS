# Super Smash Bros. 64 para Nintendo DS: investigación inicial

La [Prueba10](docs/versus-prueba10.md) incorpora Mario contra CPU con tres vidas,
daño en ambos sentidos y poses compactas. El [estudio de optimización](docs/optimizacion-y-escalado.md)
registra técnicas, mediciones y recortes propuestos para llegar a cuatro luchadores.

La [Prueba09](docs/animaciones-prueba09.md) corrige la asignación de articulaciones
auxiliares, añade carrera/frenado y captura, y revisa los 50 clips convertidos.

La [Prueba08](docs/ataques-prueba08.md) incorpora el repertorio de Mario, con
eventos/animaciones originales y poses compactas. Incluye la tabla nueva de controles.

La [Prueba07](docs/especial-prueba07.md) añade Super Jump Punch como recuperación,
con animación/curva originales y caída especial. Se activa con L.

La [Prueba06](docs/recuperacion-prueba06.md) añade segundo salto, animaciones
originales de salto aéreo/captura y un recorrido automático completo de regreso.

La [Prueba05](docs/bordes-prueba05.md) añade paredes, cara inferior y agarre de
bordes de Dream Land; conserva la cámara automática de Prueba04.

La [Prueba04](docs/camara-prueba04.md) añade cámara automática según posición y
separación de los luchadores, basada parcialmente en el código original.

La [Prueba03](docs/dreamland-prueba03.md) convierte Dream Land original desde la
ROM del usuario: cuatro capas gráficas, texturas, fondo y plataformas. El
[proceso del port](docs/proceso-del-port.md) explica los pasos y el alcance actual.

**Requisito permanente:** DS original/DS Lite, 4 MiB y ARM9 ~67 MHz. Los límites
y presupuestos están en [la nota técnica](docs/limites-ds.md) y `AGENTS.md`.
Prueba01 fue confirmada por el usuario en su DS con DSPico. La
[Prueba02 de combate](docs/combate-prueba02.md) añade un jab y un blanco pasivo,
compartiendo recursos entre ambas instancias; requiere nueva validación física.

El objetivo es un port nativo de Smash 64 a Nintendo DS. La [Prueba 01 para DS y DSPico](docs/prueba-ds-dspico.md) integra el modelo, texturas y cinco animaciones originales de Mario con el movimiento adaptado, en un jardín de entrenamiento propio con tres plataformas. El archivo para copiar a la microSD se encuentra en `Para tu DS/SSB-NDS-Prueba01.nds`. Todavía faltan combate y mediciones en hardware; la viabilidad del juego completo queda por demostrar.

Ya se realizó una primera lectura del código de la decompilación. El [estudio técnico](docs/estudio-decompilacion.md) registra la revisión examinada, el mapa de sistemas, el orden de actualización, las dependencias de N64 y la primera adaptación propuesta. La copia de referencia está en `upstream/ssb-decomp-re`.

El primer [laboratorio de movimiento en C](docs/motion-lab.md) aísla siete operaciones numéricas y permite compararlas con funciones extraídas del código de referencia mediante `python tools/verify_motion.py`. Incluye un experimento de salto que genera CSV para inspección. Todavía no implementa el runtime del luchador ni produce un `.nds`.

También se revisó [BattleShip como referencia de portabilidad](docs/estudio-battleship.md). Su copia está en `upstream/BattleShip`, con su fork de decompilación fijado a la revisión que utiliza el port. Las siete funciones del laboratorio tienen cuerpos idénticos en ambas referencias; `python tools/compare_battleship.py` reproduce la comparación.

La [escena provisional de movimiento](docs/escena-movimiento.md) tiene pruebas ejecutadas en PC y trazas por tic. Incluye el código libnds para controles, dibujo y telemetría. El SDK está en `C:/devkitPro` y melonDS en `.tools/melonDS`; ver [instalación y primer build](docs/instalacion-ds.md). Recompilar con `python tools/build_nds.py --devkitpro C:/devkitPro` y abrir con `Abrir prueba DS.cmd`. El arranque en melonDS 1.1 ya se verificó visualmente; falta completar las pruebas de controles y hardware real.

## Punto de partida

La ROM propia del usuario ya fue [verificada y utilizada para la primera extracción](docs/rom-y-primeros-recursos.md): US v1.0, con el hash exacto requerido. El [visor de Mario](docs/visor-mario.md) convierte 25 articulaciones, 320 triángulos, tres texturas y cinco clips originales. La escena integrada se compila con `python tools/build_nds.py --devkitpro C:/devkitPro --demo scene` y se abre con `Abrir mapa DS.cmd`. Las reglas y el mapa aún son provisionales.

- REA local: `C:\ruta\rea`, versión declarada 6.7.0. No se encontraron `node_modules` ni `dist` en esa copia.
- El SDK DS está instalado en `C:\devkitPro`; la compilación usa rutas explícitas sin depender del PATH. melonDS portable está en `.tools/melonDS`.
- Existe una decompilación pública: https://github.com/VetriTheRetri/ssb-decomp-re
- Su README describe la reconstrucción de la ROM N64 y requiere una ROM base aportada por el usuario para extraer datos. Reconstruir esa ROM no produce un ejecutable para DS.

## Enfoque

Estudiar primero la decompilación y separar lógica de juego de dependencias de N64. Adaptar después cada sistema a DS. REA puede ayudar a investigar comportamiento o funciones desconocidas, pero su compatibilidad con la ROM concreta debe verificarse: el soporte MIPS ELF documentado en REA no demuestra soporte directo para una ROM N64.

El plan inicial apunta a una DS original/DS Lite. DSi sería un objetivo distinto que debe acordarse si se desea aprovechar sus recursos adicionales.

## Hitos y criterios para avanzar

1. **Entorno DS.** Preparar devkitARM y las bibliotecas actuales de devkitPro siguiendo sus ejemplos oficiales. Compilar un ejemplo mínimo, abrirlo en un emulador y comprobar pantalla, botones y tiempo por cuadro. Resultado: primer `.nds` reproducible.
2. **Mapa del juego original.** Fijar una revisión de la decompilación; identificar actualización de luchadores, estados, colisiones, animación, gráficos, audio y carga de datos. Documentar dependencias, procedencia y condiciones de reutilización antes de incorporar código.
3. **Primera prueba del port.** Adaptar un fragmento identificado de lógica del juego, con entradas y resultados de referencia. Mostrar un escenario simple y un luchador con representación provisional; comprobar movimiento, salto y colisión con el suelo. Una demo independiente de plataformas solo validaría el entorno, no sería evidencia de que Smash está portado.
4. **Prueba de combate.** Agregar un segundo luchador, un ataque, daño, retroceso y pérdida de vida. Comparar el comportamiento con la referencia y medir memoria y tiempo por cuadro.
5. **Decisión de alcance.** Con esas mediciones, decidir complejidad visual, conversiones de recursos y cantidad de luchadores simultáneos. Añadir contenido, sonido y menús de forma gradual. El multijugador inalámbrico queda para una fase posterior.

## Riesgos por comprobar

- Dependencias del procesador MIPS, ensamblador, orden de bytes y supuestos numéricos al pasar a ARM.
- Sustitución de comandos gráficos y servicios de N64 por implementaciones para DS.
- Presupuesto de memoria, coste de animaciones y conversión de modelos y texturas.
- Adaptación del control analógico a la cruceta y definición de caminar/correr.
- Rendimiento real: no prometer fidelidad completa ni una tasa de cuadros antes de medir.

## Información pendiente

- El usuario dispone de una DS y DSPico. Está pendiente su primera prueba física del paquete.

## Referencias consultadas

- Decompilación y proceso de extracción: https://github.com/VetriTheRetri/ssb-decomp-re
- Ejemplos oficiales DS: https://github.com/devkitPro/nds-examples
- Biblioteca DS: https://github.com/devkitPro/libnds
- Documentación local de REA: `C:\ruta\rea\README.md` y `docs\ghidra-mips.md`.

REA se ha estudiado como referencia, pero no se ha ejecutado para analizar esta ROM. Su repositorio no se ha modificado. El SDK y melonDS ya se instalaron con autorización del usuario.
