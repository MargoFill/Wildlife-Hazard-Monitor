import cv2
import glob
import numpy as np
import pandas as pd
import os

SOURCE_DATASET = "Deer Dataset.v1i.yolov5pytorch"
SPLITS = ["train", "valid", "test"]
VALID_EXTS = ("*.jpg", "*.jpeg", "*.png", "*.bmp")

def convert_animals_to_csv():
    for split in SPLITS:
        input_img_dir = os.path.join(SOURCE_DATASET, split, "images")
        
        image_paths = []
        for ext in VALID_EXTS:
            image_paths.extend(glob.glob(os.path.join(input_img_dir, ext)))
            image_paths.extend(glob.glob(os.path.join(input_img_dir, ext.upper())))
            
        if not image_paths:
            print(f"⚠️ No pictures found in  '{input_img_dir}' folder.")
            continue

        animal_csv_rows = []
        
        for path in image_paths:
            img = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
            if img is None:
                continue
            
            # Resize if format is wrong
            if img.shape[1] != 32 or img.shape[0] != 24:
                img = cv2.resize(img, (32, 24), interpolation=cv2.INTER_AREA)

            # brightness->celsius
            temp_frame = 5.0 + (img.astype(np.float32) / 255.0) * 27.0
            
            animal_csv_rows.append([round(x, 1) for x in temp_frame.flatten()])

        # Save Csv files
        csv_filename = f"dataset_animal_{split}.csv"
        pd.DataFrame(animal_csv_rows).to_csv(csv_filename, index=False, header=False)
        
        print(f"✅ {split.upper()}: Converted {len(animal_csv_rows)} frames '{csv_filename}'.")

if __name__ == "__main__":
    convert_animals_to_csv()