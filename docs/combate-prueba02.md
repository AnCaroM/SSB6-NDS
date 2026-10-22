# Prueba02: jab y blanco de entrenamiento

Copiar `Para tu DS/Prueba02/SSB-NDS-Prueba02.nds` a la microSD de DSPico y abrirlo
igual que Prueba01. El binario Prueba01, confirmado por el usuario en DS física,
se conserva. Esta nueva versión aún requiere una nueva prueba en hardware.

Controles: cruceta para mover, A para saltar, X para golpear, B para andar lento,
Abajo para bajar de plataformas, Y para reiniciar, START para pausar y SELECT
para una demostración automática. Acerca a Mario al blanco azulado y pulsa X.
Un botón mantenido no repite ataques. El blanco es pasivo y no ataca.

## Procedencia y alcance

- Mario comparte una sola malla, tres texturas y todas las poses entre ambos
  personajes. No se crea una segunda copia de los recursos ni se reserva memoria
  por cuadro. El color azulado distingue al blanco; no es otro luchador portado.
- Se añadió la animación original `FTMarioAnimJab1`, recurso 606: 19 muestras.
  El archivo `202_MarioMainMotion.c` establece inicio en tic 2, eliminación de
  hitboxes dos tics después, daño 2, radio 160, ángulo 361, escala 50 y base 8.
- La fórmula neutral de retroceso se adapta de `ftParamGetCommonKnockback`:
  peso 1, ratio 100%, handicaps 1 y sin daño fijo. La duración de reacción usa
  `knockback / 1.875`, como `ftParamGetHitStun`.
- La posición del volumen de ataque, la cápsula receptora, el hitstop, la
  integración de deriva y la selección de estados son aproximaciones. Todavía
  no hay hitboxes vinculadas a huesos, hurtboxes originales, interpretación de
  ángulo 361 completa, staling, DI, combos, escudos, rival con IA ni ataque aéreo.
  El blanco se desliza horizontalmente y reaparece si cae de la isla.

## Potencia y verificación

El presupuesto persistente está en `AGENTS.md` y `docs/limites-ds.md`. Los recursos
se comparten: dos luchadores suman 640 triángulos de Mario, más el jardín. El
comprobador reserva margen adicional para el escenario y el recorte. Ejecutar:

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --demo combat
python tools/check_ds_budget.py --demo combat
python tools/package_ds_test.py --version 02
```

La pantalla inferior añade `Trabajo`, `Pico` y `Periodo`, medidos con el reloj de
ticks de calico sin reprogramar temporizadores. Trabajo cubre lógica, envío de
geometría y actualización de consola antes de esperar VBlank; no mide por sí solo
la finalización del renderizado de GPU. Periodo es el intervalo entre comienzos
de cuadros e incluye la espera. En una DS a velocidad normal debería rondar
16,7 ms si mantiene el refresco; intervalos sostenidos mayores indican que hay
que medir y reducir trabajo. Las cifras de emulación no sustituyen la medición
en tu DS.

Se prueban inicio y ventana activa, hitstop, un solo impacto por ataque, ausencia
de repetición al mantener X, alcance/dirección/altura, aumento de daño, deriva,
reinicio de daño tras KO y repetición determinista. Las pruebas anteriores de
movimiento, plataformas y lectura de animaciones siguen ejecutándose.

REA se ha revisado mediante su código/documentación y su habilidad local; no se
ha ejecutado un análisis con REA. No hay herramientas REA conectadas en esta
sesión. `rea/docs/ghidra-mips.md` excluye MIPS en Windows Ghidra P0 y no demuestra
soporte de ROM N64. El desarrollo de esta prueba usa la decompilación disponible.
