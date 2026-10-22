# Prueba03: Dream Land original

El archivo para DSPico es `Para tu DS/Prueba03/SSB-NDS-Prueba03.nds`. Se copia a
la microSD y se abre igual que las pruebas anteriores. Contiene todos los datos
necesarios; la ROM N64 no se copia a la tarjeta.

Esta versión convierte los recursos originales de Dream Land de la ROM US v1.0
aportada por el usuario. No dibuja el jardín de entrenamiento. Incluye las cuatro
capas gráficas del descriptor de escenario, sus texturas, el fondo original
adaptado de 300x220 a 256x192 y los cuatro suelos horizontales del mapa original.
Mario y el blanco pasivo conservan el movimiento y jab de Prueba02.

## Controles

Cruceta: mover; A: saltar; X: golpe; B: andar lento; Abajo: bajar de plataforma;
Y: reiniciar; START: pausa; SELECT: demostración; R: alternar vista completa y
cercana. Dream Land es bastante más grande que el jardín: Mario se ve más pequeño
en la vista completa. Para llegar a la plataforma superior hay que subir primero
a una lateral.

## Conversión y presupuesto

`extract_dreamland.py` verifica el SHA-1 de la ROM, extrae los recursos 88, 103,
104, 152 y 255 y registra sus hashes. Resuelve las cadenas internas y las externas,
incluida la tabla de identificadores situada tras el flujo comprimido.
`convert_dreamland.py` recorre árboles de articulaciones y listas F3DEX2 de las
cuatro capas, convierte coordenadas a punto fijo y mantiene texturas indexadas.

El escenario aporta 175 triángulos / 525 vértices, más un cuadrilátero para el
fondo. Sus 23 texturas CI4 ocupan 91.904 bytes; el fondo CI8 ocupa 65.536. Con las
texturas de Mario suman 165.632 bytes, dentro de los 256 KiB dedicados a texturas
en VRAM A+B. Las paletas usan VRAM E; la consola inferior conserva VRAM C.

Los dos Mario comparten sus recursos. La escena completa envía 816 polígonos y
2449 vértices antes del recorte. El comprobador reserva margen adicional. La
compilación ARM9 ronda 1,37 MB entre texto/datos/BSS, sin contar toda la memoria de
ejecución. Las limitaciones permanentes están en `AGENTS.md` y `limites-ds.md`.

## Alcance actual

Se muestra una pose estática del escenario original. Los combinadores RDP y los
comportamientos de billboard se aproximan. Aún faltan animaciones ambientales,
viento de Whispy, música y el motor original completo de colisiones de paredes,
techos y agarre de bordes. Las plataformas horizontales y el límite inferior de
caída utilizan las coordenadas originales; su resolución de colisiones sigue
siendo la implementación reducida del prototipo.

Prueba01 y Prueba02 permanecen intactas. La confirmación de funcionamiento físico
del usuario corresponde a Prueba01; Prueba03 requiere su propia prueba en DS.

```powershell
python tools/build_nds.py --devkitpro C:/devkitPro --demo dreamland
python tools/check_ds_budget.py --demo dreamland
python tools/package_ds_test.py --version 03
```
