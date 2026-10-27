# Prueba19: escudo, rodar, caer y levantarse

Primera entrega del "combate más completo". **Aún no se ha visto en DS real.**
Vale para Mario y para Link: las animaciones y los tiempos salen de la ROM y
de la decompilación de referencia; la integración es reducida y propia.

## Controles nuevos

| Entrada | Acción |
|---|---|
| R (mantener) | Escudo (`GuardOn`, `Guard`, `GuardOff` al soltar) |
| R + toque lateral | Rodar hacia delante o hacia atrás (`EscapeF`/`EscapeB`) |
| R al correr | Rodar hacia delante |
| A con escudo | Saltar desde el escudo |
| L con escudo | Agarrar (los agarres atraviesan escudos) |
| R justo antes de caer golpeado | Amortiguar la caída (`Passive`; con dirección, `PassiveStandF/B`) |
| En el suelo: arriba o R | Levantarse (`DownStand`) |
| En el suelo: lado | Rodar tumbado (`DownForward`/`DownBack`) |
| En el suelo: X o B | Ataque al levantarse (`DownAttack`) |
| Tocar la pantalla | Mapa completo (antes R) |

## Escudo

Fuente: `ftcommonguard1.c`, `ftcommonguard2.c`, `ftmain.c` (ftMainProcParams),
`ftcommon.h`, `FTAttributes` de cada luchador.

- Vida 55 al aparecer. Mientras está arriba pierde 1 cada 16 tics; abajo
  recupera 1 cada 10 tics.
- Radio = (0,65·vida/55 + 0,35) × `shield_size` (Mario 260, Link 280),
  centrado en `YRotN` al final de `GuardOn` (Mario 0,135; Link −15,182).
  Con poca vida la burbuja deja la cabeza fuera y se puede golpear por encima.
- Golpe bloqueado: no hay daño; el escudo pierde el daño del golpe; los dos
  quedan en pausa de impacto (daño/3+5) y el defensor retrocede
  `(daño·1,62+4)` tics a 2 unidades por tic sin poder actuar.
- `GuardOn` dura 5 tics; si R se soltó, al acabar pasa a `GuardOff` (13
  tics). `Guard` mantiene el último cuadro de `GuardOn` (la pose de escudo
  del original es un juego de huesos aparte que no se ha convertido).
- Los proyectiles también se bloquean: bola de fuego, boomerang (vuelve) y
  la explosión o el rebote de la bomba.
- Se rompe si la vida llega a 0 por golpes o por desgaste: sale disparado
  hacia arriba (`shield_break_vel_y`: Mario 70, Link 90), cae, se levanta y
  queda mareado (`FuraFura`) durante `max(0, 400 − porcentaje) + 90` tics;
  cada pulsación o cambio de dirección descuenta 4 tics más. La vida del
  escudo queda en 30 y no se recupera mientras está mareado.

## Rodar

Fuente: `ftcommonescape.c`, guiones `RollF`/`RollB` y el `TransN` de las
animaciones (`ftPhysicsApplyGroundVelTransN`).

- Con escudo, un toque del mando a ≥56 en los 4 primeros tics lo inicia.
- Desplazamiento por tic sacado del `TransN` de cada animación (`dx_*` en
  el modelo convertido): Mario 900 unidades, Link similar.
- Intangible de los tics 4 a 20. `RollF` pone `flag1` en el tic 20: el
  original invierte `lr` ahí, pero vuelve a negar la velocidad mientras el
  giro del modelo no coincide con `lr`, así que el desplazamiento sigue
  siempre hacia delante (los 900 completos). Como en SSB64, al acabar de
  rodar hacia delante el luchador queda mirando hacia atrás; aquí la
  orientación cambia al terminar, cuando la animación ya giró el cuerpo.
- No se sale del borde: la posición se limita a la plataforma actual.
- Los que ruedan no empujan ni son empujados por el cuerpo del rival.

## Caer golpeado, amortiguar y levantarse

