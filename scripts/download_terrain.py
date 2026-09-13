#!/usr/bin/env python3
"""
Downloads elevation (Mapzen Terrarium DEM) and satellite orthophotos (ESRI World Imagery)
for Himalayan regions using Web Mercator slippy tile coordinates.
"""

import os
import math
import argparse
from pathlib import Path
import requests
from tqdm import tqdm

# Presets for iconic Himalayan areas
PRESETS = {
    "everest": {
        "name": "Mount Everest & Sagarmatha National Park",
        "zoom": 12,
        "x_min": 3036,
        "x_max": 3039,  # 4 tiles wide (1024 px)
        "y_min": 1715,
        "y_max": 1718,  # 4 tiles high (1024 px)
        "center_lat": 27.9881,
        "center_lon": 86.9250,
    }
}

USER_AGENT = "Whiteout-Engine/1.0 (Himalaya 1:1 Simulation)"

def lat_lon_to_tile(lat_deg, lon_deg, zoom):
    lat_rad = math.radians(lat_deg)
    n = 2.0 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return xtile, ytile

def tile_to_lon(x, z):
    return x / (2.0 ** z) * 360.0 - 180.0

def tile_to_lat(y, z):
    n = math.pi - 2.0 * math.pi * y / (2.0 ** z)
    return math.degrees(math.atan(math.sinh(n)))

def download_tile(url, dest_path, session):
    if dest_path.exists() and dest_path.stat().st_size > 0:
        return True  # Already cached
    
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        r = session.get(url, timeout=30)
        if r.status_code == 200:
            with open(dest_path, "wb") as f:
                f.write(r.content)
            return True
        else:
            print(f"Warning: HTTP {r.status_code} for {url}")
            return False
    except Exception as e:
        print(f"Error downloading {url}: {e}")
        return False

def download_region(preset_name="everest", output_base_dir="data"):
    preset = PRESETS.get(preset_name)
    if not preset:
        raise ValueError(f"Unknown preset: {preset_name}")
    
    zoom = preset["zoom"]
    x_min, x_max = preset["x_min"], preset["x_max"]
    y_min, y_max = preset["y_min"], preset["y_max"]
    
    cache_dir = Path(output_base_dir) / "cache"
    dem_cache = cache_dir / "dem" / str(zoom)
    sat_cache = cache_dir / "sat" / str(zoom)
    
    tiles_to_download = []
    for x in range(x_min, x_max + 1):
        for y in range(y_min, y_max + 1):
            tiles_to_download.append((x, y))
            
    print(f"=== Downloading {preset['name']} ===")
    print(f"Grid: Zoom {zoom}, X: {x_min}..{x_max}, Y: {y_min}..{y_max} ({len(tiles_to_download)} tiles)")
    
    west = tile_to_lon(x_min, zoom)
    east = tile_to_lon(x_max + 1, zoom)
    north = tile_to_lat(y_min, zoom)
    south = tile_to_lat(y_max + 1, zoom)
    print(f"Bounding Box: Lon [{west:.4f}°E, {east:.4f}°E], Lat [{south:.4f}°N, {north:.4f}°N]")
    
    session = requests.Session()
    session.headers.update({"User-Agent": USER_AGENT})
    
    print("\n1. Fetching Mapzen Terrarium DEM tiles (AWS S3 Copernicus/SRTM)...")
    for x, y in tqdm(tiles_to_download, desc="DEM Tiles"):
        url = f"https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{zoom}/{x}/{y}.png"
        dest = dem_cache / f"{x}_{y}.png"
        download_tile(url, dest, session)
        
    print("\n2. Fetching ESRI World Imagery Satellite tiles (ArcGIS)...")
    for x, y in tqdm(tiles_to_download, desc="Satellite Tiles"):
        # ArcGIS URL format: tile/{z}/{y}/{x}
        url = f"https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{zoom}/{y}/{x}"
        dest = sat_cache / f"{x}_{y}.jpg"
        download_tile(url, dest, session)
        
    print("\n✓ Terrain tiles downloaded successfully to cache.")
    return {
        "zoom": zoom,
        "x_range": (x_min, x_max),
        "y_range": (y_min, y_max),
        "bounds": {
            "west": west,
            "east": east,
            "south": south,
            "north": north
        }
    }

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Download Himalayan Terrain & Satellite tiles")
    parser.add_argument("--preset", default="everest", choices=list(PRESETS.keys()), help="Target preset region")
    parser.add_argument("--output", default="data", help="Output directory")
    args = parser.parse_args()
    download_region(args.preset, args.output)
