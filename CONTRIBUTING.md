# Contribuir

¡Gracias por querer ayudar! Cualquier aporte suma: probar en una DS real,
reportar errores, revisar animaciones, mejorar herramientas o escribir código.
Se aceptan issues y pull requests en español o en inglés.

El objetivo es DS original/DS Lite. Consultar `AGENTS.md` y
`docs/limites-ds.md` antes de modificar presupuestos o renderizado.

## Formas de ayudar

- **Probar en hardware:** abrir un issue con el modelo de consola, el cartucho
  o flashcart (por ejemplo DSPico), la versión (`PruebaNN`) y los contadores
  **Pico** y **>16.7ms** del marcador. Es la información más valiosa: el
  emulador no garantiza el rendimiento real.
- **Reportar errores:** qué hiciste, qué esperabas y qué pasó. Si es una
  animación, indicar el nombre del clip del visor.
- **Código y herramientas:** buscar issues abiertos o proponer uno antes de
  empezar un cambio grande, para coordinar.

## Reglas

- Mantener independientes recursos compartidos y estado por luchador.
- Conservar procedencia de lógica adaptada y diferenciarla de aproximaciones locales.
- No añadir ROMs, recursos extraídos, binarios, dependencias clonadas ni rutas personales.
- No adjuntar `.nds`, capturas del juego ni recursos convertidos a issues,
  pull requests o Releases: incorporan material de la ROM.
- No eliminar las compilaciones locales que ya funcionaron en hardware.
- No prometer rendimiento basándose sólo en el emulador o el tamaño del ELF.

## Comprobaciones sin ROM

Estas comprobaciones corren también en la CI de cada pull request:

```sh
python -m pip install -r requirements.txt
python tools/verify_source.py
python -m unittest discover -s tests -p test_mario_assets.py
python tools/check_publication.py
```

Las pruebas que necesitan recursos generados se omiten cuando faltan; no se
considera una validación completa del juego. La CI no descarga ROMs ni publica
binarios. Para las pruebas completas, seguir el README y usar una ROM propia
(`python tools/verify_scene.py` y `python -m unittest discover -s tests -p "test_*.py"`).
Si un cambio toca física o combate, las trazas grabadas de Mario deben seguir
idénticas.

## Pull requests

1. Hacer un fork y crear una rama desde `main`.
2. Mantener cada pull request centrado en un cambio.
3. Explicar el comportamiento modificado, las pruebas ejecutadas y el
   presupuesto de memoria/geometría (`tools/check_ds_budget.py`).
4. Indicar claramente si se comprobó en melonDS, en DS real o sólo en PC.

## Licencia de las contribuciones

Al enviar una contribución aceptas que se publique bajo la
[licencia MIT](LICENSE) del proyecto. No envíes código que no puedas
licenciar así; si adaptas material de otra fuente, indícalo en los
comentarios y en el pull request.
