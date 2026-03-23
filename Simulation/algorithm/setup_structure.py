"""
Setup script for algorithm folder structure reorganization.
This script:
1. Deletes old folders
2. Creates new folder structure
3. Creates placeholder Python files in simulation subfolders
"""

import os
import shutil
from pathlib import Path

# Base directory
BASE_DIR = Path(r"G:\Cursor\kato-modbus\Simulation\algorithm")

def delete_folder(folder_path):
    """Delete a folder and all its contents."""
    folder = Path(folder_path)
    if folder.exists():
        shutil.rmtree(folder)
        return True
    return False

def create_folder(folder_path):
    """Create a folder if it doesn't exist."""
    folder = Path(folder_path)
    if not folder.exists():
        folder.mkdir(parents=True, exist_ok=True)
        return True
    return False

def create_file(file_path, content=""):
    """Create a file with optional content."""
    file = Path(file_path)
    file.parent.mkdir(parents=True, exist_ok=True)
    with open(file, 'w', encoding='utf-8') as f:
        f.write(content)
    return True

def step1_delete_old_folders():
    """Step 1: Delete all old folders."""
    print("\n" + "=" * 60)
    print("STEP 1: Deleting old folders...")
    print("=" * 60)
    
    old_folders = [
        # simulation
        BASE_DIR / "simulation" / "01_Raw",
        BASE_DIR / "simulation" / "02_FIR",
        BASE_DIR / "simulation" / "03_TH",
        BASE_DIR / "simulation" / "04_DBC",
        BASE_DIR / "simulation" / "05_MLP",
        # Data
        BASE_DIR / "Data" / "01_Raw",
        BASE_DIR / "Data" / "02_FIR",
        BASE_DIR / "Data" / "03_TH",
        BASE_DIR / "Data" / "04_DBC",
        BASE_DIR / "Data" / "05_MLP",
        BASE_DIR / "Data" / "simulation",
        # Photo
        BASE_DIR / "Photo" / "01_Raw",
        BASE_DIR / "Photo" / "02_FIR",
        BASE_DIR / "Photo" / "03_TH",
        BASE_DIR / "Photo" / "04_DBC",
        BASE_DIR / "Photo" / "05_DBC_Validation",
        BASE_DIR / "Photo" / "05_FIR_Validation",
        BASE_DIR / "Photo" / "05_MLP",
        BASE_DIR / "Photo" / "05_TH_Validation",
    ]
    
    deleted_count = 0
    not_found_count = 0
    
    for folder in old_folders:
        if delete_folder(folder):
            print(f"  [DELETED] {folder.relative_to(BASE_DIR)}")
            deleted_count += 1
        else:
            print(f"  [NOT FOUND] {folder.relative_to(BASE_DIR)}")
            not_found_count += 1
    
    print(f"\n  Deleted: {deleted_count} folders")
    print(f"  Not found: {not_found_count} folders")
    return deleted_count, not_found_count

def step2_create_new_folders():
    """Step 2: Create new folder structure."""
    print("\n" + "=" * 60)
    print("STEP 2: Creating new folder structure...")
    print("=" * 60)
    
    # simulation subfolders
    simulation_folders = [
        "01_Raw",
        "02_Kalman_Fast",
        "03_Normalization",
        "04_TH_Compensation",
        "05_Kalman_Slow",
        "06_1DCNN",
    ]
    
    # Data subfolders
    data_folders = [
        "01_Raw",
        "02_Kalman_Fast",
        "03_Normalization",
        "04_TH_Compensation",
        "05_Kalman_Slow",
        "06_1DCNN",
    ]
    
    # Photo subfolders
    photo_folders = [
        "01_Raw",
        "02_Kalman_Fast",
        "03_Normalization",
        "04_TH_Compensation",
        "05_Kalman_Slow",
        "05_Kalman_Slow_Validation",
        "05_TH_Validation",
        "06_1DCNN",
    ]
    
    created_count = 0
    already_exists_count = 0
    
    # Create simulation folders
    print("\n  [simulation]")
    for folder in simulation_folders:
        folder_path = BASE_DIR / "simulation" / folder
        if create_folder(folder_path):
            print(f"    [CREATED] {folder}")
            created_count += 1
        else:
            print(f"    [EXISTS] {folder}")
            already_exists_count += 1
    
    # Create Data folders
    print("\n  [Data]")
    for folder in data_folders:
        folder_path = BASE_DIR / "Data" / folder
        if create_folder(folder_path):
            print(f"    [CREATED] {folder}")
            created_count += 1
        else:
            print(f"    [EXISTS] {folder}")
            already_exists_count += 1
    
    # Create Photo folders
    print("\n  [Photo]")
    for folder in photo_folders:
        folder_path = BASE_DIR / "Photo" / folder
        if create_folder(folder_path):
            print(f"    [CREATED] {folder}")
            created_count += 1
        else:
            print(f"    [EXISTS] {folder}")
            already_exists_count += 1
    
    print(f"\n  Created: {created_count} folders")
    print(f"  Already existed: {already_exists_count} folders")
    return created_count, already_exists_count

