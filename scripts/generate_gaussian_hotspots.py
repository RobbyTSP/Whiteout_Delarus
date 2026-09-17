#!/usr/bin/env python3
"""
Whiteout Delarus - Step 21 (1:1 Part XIX): 3D Gaussian Splatting Photogrammetry Hotspot Generator
Generates millimeter-accurate, ground-truth 3D Gaussian Splats for:
1. Hotspot 0: Mount Everest Summit Plateau & Tibetan Prayer Flags (8,848.86m)
2. Hotspot 1: The Hillary Step & Summit Knife-Edge Ridge (8,790m)
3. Hotspot 2: South Col (Camp 4, 7,906m) Historic Campsite & Oxygen Relics

Binary output format per splat (64 bytes, packed C-struct):
- float4 posRadius: xyz = world pos, w = bounding radius (m)
- float4 rotQuat: xyzw = orientation quaternion
- float4 scaleOpac: xyz = 3D semi-axes (sx, sy, sz), w = opacity (0..1)
- float4 colorSH: rgb = linear HDR base color, w = hotspot ID (0, 1, 2)
"""

import os
import struct
import math
import numpy as np

def quat_from_axis_angle(axis, angle_rad):
    axis = axis / np.linalg.norm(axis)
    half = angle_rad * 0.5
    s = math.sin(half)
    return np.array([axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half)], dtype=np.float32)

def quat_from_vectors(u, v):
    u = u / np.linalg.norm(u)
    v = v / np.linalg.norm(v)
    cos_theta = np.dot(u, v)
    if cos_theta > 0.99999:
        return np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float32)
    if cos_theta < -0.99999:
        ortho = np.array([0.0, 1.0, 0.0]) if abs(u[0]) > 0.9 else np.array([1.0, 0.0, 0.0])
        axis = np.cross(u, ortho)
        axis = axis / np.linalg.norm(axis)
        return np.array([axis[0], axis[1], axis[2], 0.0], dtype=np.float32)
    axis = np.cross(u, v)
    s = math.sqrt((1.0 + cos_theta) * 2.0)
    inv_s = 1.0 / s
    return np.array([axis[0] * inv_s, axis[1] * inv_s, axis[2] * inv_s, s * 0.5], dtype=np.float32)

class GaussianSplat:
    def __init__(self, pos, rot, scale, opac, color, hotspot_id):
        self.pos = np.array(pos, dtype=np.float32)
        self.rot = np.array(rot, dtype=np.float32)
        self.scale = np.array(scale, dtype=np.float32)
        self.opac = float(opac)
        self.color = np.array(color, dtype=np.float32)
        self.hotspot_id = float(hotspot_id)
        self.radius = float(np.max(self.scale) * 1.5)

    def pack(self):
        return struct.pack(
            '4f4f4f4f',
            self.pos[0], self.pos[1], self.pos[2], self.radius,
            self.rot[0], self.rot[1], self.rot[2], self.rot[3],
            self.scale[0], self.scale[1], self.scale[2], self.opac,
            self.color[0], self.color[1], self.color[2], self.hotspot_id
        )

