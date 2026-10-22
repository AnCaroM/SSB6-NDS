# Herramientas DS instaladas y primera compilación

Fecha: 20 de octubre de 2026.

## Instalación realizada

- SDK en `C:\devkitPro`: grupo oficial `nds-dev` y dependencias, 24 paquetes. Incluye devkitARM r68, GCC 16.1.0, binutils 2.46.0, GDB 14.1, libnds 2.0.2, calico 1.2.0 y ndstool 2.3.1.
- melonDS 1.1 para Windows x86-64 en `.tools/melonDS/melonDS.exe`, extraído de la publicación oficial de GitHub.
- No se ejecutó el instalador de Windows ni se registró pacman. Los paquetes se descargaron desde los repositorios oficiales y se extrajeron al directorio del usuario. No se cambió el PATH ni las variables globales de Windows. Las herramientas de MSYS y los SDK de otras consolas no forman parte de esta instalación.

El HTTP 403 del servidor de devkitPro se resolvió al identificar las peticiones con el User-Agent de pacman. `tools/install_nds_sdk.py` consulta los índices oficiales `dkp-windows` y `dkp-libs`, resuelve las dependencias del grupo, comprueba el SHA-256 de cada paquete contra el índice y los extrae. Esto es verificación de integridad contra metadatos obtenidos por HTTPS; no se realizó una verificación independiente de firmas PGP.

El paquete de configuración `devkit-env` se conserva en la caché y los metapaquetes se registran en el manifiesto; no se aplican sus scripts de configuración de un sistema pacman/MSYS. El script no proporciona mantenimiento ni desinstalación mediante pacman.

El inventario completo, versiones, URLs y hashes está en `C:\devkitPro\ssb-nds-install.json`. Descargas y metadatos quedan en `.tools/downloads/devkitpro`. Se conservaron los archivos de licencia incluidos en los paquetes.

## Compilación comprobada

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro
```

Se corrigió la selección de lenguaje de C11 estricto a GNU11 porque las cabeceras instaladas de libnds utilizan `asm` y la declaración `iprintf` de newlib no está visible en el modo estricto. Se mantuvieron `-Wall -Wextra -Werror` y las restricciones de optimización flotante.

Resultados:

- `build/nds/ssb-motion.nds`: ejecutable DS generado.
- `build/nds/ssb-motion.elf` y `ssb-motion.map`: imagen ARM9 y mapa.
- `build/nds/ssb-motion-build-info.json`: compilador, hashes de fuentes/ROM y tamaño (las primeras compilaciones usaban `build-info.json`).
- ELF ARM9: text=77.572, data=8.696, bss=2.488; suma=88.756 bytes. Esto no representa el máximo de RAM: faltan, entre otros, pila, asignaciones durante ejecución y el ARM7.
- SHA-256 del `.nds`: `ade95d47e696ad42adc64ee5031d10efe80c9945a56163110bfb290f45b1f22f`.

`ndstool -i` pudo leer el archivo y reportó CRC correctos para cabecera, logo, banner y área segura. Esa inspección valida el empaquetado; no demuestra que el programa arranque.

## Arranque verificado en melonDS

Tras la aprobación del usuario, se abrió melonDS 1.1 mediante Computer Use y se cargó `build/nds/ssb-motion.nds`. Se verificó visualmente el dibujo superior del suelo y la figura, la consola inferior y el contador de tics en avance. La ventana mostró `[60/60]`, una lectura puntual del emulador que no constituye una medición de rendimiento de hardware DS.

La instalación no tenía teclas asignadas. Se guardaron: izquierda/derecha con las flechas, A con X, B con Z, Start con Enter, Select con Espacio y Y con R. Backspace borra una asignación en el diálogo de melonDS, por eso se eligió Espacio para Select.

Durante la sesión interactiva se observó desplazamiento del actor, estado WALK y contador de aterrizajes igual a 2. Hubo entrada del usuario en la ventana y el agente dejó de inyectar controles. No se validaron sistemáticamente salto corto/largo, pausa, avance por tic ni velocidad reducida en el emulador. Tampoco se probó hardware real.

Para abrir manualmente la escena instalada, ejecutar `Abrir prueba DS.cmd` desde el explorador de Windows. El lanzador abre melonDS con el `.nds`; no descarga nada.

Fuentes oficiales:

- https://github.com/melonDS-emu/melonDS/releases/tag/1.1
- https://pkg.devkitpro.org/packages/windows/x86_64/dkp-windows.db
- https://pkg.devkitpro.org/packages/dkp-libs.db
