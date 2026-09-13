#!/usr/bin/env python3
"""
Whiteout Delarus - Step 7 (1:1 Part III): Alpine Sculpting & Physical Erosion Engine
Transforms raw satellite DEM into photorealistic, razor-sharp Himalayan mountain terrain:
1. Alpine Ridge & Arête Sharpening (eliminates "marshmallow" rounded hilltops)
2. Physical Hydraulic Erosion (carves vertical couloirs, avalanche chutes & gullies)
3. Thermal Erosion Simulation (creates natural talus scree cones at cliff bases)
4. Geomorphological Ambient Occlusion (Sky View Factor for deep 3D plastic depth)
5. Recomputes high-precision normal map and updated geomorphology textures.
"""

import sys
import math
import json
from pathlib import Path
import numpy as np
from PIL import Image

def laplacian_filter(arr, dx, dy):
    """Computes second spatial derivatives (Laplacian) using central differences."""
    d2y = (np.roll(arr, -1, axis=0) - 2.0 * arr + np.roll(arr, 1, axis=0)) / (dy * dy)
    d2x = (np.roll(arr, -1, axis=1) - 2.0 * arr + np.roll(arr, 1, axis=1)) / (dx * dx)
    # Fix border roll artifacts
    d2y[0, :] = d2y[1, :]
    d2y[-1, :] = d2y[-2, :]
    d2x[:, 0] = d2x[:, 1]
    d2x[:, -1] = d2x[:, -2]
    return d2x + d2y

def compute_gradients(arr, dx, dy):
    """Computes accurate central difference gradients with boundary clamping."""
    gy, gx = np.gradient(arr, dy, dx)
    return gx, gy

def sharpen_alpine_ridges(elevation, dx, dy, passes=3):
    """
    Sculpts razor-sharp arêtes, pyramidal horns, and knife-edge summit crests.
    Eliminates the satellite DEM 'marshmallow' effect where ridges are unnaturally rounded.
    Uses discrete multi-scale curvature differences to pull crests upward by 10-60m
    and steepen opposing valley headwalls into dramatic alpine arêtes.
    """
    print("  -> Applying Alpine Ridge & Arête Sharpening (Multi-Scale Discrete Curvature)...")
    h = elevation.copy()
    
    for p in range(passes):
        # 1. Radius 1 (34m) discrete curvature
        h_up    = np.roll(h, -1, axis=0)
        h_down  = np.roll(h, 1, axis=0)
        h_left  = np.roll(h, -1, axis=1)
        h_right = np.roll(h, 1, axis=1)
        curv1   = 4.0 * h - (h_up + h_down + h_left + h_right)
        
        # 2. Radius 2 (68m) broad ridge curvature
        h_up2    = np.roll(h, -2, axis=0)
        h_down2  = np.roll(h, 2, axis=0)
        h_left2  = np.roll(h, -2, axis=1)
        h_right2 = np.roll(h, 2, axis=1)
        curv2    = 4.0 * h - (h_up2 + h_down2 + h_left2 + h_right2)
        
        gx, gy = compute_gradients(h, dx, dy)
        slope = np.sqrt(gx * gx + gy * gy)
        
        # Mountainside ridge mask: convex curvature on significant slopes (> 14 deg)
        is_ridge1 = (curv1 > 1.2) & (slope > 0.25)
        is_ridge2 = (curv2 > 3.0) & (slope > 0.30)
        
        # Elevation multiplier: higher summits (Everest, Lhotse, Nuptse, Ama Dablam)
        # experience extreme periglacial frost shattering and glacial cirque steepening
        alt_gain = np.clip((h - 5000.0) / 3600.0, 0.4, 1.4)
        
        # Sharpening boost: lift narrow crests by up to 45m per pass
        sharp1 = np.where(is_ridge1, curv1 * 0.65 * alt_gain, 0.0)
        sharp2 = np.where(is_ridge2, curv2 * 0.25 * alt_gain, 0.0)
        
        total_lift = np.clip(sharp1 + sharp2, 0.0, 55.0 / (p + 1.0))
        h += total_lift
        
    print(f"     Ridge sharpening complete. Max peak lift: {np.max(h - elevation):.1f}m")
    return h

