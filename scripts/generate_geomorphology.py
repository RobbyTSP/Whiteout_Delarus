#!/usr/bin/env python3
"""
Gaea/Houdini-style Geomorphological Analysis for 1:1 Himalayas.
Generates an RGBA 1024x1024 Geomorphology map:
  - R: Couloir Fluting & Avalanche Flow Accumulation (Gullies & Drainage)
  - G: Thermal Talus Scree Deposition (Angle of Repose 25° - 38°)
  - B: Ridge Crests, Horns & Sharp Arêtes (Convex Curvature)
  - A: Jet Stream Wind Scour vs Leeward Snow Drift Accumulation
"""

import sys
from pathlib import Path
import numpy as np
from PIL import Image

def generate_geomorphology(dem_path="data/processed/everest_dem_float32.bin",
                           output_path="data/processed/everest_geomorphology.png",
                           grid_size=1024,
                           world_width=34610.0,
                           world_depth=34520.0):
    dem_file = Path(dem_path)
    if not dem_file.exists():
        print(f"Error: DEM file not found at {dem_path}")
        sys.exit(1)

    print(f"=== Generating Geomorphology Map from {dem_path} ===")
    h = np.fromfile(dem_file, dtype=np.float32).reshape((grid_size, grid_size))
    print(f"Loaded DEM: shape={h.shape}, min={h.min():.1f}m, max={h.max():.1f}m")

    dx = world_width / (grid_size - 1)
    dy = world_depth / (grid_size - 1)

    # Gradients
    gy, gx = np.gradient(h, dy, dx)
    slope_mag = np.sqrt(gx * gx + gy * gy)
    slope_deg = np.degrees(np.arctan(slope_mag))

    # Unit downhill gradient
    eps = 1e-4
    gx_norm = gx / (slope_mag + eps)
    gy_norm = gy / (slope_mag + eps)

    # 1. Channel R: Couloirs & Avalanche Chute Flow (Concavity & convergent flow)
    # Divergence of gradient = Laplacian
    lap_y, _ = np.gradient(gy, dy, dx)
    _, lap_x = np.gradient(gx, dy, dx)
    laplacian = lap_x + lap_y

    # Gullies are concave (positive laplacian in this convention)
    # Combine with flow accumulation simulation
    flow = np.maximum(0.0, laplacian * 20.0)
    
    # 5 iterations of gradient downhill flow transport
    sediment = flow.copy()
    for _ in range(8):
        # transport downhill
        flow_x = np.roll(sediment * gx_norm, 1, axis=1) - np.roll(sediment * gx_norm, -1, axis=1)
        flow_y = np.roll(sediment * gy_norm, 1, axis=0) - np.roll(sediment * gy_norm, -1, axis=0)
        sediment = np.maximum(0.0, sediment + 0.15 * (flow_x + flow_y))

    couloir_chutes = np.clip(sediment * 1.5, 0.0, 1.0)

    # 2. Channel G: Thermal Talus Scree Deposition
    # Cliff source: slopes > 42° shed rock
    cliff_source = np.clip((slope_deg - 38.0) / 12.0, 0.0, 1.0)
    
    # Repose deposition: slopes between 22° and 38°
    repose_zone = np.exp(-((slope_deg - 30.0) / 7.0) ** 2)
    
    # Transport cliff debris downhill into repose zone
    talus_debris = cliff_source.copy()
    for _ in range(12):
        # diffuse and transport along downhill vector
        step_x = np.roll(talus_debris * gx_norm, 1, axis=1)
        step_y = np.roll(talus_debris * gy_norm, 1, axis=0)
        talus_debris = talus_debris * 0.75 + 0.25 * (step_x + step_y)

    talus_scree = np.clip(talus_debris * repose_zone * 2.2, 0.0, 1.0)

    # 3. Channel B: Ridge Crests & Arêtes (Convex curvature)
    # Sharp ridges have negative laplacian (peaks, arêtes)
    ridge_sharpness = np.clip(-laplacian * 25.0, 0.0, 1.0)
    # Boost by altitude (higher ridges are sharper)
    alt_norm = np.clip((h - 5000.0) / 3800.0, 0.0, 1.0)
    ridge_sharpness = np.clip(ridge_sharpness * (0.5 + 0.5 * alt_norm), 0.0, 1.0)

    # 4. Channel A: Jet Stream Wind Scour vs Leeward Snow Accumulation
    # Himalayan Jet Stream blows from WNW (-0.92, -0.38)
    wind_dx = -0.92
    wind_dy = -0.38
    wind_exposure = -(gx_norm * wind_dx + gy_norm * wind_dy)
    # 0.0 = leeward snow accumulation, 1.0 = windward scouring
    wind_scour = np.clip((wind_exposure + 1.0) * 0.5, 0.0, 1.0)
    # Jet stream intensity increases sharply above 6500m
    wind_intensity = np.clip((h - 5500.0) / 3000.0, 0.0, 1.0)
    wind_scour = np.clip(wind_scour * wind_intensity, 0.0, 1.0)

    # Pack into RGBA image
    r = (couloir_chutes * 255.0).astype(np.uint8)
    g = (talus_scree * 255.0).astype(np.uint8)
    b = (ridge_sharpness * 255.0).astype(np.uint8)
    a = (wind_scour * 255.0).astype(np.uint8)

    rgba = np.stack([r, g, b, a], axis=2)
    img = Image.fromarray(rgba, mode="RGBA")
    
    out = Path(output_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(str(out))
    print(f"Successfully generated Geomorphology map: {out} ({out.stat().st_size / 1024:.1f} KB)")

if __name__ == "__main__":
    generate_geomorphology()
