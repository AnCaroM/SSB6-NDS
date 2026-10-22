# Prueba08: repertorio de Mario

Archivo para DSPico y melonDS: `Para tu DS/Prueba08/SSB-NDS-Prueba08.nds`.
Se copia a la microSD; no requiere copiar la ROM N64. Pruebas01 a 07 se conservan.
La cámara mantiene sus parámetros aprobados. El rival sigue siendo un blanco
pasivo, con porcentaje, aturdimiento, lanzamiento y reaparición tras caer.

## Controles nuevos

| Entrada | Acción |
|---|---|
| Cruceta / A | Mover / salto y segundo salto |
| X, soltar y volver a pulsar | Combo de dos puñetazos y patada; admite pulsación anticipada |
| Dirección + X en el suelo | Tilt lateral, arriba o abajo; diagonal inclina la patada lateral |
| Doble toque lateral, luego X | Ataque en carrera |
| Y + X + dirección | Smash lateral, arriba o abajo; diagonales inclinan el lateral |
| Y + L + X + diagonal | Variante intermedia alta/baja del smash lateral |
| X en el aire | Aéreo neutro; dirección elige frontal, trasero, arriba o abajo |
| L en el suelo | Agarre; después X o dirección lanza; dirección contraria lanza hacia atrás |
| B / Arriba+B / Abajo+B | Bola de fuego / Super Jump Punch / Mario Tornado |
| Pulsar B repetidamente durante Tornado | Elevación limitada por su ventana y carga disponible |
| X colgado / Y+X colgado | Ataque desde el borde rápido / lento |
| Y+L+X con Abajo / Arriba, sin dirección lateral, en el suelo | Atajos de prueba de los dos ataques de levantarse |
| Abajo sin atacar | Bajar de plataforma o soltarse del borde |
| R / START / SELECT | Cámara/mapa / pausa / catálogo automático de movimientos |
| START + SELECT | Reiniciar |

Para los atajos con modificadores, mantener Y antes de añadir L y X evita
iniciar un agarre. Y deja de reiniciar y B deja de caminar lento. L deja de activar el especial
hacia arriba, que pasa a Arriba+B. El resto de movimientos de recuperación se
conserva. Aún no existe el estado completo de tumbado del jugador: los ataques
de levantarse están disponibles mediante los atajos indicados. No hay escudo,
rodar, esquivar, pummel ni carga de smash; los smashes usan sus valores base.

El catálogo muestra una acción cada cuatro segundos nominales. Recorre los
ataques y sus variantes, incluidas fases del ataque de borde, agarre/espera,
dos lanzamientos y versiones de especiales en suelo/aire. Coloca a los dos
personajes al inicio de cada demostración para mostrar el contacto. No equivale
a una pelea ni a la secuencia de recuperación continua de Prueba07.

## Repertorio incorporado

Combo completo; ataque en carrera; tilts lateral alto/medio/bajo, arriba y abajo;
smash lateral con cinco inclinaciones, arriba y abajo; cinco ataques aéreos;
agarre y dos lanzamientos; bola de fuego; impactos del especial hacia arriba;
Tornado en suelo/aire; dos ataques de levantarse y ataques de borde rápido/lento.
El catálogo tiene 36 estados, contando variantes, transiciones y espera del
agarre: no son 36 ataques distintos. Los golpes de objetos (espada, bate, etc.)
no forman parte de este repertorio sin objetos.

## Datos originales y adaptación

Referencia: ssb-decomp-re `572a747d8171137718fad472c5d7473dc7309ffd`.
Las animaciones se extraen de la ROM del usuario. `mario_moves.py` interpreta
el subconjunto de eventos ofensivos de `202_MarioMainMotion.c`: esperas absolutas
y relativas, bucles, saltos a scripts compartidos, creación/modificación/borrado
de hitboxes y renovación de impactos. El recorrido incluye la continuación
implícita de FireballGround en el script compartido siguiente.

Las pruebas empaquetan independientemente los cinco words de cada comando de
impacto y comprueban su presencia exacta en el recurso binario 202. También
contrastan las descripciones de lanzamientos y los atributos de bola de fuego
con recursos originales. Se conservan daño, ángulo, crecimiento/base, peso fijo,
radio (el tamaño codificado es diámetro), ventana y grupo. Las posiciones de
impacto se calculan offline con los huesos originales: IDs 4..28 corresponden
a los 25 nodos del modelo. Se usan posiciones por tic, independientemente de la
cadencia reducida de las poses visuales. El multigolpe renueva su registro al
borrar/recrear o refrescar la ventana; un cambio a daño tardío no da otro impacto.

