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

## 🚀 Schritt 2: C++20 / Vulkan 1.4 & Slang Engine (Abgeschlossen)

Die Rendering-Engine wurde in modernem **C++20** und **Vulkan 1.3+ / 1.4** mit der **Slang Shading Language** implementiert:

* **Vulkan Dynamic Rendering (`VK_KHR_dynamic_rendering`):** Kein veraltetes `VkRenderPass`- oder `VkFramebuffer`-Boilerplate. Direktes Zeichnen in Swapchain & Depth Buffer via `vkCmdBeginRendering`.
* **Slang Shader Pipeline (`slangc`):** Moderne Shadersyntax mit strukturierten Push-Constants in [`shaders/terrain.slang`](shaders/terrain.slang). Automatische Kompilierung zu SPIR-V während des CMake-Builds.
* **1:1 Mount Everest DEM-Upload:** Das 1024×1024 Float32-Höhengitter aus Schritt 1 wird direkt in device-lokalen GPU-Grafikspeicher geladen (262.144 Vertices, 522.242 Dreiecke, Höhenprofil 3.652 m bis 8.748 m).
* **6-DOF Alpine Freiflugkamera:** Flüssiges Überfliegen des ~35 km Massivs mit 150 km Sichtweite und Turbomodus.
* **Procedurales Biome-Splatting im Shader:** Automatische Verblendung von Granitfels an Steilwänden (>45° Neigung), Firnschnee in Höhenlagen, Moränengeröll und atmosphärischem Rayleigh-Höhendunst.

## 🧗‍♂️ Schritt 3: First-Person Bergsteiger-Modus & 1:1 Bodenphysik (Abgeschlossen)

In Schritt 3 wurde Whiteout Delarus zu einem echten **First-Person Spiel** ausgebaut:

* **1:1 Terrain Collider ([`src/game/TerrainCollider.hpp`](src/game/TerrainCollider.hpp)):**
  * Kontinuierliche bilineare Höheninterpolation auf dem 1024×1024 Float32 DEM.
  * Exakte Höhenabfrage $h(x, z)$ auf den Zentimeter genau überall auf dem 34,6 km Massiv.
  * Mathematische Oberflächennormalen- und Neigungswinkelberechnung ($\text{Slope} = \arccos(N_y)$).
* **First-Person Charakter-Controller ([`src/game/Player.hpp`](src/game/Player.hpp)):**
  * **Physik & Gravitation:** Realistische Erdanziehung ($-19,62\,\text{m/s}^2$), Bodenkollision, Ground-Snapping über Moränenkämme und Sprungmechanik (`Leertaste`).
  * **Alpine Hangsteigungs-Mechanik:** Steigungen > 20° verringern realistisch die Gehgeschwindigkeit. Extrem steile Steilhänge (> 55°) können nicht einfach hochgerannt werden (Bergsteiger-Physik).
  * **Head-Bobbing & Schrittdynamik:** Organische Gangart-Oszillation proportional zur Schrittgeschwindigkeit.
  * **Huckepack & Ducken (`C` / `Strg`):** Sanfte Reduzierung der Augenhöhe von 1,75 m auf 0,95 m.
* **Echtzeit-Telemetrie & Sauerstoff-Kalkulation (HUD):**
  * Höhenmesser in Metern.
  * Hangneigung in Grad.
  * Geschwindigkeit in km/h.
  * Barometrische Sauerstoffsättigung ($O_2$): z. B. ~52% im Base Camp, ~31% am Gipfel inklusive **"DEATH ZONE > 8000m"**-Warnung!
* **Modus-Umschaltung (`Tab` / `V`):**
  * Jederzeitiger nahtloser Wechsel zwischen **First-Person zu Fuß** und **6-DOF Drohnen-Freiflug**.
* **Schnellreise-Presets zu den Landmarken (`Tasten 1–4`):**
  * **1:** Everest Base Camp (Südseite, Khumbu-Gletscher auf 5.303 m)
  * **2:** Mount Everest Gipfelgrat & Hillary Step (8.729 m)
  * **3:** Lhotse Face & South Col (8.410 m)
  * **4:** Ama Dablam Tal (4.653 m)

### 🎮 Steuerung

| Taste / Eingabe | First-Person Bergsteiger | Drohnen-Freiflug (`Tab`/`V`) |
| :--- | :--- | :--- |
| **Maus bewegen** | Freies Umsehen (Mauszeiger gelockt) | Umsehen (Pitch / Yaw) |
| **W / A / S / D** | Gehen / Laufen über den Boden | Vorwärts / Links / Zurück / Rechts fliegen |
| **Shift (Umschalttaste)** | Alpin-Sprint | Turbo-Flug (bis zu 2.000 m/s) |
| **Leertaste** | Springen (über Felsspalten / Blöcke) | Steigen |
| **C / Strg** | Ducken / Kriechen | Sinken |
| **Tab / V** | **Modus wechseln (First-Person $\leftrightarrow$ Freiflug)** | Modus wechseln |
| **1 / 2 / 3 / 4** | **Schnellreise: Base Camp / Gipfel / Lhotse / Ama Dablam** | Schnellreise |
| **ESC** | Mauszeiger freigeben / Beenden | Beenden |