def simulate_hydraulic_erosion(elevation, dx, dy, iterations=8):
    """
    Simulates physical rainfall, runoff flow velocity, and fluvial sediment transport.
    Carves deep V-shaped couloirs, avalanche chutes, and drainage canyons into steep flanks.
    """
    print("  -> Simulating Fluvial & Hydraulic Couloir Erosion...")
    h = elevation.copy()
    couloir_channels = np.zeros_like(h)
    
    for it in range(iterations):
        gx, gy = compute_gradients(h, dx, dy)
        slope = np.sqrt(gx * gx + gy * gy)
        
        # Water concentrates in steep concave gullies (positive laplacian)
        lap = laplacian_filter(h, dx, dy)
        concavity = np.maximum(0.0, lap)
        
        # Stream flow along concave gullies
        flow = 1.0 + concavity * 60.0
        
        # Erosion rate: cuts into steep couloir chutes (30° to 70°)
        slope_angle_deg = np.degrees(np.arctan(slope))
        erosive_zone = np.clip((slope_angle_deg - 24.0) / 20.0, 0.0, 1.0) * (1.0 - np.clip((slope_angle_deg - 75.0) / 10.0, 0.0, 1.0))
        erosion = erosive_zone * np.power(slope, 1.3) * np.sqrt(flow) * 0.85
        
        # Pure couloir incision (chutes are flushed downhill by avalanche snow)
        h -= erosion * 0.45
        couloir_channels += erosion
        
    couloir_norm = np.clip(couloir_channels / (np.percentile(couloir_channels, 99.0) + 1e-4), 0.0, 1.0)
    print(f"     Hydraulic erosion complete. Couloir depth carved: max={np.max(elevation - h):.1f}m")
    return h, couloir_norm

def simulate_thermal_erosion(elevation, dx, dy, iterations=8):
    """
    Thermal / mechanical rockfall erosion:
    Cliffs steeper than the talus angle of repose (~38°) slough rock.
    Symmetric 4-way cellular automata transports debris down to wall bases (25°-34°),
    forming natural talus scree cones (Schuttkegel) with zero grid bias.
    """
    print("  -> Simulating Thermal Erosion & Talus Scree Deposition...")
    h = elevation.copy()
    talus_scree_map = np.zeros_like(h)
    
    tan_repose_cliff = math.tan(math.radians(38.0))
    
    for it in range(iterations):
        # Symmetric 4-neighbor mass-conserving transfer
        for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            dist = dy if dc == 0 else dx
            h_nbr = np.roll(np.roll(h, -dr, axis=0), -dc, axis=1)
            diff = h - h_nbr
            excess = np.maximum(0.0, diff - tan_repose_cliff * dist)
            transfer = excess * 0.125
            
            # Source loses material, downhill neighbor gains material
            h -= transfer
            gain = np.roll(np.roll(transfer, dr, axis=0), dc, axis=1)
            h += gain
            talus_scree_map += gain
            
    talus_norm = np.clip(talus_scree_map / (np.percentile(talus_scree_map, 99.0) + 1e-4), 0.0, 1.0)
    print(f"     Thermal erosion complete. Talus cones created at wall bases.")
    return h, talus_norm

