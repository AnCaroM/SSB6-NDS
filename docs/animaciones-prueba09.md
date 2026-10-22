# Prueba09: revisión de animaciones

Copiar `SSB-NDS-Prueba09.nds` a la microSD de DSPico. Es autónomo; no necesita
la ROM N64 en la DS. Las Pruebas01 a 08 se conservan.

## Correcciones

- Articulaciones auxiliares: la lectura anterior aplicaba el primer script al
  cuerpo incluso cuando correspondía a un nodo auxiliar. Esto desplazaba los
  scripts restantes en ataque corriendo, golpes arriba/abajo, recuperación,
  captura y movimientos de borde. Ahora el conversor recorre primero el nodo
  auxiliar y después las articulaciones comunes.
- Carrera: doble toque lateral y mantener la dirección activa Dash y después
  Run originales. Soltar reproduce RunBrake. Antes se veía Walk2 y el motor
  volvía a limitar la velocidad a la de caminar. La velocidad y detección de
  doble toque son adaptaciones a la cruceta, no el sistema analógico completo.
- Agarre: al contactar se inicia CatchPull inmediatamente y se mantiene su
  pose final, sin repetir el tirón. El rival usa CapturePulled y sigue la
  posición del nodo28 del captor. Su rotación y las poses emparejadas de los
  lanzamientos aún son aproximaciones.
- Aéreos: mover la cruceta en el aire conserva la orientación, de modo que el
  ataque trasero funciona también si se pulsa la dirección antes de X.
- Reproducción: interpolación entera de los 189 vértices únicos entre muestras
  cada dos tics. Los clips no cíclicos incluyen ahora su cuadro final exacto.
  Es una aproximación entre poses; no interpola los huesos como el motor N64.

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Caminar |
| Doble toque lateral y mantener | Carrera; soltar frena |
| X / dirección + X | Combo / golpe direccional |
| X durante carrera | Ataque corriendo |
| Y + X + dirección | Smash |
| A | Salto; volver a pulsar para segundo salto |
| X en el aire + dirección | Aéreo neutro, frontal, trasero, arriba o abajo |
| L | Agarre; X o dirección para lanzar, contraria para lanzamiento trasero |
| B / arriba+B / abajo+B | Fuego / recuperación / Tornado |
| R / START / SELECT | Cámara / pausa / catálogo automático |
| START + SELECT | Reiniciar |

El catálogo conserva las variantes de Prueba08, con 36 estados ofensivos.
La inspección de recursos cubre 50 clips, incluyendo locomoción y captura.
El blanco sigue siendo pasivo. Cámara y escenario mantienen los parámetros
anteriores. Escudo, sonido, efectos y motor completo de daño siguen pendientes.

## Procedencia y validación

ROM propia US v1.0 y decompilación fijada a
`572a747d8171137718fad472c5d7473dc7309ffd`.

La estructura se contrastó con `ftMainSetStatus`/gestión de partes ocultas,
`lbCommonAddFighterPartsFigatree`, `dMarioMain_hiddenparts` y la tabla de
movimientos `dFTMarioMotionDescs`. Hay una discrepancia en los nombres de macros
de la decompilación: bit30, llamado XROTN, corresponde al nodo TransN; bit31,
llamado TRANSN, al nodo XRotN. Se utiliza la estructura real y sus bitfields.
TransN se separa del cuerpo para la física; las fases colgadas conservan su
desplazamiento visual respecto al borde. El movimiento de raíz de otros ataques
sigue adaptado a la escena reducida.

`python tools/audit_animations.py` comprueba todos los bancos y genera siete
hojas de contacto y `build/animation-review/audit.json`. Las hojas permiten
revisar cinco instantes de cada clip sin ejecutar el juego. No prueban los
controles ni equivalencia exacta con la ejecución original.

Las pruebas cubren estructura auxiliar de los 50 clips contra la tabla original,
cuadros finales, mezcla entera con coordenadas negativas, ciclos, selección de
golpes, carrera sostenida, frenado, orientación aérea y sujeción sin repetición,
además de las regresiones de escena, colisiones, recuperación y cámara.

La revisión automática de permisos no pudo autorizar el acceso de control a
melonDS por un límite de uso. Esta compilación no se ha probado en el emulador
ni en hardware. La inspección de poses offline y las pruebas de código no
sustituyen esa comprobación. No se afirma una validación visual completa en DS.