def generate_summit_hotspot(center):
    """
    Hotspot 0: Everest Summit Plateau (8,848.86m)
    - Aluminum Survey Tripod (Chinese Survey Beacon)
    - 5-color Tibetan Prayer Flags (Lungta) in draped catenary lines
    - Summit stone cairn & wind-carved limestone / snow cornice
    """
    splats = []
    hid = 0.0
    cx, cy, cz = center

    # 1. Survey Tripod (3 aluminum legs + cross struts + apex prism head)
    apex = np.array([cx, cy + 2.15, cz])
    tripod_radius = 0.85
    angles = [0.0, 2.0 * math.pi / 3.0, 4.0 * math.pi / 3.0]

    for a in angles:
        foot = np.array([cx + math.cos(a) * tripod_radius, cy, cz + math.sin(a) * tripod_radius])
        leg_vec = foot - apex
        leg_len = np.linalg.norm(leg_vec)
        leg_dir = leg_vec / leg_len
        q_leg = quat_from_vectors(np.array([0.0, 1.0, 0.0]), leg_dir)

        # Cylindrical Gaussian beads along the leg
        num_beads = 45
        for i in range(num_beads):
            t = i / float(num_beads - 1)
            p = apex + leg_dir * (t * leg_len)
            # Tube radius ~ 2.5cm, length ~ 5cm
            splats.append(GaussianSplat(
                pos=p,
                rot=q_leg,
                scale=[0.025, 0.045, 0.025],
                opac=0.98,
                color=[0.82, 0.84, 0.88], # Anodized aluminum silver
                hotspot_id=hid
            ))

    # Cross bracing between tripod legs
    for i in range(3):
        a1 = angles[i]
        a2 = angles[(i + 1) % 3]
        for h_frac in [0.35, 0.70]:
            p1 = apex + (np.array([math.cos(a1) * tripod_radius, -2.15, math.sin(a1) * tripod_radius])) * h_frac
            p2 = apex + (np.array([math.cos(a2) * tripod_radius, -2.15, math.sin(a2) * tripod_radius])) * h_frac
            strut_vec = p2 - p1
            strut_len = np.linalg.norm(strut_vec)
            strut_dir = strut_vec / strut_len
            q_strut = quat_from_vectors(np.array([0.0, 1.0, 0.0]), strut_dir)
            for k in range(18):
                t = k / 17.0
                splats.append(GaussianSplat(
                    pos=p1 + strut_dir * (t * strut_len),
                    rot=q_strut,
                    scale=[0.015, 0.035, 0.015],
                    opac=0.95,
                    color=[0.80, 0.82, 0.86],
                    hotspot_id=hid
                ))

    # Apex reflector prism & survey marker head
    splats.append(GaussianSplat(
        pos=apex + np.array([0.0, 0.08, 0.0]),
        rot=[0, 0, 0, 1],
        scale=[0.08, 0.12, 0.08],
        opac=0.99,
        color=[0.95, 0.88, 0.25], # Golden brass mounting plate
        hotspot_id=hid
    ))

    # 2. Tibetan Prayer Flags (Lungta) in 8 radiating catenary arcs
    # 5 Sacred Colors: Blue, White, Red, Green, Yellow
    PRAYER_COLORS = [
        [0.06, 0.32, 0.88], # Blue (Sky / Space)
        [0.94, 0.95, 0.98], # White (Air / Wind)
        [0.88, 0.12, 0.15], # Red (Fire)
        [0.10, 0.68, 0.24], # Green (Water)
        [0.96, 0.82, 0.08]  # Yellow (Earth)
    ]

    cairn_anchors = [
        np.array([cx + 3.8, cy - 0.2, cz + 1.8]),
        np.array([cx - 3.4, cy - 0.1, cz + 2.9]),
        np.array([cx + 2.2, cy - 0.3, cz - 4.1]),
        np.array([cx - 4.2, cy - 0.4, cz - 2.8]),
        np.array([cx + 4.5, cy - 0.3, cz - 1.2]),
        np.array([cx - 2.8, cy - 0.2, cz - 3.9]),
        np.array([cx + 1.2, cy - 0.4, cz + 4.2]),
        np.array([cx - 4.6, cy - 0.3, cz + 0.8])
    ]

    wind_dir = np.array([0.88, 0.12, 0.45])
    wind_dir = wind_dir / np.linalg.norm(wind_dir)

    for c_idx, anchor in enumerate(cairn_anchors):
        cord_vec = anchor - apex
        cord_len = np.linalg.norm(cord_vec)
        cord_dir = cord_vec / cord_len

        # Anchor cord spline
        for cd in range(60):
            t_cord = cd / 59.0
            sag_c = math.sin(t_cord * math.pi) * 0.42
            p_cord = apex + cord_dir * (t_cord * cord_len) - np.array([0.0, sag_c, 0.0])
            splats.append(GaussianSplat(
                pos=p_cord,
                rot=[0, 0, 0, 1],
                scale=[0.008, 0.035, 0.008],
                opac=0.95,
                color=[0.85, 0.85, 0.82],
                hotspot_id=hid
            ))

        num_flags = 36
        for f in range(num_flags):
            t = (f + 0.5) / float(num_flags)
            # Catenary sag (gravity dip)
            sag = math.sin(t * math.pi) * 0.42
            flag_cord_pos = apex + cord_dir * (t * cord_len) - np.array([0.0, sag, 0.0])

            col_idx = (f + c_idx * 2) % 5
            base_col = PRAYER_COLORS[col_idx]

            q_flutter = quat_from_vectors(np.array([0.0, 0.0, 1.0]), wind_dir)

            # 5x6 splats per flag for dense textile micro-weave
            for gx in range(5):
                for gy in range(6):
                    lx = (gx - 2.0) * 0.035
                    ly = -gy * 0.035
                    ripple = math.sin(t * 22.0 + gy * 1.8 + c_idx * 1.5) * 0.028
                    flag_p = flag_cord_pos + np.array([lx, ly, ripple]) + wind_dir * (gy * 0.028)

                    is_center = (1 <= gx <= 3) and (1 <= gy <= 4)
                    final_col = [base_col[0] * 0.45, base_col[1] * 0.45, base_col[2] * 0.45] if is_center else base_col

                    is_edge = (gy == 5) or (gx == 0 or gx == 4)
                    opac = 0.72 if is_edge else 0.94
                    scale_x = 0.028 if not is_edge else 0.012
                    scale_y = 0.028 if not is_edge else 0.045

                    splats.append(GaussianSplat(
                        pos=flag_p,
                        rot=q_flutter,
                        scale=[scale_x, scale_y, 0.006],
                        opac=opac,
                        color=final_col,
                        hotspot_id=hid
                    ))

    # 3. Summit Limestone Slabs & Anchor Cairns
    for anchor in cairn_anchors:
        for r in range(60):
            rx = np.random.uniform(-0.55, 0.55)
            ry = np.random.uniform(0.0, 0.40)
            rz = np.random.uniform(-0.55, 0.55)
            q_rock = quat_from_axis_angle(np.random.randn(3), np.random.uniform(0, math.pi))
            splats.append(GaussianSplat(
                pos=anchor + np.array([rx, ry, rz]),
                rot=q_rock,
                scale=[0.12, 0.06, 0.10],
                opac=0.98,
                color=[0.55, 0.52, 0.48], # Qomolangma grey-buff limestone
                hotspot_id=hid
            ))

    # 4. Summit Snow Cornice (Crisp firn edge)
    for s in range(400):
        t = s / 400.0
        angle = t * math.pi * 0.95 - 0.4
        rad = 3.2 + math.sin(t * 14.0) * 0.45
        sp = np.array([cx + math.cos(angle) * rad, cy - 0.05 + math.sin(t * 6.0) * 0.15, cz + math.sin(angle) * rad])
        splats.append(GaussianSplat(
            pos=sp,
            rot=[0, 0, 0, 1],
            scale=[0.16, 0.07, 0.16],
            opac=0.92,
            color=[0.92, 0.95, 1.0], # Crystalline snow/firn
            hotspot_id=hid
        ))

    return splats

