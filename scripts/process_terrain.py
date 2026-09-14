#!/usr/bin/env python3
"""
Processes downloaded DEM and Satellite tiles:
- Decodes Terrarium RGB elevation to metric heights
- Stitches tiles into seamless 1024x1024 grids
- Computes surface normal maps
- Generates 16-bit PNG heightmaps and raw float32 binary arrays for Vulkan
- Stitches matching ESRI satellite albedo textures
"""

import os
import json
import math
import argparse
from pathlib import Path
import numpy as np
from PIL import Image

def decode_terrarium_elevation(rgb_arr):
    """
    Decodes Mapzen Terrarium RGB tile to absolute elevation in meters.
    Formula: elevation = (R * 256 + G + B / 256) - 32768
    """
    r = rgb_arr[:, :, 0].astype(np.float32)
    g = rgb_arr[:, :, 1].astype(np.float32)
    b = rgb_arr[:, :, 2].astype(np.float32)
    elevation = (r * 256.0 + g + b / 256.0) - 32768.0
    return elevation

def compute_normal_map(elevation_grid, cell_size_x, cell_size_y):
    """
    Computes high-resolution tangent/world-space normal map using a 2D multi-scale Sobel-Feldman operator.
    Normals are encoded as RGB [0..255] where (128, 128, 255) is flat pointing upwards.
    """
    h = elevation_grid
    dx = cell_size_x
    dy = cell_size_y
    
    # Radius 1 Sobel-Feldman kernel (fine couloir & arête details)
    h_n  = np.roll(h, -1, axis=0)
    h_s  = np.roll(h, 1, axis=0)
    h_e  = np.roll(h, -1, axis=1)
    h_w  = np.roll(h, 1, axis=1)
    h_ne = np.roll(np.roll(h, -1, axis=0), -1, axis=1)
    h_nw = np.roll(np.roll(h, -1, axis=0), 1, axis=1)
    h_se = np.roll(np.roll(h, 1, axis=0), -1, axis=1)
    h_sw = np.roll(np.roll(h, 1, axis=0), 1, axis=1)
    
    gx1 = ((h_ne + 2.0 * h_e + h_se) - (h_nw + 2.0 * h_w + h_sw)) / (8.0 * dx)
    gy1 = ((h_se + 2.0 * h_s + h_sw) - (h_ne + 2.0 * h_n + h_nw)) / (8.0 * dy)
    
    # Radius 2 Sobel-Feldman kernel (broad massif relief)
    h_n2  = np.roll(h, -2, axis=0)
    h_s2  = np.roll(h, 2, axis=0)
    h_e2  = np.roll(h, -2, axis=1)
    h_w2  = np.roll(h, 2, axis=1)
    h_ne2 = np.roll(np.roll(h, -2, axis=0), -2, axis=1)
    h_nw2 = np.roll(np.roll(h, -2, axis=0), 2, axis=1)
    h_se2 = np.roll(np.roll(h, 2, axis=0), -2, axis=1)
    h_sw2 = np.roll(np.roll(h, 2, axis=0), 2, axis=1)
    
    gx2 = ((h_ne2 + 2.0 * h_e2 + h_se2) - (h_nw2 + 2.0 * h_w2 + h_sw2)) / (16.0 * dx)
    gy2 = ((h_se2 + 2.0 * h_s2 + h_sw2) - (h_ne2 + 2.0 * h_n2 + h_nw2)) / (16.0 * dy)
    
    gx = 0.75 * gx1 + 0.25 * gx2
    gy = 0.75 * gy1 + 0.25 * gy2
    
    # Normal vector = normalize(-gx, -gy, 1.0)
    nx = -gx
    ny = -gy
    nz = np.ones_like(elevation_grid)
    
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= length
    ny /= length
    nz /= length
    
    # Map [-1, 1] to [0, 255]
    # In OpenGL/Vulkan standard: X = East, Y = North, Z = Up
    norm_r = ((nx * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    norm_g = ((ny * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    norm_b = ((nz * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    
    normal_img = np.stack([norm_r, norm_g, norm_b], axis=2)
    return normal_img

def process_terrain(data_dir="data", zoom=12, x_range=(3036, 3039), y_range=(1715, 1718)):
    data_path = Path(data_dir)
    dem_cache = data_path / "cache" / "dem" / str(zoom)
    sat_cache = data_path / "cache" / "sat" / str(zoom)
    processed_dir = data_path / "processed"
    processed_dir.mkdir(parents=True, exist_ok=True)
    
    x_min, x_max = x_range
    y_min, y_max = y_range
    num_tiles_x = (x_max - x_min + 1)
    num_tiles_y = (y_max - y_min + 1)
    tile_size = 256
    
    grid_w = num_tiles_x * tile_size
    grid_h = num_tiles_y * tile_size
    
    print(f"=== Processing Himalayan Terrain ({grid_w}x{grid_h} pixels) ===")
    
    stitched_elevation = np.zeros((grid_h, grid_w), dtype=np.float32)
    stitched_satellite = Image.new("RGB", (grid_w, grid_h))
    
    for iy, y in enumerate(range(y_min, y_max + 1)):
        for ix, x in enumerate(range(x_min, x_max + 1)):
            px = ix * tile_size
            py = iy * tile_size
            
            # 1. DEM
            dem_file = dem_cache / f"{x}_{y}.png"
            if dem_file.exists():
                dem_img = Image.open(dem_file).convert("RGB")
                dem_arr = np.array(dem_img)
                elev_tile = decode_terrarium_elevation(dem_arr)
                stitched_elevation[py:py+tile_size, px:px+tile_size] = elev_tile
            else:
                print(f"Missing DEM tile: {dem_file}")
                
            # 2. Satellite
            sat_file = sat_cache / f"{x}_{y}.jpg"
            if sat_file.exists():
                sat_img = Image.open(sat_file).convert("RGB")
                stitched_satellite.paste(sat_img, (px, py))
            else:
                print(f"Missing Satellite tile: {sat_file}")

    min_elev = float(stitched_elevation.min())
    max_elev = float(stitched_elevation.max())
    mean_elev = float(stitched_elevation.mean())
    print(f"Elevation stats: Min = {min_elev:.1f} m, Max = {max_elev:.1f} m, Mean = {mean_elev:.1f} m")

    # Geographic coordinates
    def tile_to_lon(x, z):
        return x / (2.0 ** z) * 360.0 - 180.0
    def tile_to_lat(y, z):
        n = math.pi - 2.0 * math.pi * y / (2.0 ** z)
        return math.degrees(math.atan(math.sinh(n)))

    west = tile_to_lon(x_min, zoom)
    east = tile_to_lon(x_max + 1, zoom)
    north = tile_to_lat(y_min, zoom)
    south = tile_to_lat(y_max + 1, zoom)
    
    # Calculate real-world metric dimensions (approximate flat projection over terrain chunk)
    avg_lat_rad = math.radians((north + south) * 0.5)
    meters_per_deg_lat = 111132.954
    meters_per_deg_lon = 111412.84 * math.cos(avg_lat_rad)
    
    total_width_meters = (east - west) * meters_per_deg_lon
    total_height_meters = (north - south) * meters_per_deg_lat
    
    dx_meters = total_width_meters / grid_w
    dy_meters = total_height_meters / grid_h
    print(f"Physical footprint: {total_width_meters/1000.0:.2f} km x {total_height_meters/1000.0:.2f} km")
    print(f"Grid pitch: {dx_meters:.2f} m/px (X), {dy_meters:.2f} m/px (Y)")

    # 1. Save raw Float32 binary file (ready for Vulkan buffer upload)
    raw_bin_path = processed_dir / "everest_dem_float32.bin"
    stitched_elevation.astype(np.float32).tofile(str(raw_bin_path))
    print(f"Saved raw Float32 DEM: {raw_bin_path} ({raw_bin_path.stat().st_size / 1024 / 1024:.2f} MB)")

    # 2. Save 16-bit Grayscale PNG heightmap (normalized)
    elev_range = max(max_elev - min_elev, 1.0)
    norm_16bit = ((stitched_elevation - min_elev) / elev_range * 65535.0).clip(0, 65535).astype(np.uint16)
    png_16bit_path = processed_dir / "everest_heightmap_16bit.png"
    Image.fromarray(norm_16bit).save(str(png_16bit_path))
    print(f"Saved 16-bit Heightmap: {png_16bit_path}")

    # 3. Save stitched satellite imagery
    sat_output_path = processed_dir / "everest_satellite_albedo.jpg"
    stitched_satellite.save(str(sat_output_path), quality=92)
    print(f"Saved Satellite Albedo Map: {sat_output_path}")

    # 4. Generate & Save Normal Map
    print("Computing surface normal map...")
    normal_map_arr = compute_normal_map(stitched_elevation, dx_meters, dy_meters)
    normal_map_path = processed_dir / "everest_normal_map.png"
    Image.fromarray(normal_map_arr).save(str(normal_map_path))
    print(f"Saved Normal Map: {normal_map_path}")

    # 5. Save manifest JSON
    manifest = {
        "region_name": "Mount Everest Massif & Sagarmatha",
        "zoom_level": zoom,
        "grid_resolution": {
            "width": grid_w,
            "height": grid_h,
            "num_tiles_x": num_tiles_x,
            "num_tiles_y": num_tiles_y
        },
        "geographic_bounds": {
            "west_lon": west,
            "east_lon": east,
            "south_lat": south,
            "north_lat": north,
            "center_lon": (west + east) * 0.5,
            "center_lat": (south + north) * 0.5
        },
        "metric_extents": {
            "width_meters": total_width_meters,
            "height_meters": total_height_meters,
            "dx_meters_per_pixel": dx_meters,
            "dy_meters_per_pixel": dy_meters
        },
        "elevation_meters": {
            "min": min_elev,
            "max": max_elev,
            "mean": mean_elev,
            "range": max_elev - min_elev
        },
        "files": {
            "raw_float32": str(raw_bin_path.name),
            "heightmap_16bit": str(png_16bit_path.name),
            "satellite_albedo": str(sat_output_path.name),
            "normal_map": str(normal_map_path.name)
        },
        "vulkan_binding_guidelines": {
            "terrain_scale_x": total_width_meters,
            "terrain_scale_y": total_height_meters,
            "height_offset": min_elev,
            "height_scale": max_elev - min_elev
        }
    }

    manifest_path = processed_dir / "manifest.json"
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"Saved Geospatial Manifest: {manifest_path}")

    return manifest

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process DEM and satellite tiles")
    parser.add_argument("--data_dir", default="data", help="Base data directory")
    args = parser.parse_args()
    process_terrain(args.data_dir)
