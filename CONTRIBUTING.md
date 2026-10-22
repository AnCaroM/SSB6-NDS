# Contribuir

El objetivo es DS original/DS Lite. Consultar `AGENTS.md` y
`docs/limites-ds.md` antes de modificar presupuestos o renderizado.

- Mantener independientes recursos compartidos y estado por luchador.
- Conservar procedencia de lógica adaptada y diferenciarla de aproximaciones locales.
- No añadir ROMs, recursos extraídos, binarios, dependencias clonadas ni rutas personales.
- No eliminar las compilaciones locales que ya funcionaron en hardware.
- No prometer rendimiento basándose sólo en el emulador o el tamaño del ELF.

## Comprobaciones sin ROM

```powershell
python -m pip install -r requirements.txt
python tools/verify_source.py
python -m unittest discover -s tests -p test_mario_assets.py
python tools/check_publication.py
```

Las pruebas que necesitan recursos generados se omiten cuando faltan; no se
considera una validación completa del juego. La CI no descarga ROMs ni publica
binarios. Para las pruebas completas, seguir el README y usar una ROM propia.

Antes de proponer cambios, explicar el comportamiento modificado, pruebas y
presupuesto de memoria/geometría. Indicar claramente si se comprobó en DS real.
