import serial
import numpy as np
import matplotlib.pyplot as plt

PORT = "COM9"      # твой порт
BAUD = 921600

ser = serial.Serial(PORT, BAUD, timeout=2)

plt.ion()
fig, ax = plt.subplots()
img = ax.imshow(np.zeros((24, 32)), cmap="inferno",
                interpolation="bicubic", vmin=20, vmax=40)
cbar = plt.colorbar(img, label="°C")
ax.axis("off")
# центральная точка
ax.plot(15.5, 11.5, "w+", markersize=12)
title = ax.set_title("")

while plt.fignum_exists(fig.number):
    line = ser.readline().decode(errors="ignore").strip()
    parts = line.split(",")
    if len(parts) != 768:
        continue
    try:
        data = np.array(parts, dtype=float).reshape(24, 32)
    except ValueError:
        continue

    img.set_data(data)
    img.set_clim(data.min(), data.max())
    center = data[11:13, 15:17].mean()
    title.set_text(f"Min {data.min():.1f}   Max {data.max():.1f}   Центр {center:.1f} °C")
    plt.pause(0.001)

ser.close()