## 🏔️ Schritt 4: PBR-Materialien, POM & ESRI-Satelliten-Overlay (Abgeschlossen)

In Schritt 4 wurde die visuelle Qualität mit echten **PBR-Materialien, Parallax Occlusion Mapping (POM) und Makro-Satellitenüberlagerung** auf AAA-Niveau gehoben:

* **18 GPU-Textur-Maps im VRAM:**
  * **Makro:** 1024×1024 ESRI World Imagery Satelliten-Orthofoto + Makro-Terrain Normal Map.
  * **Mikro:** ambientCG CC0 PBR Materialsets (jeweils Albedo, Normal, Roughness, Displacement):
    * *Granit-Fels:* `Rock028` (dunkle Himalaya-Granitwände)
    * *Firnschnee:* `Snow006` (Gipfelschneefelder & Firn)
    * *Moräne/Geröll:* `Ground037` (Khumbu-Schotter & Gesteinsschutt)
    * *Gletschereis:* `Ice002` (bläuliches Gletschereis & Eisfall)
* **Vulkan Texture Engine ([`src/rhi/VulkanTexture.hpp`](src/rhi/VulkanTexture.hpp) / [`.cpp`](src/rhi/VulkanTexture.cpp)):**
  * Automatische **Hardware-Mipmap-Generierung** über `vkCmdBlitImage` zur Vermeidung von Texturflimmern in der Ferne.
  * **16-fache Anisotrope Filterung (AF 16x)** für gestochen scharfe Bodentexturen bei flachen Betrachtungswinkeln.
  * Automatische Unterscheidung zwischen sRGB (für Albedo) und linearem UNORM (für Normalen, Rauheit und Höhenkarten).
* **Slang Multi-Layer PBR & POM Shader ([`shaders/terrain.slang`](shaders/terrain.slang)):**
  * **Parallax Occlusion Mapping (POM):** Bis zu 20 Raymarching-Schritte entlang des Blickvektors erzeugen echte 3D-Risse und Tiefe im Moränengeröll und Fels unter den Füßen des Spielers.
  * **Physikalisch basierte Hang- & Höhen-Verblendung:**
    * Wände mit $> 40^\circ$ Neigung werfen Schnee ab und legen nackten Granit frei.
    * Hochebenen und sanfte Hänge über 5.300 m werden mit Firnschnee bedeckt.
    * Gletscherzungen (z. B. Khumbu-Eisfall) nutzen bläuliches Gletschereis.
  * **Multi-Skalen Detailkachelung:** Das Makro-Satellitenbild deckt die vollen 34,6 km ab, während hochauflösende PBR-Mikrotexturen alle ~30 m kacheln und mit einer zweiten Schicht bei 4.500x Granulat-Körnung selbst millimetergroße Steine detaillieren.
  * **Cook-Torrance/GGX Specular & Fresnel:** Kristalliner Glanz auf Firnschnee und Gletschereis bei flachen Einfallswinkeln.

### 🛠️ Kompilieren & Ausführen

```bash
# 1. Kompilieren
cmake --build build

# 2. Spiel mit PBR & Satellitentexturen starten
./build/whiteout

# 3. Screenshot aufnehmen
./build/whiteout --screenshot everest_pbr.png
```

---

## 🏔️ Schritt 5: 1:1 Geomorphologie, Triplanare PBR-Projektion & Geologische Strata (1:1 Part I - Abgeschlossen)

In Schritt 5 wurden die fotorealistischen und physikalischen Prinzipien moderner Terrain-Generatoren (Gaea, World Creator, Houdini) direkt in die Vulkan/Slang-Engine und das Bergsteiger-Gameplay integriert:

* **Triplanare PBR-Projektion an Steilwänden:**
  * Beseitigt die berüchtigte **vertikale Texturstreckung** an Steilwänden (z. B. der 62°–85° steilen Lhotse-Wand und Everest-Nordwand) vollständig.
  * Fels-Albedo, Normalen und Rauheit werden aus 3 orthogonalen Ebenen ($X, Y, Z$) im Weltkoordinatenraum projiziert.
  * Spezielle UDN/Whiteout-Normalenreorientierung mit Normalen-Gewichten $W = |N_{\text{geom}}|^4$ sorgt für homogene Meter-Auflösung ohne sichtbare Nähte oder Dehnungen.
