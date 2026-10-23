# Prueba12: reacciones, retroceso y sonido

Copiar `SSB-NDS-Prueba12.nds` a la microSD de DSPico. El binario lleva sus
recursos; no necesita archivos externos. Las pruebas anteriores se conservan.

## Cambios del combate

- Reacciones originales de Mario para golpes altos, medios, bajos y aéreos,
  escogidas según el aturdimiento. Lanzamientos fuertes usan DamageFlyHi/N/Lw;
  los casi verticales usan DamageFlyTop. Las poses se detienen durante la pausa
  de impacto y conservan su último cuadro mientras continúa el aturdimiento.
- Pausa de impacto US: `floor(daño / 3) + 5` tics. El porcentaje, peso neutral
  de Mario y parámetros de cada golpe determinan fuerza y aturdimiento.
- Ángulo361: 43 grados en aire; en suelo, cero bajo fuerza32 y transición
  hasta42,5 grados. Tabla Q12 cada medio grado, error angular máximo0,25 grados,
  sin trigonometría por cuadro.
- Impulso de daño separado de velocidad normal: el límite de caída44 deja de
  recortar los lanzamientos verticales. Se reduce el vector1,7 por tic en aire,
  sin invertirlo al agotarse; fricción de daño horizontal1,5 en suelo plano
  (tracción6 ×0,25). El solucionador de escenario usa ambas velocidades.
- Mario mira hacia el origen del impacto; el destello rojo dura la pausa de
  impacto. Agarrar cancela el impulso previo. El daño interrumpe ataques y
  bloquea acciones, incluido agarrarse automáticamente al borde durante stun.
- Aterrizar cancela el impulso vertical e inicia seis tics de recuperación
  local si quedaba aturdimiento. No es todavía el sistema original de caída,
  rebote, tech o levantarse.

## Sonidos

Siete efectos propios sintetizados: movimiento de ataque, golpe ligero,
golpe fuerte, fuego, salto, aterrizaje y pérdida de vida. **Son provisionales;
no son los sonidos ni las voces originales de Smash 64.** Sin música aún.

Se generan fuera de la DS mediante `tools/generate_fight_audio.py`: PCM8 mono,
16kHz, 27360 bytes compartidos. Hasta cuatro canales de hardware, con prioridad
para impactos/KO y posición estéreo moderada. No hay mezcla software, síntesis
en tiempo real ni asignaciones por cuadro. Pausar/reiniciar detiene efectos;
los tics de pausa de impacto no vuelven a emitir sonidos. Los WAV locales para
revisarlos se encuentran en `build/audio/` y no se publican como recursos N64.

## Procedencia y adaptación

Referencia fijada en `upstream.lock.json`, ssb-decomp-re revisión
`572a747d8171137718fad472c5d7473dc7309ffd`:

- `src/ft/ftparam.c`: fuerza, aturdimiento y pausa de impacto US.
- `src/ft/ftcommon/ftcommondamage.c`: niveles12/24/32 de aturdimiento,
  ángulo361 y selección de lanzamiento superior70..110 grados.
- `src/ft/ftmain.c`: velocidad de daño independiente, reducción aérea1,7 y
  fricción de daño en suelo `material * traction *0.25`.
- `src/ft/ftdata.c` y `src/ft/ftdef.h`: correspondencia de animaciones. Los
  recursos523..539 se identifican por posición en la tabla original; algunos
  nombres del inventario de archivos discrepan. DamageFlyRoll539 se convierte
  pero queda reservado; no se introduce la selección aleatoria original.

Se usan las hitboxes ofensivas convertidas anteriormente y una cápsula de daño
local. La altura del contacto determina alto/medio/bajo; no se ha reconstruido
la selección original por hueso. No hay DI/SDI, pendientes/materiales completos,
armadura ni estados completos de tumble/tech. El impulso se reduce después del
paso de movimiento local; no se afirma equivalencia cuadro por cuadro con N64.
El motor de las demostraciones pasivas antiguas conserva su integración previa;
esta nueva separación de impulso se aplica al modo 1v1.

## Validación y DS

Las pruebas nativas cubren golpes simultáneos, agarres, CPU, KO, niveles/ángulos
de daño, lanzamiento superior al límite de caída, congelación de poses/eventos,
bloqueo de entradas, aterrizaje y agotamiento del vector. Otra prueba usa
hardware de audio simulado para verificar máximo de canales, prioridades y que
un canal reutilizado no sea detenido por su dueño anterior. Las pruebas Python
comprueban conversión de los67 clips, cuantización de poses y el banco PCM.

Banco compacto de poses compartido:754914 bytes; audio:27360 bytes. Texturas:
166144/262144 bytes; geometría con reservas:944 polígonos/2881 vértices, frente
a1400/4000 presupuestados. Consultar `compilacion.json` del paquete para el
tamaño ELF exacto. El ELF no mide pico de RAM, pila, heap ni ARM7.

Compilación y pruebas en PC comprobadas. **Esta versión aún no se ha visto ni
escuchado en emulador ni en DS.** No se garantizan60Hz. La cámara mantiene los
parámetros aprobados; el menú conserva su funcionamiento anterior.

Para probar en DS:

1. Subir el volumen de la consola y entrar al modo de prueba.
2. Dejar que la CPU golpee: revisar reacción, destello y sonido único por golpe.
3. Atacar con X, lateral+Y+X y arriba+Y+X sin lateral: comparar golpe débil,
   smash horizontal y lanzamiento vertical, especialmente con daño acumulado.
4. Probar aire, agarre/L seguido de X, fuego/B y caídas fuera del escenario.
5. Usar SELECT para CPU contra CPU y observar varios combates, sonido y contador
   de Trabajo/Pico. START pausa; START+SELECT reinicia. Comparar también con
   Prueba10 para detectar regresiones de movimiento.
