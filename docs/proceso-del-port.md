# Cómo se construyó el prototipo

El resultado es un programa nativo para ARM9/ARM7 de DS, con lógica adaptada y
recursos convertidos de Smash 64. No equivale todavía al port del juego completo.

1. Se fijaron revisiones locales de `ssb-decomp-re` y BattleShip y se estudiaron
   actualización de luchadores, estados, animación, colisiones, gráficos y carga
   de datos. La decompilación permite seguir funciones y estructuras originales.
2. Se preparó devkitARM/libnds/calico y se compiló una escena mínima. Un rectángulo
   permitió comprobar movimiento y suelo antes de introducir el modelo de Mario.
3. Siete operaciones numéricas de movimiento se aislaron y compararon con cuerpos
   de funciones de la decompilación. El controlador reducido y las plataformas
   son integración propia, no todo el runtime original del luchador.
4. Se verificó la ROM US v1.0 del usuario y se leyó su tabla de recursos. Un pequeño
   ejecutable con el decodificador VPK0 de Torch descomprime los recursos. Los
   punteros internos se resuelven usando cadenas de reubicación.
5. El conversor gráfico interpreta un subconjunto de F3DEX2, reconstruye matrices
   del esqueleto y transforma triángulos, colores y coordenadas de textura. La
   caché de vértices debe conservarse entre piezas para unir codos y rodillas.
6. Las texturas CI4 y las paletas RGBA5551 se convierten al formato de DS. Se
   ajustan dimensiones, orden de nibbles, coordenadas, repetición y reflejo.
   El renderizado no reproduce todos los combinadores de color de N64.
7. Se leen los scripts Figatree/AObjEvent16 de animación. Valores, pendientes,
   esperas, interpolación cúbica y cambios inmediatos se muestrean al compilar.
   Las poses en punto fijo ahorran CPU en DS a cambio de RAM; se comparten entre
   ambas instancias de Mario. Esta evaluación no promete igualdad bit a bit.
8. Se integraron movimiento, salto, plataformas, caída y reaparición. El jab usa
   la animación original y parámetros documentados; las cápsulas de impacto y
   varios estados siguen siendo aproximaciones. El blanco pasivo reutiliza el
   modelo, las texturas y las poses de Mario.
9. Las compilaciones se verifican con pruebas de lógica en PC, inspección del
   `.nds`, revisión visual en melonDS y presupuestos de memoria/geometría. El
   usuario confirmó que Prueba01 funciona muy bien en su DS con DSPico. Esto no
   mide por sí solo el pico de RAM o el tiempo por cuadro de versiones futuras.
10. Dream Land se extrae de los recursos originales, con cuatro capas gráficas,
    texturas, fondo y suelos horizontales. Su adaptación conserva la procedencia
    de los datos y declara aparte los efectos o servicios todavía pendientes.

La DS original/DS Lite es el objetivo: ARM9 ~67 MHz sin FPU y 4 MiB compartidos.
Los presupuestos de geometría y VRAM se comprueban por separado de la RAM. REA
se ha estudiado como herramienta, pero no ha ejecutado análisis en este trabajo.
Las operaciones sobre repositorios y los conversores descritos son herramientas
normales de código, no resultados de REA.
