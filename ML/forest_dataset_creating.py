import cv2
import glob
import numpy as np
import pandas as pd
import os


SOURCE_DATASET = "Deer Dataset.v1i.yolov5pytorch"

TARGET_DATASET = "Forest Dataset"

SPLITS = ["train", "valid", "test"]
VALID_EXTS = ("*.jpg", "*.jpeg", "*.png", "*.bmp")

def process_and_mirror_dataset():
    for split in SPLITS:
        input_img_dir = os.path.join(SOURCE_DATASET, split, "images")
        output_img_dir = os.path.join(TARGET_DATASET, split, "images")

        os.makedirs(output_img_dir, exist_ok=True)# create same-structured folder

        image_paths = []
        for ext in VALID_EXTS:
            image_paths.extend(glob.glob(os.path.join(input_img_dir, ext)))
            image_paths.extend(glob.glob(os.path.join(input_img_dir, ext.upper())))

        if not image_paths:
            print(f"In this folder: '{input_img_dir}'-> no images detected.")
            continue

        forest_csv_rows = []

        for path in image_paths:
            filename = os.path.basename(path)
            img = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
            if img is None:
                continue

            #checking the size of each picture and reformatting if needed
            if img.shape[1] != 32 or img.shape[0] != 24:
                img = cv2.resize(img, (32, 24), interpolation=cv2.INTER_AREA)

            #transferrring brightness to degrees celsius
            temp_frame = 5.0 + (img.astype(np.float32) / 255.0) * 27.0


            #removing the animal from a picture by adding a noise pixel instead
            bg_temp = np.median(temp_frame)
            p85 = np.percentile(temp_frame, 85)

            forest_frame = temp_frame.copy()
            hot_mask = forest_frame > p85
            forest_frame[hot_mask] = bg_temp + np.random.normal(0, 0.4, size=np.sum(hot_mask))

            forest_csv_rows.append([round(x, 1) for x in forest_frame.flatten()])#saving deegrees measurements for csv files


            # transferring back to picture format and saving photos
            forest_img_uint8 = np.clip((forest_frame - 5.0) / 27.0 * 255.0, 0, 255).astype(np.uint8)
            save_path = os.path.join(output_img_dir, filename)
            cv2.imwrite(save_path, forest_img_uint8)

        csv_filename = f"dataset_forest_{split}.csv"
        pd.DataFrame(forest_csv_rows).to_csv(csv_filename, index=False, header=False)

        print(f"✅ {split.upper()}: Created: {len(image_paths)} pictures.")
        print(f"   ├── Saved photos to: '{output_img_dir}'")
        print(f"   └── Saved CSV:  '{csv_filename}'")

if __name__ == "__main__":
    process_and_mirror_dataset()