def compute_sky_view_ao(elevation, dx, dy):
    """
    Computes Geomorphological Horizon Ambient Occlusion (Sky View Factor).
    Narrow couloirs, deep crevices, and valley troughs receive less skylight,
    creating deep, rich contact shadows and dramatic 3D plastic depth.
    """
    print("  -> Computing Geomorphological Horizon AO (Sky View Factor)...")
    h = elevation
    grid_h, grid_w = h.shape
    
    # Sample 8 radial directions at multiple horizon search distances
    angles = np.linspace(0, 2 * np.pi, 8, endpoint=False)
    sample_distances = [2, 4, 8, 14, 22] # in pixel units (~67m to ~740m)
    
    sky_view = np.zeros_like(h, dtype=np.float32)
    
    for angle in angles:
        cos_a = math.cos(angle)
        sin_a = math.sin(angle)
        max_horizon_tan = np.zeros_like(h)
        
        for dist in sample_distances:
            step_x = int(round(dist * cos_a))
            step_y = int(round(dist * sin_a))
            metric_dist = math.sqrt((step_x * dx)**2 + (step_y * dy)**2)
            
            sampled_h = np.roll(np.roll(h, step_y, axis=0), step_x, axis=1)
            h_diff = sampled_h - h
            tan_theta = np.maximum(0.0, h_diff / max(metric_dist, 1.0))
            max_horizon_tan = np.maximum(max_horizon_tan, tan_theta)
            
        horizon_angle = np.arctan(max_horizon_tan)
        # Visible sky fraction in this direction = 1 - sin(horizon_angle)
        dir_sky_fraction = 1.0 - np.sin(horizon_angle)
        sky_view += dir_sky_fraction
        
    sky_view /= len(angles)
    
    # Enhance contrast: deep couloirs should be visibly shadowed (0.3 - 0.5), open summits 1.0
    ao_map = np.clip((sky_view - 0.25) / 0.70, 0.0, 1.0)
    ao_map = np.power(ao_map, 1.4)
    print(f"     AO computation complete. Range: {np.min(ao_map):.2f} to {np.max(ao_map):.2f}")
    return ao_map

