# Procedencia y alcance

Proyecto experimental de adaptación nativa de Super Smash Bros. 64 a Nintendo
DS original/DS Lite. No es un producto oficial de Nintendo ni el juego completo.

## Fuentes de referencia

| Fuente | Uso |
|---|---|
| [ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re) | Funciones numéricas adaptadas, formatos, eventos y comparación con la referencia |
| [BattleShip](https://github.com/JRickey/BattleShip) | Estudio de un port para PC y comparación de fuentes |
| [Torch](https://github.com/JRickey/Torch) | Decodificador VPK0 externo; `tools/vpk_decode.c` es el adaptador local |
| [devkitPro](https://devkitpro.org/) | Toolchain ARM, libnds, Calico y herramientas DS externas |
| [BlocksDS](https://blocksds.skylyrac.net/) | Documentación técnica consultada para optimización |

Las revisiones de las fuentes están en `upstream.lock.json`. Sus repositorios
y herramientas no se incluyen en este repositorio; se preparan localmente.
BattleShip y Torch contienen sus propios archivos de licencia MIT. No se
encontró un archivo de licencia general en la revisión fijada de ssb-decomp-re.
Los comentarios del código y las notas técnicas identifican las adaptaciones.

## Recursos del juego

Este repositorio contiene código fuente, conversores, pruebas y documentación.
No incluye ROMs, modelos, texturas, animaciones extraídas, capturas del juego ni
ejecutables que incorporen esos recursos. Los recursos se generan localmente
a partir de una ROM aportada por cada usuario. Los nombres y personajes
pertenecen a sus respectivos titulares.

Las compilaciones actuales incorporan recursos extraídos: por esa razón los
`.nds`, paquetes ZIP y la carpeta `Para tu DS` quedan fuera del contenido público.
No deben adjuntarse automáticamente a GitHub Releases o artefactos de CI.

## Licencia del repositorio

El código propio de este proyecto (`src/`, `platform/`, `tools/`, `tests/`),
su documentación y sus scripts se publican bajo la [licencia MIT](LICENSE).
Las contribuciones aceptadas se incorporan bajo la misma licencia.

La licencia MIT no se extiende a:

- La ROM, ni a modelos, texturas, animaciones, sonidos u otros datos extraídos
  de ella, ni a las compilaciones `.nds` que los incorporan.
- Las partes adaptadas de ssb-decomp-re (funciones numéricas, tablas y valores
  copiados, identificados en los comentarios). Esa referencia no declara
  licencia en la revisión fijada, así que esas partes no reciben derechos
  adicionales por estar aquí.
- Las dependencias y herramientas externas (devkitPro, Torch, BattleShip),
  que conservan sus propias licencias.

Este aviso documenta procedencia; no concede derechos sobre material de
terceros. Super Smash Bros., Mario, Link y Nintendo DS son marcas de sus
respectivos titulares. Este es un proyecto de aficionados, sin relación con
Nintendo ni HAL Laboratory.
