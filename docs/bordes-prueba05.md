# Prueba05: isla sólida y agarre de bordes

Copiar `Para tu DS/Prueba05/SSB-NDS-Prueba05.nds` a la microSD de DSPico.
Mantiene la cámara automática de Prueba04 aprobada por el usuario en emulador.
No cambia sus parámetros ni añade geometría o texturas.

## Qué probar

- Cruceta mover, A saltar, X jab, B caminar lento, R cámara automática/mapa.
- Al caer junto a un extremo de la isla, girar hacia ella. Las manos deben cruzar
  la altura del borde descendiendo; acercarse desde abajo subiendo no basta.
- Colgado: soltar la cruceta, luego pulsar hacia la isla para subir. A también
  permite subir. Abajo o la dirección opuesta permiten soltarse. Hay una espera
  de 30 tics antes de volver a agarrarse. Y reinicia y START pausa.
- SELECT alterna control manual y demostración: jab, agarre izquierdo con subida,
  agarre derecho con soltado, en un ciclo de 450 tics. Los agarres de la
  demostración empiezan con Mario situado cerca del borde; no prueban todavía
  toda la trayectoria necesaria para volver desde fuera de la pantalla.
- Las paredes laterales y la cara inferior frenan el paso a través de la isla.
  Las tres plataformas superiores siguen permitiendo subir desde abajo.

## Original y provisional

Referencia de código: ssb-decomp-re, revisión
`572a747d8171137718fad472c5d7473dc7309ffd`.

El contorno procede de recurso 104, vértices 0x1D64, lista de índices 0x1DD8,
líneas 0x1E08 y grupos 0x1E24. Suelo principal: línea 3; cara inferior: 4;
paredes: 5/6. Sus diez vértices quedan contrastados con la ROM en pruebas.
El bit 0x8000 de la línea de suelo habilita sus dos extremos como bordes.

Mario utiliza dimensiones originales de `203_MarioMain`: rom 0x428 + 39*4,
diamante de colisión arriba 320, centro 190, abajo 0 y ancho 150; alcance de
manos 400/360. La captura al descender y orientación se basan en
`mpProcessCheckTestLCliffCollision` y `mpProcessCheckTestRCliffCollision`.
La detención al colgarse, espera neutral para salir, espera de recaptura 30
y caída tras 1080 tics siguen referencias de `ftcommoncliffcatchwait.c`,
`ftcommoncliffclimb.c` y `ftcommon.h` para daño bajo.

El solver usa barrido contra semiplanos del contorno expandido por el diamante
de Mario, hasta tres contactos. Es una aproximación local: no reproduce el motor
original completo de colisiones; suaviza irregularidades cóncavas muy pequeñas
de la pared y no calcula todos los contactos entre las aristas del diamante.
No admite aparición arbitraria dentro de la isla ni objetos móviles.

El estado de espera usa una pose de la animación original CliffWait, recurso
573. Se guarda únicamente el primer fotograma (5760 bytes compartidos), sin
animación ambiental del colgado. Posición de apoyo y subida interpolada de 12
tics son provisionales. La animación original completa de captura/subida,
invulnerabilidad, salida por ataque/esquiva y variantes por daño alto faltan.
El blanco pasivo no puede agarrarse durante el aturdimiento ni ocupar el mismo
extremo que el jugador. No está implementado el motor completo de daño.

## Validación y coste DS

`python tools/verify_scene.py`: casos de paredes/techo, barrido rápido contra
suelo, ambos extremos, orientación, subida, soltado, recaptura, timeout y
regresiones de movimiento, plataformas, combate y cámara.
`python -m unittest discover -s tests`: contorno y flags contrastados con ROM,
formato de animaciones (incluido evento Figatree 11) y límites de recursos.
`python tools/check_ds_budget.py --demo dreamland`: ELF, texturas y geometría.

No se asigna memoria por fotograma. Sólo hay diez caras y hasta tres contactos
por actor; se mantiene el solver escalar flotante de la física existente, sin
raíces ni trigonometría. El coste nuevo es acotado pero requiere medición en DS.
No se deben interpretar los tiempos del emulador como medidas de hardware.
Pruebas01 a 04 permanecen conservadas; Prueba05 requiere su propia prueba física.

Resultado de esta compilación: ARM9 estático 1.378.968 bytes, texturas 165.632
bytes, presupuesto con reserva 936 polígonos/2849 vértices. Las pruebas nativas
y diez pruebas Python pasaron. En la variante automática de melonDS 1.1 se
observó el agarre derecho, el soltado posterior y la posición de espera sobre
la isla después de la subida izquierda (x=-2138, contador de agarres=1).
No se verificó visualmente cada fotograma de las transiciones ni se midió DS.
