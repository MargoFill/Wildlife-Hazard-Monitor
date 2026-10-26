#include "animal_prediction.h"
#include "thermal_model.h"

bool predict_animal(float* raw_frame) {
    // 1.  Frame normalization
    static float input[1][24][32];
    for (int r = 0; r < 24; r++) {
        for (int c = 0; c < 32; c++) {
            input[0][r][c] = (raw_frame[r * 32 + c] - 18.0f) / 10.0f;
        }
    }

    // Conv1 layer (1 -> 8 channels, 3x3) + ReLU + MaxPool (24x32 -> 12x16)
    static float conv1_out[8][12][16] = {0};
    for (int f = 0; f < 8; f++) {
        for (int r = 0; r < 12; r++) {
            for (int c = 0; c < 16; c++) {
                float sum = conv1_bias[f];
                for (int kr = 0; kr < 3; kr++) {
                    for (int kc = 0; kc < 3; kc++) {
                        int in_r = r * 2 + kr - 1;
                        int in_c = c * 2 + kc - 1;
                        if (in_r >= 0 && in_r < 24 && in_c >= 0 && in_c < 32) {
                            sum += input[0][in_r][in_c] * conv1_weight[f * 9 + kr * 3 + kc];
                        }
                    }
                }
                conv1_out[f][r][c] = sum > 0.0f ? sum : 0.0f; // ReLU
            }
        }
    }

    // Layer Conv2 (8 -> 16 channels, 3x3) + ReLU + MaxPool (12x16 -> 6x8)
    static float flat_features[768];
    int flat_idx = 0;
    for (int f = 0; f < 16; f++) {
        for (int r = 0; r < 6; r++) {
            for (int c = 0; c < 8; c++) {
                float sum = conv2_bias[f];
                for (int in_f = 0; in_f < 8; in_f++) {
                    for (int kr = 0; kr < 3; kr++) {
                        for (int kc = 0; kc < 3; kc++) {
                            int in_r = r * 2 + kr - 1;
                            int in_c = c * 2 + kc - 1;
                            if (in_r >= 0 && in_r < 12 && in_c >= 0 && in_c < 16) {
                                int w_idx = f * (8 * 9) + in_f * 9 + kr * 3 + kc;
                                sum += conv1_out[in_f][in_r][in_c] * conv2_weight[w_idx];
                            }
                        }
                    }
                }
                float relu_val = sum > 0.0f ? sum : 0.0f;
                flat_features[flat_idx++] = relu_val;
            }
        }
    }

    // Layer FC1 (768 -> 16) + ReLU
    static float fc1_out[16];
    for (int i = 0; i < 16; i++) {
        float sum = fc1_bias[i];
        for (int j = 0; j < 768; j++) {
            sum += flat_features[j] * fc1_weight[i * 768 + j];
        }
        fc1_out[i] = sum > 0.0f ? sum : 0.0f;
    }

    // Layer FC2 (16 -> 2 classes: 0 = Forest, 1 = Moose)
    static float logits[2];
    for (int i = 0; i < 2; i++) {
        float sum = fc2_bias[i];
        for (int j = 0; j < 16; j++) {
            sum += fc1_out[j] * fc2_weight[i * 16 + j];
        }
        logits[i] = sum;
    }

    // If the probability of the Moose class (index 1) is higher than that of the Forest class (index 0)
    return logits[1] > logits[0];
}