# Optimización DS: del 1v1 a un posible combate de cuatro

Objetivo: DS original/DS Lite, 4 MiB compartidos, ARM9 sin FPU y 60 Hz como
meta que debe medirse. La calidad de las animaciones de Prueba09 fue aprobada
por el usuario. Priorizar silueta, respuesta y claridad del combate.

## Cómo encaja un juego grande en una consola pequeña

El tamaño del juego almacenado y su memoria de trabajo son cosas distintas.
Se puede almacenar una plantilla grande y cargar sólo personajes, escenario
y audio necesarios para la partida. Es un principio de diseño que aplicaremos;
no una afirmación de cómo se implementó internamente un título comercial concreto.
La DS combina motores 2D, geometría 3D, texturas pequeñas y trabajo fuera de
tiempo real. Conviene convertir y comprimir en PC, y dejar al runtime tareas
acotadas. No se necesita cambiar de SDK para aplicar estas técnicas.

La guía de [gestión de memoria de BlocksDS](https://blocksds.skylyrac.net/tutorial/intermediate/memory_management/)
separa memoria estática, heap y pila: el tamaño del binario no mide el consumo
máximo del juego. Necesitamos medir además la pila, buffers, audio y ARM7.

## Prioridades de este port

| Técnica | Decisión / estado |
|---|---|
| Recursos compartidos entre instancias | Implementado: los dos Mario usan un único banco |
| Vértices únicos de poses | Implementado: 189 posiciones para 960 vértices enviados |
| Cuantización e interpolación entera | Prueba10: 8 bits por coordenada, rango por clip/eje |
| Agrupar triángulos por material | Prueba10: menos comienzos/finales, mismo orden y geometría |
| Animación rígida por huesos en GPU | Siguiente experimento, sin sustituir aún el formato aprobado |
| Cargar sólo seleccionados por partida | Pendiente; el contenido actual sigue enlazado al ELF |
| Física en punto fijo | Pendiente migración por subsistemas con pruebas numéricas |
| Display lists para geometría estática | Pendiente medir coste y DMA/caché |
| IA acotada | Prueba10: reglas y pulsaciones periódicas, sin búsqueda de rutas |
| Medir cuadros en DS | Prueba10 incorpora telemetría, falta obtener resultados físicos |

La [documentación de animación eficiente de BlocksDS](https://blocksds.skylyrac.net/tutorial/advanced/advanced_3d/#8-efficient-animated-models)
explica el uso de matrices de huesos en hardware, vértices rígidos asociados a
un solo hueso y display lists que restauran matrices incluso dentro de un
triángulo. Su esquema requiere no superar 30 huesos. Nuestro modelo conserva
la asociación de cada vértice a su articulación, por lo que es un candidato.
Hay que validar articulaciones auxiliares y sus transformaciones. Guardar
matrices completas de 32 bits por cuadro puede incluso costar más que nuestras
poses compactas; el formato de animación también debe optimizarse.

## Experimento medido de poses

`python tools/analyze_scaling.py` genera `build/optimization/scaling.json`.
Resultados con 50 clips y 1099 muestras:

- Poses int16: 1.246.266 bytes.
- Diccionario global sin pérdida: 1.229.186 bytes; ahorro pequeño, no prioritario.
- Coordenadas de 8 bits por clip/eje: 623.733 bytes, incluyendo mínimos/rangos.
- Error máximo por eje: 2,001 unidades de mundo, en las muestras almacenadas.

La opción compacta está incorporada a Prueba10. No se eliminan fotogramas,
ataques ni duración. Es una reducción de precisión espacial, con reconstrucción
entera. Coste: decodificar posiciones añade operaciones de CPU; el ahorro de RAM
es medido, la mejora de velocidad no se presupone. La comparación debe hacerse
con una compilación de 16 bits y otra compacta, usando la misma escena.

## Qué recortar visualmente

Orden propuesto, sujeto a comparación en pantalla física:

1. Reducir el fondo de 256×256 a 128×128: de 64 a 16 KiB en formato de 8 bits,
   ahorro de 48 KiB. Mantener la forma y contraste de plataformas y luchadores.
2. Sustituir adornos lejanos por planos y quitar caras que nunca ve la cámara.
3. Limitar partículas y proyectiles mediante pools. Preferir destellos y humo
   en planos pequeños; evitar muchas transparencias superpuestas y sombras
   volumétricas. No introducir contornos que requieran dibujar cada modelo dos veces.
4. Crear variantes de 220–240 triángulos para personajes pequeños en pantalla.
   Preservar cabeza, manos, botas y silueta de ataques; reducir detalle interior
   del torso y partes ocultas. Ajustar UV y normales, sin borrar triángulos al azar.
5. Usar paletas y texturas pequeñas compartidas. Un color alternativo no debe
   requerir copiar todas las animaciones ni necesariamente otra textura.

Son propuestas, no recortes aplicados al escenario/modelo de Prueba10.
La [guía 3D](https://blocksds.skylyrac.net/tutorial/intermediate/3d_graphics/)
describe formatos de texturas, display lists y límites del motor. El capítulo
de [técnicas avanzadas](https://blocksds.skylyrac.net/tutorial/advanced/advanced_3d/)
también propone planos orientados a cámara para formas que gastarían mucha
geometría; el fondo 2D puede dibujarse con el motor 2D en lugar de ocupar una
textura 3D grande. Eso exige rediseñar la distribución explícita de VRAM.

## Cuatro luchadores: geometría antes de prometer rendimiento

Estimación de envíos con Dream Land, reserva actual y 16 proyectiles:

| Triángulos por luchador | Polígonos con reserva | Vértices con reserva |
|---:|---:|---:|
| 320 actuales | 1592 | 4833 |
| 250 | 1312 | 3993 |
| 240 | 1272 | 3873 |
| 220 | 1192 | 3633 |

Nuestros márgenes son 1400/4000, inferiores a los límites de almacenamiento
de hardware 2048/6144. Cuatro Mario actuales caben nominalmente en esos límites
de hardware, pero incumplen nuestro margen. Eso no prueba que rindan a 60 Hz:
el recorte, área cubierta por píxeles, cambios de material y CPU también cuentan.
250 deja casi ningún margen en vértices; 220–240 es un punto de partida mejor.
Cuatro personajes distintos exigirían además presupuestar cuatro bancos propios.

## CPU, caché y herramientas

La [guía de optimización de BlocksDS](https://blocksds.skylyrac.net/docs/guides/optimization_guide/)
sirve como referencia para seleccionar optimizaciones a partir de mediciones.
Primero medir actualización, envío gráfico y cuadros fuera de presupuesto;
después optimizar los bucles que realmente cuestan. Evitar asignaciones por
cuadro y trabajos ilimitados. No reducir la frecuencia de física o de hitboxes
para ahorrar IA: pueden actualizarse decisiones lentas y conservar la simulación.

La [documentación de caché/TCM](https://blocksds.skylyrac.net/tutorial/intermediate/tcm_and_cache/)
explica que DMA y ARM7 no ven la caché del ARM9, y que ITCM/DTCM tienen capacidad
limitada. Usarlos sólo para funciones/buffers medidos; no desactivar cachés ni
copiar datos arbitrariamente a TCM. Reservar ARM7 para servicios/audio al principio;
repartir combate entre CPUs complica sincronización y no garantiza ahorro.

## Camino acordado

Prueba10 entrega primero un 1v1 de Mario contra CPU. Después: medir en DS,
corregir comportamiento de CPU y daño, ensayar carga por combate y un personaje
distinto, comparar formato de huesos y LOD, y sólo entonces prototipar cuatro
actores. Cuatro actores locales y cuatro usuarios por inalámbrico son problemas
distintos: el segundo añade sincronización, latencia y transporte.
