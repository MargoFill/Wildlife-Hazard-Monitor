## Model Architecture & Machine Learning Rationale

### Overview: TinyThermalCNN

Instead of relying on heavy pre-trained vision backbones (such as ResNet or MobileNet), this project utilizes a custom-designed **Tiny 2D Convolutional Neural Network (MooseDetector3000)** built with PyTorch and optimized for embedded inference on the ESP32-S3.

The model is trained directly on low-resolution thermal temperature matrices ($24 \times 32$ float arrays) emitted by the MLX90640 sensor, predicting binary classification: `Animal` vs `Forest`.

---

### Why MooseDetector3000 for ESP32-S3?

1. **Hardware Memory & Latency Constraints**
   * The ESP32-S3 microcontroller features limited SRAM. Standard computer vision models require megabytes of memory and millions of parameters, which cause memory overflow or high inference delays.
   * `MooseDetector3000` occupies only **~10–15 KB of memory** and executes in less than **5 milliseconds** per frame on hardware.

2. **Domain Physics over Standard RGB Processing**
   * Pre-trained ImageNet models expect high-resolution 3-channel (RGB) images with rich visual features like edges, textures, and color gradients.
   * The MLX90640 output is a 1-channel numerical matrix representing raw surface temperatures in °C. Learning thermal spatial gradients from scratch via a compact CNN is mathematically superior to forcing RGB conversions into heavy deep learning models.

---

### How the Model Works (ML Pipeline)

Input Matrix (24x32x1)
│
▼
Conv2D (8 filters, 3x3) + ReLU + MaxPool2D (2x2)  ──>  Extracts thermal intensity edges
│
▼
Conv2D (16 filters, 3x3) + ReLU + MaxPool2D (2x2) ──>  Isolates spatial heat cores & blobs
│
▼
Flatten (16x6x8 = 768 elements)
│
▼
Dense Layer (16 units) + ReLU                     ──>  Evaluates shape & temperature boundaries
│
▼
Dense Output (2 units)                            ──>  Logits for [Forest, Animal]


1. **Feature Extraction (Convolutional Layers):** 
   * The first 2D convolution applies 8 spatial filters to detect local temperature transitions between pixels.
   * Max-pooling reduces spatial dimensions ($24 \times 32 \to 12 \times 16 \to 6 \times 8$), forcing the network to become invariant to minor animal shifts within the sensor's field of view.
   
2. **Decision Layer (Fully Connected):** 
   * The extracted feature maps are flattened into a 768-element vector fed into a compact dense layer, separating thermal noise from warm-bodied animal heat signatures.



