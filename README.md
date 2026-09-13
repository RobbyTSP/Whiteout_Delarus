# Whiteout Delarus (1:1 Himalaya Simulation)

Ein 1:1 maßstabsgetreues Simulations- und Bergsteiger-Spiel des Himalaya-Gebirges in **C++20 & Vulkan** mit modernem Shader-Design (**Slang / SPIR-V**).

---

## 🗺️ Schritt 1: Geodaten- & Asset-Pipeline (Abgeschlossen)

In diesem ersten Schritt wurden die fundamentalen Höhendaten, Satellitenbilder, PBR-Oberflächentexturen und meteorologischen Echtzeitdaten für den Mount Everest & die Sagarmatha-Region bezogen und für das Vulkan-Rendering aufbereitet.

### 1. Datenquellen & Datensätze

| Datensatz | Quelle | Zweck |
| :--- | :--- | :--- |
| **Höhendaten (DEM)** | AWS / Mapzen Terrarium (Copernicus GLO-30 & SRTM) | Metrische Höhendaten mit Dezimeter-Präzision |
| **Satellitenbilder** | ESRI World Imagery (ArcGIS Online) | Weltweite hochauflösende Orthofoto-Satellitenkacheln |
| **PBR-Materialien** | ambientCG (CC0 Lizenz) | 4 K-Materialsets: Granitfels, Firnschnee, Geröll/Moräne, Gletschereis |
| **Echtzeit-Wetter** | Open-Meteo API | Temperatur, Wind, Druck & Wolkenbedeckung für den Everest-Gipfel |

---

## 🏔️ Fokus-Region: Mount Everest Massiv (Sagarmatha)

* **Zentrum:** Mount Everest (27.9881° N, 86.9250° E, 8.848 m)
* **Gipfel & Landmarken im Sektor:**
  * Mount Everest (8.848 m)
  * Lhotse (8.516 m)
  * Nuptse (7.861 m)
  * Ama Dablam (6.812 m)
  * Khumbu-Eisfall & Khumbu-Gletscher
  * Everest Base Camp (5.364 m)
* **Geografische Grenzen (Bounding Box):**
  * West: 86.8359° E
  * Ost: 87.1875° E
  * Süd: 27.7613° N
  * Nord: 28.0720° N
* **Physikalische Ausdehnung:** ~34.61 km × 34.52 km (~1.194 km²)
* **Höhenbereich:** **3.651,0 m** bis **8.753,0 m** (Δh = 5.102 m)
* **Auflösung:** 1024 × 1024 Gitterpunkte (~33,8 Meter pro Vertex)

---

## 📁 Projektstruktur

```
Whiteout_Delarus/
├── .gitignore
├── requirements.txt
├── README.md
│
├── scripts/
│   ├── download_terrain.py     # Lädt Terrarium DEM & ESRI Satellitenkacheln
│   ├── process_terrain.py      # Dekodiert Höhen, stitcht Kacheln, berechnet Normalen
│   ├── download_pbr.py         # Lädt CC0 PBR Texturen (Fels, Schnee, Moräne, Eis)
│   ├── fetch_weather.py        # Ruft Live-Wetterdaten von Open-Meteo ab
│   └── run_step1_pipeline.py   # Master-Pipeline für Schritt 1
│
├── data/
│   ├── processed/
│   │   ├── everest_dem_float32.bin     # 1024x1024 rohes Float32 Array (direkter Vulkan-Buffer)
│   │   ├── everest_heightmap_16bit.png # 16-Bit Graustufen Heightmap
│   │   ├── everest_satellite_albedo.jpg# 1024x1024 ESRI Satelliten-Orthofoto
│   │   ├── everest_normal_map.png      # Berechnete Normal Map (Tangentenraum)
│   │   └── manifest.json              # Vollständige Georeferenzierungs-Metadaten
│   │
│   ├── textures/                       # ambientCG PBR Texture Sets (1024x1024)
│   │   ├── rock/                       # Rock028 (Albedo, NormalGL, Roughness, AO, Displacement)
│   │   ├── snow/                       # Snow006 (Albedo, NormalGL, Roughness, AO, Displacement)
│   │   ├── scree/                      # Ground037 (Moräne/Geröll)
│   │   └── glacier/                    # Ice002 (Gletschereis)
│   │
│   └── weather/
│       └── everest_current.json        # Echtzeit-Wetter (Everest Summit, Base Camp, Namche)
```

---

## 🛠️ Verwendung (Python Virtual Environment)

Alle Skripte laufen isoliert in einer Python `venv`:

```bash
# 1. Virtual Environment erstellen & aktivieren
python3 -m venv .venv
source .venv/bin/activate

# 2. Abhängigkeiten installieren
pip install -r requirements.txt

# 3. Schritt 1 Pipeline ausführen
python scripts/run_step1_pipeline.py
```

---

## 🔭 Nächste Schritte (Roadmap)

* [x] **Schritt 1:** Geodaten- & Bild-Download, DEM-Stitching, PBR-Texturen, Wetter-API.
* [ ] **Schritt 2:** C++20 / Vulkan Initialisierung (Device, Swapchain, Queue Families) & Slang Shader Compiler Pipeline.
* [ ] **Schritt 3:** Terrain Rendering mit CDLOD / Clipmaps oder Mesh-Shadern für das 1:1 Himalaya-Terrain unter Verwendung von `everest_dem_float32.bin`.
* [ ] **Schritt 4:** PBR Terrain Triplanar Blending (Fels, Firnschnee, Eis, Geröll nach Höhe und Neigung) + Satelliten-Overlay.
* [ ] **Schritt 5:** Dynamische Atmosphäre, Nebel und Open-Meteo Wetter-Integration.
