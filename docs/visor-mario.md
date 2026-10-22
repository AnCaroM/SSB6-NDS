# Primer modelo original en Nintendo DS

`build/nds/ssb-mario.nds` muestra la geometría de Mario extraída de la ROM
US v1.0 del usuario. Es una aplicación nativa ARM9/libnds con renderizado en
la unidad 3D de DS. Esta prueba está separada de `ssb-motion.nds`.

## Reproducir

Con la ROM y los recursos preparados según `rom-y-primeros-recursos.md`:

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --demo mario
```

Abrir `Abrir Mario DS.cmd`, o cargar `build/nds/ssb-mario.nds` en melonDS.
El conversor se ejecuta automáticamente antes de compilar. Requiere NumPy.

Controles con el mapeo local de melonDS:

| Acción | DS | Teclado de PC |
| --- | --- | --- |
| Girar | Cruceta izquierda/derecha | Flechas izquierda/derecha |
| Acercar | A | X |
| Alejar | B | Z |
| Restablecer | Y | R |
| Girar con puntero | Arrastrar pantalla táctil | Arrastrar pantalla inferior |

## Conversión

`tools/convert_mario_model.py` comprueba el SHA-256 del recurso 296 y recorre
sus enlaces internos de reubicación. Lee el árbol de alta resolución en
`0x2200`: 25 articulaciones, de las cuales 14 tienen listas gráficas. Aplica
traslación, rotación RPY y escala según `syMatrixTraRotRpyRScaF`, acumula las
transformaciones de los padres y expande 320 triángulos / 960 vértices.

El lector limitado de F3DEX2 interpreta cargas de vértices y triángulos. Conserva
la caché de vértices ya transformados entre piezas: los anillos de codos y
rodillas se reutilizan para conectar las articulaciones. Los cambios de ST se
reconocen pero no se aplican porque este visor aún no utiliza texturas.

Las tablas de materiales están en `0x0000` y los scripts de color en `0x2680`.
Se toman sus valores iniciales, conservando el valor anterior cuando un comando
de tipo Step tiene duración positiva. La iluminación direccional se aproxima
en el conversor; los colores quedan precalculados en los vértices.

Se normaliza el modelo para el visor y se cuantizan posiciones a punto fijo
4.12. Las caras se dibujan por ambos lados para facilitar la inspección.

## Límites y siguiente paso

Esta es la pose base del árbol, sin animación. No incluye texturas, combinadores
RDP, combate, escenario ni conexión con la prueba de movimiento. Por eso faltan
los ojos, el bigote y los detalles del overol, aunque la geometría sea original.
Los colores y la luz no pretenden reproducir exactamente la imagen de N64.

El siguiente paso es convertir texturas y sus paletas; después evaluar una
animación original y conectar el modelo con la escena de movimiento.

Los archivos derivados de la ROM (`mario_mesh.c`, `.obj`, binarios y `.nds`)
permanecen en `build/`, excluido por `.gitignore`. `conversion-info.json`
registra procedencia, límites, dimensiones, articulaciones y comandos leídos.
La verificación visual en emulador no demuestra rendimiento en una DS física.

## Verificación de esta versión

Compila con `-Wall -Wextra -Werror`. ARM9: 82.332 bytes de texto, 8.696 de
datos y 2.496 de BSS; estas cifras no incluyen toda la memoria de ejecución.
`ndstool -i` informa CRC correctos de cabecera, logo, banner y área segura.
Se cargó la versión final en melonDS 1.1 y se observaron el modelo, la consola
y el contador de cuadros avanzando. La ventana indicó puntualmente `[60/60]`.

Los eventos breves de teclado y arrastre inyectados por la herramienta no
produjeron un cambio visible del ángulo. La respuesta de giro, zoom y reinicio
queda pendiente de comprobar con entrada manual sostenida; no se considera
verificada por haber implementado los controles.
