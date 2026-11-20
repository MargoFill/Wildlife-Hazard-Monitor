"""
Wildlife Hazard Monitor — Receiver Visualizer
Handles two data sources on the same serial port:
  1. Direct sensor serial stream (768 comma-separated floats + verdict, from sensor connected via USB)
  2. Reassembled LoRa frame lines printed by the receiver:
       FRAME_PIXELS,<ts>,<768 floats>,<source_addr>
"""

import time
import base64
import serial
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

PORT = "COM8"
BAUD = 115200

plt.ion()
plt.style.use("dark_background")

fig = plt.figure(figsize=(11, 6))
gs  = gridspec.GridSpec(1, 2, width_ratios=[2, 1], figure=fig, wspace=0.35)

# Left: thermal heatmap
ax_heat = fig.add_subplot(gs[0])
img = ax_heat.imshow(np.zeros((24, 32)), cmap="inferno",
                     interpolation="nearest", vmin=15, vmax=35)
cbar = plt.colorbar(img, ax=ax_heat, label="Temperature (°C)")
ax_heat.axis("off")
ax_heat.plot(15.5, 11.5, "w+", markersize=12)

status_text = ax_heat.text(
    15.5, 1.5, " WAITING FOR DATA... ", color="white", fontsize=12,
    fontweight="bold", ha="center",
    bbox=dict(boxstyle="round,pad=0.4", facecolor="#374151", alpha=0.85))

heat_title = ax_heat.set_title("Waiting for frame data...", y=-0.10, fontsize=10, color="lightgray")

# Right: temperature histogram
ax_hist = fig.add_subplot(gs[1])
ax_hist.set_facecolor("#111111")
ax_hist.set_title("Pixel distribution", fontsize=9, color="lightgray")
ax_hist.set_xlabel("°C", fontsize=8, color="lightgray")
ax_hist.set_ylabel("Count", fontsize=8, color="lightgray")
ax_hist.tick_params(colors="gray")
for spine in ax_hist.spines.values():
    spine.set_edgecolor("#444444")
hist_bars = None

ser = None
frame_counter   = 0
lora_frames     = 0
auto_scale      = True

SOURCE_DIRECT = "sensor-direct"
SOURCE_LORA   = "lora"

print(f"[VISUALIZER] Starting on {PORT} at {BAUD} baud...")


def set_status(text, color, face, edge=None):
    status_text.set_text(text)
    status_text.set_color(color)
    bbox = dict(boxstyle="round,pad=0.4", facecolor=face, alpha=0.85)
    if edge:
        bbox["edgecolor"] = edge
    status_text.set_bbox(bbox)


def render_frame(data: np.ndarray, source: str, label: str = ""):
    global hist_bars, frame_counter, lora_frames

    t_min  = float(np.min(data))
    t_max  = float(np.max(data))
    t_mean = float(np.mean(data))
    center = float(data[11:13, 15:17].mean())

    # Heatmap
    img.set_data(data)
    if auto_scale:
        span = t_max - t_min
        img.set_clim(t_min if span > 0.1 else t_min - 1.0,
                     t_max if span > 0.1 else t_max + 1.0)

    heat_title.set_text(
        f"Min {t_min:.1f}°C  |  Max {t_max:.1f}°C  |  Mean {t_mean:.1f}°C  |  Center {center:.1f}°C\n"
        f"Source: {source}  {label}"
    )

    # Histogram
    ax_hist.cla()
    ax_hist.set_facecolor("#111111")
    ax_hist.set_title("Pixel distribution", fontsize=9, color="lightgray")
    ax_hist.set_xlabel("°C", fontsize=8, color="lightgray")
    ax_hist.set_ylabel("Count", fontsize=8, color="lightgray")
    ax_hist.tick_params(colors="gray")
    for spine in ax_hist.spines.values():
        spine.set_edgecolor("#444444")
    ax_hist.hist(data.flatten(), bins=30, color="#f97316", edgecolor="none", alpha=0.8)
    ax_hist.axvline(t_mean, color="white", linestyle="--", linewidth=0.8, label=f"mean {t_mean:.1f}°C")
    ax_hist.legend(fontsize=7, labelcolor="gray")

    fig.canvas.draw_idle()
    fig.canvas.flush_events()