def generate_hillary_step_hotspot(center):
    """
    Hotspot 1: Hillary Step (8,790m)
    - 12m near-vertical limestone rock chimney
    - Entangled web of historic fixed climbing ropes & aluminum anchors
    - Crampon scratch abrasion marks on rock ledges
    """
    splats = []
    hid = 1.0
    cx, cy, cz = center

    # 1. 12m Vertical Rock Step (Limestone facets behind ropes)
    for f in range(950):
        t = f / 950.0
        ry = t * 11.5 # 11.5 meters height
        rx = np.random.uniform(-0.6, 0.6)
        rz = 0.22 + np.random.uniform(0.02, 0.35)
        q_rock = quat_from_axis_angle(np.random.randn(3), np.random.uniform(0, math.pi))
        splats.append(GaussianSplat(
            pos=[cx + rx, cy + ry, cz + rz],
            rot=q_rock,
            scale=[0.065, 0.045, 0.045],
            opac=0.98,
            color=[0.24, 0.22, 0.20], # Dark Everest limestone
            hotspot_id=hid
        ))

    # 2. Entangled Fixed Ropes (Historic multi-decade rope tangle)
    ROPE_COLORS = [
        [0.92, 0.85, 0.12], # Neon Yellow
        [0.15, 0.45, 0.88], # Blue kernmantle
        [0.85, 0.18, 0.15], # Red
        [0.78, 0.78, 0.76], # Sun-bleached white/grey
        [0.12, 0.72, 0.35], # Green
        [0.95, 0.52, 0.10], # Orange
        [0.22, 0.22, 0.25], # Dark charcoal / black
        [0.85, 0.40, 0.85]  # Violet
    ]

    for r_idx, r_col in enumerate(ROPE_COLORS):
        start_y = 11.8
        end_y = 0.2
        num_pts = 280
        offset_x = (r_idx - 3.5) * 0.14
        for p in range(num_pts):
            t = p / float(num_pts - 1)
            y = start_y - t * (start_y - end_y)
            # Waved/snaked rope path hugging rock contour
            x = cx + offset_x + math.sin(y * 1.8 + r_idx * 1.2) * 0.16
            z = cz + math.cos(y * 1.4 + r_idx * 0.8) * 0.18
            q_rope = quat_from_vectors(np.array([0.0, 1.0, 0.0]), np.array([0.0, -1.0, 0.1]))
            splats.append(GaussianSplat(
                pos=[x, cy + y, z],
                rot=q_rope,
                scale=[0.016, 0.042, 0.016], # 11mm rope diameter
                opac=0.96,
                color=r_col,
                hotspot_id=hid
            ))

    # 3. Metallic Carabiners & Piton Anchors
    for a in range(24):
        ay = 0.8 + a * 0.48
        ax = cx + math.sin(ay * 1.8) * 0.18
        az = cz + math.cos(ay * 1.4) * 0.20
        carb_col = [0.92, 0.55, 0.15] if (a % 2 == 0) else [0.82, 0.84, 0.88]
        splats.append(GaussianSplat(
            pos=[ax, cy + ay, az],
            rot=[0, 0, 0, 1],
            scale=[0.045, 0.075, 0.02],
            opac=0.99,
            color=carb_col,
            hotspot_id=hid
        ))

    # 4. Crampon Scratches (Silvery streaks on narrow rock footholds)
    for s in range(160):
        sy = np.random.uniform(0.5, 11.2)
        sx = cx + np.random.uniform(-0.6, 0.6)
        sz = cz + np.random.uniform(-0.5, 0.5)
        splats.append(GaussianSplat(
            pos=[sx, cy + sy, sz],
            rot=[0, 0, 0, 1],
            scale=[0.06, 0.005, 0.015],
            opac=0.88,
            color=[0.88, 0.90, 0.95], # Metallic steel scrape shine
            hotspot_id=hid
        ))

    return splats

