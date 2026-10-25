# Prueba17: Mario y Link en el 1v1

Tercera entrega para añadir a Link. Se puede elegir Mario o Link para cada
lado del modo de prueba y de la demo CPU vs CPU. **Aún no se ha visto en
emulador ni en DS.**

## Qué hay

- **Selección de personajes** después de elegir el modo: arriba/abajo cambia
  de lado, izquierda/derecha cambia el personaje (o tocar el nombre), A o
  tocar *JUGAR* empieza, B vuelve. Por defecto, Mario contra Link.
- **Física de Link** con sus atributos originales (`225_LinkMain.c`):
  velocidad al caminar 0,37, tracción 2, carrera 42, impulso previo al
  salto de 7 tics (Mario 3), base de salto 36, aceleración aérea 0,04, límite
  aéreo 31, sin fricción aérea, gravedad 3,4, caída máxima 64, peso 0,96.
- **Golpes de Link**: jab 1-2-3, ataques fuertes, ataque en carrera, smash,
  aéreos, agarre con gancho, lanzamientos (`FTThrowHitDesc` de sus guiones),
  ataques desde el borde y desde el suelo, con las hitboxes de la espada de
  `224_LinkMainMotion.c` (incluidos los arrays `u32` del archivo).
- **Ataque giratorio** (arriba+B): en el suelo gira con sus golpes de espada;
  en el aire sube con 69 de velocidad, gravedad ×0,23 hasta el tic 12, deriva
  ×0,5 y después caída indefensa (`ftlinkspecialhi.c`).
- **Sonidos de Link**: golpes cortantes (`Slash` S/M/L por `fgm_kind` 5),
  espadazos, aterrizaje, giro, gancho y voces (salto, salto aéreo, smash x3,
  giro, daño, KO). 50 sonidos en total, 314 KiB PCM8.
- El peso del que recibe el golpe multiplica el retroceso
  (`ftParamGetCommonKnockback`); Mario pesa 1,0 y no cambia.
- Pantalla de inicio con Mario y Link; tinte azul sólo cuando ambos lados
  son el mismo personaje.

## Cómo se hizo

- `src/fighter.h`: datos por personaje (atributos, movimientos, clips de
  cada estado, sonidos, lanzamientos). `src/fighter_attr.c` copia los
  `FTAttributes`; `tests/test_fighter_attr.py` los compara con la referencia.
- `tools/convert_fighter.py` genera también los movimientos, el mapa de clips
  y `fighter_<nombre>`. Para Mario produce **exactamente** los mismos golpes,
  marcos y tiempos que el conversor anterior (764 golpes, 1574 marcos).
- La física, el combate y el 1v1 leen esos datos. Con Mario, los recorridos
  grabados (salto, bordes, recuperación, golpes y combate CPU vs CPU de 7200
  tics) son idénticos byte a byte a los de Prueba16.
- El 1v1 dibuja a ambos personajes con el renderizador de huesos genérico;
  el Mario anterior queda para las compilaciones antiguas.

## Coste

ARM9 estático 1.313.304 B (2,5 MiB de presupuesto). Huesos: Mario 242.252 B
+ Link 248.704 B. Texturas 235.520 de 262.144 B. Audio 321.824 B. Geometría
en el peor caso (Link contra Link) con reservas: 1046 polígonos / 3187
vértices de 1400 / 4000.

## Limitaciones

- Boomerang (B) y bomba (abajo+B) de Link todavía no hacen nada: Prueba18.
- El giro sólo usa los golpes de la espada del guion; el arma giratoria
  aparte (5 de daño, tamaños 120/100/80) y `Attack100` no están.
- Al aterrizar del giro aéreo se usan los 25 tics de Mario.
- Velocidad de carrera inicial de Link: valor local 48 (Mario usa 50).
- La CPU es la misma para ambos; no aprovecha el alcance de Link.

Recompilar:
`python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --skeletal-poses`
