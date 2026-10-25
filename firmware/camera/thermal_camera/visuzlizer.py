import serial
import numpy as np
import matplotlib.pyplot as plt

PORT = "COM7"
BAUD = 921600

ser = serial.Serial(PORT, BAUD, timeout=2)

plt.ion()
plt.style.use("dark_background")
fig, ax = plt.subplots(figsize=(8, 6))
img = ax.imshow(np.zeros((24, 32)), cmap="inferno",
                interpolation="bicubic", vmin=20, vmax=40)
cbar = plt.colorbar(img, label="Temperature °C")
ax.axis("off")

# Central cross and status of MooseDetector3000
ax.plot(15.5, 11.5, "w+", markersize=12)
status_text = ax.text(15.5, 1, "", color="white", fontsize=14, 
                      fontweight="bold", ha="center", 
                      bbox=dict(boxstyle="round,pad=0.3", facecolor="black", alpha=0.8))

title = ax.set_title("", y=-0.1)


ser.reset_input_buffer()
while plt.fignum_exists(fig.number):
    line = ser.readline().decode(errors="ignore").strip()
    parts = line.split(",")
    
    # Wait for 769 elements (768 pixels + 1 result)
    if len(parts) != 769:
        continue
    try:
        data = np.array(parts[:768], dtype=float).reshape(24, 32)
        verdict = int(parts[768])
    except ValueError:
        continue

    img.set_data(data)
    img.set_clim(data.min(), data.max())
    center = data[11:13, 15:17].mean()
    
    # Visualization 
    if verdict == 1:
        status_text.set_text(" MOOSE / ANIMAL DETECTED ")
        status_text.set_color("yellow")
        status_text.set_alpha(0.7)
        status_text.set_bbox(dict(boxstyle="round,pad=0.5", facecolor="#dc2626", edgecolor="#f87171", alpha=0.7))
    else:
        status_text.set_text(" CLEAR FOREST ")
        status_text.set_color("lime")
        status_text.set_alpha(0.7)
        status_text.set_bbox(dict(boxstyle="round,pad=0.5", facecolor="#15803d", edgecolor="#4ade80", alpha=0.4))

    title.set_text(f"Min: {data.min():.1f}°C  |  Max: {data.max():.1f}°C  |  Center: {center:.1f}°C")
    plt.pause(0.001)

ser.close()