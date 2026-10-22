import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import TensorDataset, DataLoader
import pandas as pd
import numpy as np

# 1. Uploading and Preparing CSV files
def load_split(animal_csv, forest_csv):
    df_animal = pd.read_csv(animal_csv, header=None)
    df_forest = pd.read_csv(forest_csv, header=None)

    # Taking matrix 1x24x32 from 768 numbers
    X_animal = df_animal.values.reshape(-1, 1, 24, 32).astype(np.float32)
    y_animal = np.ones(len(df_animal), dtype=np.int64)  # Class 1: Animal

    X_forest = df_forest.values.reshape(-1, 1, 24, 32).astype(np.float32)
    y_forest = np.zeros(len(df_forest), dtype=np.int64) # Class 0: Forest

    X = np.vstack([X_animal, X_forest])
    y = np.hstack([y_animal, y_forest])

    # Normalization of degrees for a neural network (centering around a zero mean)
    X = (X - 18.0) / 10.0

    return torch.tensor(X), torch.tensor(y)

print("🔄 Loading the datasets...")
X_train, y_train = load_split("dataset_animal_train.csv", "dataset_forest_train.csv")
X_valid, y_valid = load_split("dataset_animal_valid.csv", "dataset_forest_valid.csv")
X_test,  y_test  = load_split("dataset_animal_test.csv",  "dataset_forest_test.csv")

train_loader = DataLoader(TensorDataset(X_train, y_train), batch_size=16, shuffle=True)
valid_loader = DataLoader(TensorDataset(X_valid, y_valid), batch_size=16, shuffle=False)
test_loader  = DataLoader(TensorDataset(X_test, y_test), batch_size=16, shuffle=False)

# 2. Tiny 2D CNN architecture for ESP32-S3 (~8 KB in memory)
class MooseDetector3000(nn.Module):
    def __init__(self):
        super().__init__()
        # Convolutional layer 1: searches for boundaries and the heat core
        self.conv1 = nn.Conv2d(1, 8, kernel_size=3, padding=1)  # 24x32 -> 24x32
        self.pool  = nn.MaxPool2d(2, 2)                        # 24x32 -> 12x16
        
        # Convolutional layer 2: compresses features
        self.conv2 = nn.Conv2d(8, 16, kernel_size=3, padding=1) # 12x16 -> 12x16
                                                                # MaxPool -> 6x8
        self.relu  = nn.ReLU()

        self.dropout = nn.Dropout(0.2)# Protects from overfitting
        
        # Fully connected layers of the solution
        self.fc1   = nn.Linear(16 * 6 * 8, 16)
        self.fc2   = nn.Linear(16, 2)                          # 2 outputs: [Forest, Animal]

    def forward(self, x):
        x = self.pool(self.relu(self.conv1(x)))
        x = self.pool(self.relu(self.conv2(x)))
        x = x.view(x.size(0), -1)
        x = self.dropout(x)
        x = self.relu(self.fc1(x))
        x = self.fc2(x)
        return x

model = MooseDetector3000()
criterion = nn.CrossEntropyLoss()
optimizer = optim.Adam(model.parameters(), lr=0.001)

# 3. ML part
EPOCHS = 13
print("\nStarting machine learning process...")

for epoch in range(1, EPOCHS + 1):
    model.train()
    train_loss, correct_train, total_train = 0.0, 0, 0
    
    for inputs, labels in train_loader:
        optimizer.zero_grad()
        outputs = model(inputs)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()

        train_loss += loss.item() * inputs.size(0)
        _, preds = torch.max(outputs, 1)
        correct_train += (preds == labels).sum().item()
        total_train += labels.size(0)

    # Validation
    model.eval()
    val_loss, correct_val, total_val = 0.0, 0, 0
    with torch.no_grad():
        for inputs, labels in valid_loader:
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            val_loss += loss.item() * inputs.size(0)
            _, preds = torch.max(outputs, 1)
            correct_val += (preds == labels).sum().item()
            total_val += labels.size(0)

    train_acc = correct_train / total_train * 100
    val_acc = correct_val / total_val * 100
    print(f"Epoch {epoch:02d}/{EPOCHS} | Train Loss: {train_loss/total_train:.4f} Acc: {train_acc:.1f}% | Val Acc: {val_acc:.1f}%")

# 4. Final test
model.eval()
correct_test, total_test = 0, 0
with torch.no_grad():
    for inputs, labels in test_loader:
        outputs = model(inputs)
        _, preds = torch.max(outputs, 1)
        correct_test += (preds == labels).sum().item()
        total_test += labels.size(0)

print(f"\n ACCURACY ON THE TEST SET: {correct_test / total_test * 100:.2f}%")

# 5. Saving model
dummy_input = torch.randn(1, 1, 24, 32)
torch.onnx.export(model, dummy_input, "thermal_model.onnx", input_names=['input'], output_names=['output'])
torch.save(model.state_dict(), "thermal_model.pth")
print("The model has been successfully saved to the files 'thermal_model.onnx' and 'thermal_model.pth'!")
