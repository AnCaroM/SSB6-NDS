# Prueba04: cámara automática para Dream Land

Archivo para DSPico: `Para tu DS/Prueba04/SSB-NDS-Prueba04.nds`.
Copiar a la microSD; no hacen falta archivos de la ROM en la tarjeta.

La vista automática empieza más cerca: altura mínima de 2800 unidades frente a
3400 en la vista cercana de Prueba03 (personajes aproximadamente un 21% mayores
a igual pose). Sigue a ambos personajes horizontal y verticalmente, amplía el
encuadre al separarse o saltar y vuelve gradualmente cuando se reúnen. Mostrar
todo el decorado deja de ser el criterio principal. No modifica las mecánicas.

Controles: cruceta mover; A saltar; X golpe; B andar lento; Abajo bajar plataforma;
Y reiniciar; START pausa; SELECT demostración del jab; R alternar cámara automática
y mapa completo. El modo de mapa es una referencia opcional. El seguimiento se
congela durante la pausa. Y reinicia también la cámara. La pantalla inferior indica
AUTO/MAPA, altura del encuadre y tiempos de trabajo y periodo.

## Procedencia y adaptación

Referencia: `upstream/ssb-decomp-re/src/gm/gmcamera.c`, revisión
`572a747d8171137718fad472c5d7473dc7309ffd`.

- `gmCameraUpdateInterests`: unión de áreas alrededor de cada luchador;
  márgenes horizontales originales 700/1000 según orientación y verticales ±700.
- `dGMCameraPlayerZoomRanges`: 1,50/1,32/1,16/1,00 para uno a cuatro luchadores.
  El prototipo integra dos; aún no integra proyectiles ni estados especiales.
- `gmCameraGetTargetAtY`: sesgo vertical gradual hasta 0,0682.
- `gmCameraPan`: seguimiento interpolado; DS utiliza coeficiente fijo 0,08,
  cercano al 0,08 del retorno vertical de Prueba01. No reproduce la función
  original de distancia para elegir la velocidad.
- `func_ovl2_8010C670`: expansión inmediata y contracción al 0,075 por tic.
- Límites originales de interés: arriba 4000, abajo -2000, derecha 3900,
  izquierda -3900. Extraídos de recurso 255, MapHeader 0x14, miembro 0x6c.
  No son los límites de eliminación ni los límites del centro de cámara.

La proyección sigue siendo ortográfica 4:3 para DS. Altura mínima 2800, foco
de cuerpo +200 y márgenes de seguridad durante seguimiento son decisiones
locales. No se han extraído los atributos originales de cámara de Mario;
no se aplican cambios por reposo prolongado ni modos KO, entrada o rebirth.
Por tanto es una adaptación parcial documentada, no la cámara N64 exacta.
Cuando un luchador supera los límites originales, deja de alejar la cámara
indefinidamente; todavía no hay indicadores de luchador fuera de pantalla.

El cálculo usa enteros en unidades de 1/16, sin asignaciones por fotograma,
trigonometría ni raíces. Conserva todos los modelos y texturas compartidos.
La proyección y la conversión desde la física flotante mantienen la ruta libnds
existente. El fondo se dibuja en coordenadas centradas para respetar el rango
4.12 de los vértices DS incluso al ampliar la vista.

## Verificación

`python tools/verify_scene.py`: pruebas nativas de seguimiento, separación,
salto, retorno gradual, extremos, límites y reinicio, además de escena y combate.
`python -m unittest discover -s tests`: conversores y contraste de límites ROM.
`python tools/check_ds_budget.py --demo dreamland`: ELF, texturas y geometría.
Estas comprobaciones no sustituyen una prueba visual ni mediciones en la DS.
Se verificó el arranque del binario final en melonDS 1.1: escenario, fondo,
ambos personajes y cámara AUTO con altura 2800. La prueba visual no cubrió
todavía el seguimiento durante desplazamientos ni el botón R. Los casos de
movimiento descritos arriba se verificaron en pruebas nativas de la lógica.

Prueba01, Prueba02 y Prueba03 se conservan. El usuario probó las dos vistas de
Prueba03 en DS y prefirió la cercana; Prueba04 requiere nueva validación física.
