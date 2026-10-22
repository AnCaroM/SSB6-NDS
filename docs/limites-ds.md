# Presupuesto técnico: DS original / DS Lite

Actualización Prueba10: el usuario confirmó que las animaciones de Prueba09
quedaron bien. La nueva compilación 1v1 contra CPU usa 967.492 bytes estáticos
ARM9, con 623.733 bytes de poses compactas compartidas. Hay 166.144 bytes de
texturas en bancos A+B (256 KiB), E para paletas y C para consola inferior.
La reserva de geometría de dos actores y ocho proyectiles es 944 polígonos y
2881 vértices. Son cifras estáticas/preventivas; el rendimiento y pico de RAM
de Prueba10 siguen pendientes en hardware. Ver `optimizacion-y-escalado.md`.

El usuario pidió conservar estas limitaciones como requisito del proyecto. La
consola objetivo es su DS con DSPico. El usuario confirmó que Prueba01 funciona
muy bien; esto valida esa compilación concreta, no las posteriores.

| Recurso | Límite o referencia | Criterio del proyecto |
| --- | --- | --- |
| ARM9 | Aproximadamente 67 MHz, sin FPU | Evitar skinning y trigonometría flotante por cuadro |
| ARM7 | Aproximadamente 33 MHz | Servicios del sistema/audio; no asumir CPU libre |
| RAM principal | 4 MiB compartidos | Dejar espacio para ARM7, heap, pila, SDK y expansión |
| Motor 3D | 2048 polígonos / 6144 vértices almacenados por cuadro | Presupuesto inicial de envíos: 1400 / 4000, con margen de recorte |
| VRAM | 656 KiB en bancos especializados | Texturas actuales en banco A, 128 KiB; respetar usos de consola y pantallas |
| Pantallas | 256 x 192 cada una | Interfaz legible y cámara que conserve el personaje visible |
| Tiempo | Objetivo 60 Hz, aproximadamente 16,7 ms/cuadro | Medir en hardware; no deducirlo del indicador de melonDS |

Prueba01: Mario tiene 320 triángulos y 960 vértices enviados; sus tres texturas
directas suman 8192 bytes y se cargan una sola vez. Las poses muestreadas se
comparten entre instancias. El ARM9 suma alrededor de 1,09 MB de texto/datos/BSS;
esa cifra no es el pico total de memoria. Añadir un rival debe reutilizar estos
recursos y sumar sobre todo estado e instrucciones de dibujo.

La RAM de poses crece linealmente con cuadros y vértices. Antes de incorporar
muchos personajes convendrá pasar a vértices compartidos, poses más compactas o
transformación de huesos en punto fijo. Añadir contenido sin medir no es aceptable.

Referencias: cabeceras instaladas de libnds/calico para bancos y relojes;
[notas de ingeniería del motor 3D](https://raddad772.github.io/2025/03/14/nds_ge_re_basics.html),
basadas en investigación del hardware, para la geometría y el recorte.
