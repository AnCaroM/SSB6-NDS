# Prueba10: Mario contra CPU

Copiar `SSB-NDS-Prueba10.nds` a la microSD de DSPico. No requiere recursos
externos. Pruebas01 a 09 quedan conservadas. El usuario aprobó las animaciones
de Prueba09; esta nueva compilación necesita otra prueba en hardware.

## Combate

Mario rojo contra Mario azul, tres vidas por lado. Ambos pueden moverse,
atacar, recibir daño, ser agarrados/lanzados, recuperarse y perder vidas.
Al perder la última vida se muestra el ganador; START+SELECT reinicia.
Reaparecer concede 90 tics de invulnerabilidad con parpadeo, aproximación local.
Los límites laterales/superior de derrota son locales (±6000 / +6000); el
inferior conserva el límite del escenario. Porcentaje, aturdimiento y vidas
son independientes. El modelo, texturas y banco de poses son compartidos.

La CPU es una máquina de reglas propia: se acerca, orienta hacia el rival,
alterna golpes y smashes, puede agarrar/lanzar y disparar, salta para perseguir
altura e intenta volver con salto aéreo y arriba+B. Tiene pulsaciones de acción
cada seis tics y selección periódica reproducible. No es la IA de Smash 64,
no tiene navegación completa de plataformas, defensa ni niveles de dificultad.
Puede tomar malas decisiones o fallar recuperaciones en otras situaciones.

## Controles

| Entrada | Acción |
|---|---|
| Cruceta / doble toque lateral sostenido | Caminar / correr |
| A | Salto y segundo salto |
| X / dirección + X | Combo / ataques direccionales y aéreos |
| Y + X + dirección | Smash |
| L, después X o dirección | Agarre y lanzamiento |
| B / arriba+B / abajo+B | Bola de fuego / recuperación / Tornado |
| Abajo sin atacar | Bajar de plataforma o soltarse del borde |
| R | Alternar cámara automática y mapa |
| START | Pausar |
| SELECT | Alternar TU vs CPU y CPU vs CPU; reinicia el combate |
| START + SELECT | Reiniciar el combate |

Los parámetros de cámara son los aprobados anteriormente.

## Optimización y medición

Se agrupan triángulos consecutivos con el mismo material para reducir comandos
de comienzo/final sin cambiar orden, texturas ni geometría. Las poses ahora
usan 8 bits por coordenada con mínimos y rangos por clip/eje; se reconstruyen
e interpolan con enteros. El error máximo por eje de los 50 clips es de unas
2 unidades de mundo (aproximadamente 0,14 píxeles con alto de cámara 2800).
Esta cifra es un límite en las muestras almacenadas, no una comparación
visual del nuevo binario en hardware. La física y las hitboxes originales
convertidas mantienen sus datos anteriores, sin esta cuantización.

Banco de poses: 1.246.266 bytes originales → 623.733 bytes, incluyendo
los nuevos mínimos/rangos. El runtime del 1v1 ocupa 952 bytes de estado.
Compilación final: ARM9 estático 967.492 bytes (0,923 MiB); texturas 166.144
de 262.144 bytes; geometría incluyendo reservas y ocho proyectiles: 944
polígonos / 2881 vértices, frente a presupuestos preventivos 1400 / 4000.
Existen otros usos de RAM: código, geometría, tablas, pila, ARM7, SDK, etc.
No se confunde tamaño del ELF con pico real de memoria.

La pantalla inferior muestra Sim (actualización del combate), Trabajo
(trabajo de CPU/envío del cuadro anterior, incluye posibles esperas del envío),
Pico, número de cuadros cuyo trabajo supera 16.667 µs y Periodo entre cuadros.
Se usa timer0 a bus/64; timers2/3 quedan al sistema. Un intervalo mayor de
unos 125 ms desborda el contador de 16 bits y puede subestimarse. No mide
directamente el tiempo completo de rasterización de GPU ni el pico de RAM.
Para comparar rendimiento usar el mismo escenario y SELECT CPU vs CPU.

## Validación y límites

Pruebas nativas: daño desde ambos lados, golpes simultáneos, hitstop,
aturdimiento que cancela ataques, agarre/lanzamiento en ambos sentidos,
reaparición, inmunidad, victoria/empate, CPU contra blanco pasivo,
recuperaciones izquierda/derecha y auto-combate hasta 7200 tics. Se guarda
`build/scene/versus.csv`; la prueba exige actividad reciente o fin de partida.
Las pruebas de conversión cuantifican el error de todos los bancos de poses.
También se ejecutan las regresiones de movimientos, colisiones y cámara.
Resultado: 21 pruebas Python y todas las suites nativas pasaron. El banco C
compacto se decodifica en PC y se compara byte a byte con la reconstrucción
offline. En el auto-combate reproducible se llega a fin de partida, sin exigir
que el resultado de esa CPU represente cualquier posible partida humana.

No se ha validado esta compilación visualmente en emulador ni en DS.
La colisión de daño usa una cápsula reducida; el empuje entre cuerpos en suelo
también es local. Captura/rotación de lanzamientos, daño y animaciones de dolor
no reproducen el motor completo. No hay escudo, sonido, efectos completos ni
comunicación entre consolas. Cuatro luchadores todavía no están implementados.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --compact-poses`

Omitir `--compact-poses` genera una referencia con poses de 16 bits para
comparaciones; la aplicación comparte el mismo sistema de combate.