Fuente: `ftcommondamage.c` (`DamageAirCommonProcMap`), `ftcommondamagefall.c`,
`ftcommonpassive*.c`, `ftcommondown*.c`.

- Aterrizar en el aire golpeado (en aturdimiento o en caída `DamageFall`):
  si R se pulsó en los 20 tics anteriores, amortigua (`Passive`, 27 tics,
  intangible 24; con dirección rueda 1200 unidades, intangible 20). Si no,
  cae (`DownBounce`, 27 tics; `D` tras una caída dando vueltas, `U` si no).
- Tumbado (`DownWait`, último cuadro de `DownBounce`): se levanta solo a los
  180 tics; arriba o R lo levantan (`DownStand`, 31 tics, intangible 22);
  un lado rueda 1050 unidades; X o B hacen el ataque al levantarse (ya
  existía como `DownAttack`). Durante `DownBounce` el golpe queda guardado
  60 tics.
- La CPU intenta amortiguar a veces, bloquea ataques cercanos, rueda si se
  la acorrala y aporrea botones cuando está mareada.

## Arreglo: flotar en el borde con la animación de caída

El sólido de la isla se calculaba como intersección de semiplanos
ensanchados por el rombo del cuerpo; en las esquinas convexas eso sobra 150
unidades, así que un luchador que pasaba el borde quedaba "apoyado" en el
aire con la animación de caída hasta deslizarse fuera. Ahora `stage.c`
construye una vez el polígono exacto (envolvente convexa de la isla barrida
por el rombo) y en el borde se cae de verdad. La CPU, además, suelta el
mando un momento al colgarse para poder subir y ya no se tira del borde
persiguiendo a un rival colgado.

## Modo "Explorar el mapa"

Sustituye al visor de Link del menú (sus animaciones ya se ven en combate).
Se elige un personaje y se recorre Dream Land sin rival: la misma física,
ataques y escudo, con vidas infinitas (caer fuera reaparece sin perder
vida) y sin ganador. SELECT no hace nada; B en pausa vuelve al menú. En
`versus.c` es `versus_init_solo`: el segundo lado queda aparcado fuera del
mapa y nunca se actualiza, y la cámara sigue a un solo luchador.

## Sonidos nuevos

`GuardOn`, `GuardOff`, `ShieldBreak`, `Escape` (rodar), `LinkDownBounce` y
`LinkGrunt2` (voz de amortiguar de Link). 60 sonidos en total.

## Simplificaciones

- El escudo no se inclina con el mando y no hay empuje al atacante ni
  rebote de los ataques (`attack_rebound`).
- Sin daño reducido al estar tumbado (`damage_mul`), sin "smash DI" en la
  pausa de impacto, sin amortiguar contra paredes ni techos.
- `DownBack` entra por un salto dentro de `DownForwardU`; se usan los eventos
  de `DownForwardD` (todos los caminos llevan ahí).
- El dibujo del escudo es un disco translúcido, no la esfera del original.

## Coste

ARM9 estático 1.540.292 B (antes 1.375.800: 22 animaciones nuevas por
luchador, 115 KiB de poses, y 35 KiB de audio). Audio 383.280 B (60
sonidos). Texturas sin cambios (235.520 de 262.144 B). Geometría sin cambios.
Pruebas en PC (`tests/test_guard.c`): tiempos del escudo, desgaste y
recuperación, golpe bloqueado, agarre a través del escudo, rodar con vuelta
y límite del borde, amortiguar, caída y levantarse de las tres formas,
rotura con mareo y aporreo, y CPU contra CPU acotada. Los recorridos grabados
de Mario `long`, `short`, `ledge`, `special-hi` y `moves` siguen idénticos;
`recovery-*` y `special-*` cambian por el arreglo del borde (ya no se flota
en el saliente; la ruta guionizada de la recuperación especial gira dos
cuadros más tarde) y `versus.csv` porque la CPU ahora usa el escudo.