def handle_direct_line(parts: list[str]):
    """768 pixel floats + 1 verdict from the sensor serial stream."""
    global frame_counter
    try:
        pixels  = np.array(parts[:768], dtype=float)
        verdict = int(parts[768])
    except ValueError as e:
        print(f"[PARSE ERROR] {e}")
        return

    frame_counter += 1
    nan_count = int(np.isnan(pixels).sum() + np.isinf(pixels).sum())
    if nan_count:
        print(f"[WARNING] Frame {frame_counter} has {nan_count} NaN/Inf pixels")
        pixels = np.nan_to_num(pixels, nan=20.0, posinf=40.0, neginf=0.0)

    data = pixels.reshape(24, 32)
    print(f"[DIRECT frame {frame_counter}] verdict={verdict}  "
          f"min={data.min():.1f}  max={data.max():.1f}  mean={data.mean():.1f}")

    if verdict == 1:
        set_status(f" ANIMAL DETECTED (frame {frame_counter}) ",
                   "yellow", "#dc2626", "#f87171")
    else:
        set_status(f" CLEAR (frame {frame_counter}) ",
                   "lime", "#15803d", "#4ade80")

    render_frame(data, SOURCE_DIRECT, f"frame {frame_counter}")


def handle_lora_frame(line: str):
    """
    FRAME_PIXELS,<ts>,<768 floats>,<source_addr>
    Emitted by the receiver main.cpp after full frame reassembly.
    """
    global lora_frames
    parts = line.split(",")
    # parts[0]="FRAME_PIXELS", parts[1]=ts, parts[2..769]=pixels, parts[770]=source
    if len(parts) < 771:
        print(f"[LORA FRAME] Short line ({len(parts)} parts), skipping")
        return
    try:
        pixels = np.array(parts[2:770], dtype=float)
    except ValueError as e:
        print(f"[LORA FRAME PARSE ERROR] {e}")
        return

    source = parts[770] if len(parts) > 770 else "unknown"
    ts     = parts[1]
    lora_frames += 1

    nan_count = int(np.isnan(pixels).sum() + np.isinf(pixels).sum())
    if nan_count:
        print(f"[WARNING] LoRa frame {lora_frames} has {nan_count} NaN/Inf pixels")
        pixels = np.nan_to_num(pixels, nan=20.0, posinf=40.0, neginf=0.0)

    data = pixels.reshape(24, 32)
    print(f"[LORA frame {lora_frames}] ts={ts} src={source}  "
          f"min={data.min():.1f}  max={data.max():.1f}  mean={data.mean():.1f}")

    set_status(f" LORA FRAME #{lora_frames} from {source} ",
               "white", "#1e3a8a", "#60a5fa")
    render_frame(data, f"LoRa / {source}", f"ts={ts}")


# ── Main loop ────────────────────────────────────────────────────────────────
while plt.fignum_exists(fig.number):
    # Maintain connection
    if ser is None or not ser.is_open:
        try:
            ser = serial.Serial()
            ser.port     = PORT
            ser.baudrate = BAUD
            ser.timeout  = 0.05
            ser.dtr      = False
            ser.rts      = False
            ser.open()
            print(f"[SERIAL] Connected to {PORT}")
        except serial.SerialException:
            fig.canvas.draw_idle()
            fig.canvas.flush_events()
            time.sleep(0.2)
            continue

    try:
        raw_line = ser.readline()
    except serial.SerialException:
        print("[SERIAL] Disconnected — waiting for reconnect...")
        try:
            ser.close()
        except Exception:
            pass
        ser = None
        set_status(" DISCONNECTED ", "cyan", "#1e3a8a")
        continue

    if not raw_line:
        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        continue

    line = raw_line.decode("utf-8", errors="ignore").strip()
    if not line:
        continue

    # Strip leading boot garbage before first '[' or digit
    if "[" in line and not line.startswith("[") and not line.startswith("FRAME"):
        line = line[line.find("["):]

    # ── Route the line ────────────────────────────────────────────────────
    if line.startswith("FRAME_PIXELS,"):
        handle_lora_frame(line)
        continue

    parts = line.split(",")

    if len(parts) == 769:
        # Direct sensor stream: 768 floats + verdict
        handle_direct_line(parts)
        continue

    # Everything else is a log line — print and update status
    print(f"ESP32 -> {line}")
    if "[SLEEP]" in line:
        set_status(" SLEEPING (PIR WAIT) ", "cyan", "#1e3a8a")
    elif "[WAKE]" in line:
        frame_counter = 0
        set_status(" PIR TRIGGERED — READING... ", "white", "#374151")
    elif "[ALERT]" in line or "WILDLIFE" in line:
        set_status(" DETECTION ALERT ", "yellow", "#92400e", "#fbbf24")

    fig.canvas.draw_idle()
    fig.canvas.flush_events()

if ser and ser.is_open:
    ser.close()