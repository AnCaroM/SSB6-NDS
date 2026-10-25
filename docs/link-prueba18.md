# Prueba18: boomerang, bomba y agacharse

Cuarta entrega de Link. **Aún no se ha visto en emulador ni en DS.**

## Agacharse (Mario y Link)

Mantener abajo en el suelo: `Squat` y después `SquatWait` en bucle; al
soltar se levanta con `SquatRv`. Abajo+X desde agachado es el ataque bajo.
Tocar abajo sobre una plataforma elevada sigue bajando de ella. El cuerpo no
se encoge para esquivar golpes (la cápsula de daño es la misma).

## Boomerang (B de Link)

Fuente: `ftlinkspecialn.c`, `wplinkboomerang.c`, `226_LinkSpecial1.c`.

- Sale en el tic 26 del guion (`flag0`), desde Link + (150 hacia delante, 290).
- Normal 85 de velocidad y 160 tics de vida; con Y (smash) 114 y 190.
- Arriba o abajo al salir: ±30° (la cruceta es digital).
- Ida: pierde 1,4 por tic hasta 10 y entonces vuelve (sonido de vuelta).
- Vuelta: +1 por tic hasta 90 y gira hacia el pecho de Link como máximo
  0,75° por tic (1,5° si volvió por golpear) durante 140 tics.
- Se atrapa a menos de 180 unidades (sonido de atrapar).
- Golpe: 9 % de ida y 8 % de vuelta, ángulo 70, escala 30, base 55, radio
  100, sonido KickL. Un golpe por lanzamiento; si golpea a la ida, pierde 5 de
  velocidad y vuelve.
- Con un boomerang fuera, B hace la animación sin lanzar otro
  (`SpecialNEmpty`).
- Se dibuja con la pieza "boomerang en la mano" del modelo, girando.

## Bomba (abajo+B de Link)

Fuente: `ftlinkspeciallw.c`, `itlinkbomb.c`, `225_LinkMain.c`.

- Abajo+B saca la bomba en el tic 29; Link la sostiene con el escudo a la
  espalda. Mecha de 300 tics desde que aparece; si se acaba, explota en la mano.
- X la lanza (Y+X lanzamiento fuerte, arriba/abajo cambian la dirección).
  Abajo+B con una bomba en la mano la lanza fuerte hacia delante. Velocidades
  de la tabla de lanzamientos × 0,6 de la bomba: suelo 36@15°, fuerte 66@8°,
  arriba 39 (fuerte 66), abajo 39@−70°; aire 45@8°, fuerte 72@7°, arriba 48
  (72), abajo 45@−90° (84). Sale en el tic 8 del lanzamiento.
- En vuelo: gravedad 1,2, velocidad máxima 100. Explota si choca contra el
  rival o el suelo a más de 36 horizontal o 25 vertical; si no, golpea con
  2+0,1·velocidad (ángulo 80, escala 20, base 60) y rebota. Rebota una vez en
  el suelo (×0,15) y a la segunda se queda quieta.
- Explosión: 5 %, ángulo 361, escala 20, base 60, radio 300/230/150 durante 6
  tics, sonido ExplodeL.
- En los últimos 96 tics de la mecha se hincha 1,0–1,8 y parpadea.

## Simplificaciones (no es el motor completo)

- La bomba es un dibujo provisional (círculo azul con chispa), no el modelo
  original. El boomerang no choca con el escenario ni sale de la cámara.
- La explosión no daña a Link (en el original sí). Nadie más puede recoger,
  reflejar ni golpear la bomba; Link no la suelta al recibir golpes.
- Una sola bomba a la vez. El lanzamiento dura 20 tics con la animación
  `LightThrowF`/`LightThrowAirF`; atrapar el boomerang no cambia la animación.

## Coste

ARM9 estático 1.375.800 B. Audio 347.740 B (54 sonidos). Texturas 235.520 de
262.144 B. Pruebas en PC: agacharse con ambos, boomerang (salida en el tic
26, vuelta, atrapada, golpe de 9 %), bomba (sacar, lanzar, explotar sobre
Mario, mecha de 300 tics). Los recorridos grabados de Mario siguen idénticos.
