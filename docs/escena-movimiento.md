# Escena provisional de movimiento y destino DS

Ya existe una escena reducida que usa el núcleo numérico comprobado en el laboratorio. Se compila y ejecuta en PC para verificar su integración. La aplicación libnds se compiló como `build/nds/ssb-motion.nds` y su arranque se verificó visualmente en melonDS 1.1; la prueba de todos los controles y de hardware real sigue pendiente. Ver [instalación y primer build](instalacion-ds.md) para el resultado actual, que reemplaza el bloqueo de SDK descrito al final como antecedente.

## Archivos

- `src/motion_scene.h` / `src/motion_scene.c`: estado y avance de un tic, independientes de pantalla y controles físicos.
- `src/scene_trace.c`: ejecutable host que genera trazas con entrada y salida por tic.
- `platform/nds/main.c`: entrada de botones, dibujo de una figura provisional y telemetría con libnds.
- `tests/test_scene.c`: contratos de integración de la escena.
- `tools/verify_scene.py`: compila pruebas y generador host, ejecuta pruebas y guarda tres trazas.
- `tools/build_nds.py`: detecta SDK y construye el destino DS mediante argumentos de proceso, compatibles con rutas Windows que contengan espacios.

## Qué funciona en las pruebas host

Los casos ejecutados verifican la espera de tres tics después de entrar en preparación de salto, la menor altura al liberar pronto el botón, ausencia de salto repetido al mantenerlo pulsado, reposo estable, frenado, contacto con suelo, caída por un borde, reaparición sin reiniciar el contador de tics y limitación del stick.

También se ejecutó dos veces una misma secuencia de 1.200 tics, comparando el estado completo en memoria en cada paso dentro del mismo ejecutable. Esto comprueba repetibilidad local; no equivale a una prueba entre arquitecturas ni contra N64. Las aserciones están explícitamente habilitadas con `-UNDEBUG`; la compilación del test falla si se deshabilitan.

Resultado: `PASS: jump timing, short hop, held input, floor, ledge, respawn, replay and input clamp`.

```powershell
python tools/verify_scene.py
python tools/plot_scene.py
```

El segundo comando requiere matplotlib y genera `build/scene/movement.png`. La gráfica representa datos de la escena ejecutada en PC, no una captura de emulación DS.

El primero genera `build/scene/test_scene.exe`, `scene_trace.exe`, `verification.json` y trazas `long.csv`, `short.csv`, `ledge.csv` con 300 tics cada una. Los ejecutables no llevan `.exe` en otros sistemas.

## Diferencia respecto del motor original

La escena es un banco de integración, no una implementación completa de los estados de Mario. Las operaciones de física provienen del laboratorio; los parámetros numéricos son los de Mario, pero se simplifican deliberadamente las reglas circundantes:

| Aspecto | Implementación actual |
|---|---|
| Estados | WAIT, WALK, KNEE, AIR y LAND; AIR no se divide por ápice ni simula la finalización de una animación original. |
| Controles | Stick lógico limitado a -80..80 y botón de salto. No hay salto por stick. |
| Salto corto | Liberación durante los tres tics de preparación; fuerza mediante la función comprobada. |
| Movimiento de suelo | Caminar directo y cambio de sentido inmediato. No implementa giro, dash ni carrera originales. |
| Aire | Gravedad, límite vertical, aceleración horizontal y fricción. No hay retroceso, fastfall ni segundo salto. |
| Suelo | Segmento plano de x=-900 a x=900; contacto por cruce vertical. No usa las colisiones originales. |
| Aterrizaje | Recuperación provisional de tres tics; no se corresponde con una animación extraída. |
| Reaparición | Al caer bajo y=-1800; se conserva el contador de tics y el historial del botón para evitar un salto artificial. |
| Planificación | Un actor y una secuencia explícita de operaciones; no reproduce las colas GObj del combate. |

La velocidad de suelo al frenar usa un coeficiente provisional de material 4 multiplicado por la tracción de Mario 1,5. Los parámetros del laboratorio se documentan en `docs/motion-lab.md`; los cuerpos originales aún son la referencia para futuras ampliaciones. No atribuir fidelidad al juego completo a estas pruebas.

## Aplicación DS preparada

La pantalla superior dibuja una figura de 8×16 píxeles y un suelo. La inferior presenta contador de tics, estado, posición, velocidad, tipo de salto y contactos/reapariciones. No necesita una ROM para esta escena ni incorpora recursos visuales del juego.

Controles previstos:

- Cruceta izquierda/derecha: stick lógico -80/+80.
- B mantenido: magnitud reducida a 24 para caminar más despacio.
- A: salto; liberarlo pronto selecciona salto corto.
- START: pausa.
- SELECT durante pausa: avanzar un tic con la entrada actual.
- Y: reiniciar la escena.

Estos controles y la presentación están implementados en el código; su funcionamiento físico está pendiente de verificar. El ciclo espera VBlank y ejecuta un tic por iteración, sin recuperación de tics perdidos. No hay todavía medición de tiempo de cuadro ni garantía de velocidad en DS.

## Construcción DS y bloqueo observado

Se consultaron los ejemplos oficiales de devkitPro y las reglas actuales de DS:

- `upstream/nds-examples`, revisión `f1ba715a451c6407f8b0f805999d0153062ff552`.
- `upstream/devkitarm-rules`, revisión `51d6f3a928e1c6486afaea114e28093c025feeeb`.

`tools/build_nds.py` sigue `ds_rules`: compila para ARMv5TE/ARM946E-S, utiliza `calico/share/ds9.specs`, enlaza libnds/calico y empaqueta con `ndstool`, el ARM7 predeterminado y el icono del SDK. Es una ruta de construcción preparada a partir de esas reglas, aún no validada mediante una compilación completa.

```powershell
python tools/build_nds.py --check --devkitpro C:/devkitPro
python tools/build_nds.py --devkitpro C:/devkitPro
```

También acepta `DEVKITPRO` y `DEVKITARM` del entorno. No modifica el PATH global ni instala herramientas. Si compila, el destino será `build/nds/ssb-motion.nds` y se guardarán mapa, tamaño del ELF, compilador y hashes en ese directorio.

Comprobaciones realizadas: no se encontró devkitARM ni una instalación en `C:/devkitPro`; la consulta HTTPS al índice oficial de paquetes devolvió HTTP 403 incluso fuera del sandbox. Docker respondió que no existe el conducto del motor Linux. Por ello no se pudo preparar la cadena de herramientas ni generar el `.nds` en este hito. No se ejecutó el instalador global de devkitPro ni se intentó cambiar Docker.

El chequeo del script contra `.tools/devkitPro` devuelve correctamente el listado de compilador, bibliotecas, especificaciones, ARM7 e icono que faltan. Este resultado verifica la detección de requisitos ausentes, no la construcción del destino.

## Siguiente comprobación

Con un SDK disponible: compilar el destino, corregir cualquier incompatibilidad real de API/enlace, abrirlo en emulador y verificar pantalla, botones, pausa y avance por tic. Solo después medir en consola e integrar las estructuras, animaciones y colisiones originales con la guía de BattleShip.
