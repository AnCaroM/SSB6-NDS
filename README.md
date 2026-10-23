# SSB NDS

Port nativo experimental de Super Smash Bros. 64 para **Nintendo DS original y
DS Lite**. El estado actual permite Mario contra una CPU sencilla en Dream Land,
con tres vidas, ataques, agarres, proyectiles y recuperación.

El proyecto estudia y adapta partes de una decompilación de referencia. Todavía
no reproduce el motor completo del juego. La CPU es propia y provisional.

**El repositorio no incluye ROMs ni recursos del juego.** Cada usuario genera
localmente los recursos desde su ROM. Las compilaciones actuales incorporan
esos recursos y también se excluyen del repositorio y de la CI. Consultar
[procedencia y licencias](NOTICE.md).

## Estado actual

- Mario y Dream Land convertidos localmente desde la ROM US v1.0.
- Repertorio de ataques, salto aéreo, bordes y recuperación.
- Cámara automática adaptada a 256 × 192.
- Mario contra CPU y demostración CPU contra CPU; tres vidas y ganador.
- 68 clips compartidos; ataques y daño conservan cada cuadro original.
- Reacciones de daño completas, caída tras lanzamiento y transición de salida;
  sonido provisional retirado en [Prueba13](docs/animaciones-prueba13.md).
- Efectos y voces originales de Mario convertidos de la ROM del usuario
  ([Prueba14](docs/sonido-prueba14.md), sin probar en DS).
- Pruebas de lógica en PC, presupuestos DS y herramientas de inspección.
- Pantalla de inicio y selección de modo (Prueba11, sin probar en DS);
  ver [docs/menu-prueba11.md](docs/menu-prueba11.md).

Prueba09 recibió confirmación del usuario de que sus animaciones se ven bien.
Prueba10 necesita validación física propia: no se garantiza 60 Hz ni se ha
medido el pico total de RAM. El siguiente objetivo es mejorar el 1v1; cuatro
luchadores y multijugador inalámbrico permanecen en investigación.

## Compilar en Windows

La cadena de compilación ha sido utilizada en Windows con Python 3.13,
devkitARM r68, libnds 2.0.2, Calico 1.2.0 y Zig 0.15.2. Las herramientas DS
se instalan aparte mediante [devkitPro](https://devkitpro.org/wiki/Getting_Started).
La CI de Linux sólo comprueba código que no necesita una ROM.

### 1. Preparar Python y las referencias

Ejecutar desde la raíz del proyecto:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python -m pip install --target .tools/zig ziglang==0.15.2
python tools/setup_sources.py
```

`setup_sources.py` descarga ssb-decomp-re y Torch en revisiones fijadas por
[upstream.lock.json](upstream.lock.json). No reinicia ni reemplaza checkouts
existentes con cambios o revisiones distintas. `--check` sólo comprueba su
presencia y revisión. BattleShip completo es una referencia opcional de estudio.

### 2. Preparar los recursos propios

Se admite Super Smash Bros. US v1.0 de 16 MiB, normalizada a big-endian:
SHA-1 `e2929e10fccc0aa84e5776227e798abc07cedabf`.
La herramienta admite `.z64`, `.n64`, `.v64` y un ZIP con una única ROM.
No se descarga ninguna ROM.

```powershell
python tools/prepare_rom.py "C:/ROMs/mi-copia.z64"
python tools/extract_initial_assets.py
```

Esto crea `build/rom`, recursos iniciales y un decodificador local VPK0.
La conversión de animaciones y escenario se realiza durante el build siguiente.

### 3. Compilar el 1v1

Ajustar la ruta a la instalación local del SDK:

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --check
python tools/build_nds.py --devkitpro C:/devkitPro --demo versus --compact-poses
python tools/check_ds_budget.py --demo versus
```

Resultado: `build/nds/ssb-versus.nds`. Se puede abrir en melonDS o copiar a una
microSD compatible con DSPico. No hace falta copiar la ROM N64 a la DS.
Omitir `--compact-poses` permite comparar con el banco de poses de 16 bits.

## Controles

| Entrada | Acción |
|---|---|
| Cruceta / doble toque lateral sostenido | Caminar / correr |
| A | Salto y segundo salto |
| X + dirección | Ataques en suelo o aire |
| Y + X + dirección | Smash |
| L, después X o dirección | Agarre y lanzamiento |
| B / arriba+B / abajo+B | Fuego / recuperación / Tornado |
| R | Cámara automática / mapa |
| START | Pausa |
| SELECT | Alternar jugador contra CPU y CPU contra CPU; reinicia |
| START + SELECT | Reiniciar |

## Pruebas

Comprobaciones sin ROM ni referencias externas:

```powershell
python tools/verify_source.py
python -m unittest discover -s tests -p test_mario_assets.py
```

Las pruebas dependientes de recursos se omiten si no existen. Para la suite
completa, después de convertir los recursos propios:

```powershell
python tools/verify_motion.py
python tools/verify_scene.py
python -m unittest discover -s tests -p "test_*.py"
python tools/analyze_scaling.py
```

La comprobación completa local pasó 21 pruebas Python, las suites nativas y
la comparación del decodificador compacto. Las pruebas de PC y presupuestos
estáticos no sustituyen las mediciones de DS real.

## Organización y documentación

- `src/`: lógica de movimiento, combate, cámara y CPU.
- `platform/nds/`: integración libnds y renderizado.
- `tools/`: extracción, conversión, compilación y verificaciones.
- `tests/`: pruebas numéricas, de combate y de recursos.
- `docs/`: investigación, procedencia e historial técnico.
- `build/`, `.tools/`, `upstream/`, `Para tu DS/`: contenido local ignorado.

Consultar [Prueba10](docs/versus-prueba10.md),
[optimización y escalado](docs/optimizacion-y-escalado.md),
[límites DS](docs/limites-ds.md),
[proceso del port](docs/proceso-del-port.md) y
[estudio de la decompilación](docs/estudio-decompilacion.md).
Las notas antiguas documentan hitos y pueden describir controles superados.

## Contribuir y publicar

Ver [CONTRIBUTING.md](CONTRIBUTING.md) y las restricciones de [AGENTS.md](AGENTS.md).
Antes de publicar cambios, comprobar los archivos indexados:

```powershell
python tools/check_publication.py
```

No añadir ROMs, modelos/texturas/animaciones extraídos, ejecutables, SDKs,
checkouts externos, credenciales ni compilaciones del juego a Git o Releases.
Este repositorio todavía no declara una licencia global; ver [NOTICE.md](NOTICE.md).
