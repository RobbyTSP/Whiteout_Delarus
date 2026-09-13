#!/usr/bin/env python3
"""
Step 1 Master Pipeline:
Downloads and processes all initial Himalayan assets:
1. Mapzen Terrarium DEM (Copernicus GLO-30 / SRTM)
2. ESRI World Imagery (High-Res Satellite Orthophoto)
3. Elevation & Normal Map Processing (Raw Float32 + 16-bit PNG)
4. ambientCG CC0 PBR Materials (Rock, Snow, Scree, Glacier)
5. Open-Meteo Realtime Everest Weather
"""

import sys
import time
from pathlib import Path

from download_terrain import download_region
from process_terrain import process_terrain
from download_pbr import download_pbr_materials
from fetch_weather import fetch_weather

def main():
    print("=" * 60)
    print(" WHITEOUT DELARUS - STEP 1 DATA PIPELINE")
    print(" 1:1 Scale Himalaya (Mount Everest / Sagarmatha Focus)")
    print("=" * 60)
    
    start_time = time.time()
    
    # 1. Download Terrain and Satellite Tiles
    print("\n[Step 1.1] Downloading DEM and Satellite Orthophoto Tiles...")
    region_info = download_region(preset_name="everest", output_base_dir="data")
    
    # 2. Process Terrain Raster into Engine-ready formats
    print("\n[Step 1.2] Decoding Elevation & Generating Vulkan Terrain Assets...")
    manifest = process_terrain(
        data_dir="data",
        zoom=region_info["zoom"],
        x_range=region_info["x_range"],
        y_range=region_info["y_range"]
    )
    
    # 3. Download PBR Materials
    print("\n[Step 1.3] Fetching CC0 PBR Material Textures (ambientCG)...")
    download_pbr_materials(output_dir="data/textures")
    
    # 4. Fetch Real-time Weather
    print("\n[Step 1.4] Querying Live Himalayan Atmospheric Weather (Open-Meteo)...")
    fetch_weather(output_dir="data/weather")
    
    elapsed = time.time() - start_time
    print("\n" + "=" * 60)
    print(f" STEP 1 COMPLETED SUCCESSFULLY in {elapsed:.1f}s!")
    print("=" * 60)
    print(f"Region:          {manifest['region_name']}")
    print(f"Dimensions:      {manifest['grid_resolution']['width']} x {manifest['grid_resolution']['height']} vertices")
    print(f"Footprint:       {manifest['metric_extents']['width_meters']/1000.0:.2f} km (W) x {manifest['metric_extents']['height_meters']/1000.0:.2f} km (H)")
    print(f"Elevation Range: {manifest['elevation_meters']['min']:.1f} m to {manifest['elevation_meters']['max']:.1f} m")
    print("Outputs generated in data/processed/, data/textures/, data/weather/")
    print("=" * 60)

if __name__ == "__main__":
    main()
