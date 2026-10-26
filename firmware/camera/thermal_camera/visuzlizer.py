import time
import serial
import numpy as np
import matplotlib.pyplot as plt

PORT = "COM8"
BAUD = 115200

plt.ion()
plt.style.use("dark_background")
fig, ax = plt.subplots(figsize=(9, 6))

# Initialize with fixed 20-40°C scale first
img = ax.imshow(np.zeros((24, 32)), cmap="inferno",
                interpolation="nearest", vmin=15, vmax=35)
cbar = plt.colorbar(img, label="Temperature (°C)")
ax.axis("off")

# Central crosshair
ax.plot(15.5, 11.5, "w+", markersize=12)

# Status banner at top
status_text = ax.text(15.5, 1.5, " WAITING FOR ESP32... ", color="white", fontsize=13,
                      fontweight="bold", ha="center",
                      bbox=dict(boxstyle="round,pad=0.4", facecolor="#374151", alpha=0.85))

title = ax.set_title("Waiting for frame data...", y=-0.12, fontsize=11, color="lightgray")

ser = None
frame_counter = 0
auto_scale = True  # Set to False if you want to lock the colorbar to 15-35°C

print(f"[VISUALIZER] Starting monitor on {PORT} at {BAUD} baud...")

while plt.fignum_exists(fig.number):
    # 1. Maintain Serial Connection across ESP32 Deep Sleep cycles
    if ser is None or not ser.is_open:
        try:
            ser = serial.Serial()
            ser.port = PORT
            ser.baudrate = BAUD
            ser.timeout = 0.05  # Short timeout keeps GUI responsive
            # Prevent DTR/RTS toggle from hard-resetting the ESP32 on connect
            ser.dtr = False
            ser.rts = False
            ser.open()
            print(f"[SERIAL] Connected to {PORT}")
        except serial.SerialException:
            # Keep GUI alive while waiting for ESP32 to wake up / reconnect
            fig.canvas.draw_idle()
            fig.canvas.flush_events()
            time.sleep(0.2)
            continue

    # 2. Read incoming serial lines
    try:
        raw_line = ser.readline()
    except serial.SerialException:
        print("[SERIAL] Disconnected (ESP32 likely entered deep sleep). Waiting for wake...")
        try:
            ser.close()
        except Exception:
            pass
        ser = None
        status_text.set_text(" ESP32 IN DEEP SLEEP ")
        status_text.set_color("cyan")
        status_text.set_bbox(dict(boxstyle="round,pad=0.4", facecolor="#1e3a8a", alpha=0.8))
        continue

    # Always pump GUI events even when no serial line arrives
    if not raw_line:
        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        continue

    line = raw_line.decode("utf-8", errors="ignore").strip()
    if not line:
        continue

    # Clean up any leading boot garbage before '[' or numbers
    if "[" in line and not line.startswith("["):
        line = line[line.find("["):]

    parts = line.split(",")

    # 3. Print non-frame ESP32 logs ([BOOT], [WAKE], [CAMERA], [MODEL]) to console
    if len(parts) != 769:
        print(f"ESP32 -> {line}")
        if "[SLEEP]" in line:
            status_text.set_text(" SLEEPING (WAITING FOR PIR) ")
            status_text.set_color("cyan")
            status_text.set_bbox(dict(boxstyle="round,pad=0.4", facecolor="#1e3a8a", alpha=0.8))
        elif "[WAKE]" in line:
            frame_counter = 0
            status_text.set_text(" PIR TRIGGERED - READING... ")
            status_text.set_color("white")
        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        continue

    # 4. Parse the 768 pixels + 1 prediction verdict
    try:
        raw_pixels = np.array(parts[:768], dtype=float)
        verdict = int(parts[768])
    except ValueError as e:
        print(f"[PARSE ERROR] Corrupt frame line: {e}")
        continue

    frame_counter += 1

    # 5. Diagnose NaN / Inf / Corrupt values (Crucial for debugging 8/8 false positives!)
    nan_count = np.isnan(raw_pixels).sum() + np.isinf(raw_pixels).sum()
    zero_count = (raw_pixels == 0.0).sum()

    if nan_count > 0:
        print(f"[WARNING] Frame {frame_counter} contains {nan_count} NaN/Inf pixels!")
        data = np.nan_to_num(raw_pixels, nan=20.0, posinf=40.0, neginf=0.0).reshape(24, 32)
    else:
        data = raw_pixels.reshape(24, 32)

    t_min = float(np.min(data))
    t_max = float(np.max(data))
    t_mean = float(np.mean(data))
    center = float(data[11:13, 15:17].mean())

    print(
        f"[FRAME {frame_counter}/8] Verdict={verdict} | "
        f"Min={t_min:.1f}°C | Max={t_max:.1f}°C | Mean={t_mean:.1f}°C | "
        f"Zeros={zero_count} | NaNs={nan_count}"
    )

    # 6. Update Heatmap
    img.set_data(data)
    if auto_scale:
        # Avoid zero-span clim crash if all pixels are identical
        if abs(t_max - t_min) < 0.1:
            img.set_clim(t_min - 1.0, t_max + 1.0)
        else:
            img.set_clim(t_min, t_max)

    if verdict == 1:
        status_text.set_text(f" MOOSE DETECTED (Frame {frame_counter}/8) ")
        status_text.set_color("yellow")
        status_text.set_bbox(dict(boxstyle="round,pad=0.5", facecolor="#dc2626", edgecolor="#f87171", alpha=0.8))
    else:
        status_text.set_text(f" CLEAR FOREST (Frame {frame_counter}/8) ")
        status_text.set_color("lime")
        status_text.set_bbox(dict(boxstyle="round,pad=0.5", facecolor="#15803d", edgecolor="#4ade80", alpha=0.6))

    title.set_text(
        f"Min: {t_min:.1f}°C  |  Max: {t_max:.1f}°C  |  Mean: {t_mean:.1f}°C  |  Center: {center:.1f}°C"
    )

    fig.canvas.draw_idle()
    fig.canvas.flush_events()

if ser and ser.is_open:
    ser.close()