* **Authentische Everest-Geologie & Schichtenfolge (Strata):**
  * **Greater Himalayan Crystalline (< 7.000 m):** Dunkles Gneis-, Granit- und Migmatit-Basement des Khumbu-Tals.
  * **North Col Formation (7.000 m – 8.200 m):** Dunkelgraue und rostbraune metamorphe Pelit-Schiefer und Phyllite.
  * **Das "Yellow Band" (8.200 m – 8.600 m):** Das berühmte, ~400 m mächtige Band aus gold-ockerfarbenem, rekristallisiertem dolomitischem Marmor, das mit ~15° Nordost-Neigung die Südwest- und Nordwand durchzieht (sowohl aus der Ferne als auch aus nächster Nähe detailliert sichtbar).
  * **Qomolangma-Formation (> 8.600 m):** Ordovizischer Gipfelkalkstein und heller Marmor der Gipfelpyramide und des Hillary Step.
* **Gaea-inspirierte Geomorphologie-Map ([`scripts/generate_geomorphology.py`](scripts/generate_geomorphology.py)):**
  * Aus dem 1024×1024 Float32 DEM berechnete 4-Kanal Geomorphologie-Textur (19. Bindung im Descriptor Set):
    * **Rot (Couloir Fluting):** Hydro- und Lawinenfluss-Konvergenz in Rinnen und Karen.
    * **Grün (Thermale Schuttkegel):** Felssturz-Ablagerungen an Steilwandfüßen im Schüttungswinkel von 25°–38° (Talus Scree Fans).
    * **Blau (Grat-Schärfe):** Konvexe Laplace-Krümmung für messerscharfe Grate und Arêtes.
    * **Alpha (Jet-Stream Wind-Scour):** Luv-Abblasung nackten Felses vs. Lee-Schneedrift.
* **Direktionale Lawinen-Rinnen (Couloir Fluting):**
  * Entlang des Hanggradienten $\nabla h$ fließen Lawinen und Spindrift die Falllinie hinab.
  * Schnee und Firneis klammern sich tief in die Rinnenfurchen, während die scharfen Felsrippen kahl bleiben.
* **Stratosphärisches Alpen-Licht & Klima-Physik:**
  * **Atmosphärische Dichte:** Über 7.000 m sinkt der Luftdruck auf ~33% des Meeresniveaus; der Himmel dunkelt von Azurblau in tiefes, kosmisches Indigo-Nachtblau ab.
  * **Diamond Dust & Microfacet-Glanz:** Kristalliner Mikro-Glimmer auf Firnschnee und Gletschereis.
  * **Jet-Stream-Widerstand:** Bis zu 140+ km/h Gegenwind am Gipfelgrat mit Windchill-Temperaturen bis unter $-70^\circ\text{C}$.
  * **Bodenphysik:** Unstabile Talus-Schotterhänge erzeugen Rutschbewegungen und Trittunsicherheit unter den Bergschuhen.

### 🎮 Neue CLI-Optionen

```bash
# Geomorphologie-Karte neu berechnen (Python venv)
.venv/bin/python scripts/generate_geomorphology.py

# Kompilieren
cmake --build build

# Direkt an Preset starten (1=Base Camp, 2=Summit, 3=Lhotse, 4=Ama Dablam)
./build/whiteout --preset 2

# Spezifische Kameraposition für Screenshots
./build/whiteout --cam -8537 12500 -12000 90 -45 --screenshot everest_drone.png
```

---

## 🔭 Nächste Schritte (Roadmap)

* [x] **Schritt 1:** Geodaten- & Bild-Download, DEM-Stitching, PBR-Texturen, Wetter-API.
* [x] **Schritt 2:** C++20 / Vulkan Initialisierung, Dynamic Rendering, Device-Local Buffers & Slang Shader Pipeline.
* [x] **Schritt 3:** First-Person Bergsteiger-Spiel mit 1:1 Terrain-Kollision, Laufen, Springen, Hangphysik & Schnellreise.
* [x] **Schritt 4:** 18-Kanal PBR-Pipeline, POM (Parallax Occlusion Mapping), ESRI-Satelliten-Overlay & Slope Splatting.
* [x] **Schritt 5 (1:1 Part I):** Triplanare PBR-Projektion (streckungsfreie Steilwände), Gaea-Geomorphologie, Couloir Fluting, Everest Yellow Band Strata & stratosphärische Alpin-Physik.
* [ ] **Schritt 6 (1:1 Part II):** CDLOD / Terrain Clipmaps für kontinuierliches geometrisches LOD-Streaming, volumetrische Wolken/Nebel, dynamischer Tag-Nacht-Zyklus & Live Open-Meteo Wetter.

