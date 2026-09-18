#!/usr/bin/env python3
"""
Downloads full 8K CC0 PBR material texture sets from ambientCG:
- Rock (Granite cliff & alpine bedrock): Rock028 (8K-JPG)
- Snow (Himalayan firn & alpine snowpack): Snow006 (8K-JPG)
- Scree (Glacial moraine, scree & gravel): Ground037 (8K-JPG)
- Glacier (Glacier blue ice & crevasses): Ice002 (4K-JPG -> 8K Lanczos high-detail synthesis)

Extracts Color/Albedo, NormalGL, Roughness, AO, and Displacement maps.
"""

import os
import io
import sys
import time
import zipfile
from pathlib import Path
import urllib.request
import numpy as np
from PIL import Image

MATERIALS = {
    "rock": {
        "id": "Rock028",
        "description": "Granite cliff & alpine bedrock",
        "zip_name": "Rock028_8K-JPG.zip",
        "is_8k": True
    },
    "snow": {
        "id": "Snow006",
        "description": "Himalayan firn & alpine snowpack",
        "zip_name": "Snow006_8K-JPG.zip",
        "is_8k": True
    },
    "scree": {
        "id": "Ground037",
        "description": "Glacial moraine, scree & gravel",
        "zip_name": "Ground037_8K-JPG.zip",
        "is_8k": True
    },
    "glacier": {
        "id": "Ice002",
        "description": "Glacier ice, seracs & blue ice",
        "zip_name": "Ice002_4K-JPG.zip",
        "is_8k": False  # Upscale 4K to 8K via Lanczos
    }
}

USER_AGENT = "Whiteout-Engine/1.0 (8K PBR Downloader)"

TARGET_MAPS = {
    "_Color.jpg": "albedo.jpg",
    "_NormalGL.jpg": "normal.jpg",
    "_Roughness.jpg": "roughness.jpg",
    "_AmbientOcclusion.jpg": "ao.jpg",
    "_Displacement.jpg": "displacement.jpg"
}

def download_with_progress(url, dest_path):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    print(f"Connecting to {url}...")
    with urllib.request.urlopen(req, timeout=60) as resp, open(dest_path, "wb") as f:
        total_size = int(resp.headers.get("Content-Length", 0))
        downloaded = 0
        chunk_size = 1024 * 1024  # 1 MB
        t0 = time.time()
        last_print = t0

        while True:
            chunk = resp.read(chunk_size)
            if not chunk:
                break
            f.write(chunk)
            downloaded += len(chunk)
            now = time.time()
            if now - last_print > 1.5 or downloaded == total_size:
                speed = (downloaded / (1024 * 1024)) / max(0.01, now - t0)
                percent = (downloaded / total_size * 100) if total_size else 0
                print(f"  [{downloaded / (1024*1024):.1f} / {total_size / (1024*1024):.1f} MB] ({percent:.1f}%) @ {speed:.2f} MB/s", end="\r")
                last_print = now
    print(f"\n✓ Download complete ({total_size / (1024*1024):.1f} MB in {time.time() - t0:.1f}s)")

def upscale_normal_map_to_8k(img_4k):
    """
    Upscales a normal map to 8K using Lanczos filtering and re-normalizes vectors
    to maintain mathematically exact unit length (nx^2 + ny^2 + nz^2 = 1.0).
    """
    upscaled = img_4k.resize((8192, 8192), Image.Resampling.LANCZOS)
    arr = np.array(upscaled, dtype=np.float32) / 255.0 * 2.0 - 1.0
    lengths = np.sqrt(np.sum(arr * arr, axis=2, keepdims=True))
    lengths = np.maximum(lengths, 1e-6)
    arr_norm = arr / lengths
    arr_uint8 = ((arr_norm * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    return Image.fromarray(arr_uint8)

def process_material(category, info, output_dir, temp_dir):
    cat_dir = Path(output_dir) / category
    cat_dir.mkdir(parents=True, exist_ok=True)

    # Check if all targets already exist at 8192x8192
    all_8k = True
    for target in TARGET_MAPS.values():
        p = cat_dir / target
        if not p.exists():
            all_8k = False
            break
        try:
            with Image.open(p) as im:
                if im.size != (8192, 8192):
                    all_8k = False
                    break
        except Exception:
            all_8k = False
            break

    if all_8k:
        print(f"✓ {category.capitalize()} ({info['id']}) already present at 8192x8192.")
        return

    zip_name = info["zip_name"]
    zip_path = Path(temp_dir) / zip_name
    url = f"https://ambientcg.com/get?file={zip_name}"

    if not zip_path.exists():
        print(f"\nDownloading {category.capitalize()} ({info['id']}): {info['description']}...")
        download_with_progress(url, zip_path)
    else:
        print(f"\nUsing cached {zip_name}...")

    print(f"Extracting maps for {category}...")
    with zipfile.ZipFile(zip_path, "r") as z:
        namelist = z.namelist()
        for suffix, target_name in TARGET_MAPS.items():
            matched = next((f for f in namelist if f.endswith(suffix)), None)
            if matched:
                dest = cat_dir / target_name
                if info["is_8k"]:
                    # Direct extraction of 8K file
                    with z.open(matched) as src, open(dest, "wb") as dst:
                        dst.write(src.read())
                    print(f"  Extracted: {target_name} ({dest.stat().st_size / (1024*1024):.2f} MB)")
                else:
                    # 4K source -> upscale to 8K
                    print(f"  Upscaling {matched} to 8192x8192 (Lanczos)...")
                    with z.open(matched) as src:
                        img = Image.open(io.BytesIO(src.read()))
                        if target_name == "normal.jpg":
                            img_8k = upscale_normal_map_to_8k(img)
                        else:
                            img_8k = img.resize((8192, 8192), Image.Resampling.LANCZOS)
                        img_8k.save(dest, quality=95)
                    print(f"  Saved 8K: {target_name} ({dest.stat().st_size / (1024*1024):.2f} MB)")
            else:
                print(f"  Notice: {suffix} not found in {zip_name}")

    # Clean up zip to save disk space
    if zip_path.exists():
        zip_path.unlink()
        print(f"  Removed temporary archive {zip_name}")

def main():
    out_dir = Path("data/textures")
    temp_dir = Path("data/cache/pbr_temp")
    temp_dir.mkdir(parents=True, exist_ok=True)

    print("=========================================================")
    print("  Whiteout Delarus - 8K PBR Material Pipeline (Step 26) ")
    print("  Downloading 8192x8192 Rock, Snow, Scree & Glacier Sets  ")
    print("=========================================================")

    for category, info in MATERIALS.items():
        process_material(category, info, out_dir, temp_dir)

    print("\n--- Verifying all extracted 8K textures ---")
    all_ok = True
    for category in MATERIALS.keys():
        cat_dir = out_dir / category
        for target in TARGET_MAPS.values():
            p = cat_dir / target
            if p.exists():
                with Image.open(p) as im:
                    print(f"  {category}/{target}: {im.size} ({p.stat().st_size / (1024*1024):.2f} MB)")
                    if im.size != (8192, 8192):
                        all_ok = False
            else:
                print(f"  MISSING: {category}/{target}")
                all_ok = False

    if all_ok:
        print("\n>>> SUCCESS: All 20 PBR textures are verified at 8192x8192 (8K) resolution! <<<")
    else:
        print("\n>>> WARNING: Some textures failed validation! <<<")

if __name__ == "__main__":
    main()