def step3_create_python_files():
    """Step 3: Create placeholder Python files."""
    print("\n" + "=" * 60)
    print("STEP 3: Creating placeholder Python files...")
    print("=" * 60)
    
    python_files = {
        # 01_Raw
        "simulation/01_Raw/mq2_raw_analysis.py": "# MQ2 raw data analysis\n",
        "simulation/01_Raw/mq3_raw_analysis.py": "# MQ3 raw data analysis\n",
        "simulation/01_Raw/mq5_raw_analysis.py": "# MQ5 raw data analysis\n",
        "simulation/01_Raw/mq135_raw_analysis.py": "# MQ135 raw data analysis\n",
        
        # 02_Kalman_Fast
        "simulation/02_Kalman_Fast/mq2_kalman_fast.py": "# MQ2 Kalman Fast filter\n",
        "simulation/02_Kalman_Fast/mq3_kalman_fast.py": "# MQ3 Kalman Fast filter\n",
        "simulation/02_Kalman_Fast/mq5_kalman_fast.py": "# MQ5 Kalman Fast filter\n",
        "simulation/02_Kalman_Fast/mq135_kalman_fast.py": "# MQ135 Kalman Fast filter\n",
        
        # 03_Normalization
        "simulation/03_Normalization/mq2_normalization.py": "# MQ2 Normalization\n",
        "simulation/03_Normalization/mq3_normalization.py": "# MQ3 Normalization\n",
        "simulation/03_Normalization/mq5_normalization.py": "# MQ5 Normalization\n",
        "simulation/03_Normalization/mq135_normalization.py": "# MQ135 Normalization\n",
        
        # 04_TH_Compensation
        "simulation/04_TH_Compensation/th_compensation.py": "# Temperature and Humidity Compensation\n",
        
        # 05_Kalman_Slow
        "simulation/05_Kalman_Slow/kalman_slow.py": "# Kalman Slow filter\n",
        
        # 06_1DCNN
        "simulation/06_1DCNN/train_1dcnn.py": "# 1DCNN Training\n",
        "simulation/06_1DCNN/inference_test.py": "# 1DCNN Inference Test\n",
    }
    
    created_count = 0
    for file_path, content in python_files.items():
        full_path = BASE_DIR / file_path
        if create_file(full_path, content):
            print(f"  [CREATED] {file_path}")
            created_count += 1
    
    print(f"\n  Created: {created_count} Python files")
    return created_count

def main():
    print("=" * 60)
    print("Algorithm Folder Structure Reorganization Script")
    print("=" * 60)
    
    # Verify base directory exists
    if not BASE_DIR.exists():
        print(f"\nERROR: Base directory does not exist: {BASE_DIR}")
        return
    
    print(f"\nBase directory: {BASE_DIR}")
    
    # Execute steps
    step1_delete_old_folders()
    step2_create_new_folders()
    step3_create_python_files()
    
    # Final summary
    print("\n" + "=" * 60)
    print("REORGANIZATION COMPLETE!")
    print("=" * 60)
    
    # Show final structure
    print("\nFinal folder structure:")
    for category in ["simulation", "Data", "Photo"]:
        category_path = BASE_DIR / category
        if category_path.exists():
            print(f"\n  [{category}]")
            for item in sorted(category_path.iterdir()):
                if item.is_dir():
                    print(f"    - {item.name}/")
    
    print("\n" + "=" * 60)

if __name__ == "__main__":
    main()
