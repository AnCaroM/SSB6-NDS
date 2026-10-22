# Prueba06: segundo salto y recuperación de Mario

Archivo: `Para tu DS/Prueba06/SSB-NDS-Prueba06.nds`. Se copia a la microSD
de DSPico; también se puede abrir directamente en melonDS. No requiere copiar
la ROM N64. Las cinco pruebas anteriores permanecen conservadas.

## Controles y demostración

- A: salto desde el suelo. Soltar y pulsar A otra vez en el aire: segundo salto.
  Mantener A no dispara el segundo salto y una tercera pulsación no añade otro.
- Cruceta: movimiento/control aéreo; X jab; B caminar lento; R cámara/mapa.
- Colgado: A o dirección hacia la isla para subir; Abajo o dirección opuesta
  para soltarse. START pausa, Y reinicia.
- SELECT: demostración automática de recuperación por la derecha y la izquierda,
  con un ciclo de 480 tics. Mario empieza sobre la isla, camina fuera, cae,
  usa su salto aéreo, vuelve al borde, se agarra y sube. Sólo se coloca su posición
  inicial en el suelo; las transiciones ocurren mediante entradas normales.
- La pantalla inferior indica J (saltos consumidos), A (saltos aéreos ejecutados)
  y B (agarres). El blanco continúa pasivo; el combate es el prototipo anterior.

## Datos originales y alcance

Referencia: ssb-decomp-re revisión `572a747d8171137718fad472c5d7473dc7309ffd`.
`ftCommonJumpAerialSetStatus`, `203_MarioMain` y `mpCommonSetFighterGround/Air`:

- Dos saltos en total; salir andando de la isla consume el salto de suelo.
- Impulso aéreo vertical `((80 * 0,7) + 26) * 0,9 = 73,8`.
  Nuestro orden de actualización aplica después gravedad 2,4, dejando 71,4
  en el primer tic integrado. No se afirma equivalencia del orden del motor N64.
- Impulso horizontal `stick_x * 0,35`. Selección de JumpAerialF/B según orientación
  previa y umbral original -10. Al aterrizar se restaura la cuenta; al agarrarse
  se deja un salto aéreo disponible, conforme a la transición suelo/aire original.

Los atributos también se contrastan directamente con los bytes de recurso 203,
FTAttributes 0x428, miembros 15..18 y 25. Las animaciones vienen de recursos
511 JumpAerialF, 512 JumpAerialB y 572 CliffCatch. Se reproduce la captura de ocho
tics antes de pasar a la pose estática de CliffWait. La subida aún tiene la
transición local de doce tics de Prueba05; no incluye toda la animación original.

JumpAerialF/B se almacenan con muestras cada dos tics (31/46 poses para 61/91
tics). El reproductor mantiene la duración, reteniendo cada muestra dos tics;
no interpola vértices en tiempo real. La captura conserva sus nueve muestras.
Mientras dura el salto aéreo se usa su animación, incluso tras el ápice.

El control aéreo y el giro instantáneo del prototipo siguen siendo provisionales:
la animación F/B se elige antes de ese giro local. No se ha portado el sistema
completo de orientación, físicas por estado, interrupciones ni invulnerabilidad.
No incluye todavía Super Jump Punch ni otros especiales.

## Validación y límites DS

`python tools/verify_scene.py`: mantener A, rechazo del tercer salto, restauración
al aterrizar/agarrarse, impulso, selección hacia atrás y dos recorridos completos
de salida/salto/agarre/subida sin KO. La misma secuencia se compila para DS.
Genera `build/scene/recovery-left.csv` y `recovery-right.csv` para revisar posiciones
y estados, además de las regresiones de cámara, combate, paredes y plataformas.

`python -m unittest discover -s tests`: formatos y recursos ROM, atributos,
límites, duración y muestreo de animaciones.
`python tools/check_ds_budget.py --demo dreamland`: memoria estática, texturas
y geometría. Las poses se comparten; no hay skinning, trigonometría ni asignación
de memoria por fotograma. Cámara y recursos gráficos del mapa no cambian.

Las pruebas nativas no prueban el comportamiento MIPS exacto ni el rendimiento
físico de DS. Esta versión necesita su propia prueba en la consola.

Resultado: once pruebas Python y todos los ejecutables de pruebas nativas
pasaron. Ambos recorridos terminan en espera sobre la isla, x=±2138, con un
salto aéreo y un agarre, sin KO. ARM9 estático: 1.874.820 bytes; poses compartidas:
1.584.000 bytes; texturas: 165.632 bytes. Con reserva gráfica: 936 polígonos y
2849 vértices. Memoria estática excluye pila, heap, ARM7 y otros gastos.

En la variante automática de melonDS 1.1 se observaron el salto aéreo
(J=2, A=1, B=0), el agarre izquierdo (J=1, A=1, B=1) y la espera sobre la isla
tras el regreso derecho (J=0, A=1, B=1). No se verificó cada fotograma ni los
controles manuales de esta compilación. No hay medición nueva de hardware DS.
