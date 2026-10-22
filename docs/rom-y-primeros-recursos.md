# ROM verificada y primera extracción

Se verificó el ZIP aportado por el usuario, `C:\ROMs\Super Smash Bros. (USA).zip`. Contiene una única ROM de 16.777.216 bytes en formato z64 big-endian, con título interno `SMASH BROTHERS`, código `NALE` y byte de revisión 0.

Su SHA-1 es `e2929e10fccc0aa84e5776227e798abc07cedabf` y su MD5 es `f7c52568a31aadf26e14dc2b6416b2ed`: coinciden exactamente con la referencia US v1.0 publicada en el README de la decompilación y la configuración de BattleShip.

## Copia local

`tools/prepare_rom.py` leyó el ZIP, comprobó la identidad de la ROM y escribió `build/rom/baserom.us.z64` y `build/rom/rom-info.json`. No extrajo rutas arbitrarias del ZIP ni modificó el archivo original. Los resultados permanecen bajo `build/`, excluido por `.gitignore`.

Reproducir desde la raíz:

```powershell
python tools/prepare_rom.py "C:\ROMs\Super Smash Bros. (USA).zip"
python tools/extract_initial_assets.py
```

La primera herramienta también admite ROMs `.n64` y `.v64`: normaliza el orden de bytes y exige el mismo SHA-1 de referencia antes de escribir la copia.

## Recursos extraídos

El extractor lee las 2.132 entradas de recursos de la ROM US y guarda un catálogo en `build/assets/us/resource-catalog.json`. Descomprime únicamente el siguiente subconjunto:

| ID | Recurso | Bytes descomprimidos |
|---:|---|---:|
| 202 | MarioMainMotion | 6.560 |
| 203 | MarioMain | 1.904 |
| 255 | GRPupupuMap | 192 |
| 296 | MarioModel | 29.968 |
| 298 | MarioShieldPose | 10.752 |
| 299 | MarioSecondaryImage | 192 |

Los `.bin` y los bloques comprimidos de origen están en `build/assets/us`. `GRPupupuMap` corresponde a datos de mapa/colisión; no representa toda la geometría visual ni el comportamiento de Dream Land. `MarioMainMotion` no equivale a todas las animaciones del personaje y puede referenciar otros archivos.

## Procedencia y comprobaciones

La interpretación de la tabla sigue `ssb-decomp-re/tools/relocData.py` y `BattleShip/tools/examine_blob_layout.py`. El descompresor VPK0 es `BattleShip/torch/lib/libvpk0/vpk0.c`, en la revisión exacta de Torch fijada por BattleShip: `3aa9c9739af95659f19adc73afeb12df93957fc3`. No se modificó ese árbol. El adaptador de archivo propio está en `tools/vpk_decode.c`, compilado como herramienta host con Zig.

Se verificaron:

- El hash de la ROM antes de acceder a la tabla.
- Los límites y el orden de los intervalos de la tabla de recursos.
- La revisión y ausencia de cambios locales en el descompresor utilizado.
- Los seis tamaños de salida frente a los tamaños declarados en la ROM.
- Once campos flotantes de `MarioMain`, a partir del bloque de atributos en offset 0x428: caminar, tracción, componentes de salto, aceleración y velocidad aérea, fricción, gravedad y velocidades terminales. Sus patrones de 32 bits coinciden con los valores estudiados en C.

Resultado ejecutado: `PASS: 6 resource sizes and 11 Mario attribute bit patterns verified`.

El manifiesto `build/assets/us/extraction-info.json` registra offsets, tamaños, hashes de recursos, procedencia del descompresor y los campos comprobados. Las comprobaciones de tamaño y de atributos no constituyen una validación independiente de cada vértice, textura o animación.

## Próxima integración

Los recursos permanecen big-endian y conservan sus cadenas de reubicación sin resolver. Todavía no se cargan en el `.nds`. Para mostrar el Mario original hay que identificar su jerarquía de articulaciones, listas gráficas, materiales, texturas y referencias externas; convertir ese subconjunto a formatos de DS y validar primero una pose estática. La animación y su conexión con la lógica de movimiento vienen después.
