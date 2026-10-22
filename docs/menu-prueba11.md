# Prueba11: pantalla de inicio y selección de modo

Prueba11 añade una interfaz de inicio sobre el 1v1 de Prueba10. El combate,
la CPU, la cámara y los recursos son los mismos. Pruebas01 a 10 quedan
conservadas. **Esta compilación no se ha probado en emulador ni en DS.**

## Pantallas

- **Inicio.** Pantalla superior: Dream Land con los dos Mario en reposo, un
  emblema translúcido y un paneo lento. Pantalla inferior: título y
  "PULSA START" parpadeando. START, A o tocar la pantalla continúa.
- **Selección de modo.** MODO DE PRUEBA (Mario contra CPU, tres vidas),
  DEMO CPU vs CPU y CONTROLES. Arriba/abajo y A, o tocar la opción.
  B vuelve al inicio.
- **Controles.** Resumen de botones; A, B o tocar vuelve a la lista.
- **Combate.** Marcador con porcentaje coloreado (blanco/amarillo ≥50 %/
  rojo ≥100 %), vidas y los medidores de tiempo de Prueba10.
  - En pausa, B vuelve a la selección de modo.
  - Al terminar: A revancha, B menú.
  - START+SELECT reinicia y SELECT alterna TU/CPU vs CPU, como antes.

Los botones mantenidos al iniciar, pedir revancha o reiniciar se consumen hasta
soltarlos. Confirmar con A ya no provoca un salto accidental; otra pulsación
de A funciona normalmente. El resto de botones nuevos siguen disponibles.
Las descripciones de controles están abreviadas para caber en sus columnas;
las pruebas verifican los mismos textos que dibuja la consola de 32 columnas.

Al arrancar y al cambiar de pantalla hay un fundido corto desde negro
(`setBrightness`). El texto usa colores ANSI de la consola de libnds; si algún
código no se aplica, el texto se ve en el color por defecto.

## Coste en DS

- Lógica del menú en `src/menu.c`: sin SDK, sin asignaciones, probada en PC
  (`tests/test_menu.c`, incluida en `tools/verify_source.py`).
- VRAM: sin texturas ni bancos nuevos. El texto usa la consola del banco C.
- Geometría del inicio: escenario + dos Mario como en combate, sin proyectiles,
  más el emblema (26 polígonos / 104 vértices sin textura). Queda por debajo
  del cálculo de Prueba10 (944 / 2881) + 18 polígonos netos.
- El círculo del emblema se tabula una vez al arrancar con `sinLerp/cosLerp`;
  el paneo usa una consulta `sinLerp` por cuadro (tabla, no trigonometría).
- La consola inferior sólo se redibuja al cambiar de pantalla o de opción y,
  en combate, con la misma cadencia de Prueba10.

## Pendiente

Compilado y enlazado con devkitARM r68/libnds 2.0.2/Calico 1.2.0: ARM9 estático
973.052 bytes, sin contar pico de pila/heap ni ARM7. Las siete suites nativas
sin ROM pasan, incluidas las regresiones de botones y ancho de texto del menú.
El máximo estimado del inicio con reserva es 962 polígonos / 2953 vértices;
no cambia la asignación de texturas. Estos datos no son mediciones de rendimiento.

Falta comprobar en melonDS y después en DS/DSPico:
legibilidad de colores, posición del emblema, fundido y respuesta táctil.
Los nombres de modo son provisionales; no hay más modos implementados.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --compact-poses`
