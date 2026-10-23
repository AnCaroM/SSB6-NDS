# Prueba13: animaciones de combate

Copiar `SSB-NDS-Prueba13.nds` a la microSD de DSPico. Se conservan las versiones
anteriores. Esta versión no reproduce los efectos sintetizados de Prueba12.

## Qué se corrigió

La conversión anterior guardaba muchos ataques y reacciones cada dos tics y
reconstruía el intermedio mezclando posiciones de vértices. Un hueso que gira
rápido no describe una línea recta: esa mezcla acortaba o desplazaba miembros
y perdía poses breves. La comparación offline con las poses originales por tic
mostró diferencias de hasta unos 252 puntos de mundo en Jab2 y 206 en FTilt.
Es una medida de vértices, no un error en píxeles ni una medición en hardware.

Ahora los 36 movimientos de `SPECS` y los recursos de daño 523..540 conservan
cada tic original. Se mantienen los huesos, curvas, velocidad de reproducción
y eventos de ataque de la conversión anterior. No se acelera el golpe ni se
cambia cuándo está activa su hitbox. La cuantización sigue limitada a 2,1
unidades por eje en las poses almacenadas; el máximo actual ronda 2,001.

También se corrigió la presentación de las reacciones:

- En el contacto se muestra el primer cuadro original de retroceso (tic 1),
  evitando congelar durante todo el impacto el cuadro neutro de preparación.
- Al terminar el aturdimiento, la animación puede acabar su recorrido en vez
  de saltar inmediatamente a reposo. Esto no prolonga el bloqueo de controles:
  moverse, saltar o atacar cuando vuelve el control interrumpe esa continuación.
- Tras un lanzamiento fuerte se usa `DamageFall`, recurso 540, si Mario sigue
  en el aire al terminar la reacción. Aterrizar usa la animación de aterrizaje.
- La salida hacia caída/reposo/aterrizaje tiene dos cuadros intermedios de
  mezcla y llega a la pose nueva en el tercero. No se mezcla el inicio de un
  golpe nuevo ni se retrasa la animación ofensiva.

El inicio en tic1 y las transiciones son decisiones de presentación de este
port. El motor original tiene otro orden de estados/hitlag; no se afirma
equivalencia exacta. Continúan pendientes tech, rebotes y todo el sistema de
tumble original. Las hurtboxes siguen siendo una cápsula de integración.

## Sonido y recursos DS

Se retiró la reproducción de sonidos provisionales del binario versus: no se
enlazan el reproductor ni el banco PCM, y no se inicializan canales de sonido.
Las herramientas y pruebas de Prueba12 quedan disponibles como historial.

Los 68 clips comparten un banco compacto de 1.308.318 bytes entre ambos Mario.
ARM9 estático: 1.661.912 bytes, aproximadamente 1,585 MiB, frente al presupuesto
preventivo de 2,5 MiB. Es más memoria que Prueba12 a cambio de conservar las
poses rápidas; no añade skinning ni trigonometría por cuadro. Las transiciones
usan el mismo búfer de pose y sólo hacen trabajo adicional durante su salida.

Texturas 166.144/262.144 bytes; geometría presupuestada 944 polígonos y 2881
vértices, dentro de 1400/4000. ELF estático no equivale al pico total de RAM ni
incluye todos los usos de ARM7/pila/heap. El objetivo sigue siendo DS/DS Lite;
el rendimiento final requiere consola física.

## Cómo se revisa

Las pruebas nativas verifican la continuación tras stun, interrupción por una
acción válida, transición a caída de daño, aterrizaje y congelación de poses,
además de todas las pruebas anteriores de combate y recuperación. Las pruebas
Python comprueban que todo el repertorio y el daño usan paso 1 y que las poses
del decodificador C coinciden con la conversión offline.

Hay una ROM diagnóstica independiente:

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --compact-poses --animation-review
```

Genera `build/nds/ssb-versus-review.nds`: repite jab, patada lateral, smash
lateral, smash vertical, aéreo frontal y aéreo abajo, a un tercio de velocidad,
en ambas direcciones. La CPU rival queda pasiva para repetir el mismo contacto.
La pantalla inferior identifica ataque, tic y reacción. Los doce escenarios
tienen comprobación de contacto y reacción en PC. Esta ROM no es la entrega
jugable y sus tiempos de perfil incluyen el modo de diagnóstico.

Se observaron en melonDS 1.1 las fases de recuperación, lanzamiento horizontal
y ambos sentidos usando esa ROM de revisión. También se comprobó el arranque,
entrada por pantalla táctil y combate CPU contra CPU de la compilación jugable
final en melonDS 1.1, con ataques, lanzamientos y pérdida de vidas. Son comprobaciones visuales
parciales; no una validación exhaustiva de los 36 ataques en emulador ni una
prueba física de esta versión.

## Para comparar en tu DS

1. X y pulsaciones sucesivas de X: revisar el combo corto y la recuperación.
2. Lateral+X: revisar la pierna durante la patada y el regreso a reposo.
3. Lateral+Y+X y arriba+Y+X sin lateral: revisar extensión, contacto y vuelo.
4. Saltar con A y atacar con dirección+X; probar también los especiales B,
   arriba+B y abajo+B. La cámara y los controles conservan sus parámetros.
5. Dejar recibir golpes: la reacción debe verse en el impacto y terminar sin
   un corte brusco; cuando se recupera el control, debe poder interrumpirse.

Los controles, el modo 1v1 y los menús mantienen su funcionamiento anterior.