def compute_high_precision_normals(elevation, dx, dy):
    """Computes high-precision normal map using 5-point Sobel operator."""
    print("  -> Computing High-Precision Terrain Normal Map...")
    # Sobel kernels for smoother derivatives
    gy, gx = np.gradient(elevation, dy, dx)
    
    # Normal = normalize(-gx, -gy, 1.0)
    nz = np.ones_like(elevation)
    nx = -gx
    ny = -gy
    
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= length
    ny /= length
    nz /= length
    
    # Map to [0..255] RGB: X = East, Y = North, Z = Up (OpenGL/Vulkan standard)
    norm_r = ((nx * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    norm_g = ((ny * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    norm_b = ((nz * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    
    normal_img = np.stack([norm_r, norm_g, norm_b], axis=2)
    return normal_img

def sculpt_himalayas():
    dem_path = Path("data/processed/everest_dem_raw_float32.bin")
    if not dem_path.exists():
        dem_path = Path("data/processed/everest_dem_float32.bin")
        
    manifest_path = Path("data/processed/manifest.json")
    with open(manifest_path, "r") as f:
        manifest = json.load(f)
        
    grid_w = manifest["grid_resolution"]["width"]
    grid_h = manifest["grid_resolution"]["height"]
    dx = manifest["metric_extents"]["dx_meters_per_pixel"]
    dy = manifest["metric_extents"]["dy_meters_per_pixel"]
    
    print("==========================================================")
    print(" WHITEOUT DELARUS: STEP 7 (1:1 PART III) ALPINE SCULPTING")
    print(f" Grid: {grid_w}x{grid_h} | dx={dx:.2f}m, dy={dy:.2f}m")
    print("==========================================================")
    
    h_raw = np.fromfile(dem_path, dtype=np.float32).reshape((grid_h, grid_w))
    print(f"Raw Satellite DEM Elevation: {h_raw.min():.1f}m to {h_raw.max():.1f}m")
    
    # 1. Sharpen Alpine Ridges (Arêtes, Pyramidal Horns, Hillary Step)
    h_sharpened = sharpen_alpine_ridges(h_raw, dx, dy, passes=3)
    
    # 2. Hydraulic Erosion (Couloir Fluting & Avalanche Chutes)
    h_eroded, couloir_chutes = simulate_hydraulic_erosion(h_sharpened, dx, dy, iterations=8)
    
    # 3. Thermal Erosion (Talus Scree Cone Formation at Cliff Bases)
    h_final, talus_scree = simulate_thermal_erosion(h_eroded, dx, dy, iterations=12)
    
    print(f"\nFinal Sculpted 1:1 DEM Elevation: min={h_final.min():.1f}m, max={h_final.max():.1f}m")
    
    # 4. Ridge Crest Mask (Convex Curvature on Final Sculpted Mesh)
    lap = laplacian_filter(h_final, dx, dy)
    ridge_sharpness = np.clip(-lap * 35.0, 0.0, 1.0)
    alt_norm = np.clip((h_final - 5200.0) / 3600.0, 0.0, 1.0)
    ridge_sharpness = np.clip(ridge_sharpness * (0.6 + 0.4 * alt_norm), 0.0, 1.0)
    
    # 5. Geomorphological Ambient Occlusion (Sky View Factor)
    ao_map = compute_sky_view_ao(h_final, dx, dy)
    
    # 6. Save Sculpted DEM Binary (Used directly by Vulkan GPU Buffer & TerrainCollider)
    out_dem = Path("data/processed/everest_dem_float32.bin")
    h_final.astype(np.float32).tofile(out_dem)
    print(f"\n[OK] Saved Sculpted 1:1 DEM to {out_dem} ({out_dem.stat().st_size / (1024*1024):.2f} MB)")
    
    # 7. Save 16-bit PNG Heightmap
    min_elev = float(h_final.min())
    max_elev = float(h_final.max())
    h_norm16 = ((h_final - min_elev) / (max_elev - min_elev) * 65535.0).clip(0, 65535).astype(np.uint16)
    img16 = Image.fromarray(h_norm16, mode="I;16")
    img16.save("data/processed/everest_heightmap_16bit.png")
    print("[OK] Saved 16-bit Heightmap: data/processed/everest_heightmap_16bit.png")
    
    # 8. Compute and Save High-Precision Normal Map
    normal_img = compute_high_precision_normals(h_final, dx, dy)
    Image.fromarray(normal_img).save("data/processed/everest_normal_map.png")
    print("[OK] Saved High-Precision Normal Map: data/processed/everest_normal_map.png")
    
    # 9. Pack and Save 4-Channel Geomorphology Texture
    # Channel R: Couloirs & Avalanche Chute Fluting
    # Channel G: Thermal Talus Scree Cones (Repose 25°-34°)
    # Channel B: Knife-Edge Ridge Crests & Arêtes
    # Channel A: Geomorphological Ambient Occlusion (Sky View Factor)
    r = (couloir_chutes * 255.0).clip(0, 255).astype(np.uint8)
    g = (talus_scree * 255.0).clip(0, 255).astype(np.uint8)
    b = (ridge_sharpness * 255.0).clip(0, 255).astype(np.uint8)
    a = (ao_map * 255.0).clip(0, 255).astype(np.uint8)
    
    geom_rgba = np.stack([r, g, b, a], axis=2)
    Image.fromarray(geom_rgba, mode="RGBA").save("data/processed/everest_geomorphology.png")
    print("[OK] Saved Sculpted Geomorphology Texture (RGBA with AO): data/processed/everest_geomorphology.png")
    
    # 10. Update manifest.json
    manifest["elevation_meters"]["min"] = float(min_elev)
    manifest["elevation_meters"]["max"] = float(max_elev)
    manifest["elevation_meters"]["mean"] = float(np.mean(h_final))
    manifest["elevation_meters"]["range"] = float(max_elev - min_elev)
    manifest["vulkan_binding_guidelines"]["height_offset"] = float(min_elev)
    manifest["vulkan_binding_guidelines"]["height_scale"] = float(max_elev - min_elev)
    
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print("[OK] Updated manifest.json with sculpted elevation extents.")
    print("==========================================================")
    print(" ALPINE SCULPTING & EROSION ENGINE FINISHED SUCCESSFULLY!")
    print("==========================================================")

if __name__ == "__main__":
    sculpt_himalayas()
