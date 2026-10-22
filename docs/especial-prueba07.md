# Prueba07: Super Jump Punch como recuperación

Copiar `Para tu DS/Prueba07/SSB-NDS-Prueba07.nds` a la microSD DSPico, o abrirlo
en melonDS. Conserva la cámara y las seis pruebas anteriores.

## Controles

- L: especial hacia arriba. La cruceta horizontal al pulsarlo elige dirección
  e inclinación; sin dirección mantiene la orientación actual.
- A: salto; soltar y pulsar otra vez, salto aéreo. X jab; B andar lento.
- Cruceta mover/control aéreo; Abajo bajar plataforma o soltarse del borde.
- R cámara/mapa, START pausa, Y reinicio, SELECT demostración automática.

La demostración alterna cuatro recorridos de 240 tics: regreso con salto aéreo
por derecha/izquierda y dos recorridos más profundos que requieren además el
especial. En estos últimos Mario camina fuera, salta en el aire al tic 70 y usa
el especial al 100. Sólo la posición inicial se coloca sobre la isla; el resto
ocurre con entradas normales y colisiones. En pruebas nativas, omitir el especial
en el recorrido profundo provoca KO; con él regresa al suelo sin KO.

El especial sirve para recuperarse: todavía no causa daño, genera monedas ni
sonido. El combate conserva el jab de prueba. Después del especial se entra
en caída especial: no admite otro salto ni especial hasta aterrizar o agarrarse.
El aterrizaje especial tiene 25 tics de recuperación. Agarrarse restaura el uso.

## Procedencia y aproximaciones

Referencia ssb-decomp-re `572a747d8171137718fad472c5d7473dc7309ffd`:
`ftmariospecialhi.c`, `ftmario.h`, `ftphysics.c`, `ftcommonfallspecial.c`,
`202_MarioMainMotion.c` y recurso 637 SuperJumpPunchAir.

La curva TransN original tiene seis tics iniciales sin ascenso, después curvas
Hermite para Y/Z. `special_hi.c` adapta sus valores y derivadas, verificados
contra los tracks decodificados de la ROM para cada tic 0..40. Las diferencias
de posición se multiplican por tamaño original 1,12. En inicio aéreo se anula
la velocidad vertical y se divide la horizontal por 1,5; durante el arranque se
usa gravedad 0,5. En vuelo guiado por TransN se aplica el multiplicador 0,95.

Se conserva el umbral de inclinación 50 y su máximo ±18 grados para stick ±80,
así como el umbral 20 para orientar el modelo. La dirección se fija al pulsar L;
todavía no se reproduce la ventana original de cambios durante los primeros
seis tics ni todas las combinaciones de flags, proyección y paso de plataformas.
La rotación usa un polinomio acotado, sin trigonometría en tiempo real. El
coordinador y orden de actualización son locales, no equivalencia MIPS exacta.

La caída especial usa límite horizontal 30*0,6=18 y el aterrizaje se adapta a
25 tics de recuperación (referencia 7/0,28). El resolver de superficies y la
subida del borde siguen siendo los de los prototipos anteriores. Invulnerabilidad,
golpes múltiples, efectos y audio originales no están implementados.

La pose original del especial se convierte quitando su desplazamiento TransN:
ese desplazamiento mueve al personaje mediante la física y no debe duplicarse
en el modelo. Se guardan 21 poses, una cada dos tics, para 41 tics de especial.
FallSpecial, recurso 541, conserva tres poses para un bucle de seis tics.
Se comparten los datos entre ambos personajes, sin skinning ni asignaciones
por fotograma. La espera colgado sigue estática y la subida sigue provisional.

## Verificación DS

`python tools/verify_scene.py`: lanzamiento, prohibición de repetir, caída especial,
aterrizaje, dos recuperaciones profundas y regresiones de salto/cámara/combate.
Genera `special-hi.csv` (curva nativa) y `special-left/right.csv` (trayectorias).
`python -m unittest discover -s tests`: contraste independiente curva C/ROM,
animaciones y recursos. `python tools/check_ds_budget.py --demo dreamland`:
memoria, texturas y geometría. Las pruebas nativas no sustituyen DS física.

Resultado: los ejecutables nativos y trece pruebas Python pasaron. Los dos
recorridos profundos terminan en el suelo, x=±1717,3, con un salto aéreo y un
especial, sin KO. ARM9 estático: 2.015.768 bytes; poses compartidas: 1.722.240;
texturas: 165.632. Geometría con reserva: 936 polígonos y 2849 vértices. El
estático no incluye toda la RAM de ejecución y no demuestra tiempo por frame.

El contraste ROM/curva detectó y corrigió un error del conversor de animación:
SetTargetRate cambiaba retrospectivamente muestras anteriores. Ahora el cambio
de derivada entra en vigor en el tiempo del comando, con prueba de regresión.
Los recursos de esta versión se regeneraron; los binarios anteriores se conservan.

En melonDS 1.1 se verificaron el arranque del modo automático y una muestra del
especial en estado UP-B, con salto aéreo consumido (J=2, A=1). No se inspeccionó
cada fotograma, el aterrizaje especial ni la entrada manual por L. Las trayectorias
completas y bloqueos se verificaron en pruebas nativas. Falta probar esta versión
en DS física.
