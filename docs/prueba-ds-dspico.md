# Prueba 01 en DS / DS Lite con DSPico

Esta compilación integra el modelo, tres texturas y cinco clips de animación de
Mario, convertidos de la ROM US v1.0 del usuario. Se puede caminar, saltar,
aterrizar, bajar de plataformas y reaparecer tras caer. El jardín de entrenamiento
es un escenario propio con una isla y tres plataformas. Las reglas, la cámara,
el cambio de animaciones y las colisiones son todavía una integración provisional;
no incluye combate, rivales ni audio.

## Cargar en tu consola

1. Apaga la DS y conecta la microSD de tu DSPico al PC.
2. Copia `SSB-NDS-Prueba01.nds` de `Para tu DS` a una carpeta de la microSD,
   por ejemplo `/Homebrew/`. Si usas el ZIP, extráelo primero.
3. Expulsa la microSD desde Windows, ponla en la DSPico e inserta la tarjeta en la DS.
4. Enciende la DS y abre el menú habitual de tu DSPico.
5. Busca `SSB-NDS-Prueba01.nds` y pulsa A para ejecutarlo.

El archivo contiene todos los recursos de esta prueba. La aplicación no lee
archivos externos de la SD y no guarda partidas. Para usarla no hay que copiar
la ROM N64 ni los directorios `build`, `.tools` o `upstream` a la tarjeta.

Estas instrucciones suponen que tu DSPico ya abre otros `.nds`. La guía oficial
recomienda colocar las ROM en una carpeta de la microSD y abrirlas con A:
[DSpico Guide, pasos 9 y 10](https://github.com/LNH-team/dspico/blob/develop/GUIDE.md).
[Pico Loader](https://github.com/LNH-team/pico-loader) declara soporte para
homebrew de DS y DSPico. Esta compilación concreta aún necesita tu prueba física.

## Controles de la DS

| Botón | Acción |
| --- | --- |
| Cruceta izquierda/derecha | Mover |
| A | Saltar; una pulsación corta reduce el salto |
| B mantenido + cruceta | Andar despacio |
| Abajo | Bajar de una plataforma elevada |
| Y | Volver al inicio |
| START | Pausar/reanudar |
| SELECT | Activar/desactivar demostración automática |

La pantalla superior muestra el escenario. La inferior muestra controles,
estado, animación, posición y contadores. `Suelo: 0` identifica la isla;
`1` y `2`, las plataformas laterales; `3`, la superior; `-1`, el aire.
La demostración automática sube a la plataforma superior, baja de ella y camina.
Las direcciones o A devuelven inmediatamente el control manual.

Para la primera prueba: espera a que aparezca el jardín, mueve a Mario, salta,
aterriza en una plataforma y pulsa Abajo para atravesarla. Pulsa Y si quieres
volver al centro. Después activa SELECT y observa al menos dos ciclos. Si hay
un fallo, el dato más útil es si ocurre al cargar, al saltar o tras varios minutos,
y si el contador de tics de la pantalla inferior sigue avanzando.

## Compilar y preparar el paquete

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --demo scene
python tools/package_ds_test.py --version 01
```

El paquete contiene `.nds`, instrucciones, SHA-256 e información de compilación.
Objetivo: DS original/DS Lite, ARM9 ARMv5TE, renderizador 3D de libnds, ARM7 de
calico. El encabezado DSi-enhanced generado por ndstool también incluye una ruta
DS; el programa no depende de la RAM adicional de DSi.

El ARM9 de esta versión suma aproximadamente 1,09 MB entre texto/datos/BSS.
Esto no es una medida del máximo de RAM: se añaden pila, heap, ARM7 y otros usos.
Las texturas directas ocupan 8.192 bytes en VRAM A y la malla de Mario contiene
320 triángulos. Las posiciones animadas se muestrean al compilar para ahorrar
trabajo de CPU durante la reproducción.

Verificaciones: pruebas de movimiento y de plataformas en PC, cinco pruebas del
lector de animaciones y CRC de la cabecera/logo/banner del `.nds`. Las observaciones
de melonDS se guardan por separado. La velocidad y compatibilidad en tu DS se
confirmarán con la prueba física. El usuario ya confirmó posteriormente que
Prueba01 funciona muy bien en su DS con DSPico. Esto no valida Prueba02.