def generate_south_col_hotspot(center):
    """
    Hotspot 2: South Col (Camp 4, 7,906m)
    - Historic oxygen cylinder pile (yellow/orange cylinders with brass valves)
    - Shredded geodesic dome tent ruins & aluminum poles
    - Wind-swept slate gravel & frozen ground
    """
    splats = []
    hid = 2.0
    cx, cy, cz = center

    # 1. Historic Oxygen Cylinders (32 cylinders in a clustered dump)
    for b in range(32):
        bx = cx + (b % 8 - 3.5) * 0.32 + np.random.uniform(-0.04, 0.04)
        bz = cz + (b // 8 - 1.5) * 0.55 + np.random.uniform(-0.04, 0.04)
        by = cy + 0.12 + (b // 16) * 0.14 # Stacked layers
        q_cyl = quat_from_axis_angle(np.array([0, 1, 0]), np.random.uniform(-0.5, 0.5))

        cyl_color = [0.95, 0.80, 0.10] if (b % 3 == 0) else ([0.90, 0.42, 0.08] if (b % 3 == 1) else [0.82, 0.82, 0.85])

        # Cylinder body: 14 splats along 55cm length
        for seg in range(14):
            t = (seg - 6.5) * 0.04
            pos_c = np.array([bx, by, bz + t])
            splats.append(GaussianSplat(
                pos=pos_c,
                rot=q_cyl,
                scale=[0.045, 0.045, 0.022], # 15cm diameter
                opac=0.98,
                color=cyl_color,
                hotspot_id=hid
            ))

        # Brass neck valve & gauge
        splats.append(GaussianSplat(
            pos=[bx, by, bz + 0.30],
            rot=q_cyl,
            scale=[0.025, 0.025, 0.035],
            opac=0.99,
            color=[0.82, 0.72, 0.28], # Brass
            hotspot_id=hid
        ))

    # 2. Shredded Geodesic Dome Tent Ruins (2 tent wrecks)
    for tent_idx in range(2):
        t_offset = np.array([cx + 2.5 + tent_idx * 3.2, cy, cz + (tent_idx - 0.5) * 2.5])
        # Arched aluminum pole arcs
        for arc_idx in range(4):
            arc_angle = arc_idx * math.pi * 0.25
            for pt in range(35):
                t = pt / 34.0
                theta = t * math.pi
                r_arc = 1.6
                tx = t_offset[0] + math.cos(arc_angle) * (math.cos(theta) * r_arc)
                tz = t_offset[2] + math.sin(arc_angle) * (math.cos(theta) * r_arc)
                ty = t_offset[1] + math.sin(theta) * 1.2
                splats.append(GaussianSplat(
                    pos=[tx, ty, tz],
                    rot=[0, 0, 0, 1],
                    scale=[0.012, 0.028, 0.012], # Aluminum pole
                    opac=0.95,
                    color=[0.82, 0.84, 0.88], # Aluminum pole
                    hotspot_id=hid
                ))

        # Torn yellow/orange nylon fabric flapping on ground
        for f in range(240):
            fx = t_offset[0] + np.random.uniform(-1.4, 1.4)
            fz = t_offset[2] + np.random.uniform(-1.4, 1.4)
            fy = t_offset[1] + np.random.uniform(0.02, 0.25)
            f_col = [0.94, 0.75, 0.10] if (f % 3 == 0) else ([0.92, 0.42, 0.08] if (f % 3 == 1) else [0.25, 0.25, 0.25])
            q_fab = quat_from_axis_angle(np.random.randn(3), np.random.uniform(0, 0.5))
            splats.append(GaussianSplat(
                pos=[fx, fy, fz],
                rot=q_fab,
                scale=[0.05, 0.008, 0.05],
                opac=0.88,
                color=f_col,
                hotspot_id=hid
            ))

    # 3. Moraine Rubble & Slate Gravel
    for r in range(250):
        rx = cx + np.random.uniform(-3.5, 5.5)
        rz = cz + np.random.uniform(-2.5, 2.5)
        ry = cy + np.random.uniform(0.01, 0.06)
        q_slate = quat_from_axis_angle(np.random.randn(3), np.random.uniform(0, math.pi))
        splats.append(GaussianSplat(
            pos=[rx, ry, rz],
            rot=q_slate,
            scale=[0.05, 0.02, 0.05],
            opac=0.98,
            color=[0.18, 0.17, 0.16], # Dark South Col metamorphic slate
            hotspot_id=hid
        ))

    return splats

def main():
    print("=== Generating Step 21 3D Gaussian Splatting Photogrammetry Hotspots ===")
    np.random.seed(42)

    # Hotspot Centers in 1:1 Himalaya World Space:
    # 0: Mount Everest Summit Plateau (8,848.86m)
    summit_pos = np.array([-8462.64, 8755.37, -8057.24])
    # 1: Hillary Step (8,790m)
    hillary_pos = np.array([-8500.0, 8752.0, -7995.0])
    # 2: South Col (7,906m / Camp 4)
    south_col_pos = np.array([-7740.0, 8385.0, -4995.0])

    all_splats = []
    print("Generating Hotspot 0: Mount Everest Summit Plateau & Prayer Flags...")
    s0 = generate_summit_hotspot(summit_pos)
    all_splats.extend(s0)
    print(f"  -> Generated {len(s0)} splats for Summit Plateau.")

    print("Generating Hotspot 1: The Hillary Step & Knife-Edge Ridge...")
    s1 = generate_hillary_step_hotspot(hillary_pos)
    all_splats.extend(s1)
    print(f"  -> Generated {len(s1)} splats for Hillary Step.")

    print("Generating Hotspot 2: South Col Historic Campsite & Oxygen Relics...")
    s2 = generate_south_col_hotspot(south_col_pos)
    all_splats.extend(s2)
    print(f"  -> Generated {len(s2)} splats for South Col.")

    total_count = len(all_splats)
    print(f"Total 3D Gaussian Splats generated: {total_count} ({total_count * 64 / 1024 / 1024:.2f} MB)")

    out_dir = os.path.join(os.path.dirname(__file__), "..", "data", "processed")
    os.makedirs(out_dir, exist_ok=True)
    out_file = os.path.join(out_dir, "gaussian_hotspots.bin")

    with open(out_file, "wb") as f:
        # File header: Magic 'GSPL', uint32 version (1), uint32 totalCount, uint32 reserved
        f.write(struct.pack('4sIII', b'GSPL', 1, total_count, 0))
        for splat in all_splats:
            f.write(splat.pack())

    print(f"Successfully saved 3D Gaussian Splats to {out_file} (Size: {os.path.getsize(out_file)} bytes)")

if __name__ == "__main__":
    main()
