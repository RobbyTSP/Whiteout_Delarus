#!/usr/bin/env python3
"""
Downloads CC0 PBR material texture sets from ambientCG:
- Rock (Granite mountain face)
- Snow (Alpine firn / powder)
- Scree (Mountain moraine / rock debris)
- Glacier (Glacier ice & crevasses)

Extracts Color/Albedo, NormalGL, Roughness, AO, and Displacement maps.
"""

import os
import io
import zipfile
import argparse
from pathlib import Path
import requests
from tqdm import tqdm

MATERIALS = {
    "rock": {
        "id": "Rock028",
        "description": "Granite cliff & alpine bedrock",
        "resolution": "1K-JPG"
    },
    "snow": {
        "id": "Snow006",
        "description": "Himalayan firn & alpine snowpack",
        "resolution": "1K-JPG"
    },
    "scree": {
        "id": "Ground037",
        "description": "Glacial moraine, scree & gravel",
        "resolution": "1K-JPG"
    },
    "glacier": {
        "id": "Ice002",
        "description": "Glacier ice, seracs & compressed blue ice",
        "resolution": "1K-JPG"
    }
}

USER_AGENT = "Whiteout-Engine/1.0 (PBR Downloader)"

# Maps texture suffix in ambientCG zip to standard game engine filename
TARGET_MAPS = {
    "_Color.jpg": "albedo.jpg",
    "_NormalGL.jpg": "normal.jpg",
    "_Roughness.jpg": "roughness.jpg",
    "_AmbientOcclusion.jpg": "ao.jpg",
    "_Displacement.jpg": "displacement.jpg"
}

def download_pbr_materials(output_dir="data/textures"):
    out_path = Path(output_dir)
    out_path.mkdir(parents=True, exist_ok=True)
    
    session = requests.Session()
    session.headers.update({"User-Agent": USER_AGENT})
    
    print("=== Downloading ambientCG CC0 PBR Materials ===")
    
    for category, info in MATERIALS.items():
        cat_dir = out_path / category
        cat_dir.mkdir(parents=True, exist_ok=True)
        
        # Check if already downloaded
        all_present = all((cat_dir / target_name).exists() for target_name in TARGET_MAPS.values())
        if all_present:
            print(f"✓ {category.capitalize()} ({info['id']}) already downloaded.")
            continue
            
        zip_name = f"{info['id']}_{info['resolution']}.zip"
        url = f"https://ambientcg.com/get?file={zip_name}"
        
        print(f"Downloading {category.capitalize()} ({info['id']}): {info['description']}...")
        r = session.get(url, timeout=60, stream=True)
        if r.status_code != 200:
            print(f"Error: HTTP {r.status_code} fetching {url}")
            continue
            
        zip_bytes = io.BytesIO(r.content)
        with zipfile.ZipFile(zip_bytes) as z:
            namelist = z.namelist()
            for suffix, target_name in TARGET_MAPS.items():
                matched_file = next((f for f in namelist if f.endswith(suffix)), None)
                if matched_file:
                    dest_file = cat_dir / target_name
                    with z.open(matched_file) as src_f, open(dest_file, "wb") as dst_f:
                        dst_f.write(src_f.read())
                    print(f"  Extracted: {target_name} ({dest_file.stat().st_size / 1024:.1f} KB)")
                else:
                    print(f"  Notice: {suffix} not found in {zip_name}")
                    
    print("\n✓ PBR Materials ready in data/textures/")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Download ambientCG PBR materials")
    parser.add_argument("--output", default="data/textures", help="Destination folder")
    args = parser.parse_args()
    download_pbr_materials(args.output)
