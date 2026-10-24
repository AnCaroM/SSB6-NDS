# Prueba15: animación por huesos

Prueba15 cambia cómo se guardan y dibujan las animaciones de Mario, para dejar
espacio a un segundo luchador (Link). El juego, la cámara, el menú y el sonido
no cambian. Las versiones anteriores se conservan. **Aún no se ha visto en
emulador ni en DS.**

## Qué cambió

Antes se guardaba la posición de los 189 vértices únicos de Mario en cada
tic (banco de poses compacto: 1,25 MiB). Ahora cada tic guarda los 9 canales
locales de cada hueso (traslación, giro alabeo/cabeceo/guiñada, escala) más el
prefijo TransN/XRotN, y sólo los canales que cambian dentro de cada clip; el
resto es una fila base por clip. En la DS se reconstruyen las 25 matrices con
comandos de matriz del hardware (`glTranslate/glRotate*i/glScale` +
`glStoreMatrix`) y cada vértice se envía en coordenadas locales de su hueso
tras `glRestoreMatrix` (piel rígida, como el original: cada vértice pertenece a
un hueso). Mario usa 78 cambios de matriz por dibujo.

| | Prueba14 (vértices compactos) | Prueba15 (huesos) |
|---|---|---|
| Banco de animación | 1.308.318 bytes | **232.563 bytes** |
| ARM9 estático | 1.895.660 bytes | **823.276 bytes** |
| Tics guardados | ataques/daño cada tic, resto cada 2 | **todos cada tic** |

Formatos: traslación int16 en unidades de modelo ×8; giros int16 con 32768 por
vuelta (unidades de libnds); escala 4.12; vértices locales v16 en unidades de
modelo /64. La mezcla de recuperación de daño mezcla canales (giros por el
camino corto) en lugar de vértices.

## Precisión

- Offline (Python, matrices en coma flotante con seno/coseno 4.12): máximo
  0,35 unidades de mundo frente a la pose original.
- Host en C imitando la DS (matrices 20.12 encadenadas, trigonometría 4.12):
  máximo 1,33 unidades frente al banco de 16 bits en las 2306 poses
  muestreadas; límite del proyecto 2,1 (el banco compacto llegaba a ~2,0).
- El comportamiento exacto del motor geométrico de la DS (redondeos, orden de
  multiplicación) sólo se confirma viéndolo en melonDS y en DS.

## Coste en tiempo

Por luchador y cuadro: copiar ~234 valores, 25 huesos × ~6 comandos de matriz,
78 restauraciones durante el envío de vértices. Antes la CPU decodificaba y
mezclaba 567 valores por luchador. Medir Trabajo/Pico en DS.

## Opciones de compilación

`--skeletal-poses` es el nuevo formato; `--compact-poses` y el banco de 16 bits
se conservan para comparar. Pila de matrices: ranuras 3..29 reservadas para el
esqueleto; quien llame al renderizador no debe anidar más de 2 `glPushMatrix`.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --skeletal-poses`
