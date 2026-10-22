"""Plot recorded host traces; this is not a screenshot of DS emulation."""
import os
from pathlib import Path
import csv

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault('MPLCONFIGDIR', str(ROOT / '.cache/matplotlib'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

fig, axes = plt.subplots(1, 2, figsize=(10, 3.7), constrained_layout=True)
for scenario, label, color in [('long', 'Salto largo', '#2463b3'), ('short', 'Salto corto', '#dd8032')]:
    with (ROOT / f'build/scene/{scenario}.csv').open() as stream:
        rows = list(csv.DictReader(stream))[:120]
    axes[0].plot([int(row['tick']) for row in rows], [float(row['y']) for row in rows], label=label, color=color)
with (ROOT / 'build/scene/ledge.csv').open() as stream:
    rows = list(csv.DictReader(stream))[:150]
axes[1].plot([int(row['tick']) for row in rows], [float(row['y']) for row in rows], color='#367a58')
axes[0].set_title('Salto desde un suelo plano')
axes[1].set_title('Caída por borde y reaparición provisional')
axes[0].legend(frameon=False)
for ax in axes:
    ax.set_xlabel('Tic de la escena')
    ax.set_ylabel('Altura (unidades del juego)')
    ax.grid(alpha=0.2)
    ax.spines[['top', 'right']].set_visible(False)
fig.suptitle('Laboratorio de movimiento · trazas ejecutadas en PC', fontsize=13)
fig.savefig(ROOT / 'build/scene/movement.png', dpi=150)
print(ROOT / 'build/scene/movement.png')