La bola de fuego usa el sprite CI4/paleta original del recurso 297 convertido
a RGBA16 de 16x16. Sus parámetros de combate vienen del 204: 7 %, ángulo 361,
crecimiento 25 y base 10. Se adapta la vida 140, velocidad 50 a -5 grados,
gravedad 1,2 y rebote 0,85 de `wpmariofireball.c`. Se permiten cuatro proyectiles
compartiendo una textura. El rebote está limitado a superficies horizontales;
paredes, techos, reflejo y absorción del motor original aún faltan.

Los lanzamientos usan 12/16 %, 45 grados y liberación por el flag original.
El agarre sujeta al blanco con una posición simplificada; no hay escape ni
animaciones completas de daño/captura del rival. El Tornado tiene ventanas
repetidas y elevación por pulsaciones dentro del flag original, pero su cambio
suelo/aire y carga son una adaptación reducida. Super Jump Punch ahora tiene
sus ventanas de daño; no se garantiza la cadena completa contra el blanco
porque el motor de daño todavía es parcial.

La fórmula de knockback se especializa a peso/ratio/handicaps neutrales. Continúan
siendo provisionales la cápsula de daño 2D del blanco, hitstop de cuatro tics,
integración de velocidades, transición estrecha del ángulo 361, aterrizaje de
aéreos y colocación de agarres/borde. Algunas animaciones llevan desplazamiento
de raíz visible; sus colisiones con el escenario no reproducen todos los
procedimientos TransN originales. No hay stale moves, DI, cancelaciones completas,
IA, invulnerabilidad de ataques, audio ni efectos/monedas originales.

## Presupuesto DS y validación

Las posiciones de vértices idénticas se comparten dentro de cada pose. Se pasó
de 960 posiciones repetidas a 189 únicas; una tabla de índices conserva los 960
vértices de dibujo, sus colores y UV. El test reconstruye todos los vértices y
comprueba la identidad de hueso/posición. No se elimina geometría ni se hace
skinning por fotograma. Los clips nuevos guardan una muestra cada dos tics,
reteniéndola durante dos tics sin cambiar la duración del movimiento.

Los 46 clips ocupan 1.188.432 bytes de poses, compartidas por los dos Mario.
La bola de fuego añade 512 bytes de textura. La reserva gráfica contempla hasta
cuatro cuadriláteros adicionales. No hay asignaciones dinámicas por fotograma.
El tiempo real de CPU y el pico de RAM necesitan medición en DS; el ELF estático
no incluye pila, heap, ARM7 ni todos los gastos de ejecución.

`python tools/verify_scene.py`: selección, combo, reproducción de todo el catálogo,
agarrar/lanzar, proyectiles y multigolpes, además de las regresiones anteriores.
`build/scene/moves.csv` registra impactos, porcentaje máximo y KO de cada caso.
`python -m unittest discover -s tests`: dieciocho pruebas de conversión, datos ROM,
ventanas, daño tardío, multigolpe, fuego, mapas y conservación de vértices.
`python tools/check_ds_budget.py --demo dreamland`: presupuestos de DS original.

Esta compilación requiere su propia prueba en DS. La presencia de ataques y
datos originales no significa que el motor completo de Smash 64 esté portado.

Resultado final: ARM9 estático 1.519.712 bytes; texturas compartidas 166.144 bytes;
presupuesto con reserva y cuatro proyectiles: 940 polígonos/2865 vértices.
Las dieciocho pruebas Python y todos los ejecutables nativos pasaron. En el caso
de prueba, DAIR conecta siete impactos, Tornado de suelo catorce y los lanzamientos
alcanzan 12/16 % antes de que el blanco pueda reaparecer por KO. El informe guarda
el porcentaje máximo para no confundir la reaparición con ausencia de daño.

En melonDS 1.1 se comprobó el arranque, la conservación visual del modelo compacto,
el catálogo avanzando, ataque en carrera con 12 % y especial hacia arriba con
5 % en muestras observadas. No se inspeccionó visualmente cada movimiento,
proyectil, variante o combinación manual. La cobertura completa del catálogo
descrita arriba corresponde a pruebas nativas, no a DS física.
