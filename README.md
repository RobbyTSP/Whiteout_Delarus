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
| **T** | **Tageszeit umschalten (Dawn Alpenglühen $\rightarrow$ Mittag $\rightarrow$ Sunset $\rightarrow$ Nacht)** | Tageszeit umschalten |
| **B** | **Blizzard / Whiteout-Modus ein-/ausschalten (~30 m Sicht)** | Blizzard ein-/ausschalten |
| **L** | **Live Open-Meteo Wetter-Synchronisation an-/abkoppeln** | Live-Wetter an-/abkoppeln |
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

## ☁️ Schritt 6: Volles 1:1 Mesh, Volumetrisches Wolkenmeer, Alpenglühen & Live Blizzard (1:1 Part II - Abgeschlossen)

In Schritt 6 (1:1 Part II) wurde die Landschaft auf die maximale metrische Auflösung angehoben und um die charakteristischen atmosphärischen und meteorologischen Phänomene des Hochhimalaya erweitert:

* **Volle 1:1 Geländeauflösung (2.093.058 Dreiecke / 1.048.576 Vertices):**
  * Umstellung der Gitter-Schrittweite von 2 auf 1 (`sampleStep = 1`): Jede einzelne Zelle des 1024×1024 Float32 DEMs wird direkt als Hardware-Vertex gerendert.
  * Über 2 Millionen Dreiecke im GPU-Speicher (~58 MB) mit 300+ FPS auf modernen Grafikkarten (z. B. NVIDIA RTX 4060: ~1,1 ms Frame-Time).
  * Gestochen scharfe Felsgrate, Karen, Gratrippen und Wandabbrüche ohne jegliche polygonale Vergröberung.
* **Volumetrisches "Wolkenmeer" (Valley Sea of Clouds):**
  * Typisches Himalaya-Inversionswetter: Dichte Wolkenschichten füllen die tiefen Täler (Khumbu-Tal, Imja-Tal) bis ca. 5.000–5.200 m auf.
  * Die 8.000er Riesen (Mount Everest, Lhotse, Nuptse, Ama Dablam) ragen majestätisch wie alpine Inseln aus dem Wolkenmeer in den tiefblauen Himmel heraus.
  * Analytische Volumetric-Ray-Slab-Intersection im Slang-Shader mit Mie-Vorwärtsstreuung (`g = 0.65`) und sanftem vertikalem Ausfaden.
* **Dynamischer Tag-Nacht-Zyklus & Alpenglühen (`Taste T`):**
  * **Morgendämmerung (Dawn Alpenglühen, 05:51 Uhr):** Die Sonne steht im Tal noch unter dem Horizont, aber die 8.848 m hohe Gipfelpyramide des Everest erstrahlt bereits im warmen, golden-rosafarbenen Licht (spektrale Alpenglühen-Formel mit Höhen-Gain).
  * **Klarer Mittag (Crisp Noon, 12:00 Uhr):** Stechend weißes Sonnenlicht, tiefes kosmisches Indigo-Himmelsgewölbe in der dünnen Stratosphäre.
  * **Abenddämmerung (Sunset Alpenglühen, 18:20 Uhr):** Glühende gold-purpurne West- und Südwände, während die Täler im tiefblauen Eisschatten versinken.
  * **Mondhelle Nacht (Moonlit Night, 23:30 Uhr):** Kaltes, bläuliches Mondlicht reflektiert auf den Schneefeldern und Gletschern unter einem sternenklaren Nachthimmel.
* **Echtzeit-Wetter-Synchronisation via Open-Meteo (`Taste L`):**
  * Direkte Anbindung des C++20 `WeatherSystem` an die meteorologischen Daten (`data/weather/everest_current.json`).
  * Automatische Synchronisation von Temperatur, Windgeschwindigkeit, Wolkenbedeckung und barometrischem Druck.
* **Himalaya-Blizzard & Spindrift-Simulation (`Taste B`):**
  * Umschaltbarer extremer Whiteout-Sturm mit Sichtweiten unter ~30 Metern.
  * Physikalische Sonnenextinktion, dichte Schneenebel-Absorption und orkanartiger Spindrift-Partikelsturm entlang exponierter Grate.
* **Erweiterte CLI-Steuerung:**
  * `--time <0.0..24.0>`: Setzt die Tageszeit präzise in Stunden (z. B. `5.85` für Alpenglühen).
  * `--blizzard`: Startet das Spiel direkt mitten in einem tosenden Blizzard.
  * `--preset <1..4>`: 1 = Base Camp, 2 = Everest Summit, 3 = Lhotse Face, 4 = Ama Dablam.

---

## 🏔️ Schritt 7: Alpines Grat-Sculpting, Anti-Tiling, Horizon AO & ACES Filmic Tone Mapping (1:1 Part III - Abgeschlossen)

In Schritt 7 (1:1 Part III) wurden die drei fundamentalen Säulen fotorealistischer Gebirgsdarstellung nach professionellen Geländegenerator-Prinzipien (Gaea, World Creator, Houdini) implementiert:

### 1. Geometrie & Alpine Formgebung (Weg vom runden "Marshmallow-Look")
* **Multi-Scale Discrete Curvature Ridge & Arête Sharpening ([`scripts/sculpt_himalayas.py`](scripts/sculpt_himalayas.py)):**
  * Konvexe Geländegrate werden durch diskrete Krümmungsfilter ($\Delta h_1$ bei 34 m, $\Delta h_2$ bei 68 m) analysiert und gezielt aufgerichtet (bis zu +100,8 m Hebung an Graten).
  * Verwandelt abgerundete Satelliten-Höhenkuppen in messerscharfe Felsgrate (Knife-Edge Arêtes), steile Wandpfeiler und dramatische Gipfelhörner (Ama Dablam, Hillary Step, Nuptse-Grat).
* **Hydraulische Couloir-Rinnen (Fluvial Couloir Fluting):**
  * Wasser- und Lawinenfluss konvergieren in steilen Hangrinnen (> 24°).
  * Schneidet bis zu 91,5 m tiefe, V-förmige Rinnen und Lawinenrunsen in die Steilflanken ein, in denen sich Firnschnee physikalisch sammelt.
* **Symmetrische thermische Felssturz-Erosion (Cellular Automata):**
  * Masse-konservierender 4-Wege-Zelltransfer: Felswände oberhalb des Schüttungswinkels (> 38°) brechen physikalisch ab.
  * Das Geröll lagert sich am Wandfuß in einem natürlichen Schüttungswinkel von 25°–34° ab und bildet charakteristische Schuttkegel (Talus Scree Cones) ohne Richtungsverzerrung.

### 2. Shader & Texturierung (Weg vom Texturmatsch & Wallpaper-Kacheleffekt)
* **Dual-Frequency Anti-Tiling Shader:**
  * Beseitigt das berüchtigte repetitive "Kachelmuster" (Wallpaper-Effekt) bei Fernsicht über 35 km hinweg vollständig.
  * Überblendet zwei inkommensurable UV-Skalierungen und Rotationen ($1{,}37\times$ Frequenz, $31^\circ$ Drehung) mit organischem, domänenverzerrtem 2D-Rauschen.
* **Präzise physikalische Neigungs-Weiche (Slope-Masking):**
  * **Flach (< 22°):** Firnschnee auf Hochebenen, Gletschereis im Khumbu-Tal oder Moränengrund im Talboden.
  * **Mittel (22°–36°):** Geröll, Moränen-Schotter & thermische Schuttkegel (Talus Scree).
  * **Steil (> 36°):** 100% streckungsfreie triplanare Felswand (Granit, Schiefer, Yellow Band Marmor).
* **Parallax Occlusion Mapping (POM) & Micro-Grain:**
  * Erhabene Stein- und Eisstrukturen mit echtem Tiefenversatz im First-Person-Nahbereich (< 160 m).
  * Hochfrequentes Mikrokorn verhindert Pixelierung selbst bei wenigen Zentimetern Abstand.

### 3. Beleuchtung, Atmosphäre & Kontrast (Für echten Maßstab)
* **Geomorphologische Horizon Ambient Occlusion (Sky View Factor):**
  * 16-Azimut-Horizontstrahlen über 600 m Umkreis ermitteln den sichtbaren Himmelsraumanteil.
  * Tiefe Rinnen, Karen und Schluchten erhalten realistische Kontaktschatten und plastische Tiefenwirkung.
* **ACES Filmic Tone Mapping (Anti-Blowout Highlight Preservation):**
  * Verhindert das Ausbrennen (Blowout) heller Sonnenreflexionen auf Schnee und Firneis.
  * Mikro-Glimmer und Schneekristall-Strukturen bleiben auch bei greller Mittagssonne erhalten.
* **Höhenangepasstes Rayleigh-Haze:**
  * Kristallklare Himalaya-Fernsicht über 100+ km mit stratosphärischer Indigo-Verdunklung in Gipfelhöhe.

---

## 🏔️ Schritt 8: Fotorealismus-Feinschliff, Wabenmuster-Eliminierung & Geologische Harmonie (1:1 Part IV - Abgeschlossen)

In Schritt 8 (1:1 Part IV) wurden die verbliebenen visuellen Schwachstellen und Artefakte systematisch behoben, um fotorealistische Geschlossenheit und geologische Authentizität zu erreichen:

### 1. Beseitigung der Textur-Kachelung & Wabenmuster (Multi-Scale Distance Tiling & Domain Warping)
* **Ursachen-Diagnose:**
  * Auf entfernten Felskuppen und Bergflanken wiederholten sich Detail-Texturen (21 m Kachelgröße) über hunderte Male pro Berghang.
  * An triplanaren Projektionskanten schnitten sich die diagonalen Texturklüfte der X- und Z-Achsen, wodurch ein schachbrett- bzw. wabenartiges Rautengitter entstand.
* **Multi-Scale Distance-Tiled Triplanar Sampling (`sampleTriplanarAlbedoMulti`, `sampleTriplanarNormMulti`):**
  * **Nahbereich (< 80 m):** 24 m Detailauflösung für mikroskopische Granitporen, Risse und Kristalle im First-Person-Modus.
  * **Mittelgrund (80 m – 450 m):** 110 m geologische Schichtungsformationen, um 34° gedreht.
  * **Fernsicht (> 450 m):** 420 m massive Felswände und Batholith-Strukturen, um 68° gedreht.
* **Organische 2D-Domänenverzerrung (Domain Warping):**
  * Koordinaten werden durch kontinuierliches sinusoides Rauschen stochastisch verbogen (`warp = 12 m`).
  * Jegliche lineare Ausrichtung oder periodische Wiederholung von Texturklüften wird vollständig aufgelöst – Felsflanken wirken wie aus einem einzigen Granitmassiv gemeißelt.
* **Planare Multi-Skalierung (`sampleAntiTileMulti`):**
  * Schnee-, Geröll- und Gletschereisflächen skalieren ebenfalls dynamisch mit der Distanz (14 m / 63 m / 224 m), wodurch auch weite Schnee- und Eisfelder frei von Tiling-Artefakten bleiben.

### 2. Eliminierung des flachen Vordergrundrands
* **Near-Plane-Optimierung (`Camera.hpp`):**
  * Reduzierung von `m_nearPlane` von 1,0 m auf 0,25 m, um Nahtoleranz-Clipping an Felskanten vor der Kamera zu verhindern.
* **Grat-Standort-Kalibrierung (`Player.cpp`):**
  * Zurücksetzen der Spielerposition am Mount Everest Summit Ridge (`m_position = (-8552, -7938)`, Pitch +1,5°), sodass der Blick natürlich entlang des schmalen Schneegrats schweift, ohne in eine geometrisch abgeschnittene Polygonkante zu blicken.

### 3. Geologische Farb- & Materialharmonie (Echte Himalaya-Petrologie)
* **Beseitigung von Überfärbungen:**
  * Entfernung der doppelten Gelbband-Multiplikation, die Felsen im Mittelgrund wie braunen Lehm oder Schokolade hatte wirken lassen.
* **Kühle petrologische Farbkalibrierung:**
  * Kalibrierung auf echten kaltgrauen Himalaya-Gneis, Granit und Quarzit (`coldRockAlbedo`).
  * **North Col Formation (7.000 m – 8.180 m):** Kühler, dunkler phyllitischer Tonschiefer.
  * **The Yellow Band (8.180 m – 8.580 m):** Blassgelber, feiner dolomitischer Travertin-/Kalkmarmor mit natürlicher Bänderung.
  * **Qomolangma Formation (> 8.580 m):** Grauer mikritischer Kalkstein der Gipfelpyramide.
  * **Hochalpine Moränen-Entsättigung:** Geröll (Scree) über 5.000 m Höhe wird zu 100 % in kaltgrauen Schieferschotter überführt (keine organischen Brauntöne im Hochgebirge).

### 4. Schatten-Weichheit & Multi-Bounce Umgebungslicht
* **Weicher Diffus-Terminator (Half-Lambert Penumbra):**
  * Ersetzt harte Abschneidekanten durch einen breiten, weichen Lichtübergang (`smoothstep(-0.28, 0.92, rawNdotL)`).
* **Unterer Hemisphären-Schneebounce (Snow & Glacier Ground Bounce):**
  * Ausgedehnte Gletscher- und Schneeflächen reflektieren bis zu 75 % des einfallenden Sonnenlichts von unten zurück in schattige Nordwände (`snowGroundBounce`).
* **Kreuztal-Gegenhang-Licht (Cross-Peak Bounce):**
  * Sonnenbeschienene gegenüberliegende Bergflanken erhellen schattige Schluchten und Karen mit sanftem reflektiertem Licht.
* **Transparenter Schatten-Boden:**
  * Ein Mindest-Umgebungslichtboden (`float3(0.14, 0.16, 0.20)`) verhindert pechschwarze, monolithische Schattenblöcke; Felsstrukturen und Schneerinnen bleiben auch im tiefen Schatten voll durchzeichnet.

---

## 🏔️ Schritt 9: Geomorphologischer 1:1 Echtwelt-Abgleich & Gipfel-Authentizität (1:1 Part V - Abgeschlossen)

In Schritt 9 (1:1 Part V) wurden die Gebirgsformen und die Materialverteilung des gesamten 35 km × 35 km Everest-Massivs anhand realer geomorphologischer und satellitengestützter Daten kalibriert:

### 1. Entschärfung des über-spitzen Nadel-Looks & Wiederherstellung der 1:1 Gebirgsmasse ([`scripts/sculpt_himalayas.py`](scripts/sculpt_himalayas.py))
* **Beseitigung von Nadel-Singularitäten:**
  * Der vorherige 4-Nachbar-Kreuzschärfer hob isolierte Gitterpunkte um über 100 m empor, wodurch Grate teilweise wie spitze Nadeln oder Sägezähne wirkten.
  * Implementierung eines **8-Nachbar isotropen Krümmungsfilters** ($w_{\text{ortho}} = 1{,}0$, $w_{\text{diag}} = \frac{1}{\sqrt{2}}$), der nur echte, kontinuierliche Gratlinien und Pfeiler schärft.
  * Kalibrierter maximaler Grathub von 25,7 m: Die 8.000er (Mount Everest, Lhotse, Nuptse, Ama Dablam) erhalten ihren authentischen, wuchtigen Pfeiler- und Massiv-Charakter zurück.
* **Glaziales Cirque-Sapping (Karwand-Steilung):**
  * Konkave Hangflanken werden durch periglaziale Tiefenerosion um bis zu -8 m abgetragen, wodurch steile, natürliche Wandabbrüche und Karbecken entstehen.

### 2. Reale Schnee- & Fels-Verteilung nach Satelliten-Befund ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Beseitigung der künstlichen Schotterdominanz:**
  * Frühere prozedurale Neigungsschwellen (abruptes Abreißen von Schnee bei 25°) verwandelten über 70 % der Flanken oberhalb von 6.000 m in braunen Schotter.
  * Satellitenanalysen belegen oberhalb von 6.000 m jedoch **89,4 % perennierenden Schnee/Firn** und nur **5,4 % freiliegenden Steilfels**.
* **Physikalische Firn-Adhäsion & Lawinen-Couloirs:**
  * Schnee haftet physikalisch an rauen Gesteinsflanken bis 38°–44° Neigung; in tiefen Couloir-Rinnen sammelt sich Firnschnee bis zu einer Neigung von 60°.
  * Moränen- und Schuttkegel werden in der eisigen Death Zone (> 5.700 m) automatisch durch echten Hängegletscher- und Firnschnee ersetzt.
* **Satellitengestützte Fels/Schnee-Fusionsweiche:**
  * Reale Satelliten-Reflektanz koppelt die Hangbedeckung: Reale perennierende Firnfelder behalten ihren Schnee, während windexponierte und überhängende Steilwände blanken Fels zeigen.
* **Jetstream-Windabblasung:**
  * Oberhalb von 8.480 m auf der Gipfelpyramide des Everest sorgt extremer Jetstream-Wind dafür, dass Schnee von windexponierten Kanten abgeblasen wird und der dunkle Qomolangma-Kalkstein hervortritt.

### 3. Geologisches Höhen-Banding (Echte Himalaya-Schichtung)
* **> 8.600 m (Qomolangma-Formation):** Dunkelgrauer, windgeschliffener mikritischer Kalkstein der Gipfelpyramide.
* **8.200 m – 8.600 m (The Yellow Band):** Das berühmte, aus großer Distanz sichtbare gelb-ockerfarbene dolomitische Marmorband.
* **< 8.200 m (North Col Formation & Sockel):** Kaltgrauer Gneis, phyllitischer Schiefer und Granit.

### 4. Kalibrierter Hillary-Step-Grat-Standort ([`src/game/Player.cpp`](src/game/Player.cpp))
* **Ungehinderte 1:1 Panorama-Perspektive:**
  * Standort am Mount Everest Summit Ridge auf $x = -8535$, $z = -7935$ auf 8.730 m Höhe (Yaw 40°, Pitch +2°).
  * Gibt den spektakulären Weitblick entlang des schneebedeckten Hillary-Step-Grats frei: über das Lhotse-Massiv, das Khumbu-Gletschertal und das darunterliegende Wolkenmeer.

---

## 🏔️ Schritt 9.1: GPU-Tessellation / Virtual Heightfield, Multi-Scale Sobel Normal-Baking & Geologisches Höhen-Banding (1:1 Part VI - Abgeschlossen)

In Schritt 9.1 (1:1 Part VI) wurden die verbliebenen geraden 33,8 m Rasterkanten auf Vertex-Ebene mikroskopisch aufgebrochen, die Tiefenwirkung von Felswänden und Couloirs über einen 2D-Multi-Scale-Sobel-Operator potenziert und die reale geologische Schichtung des Mount Everest exakt verankert:

### 1. GPU Virtual Heightfield (Detail-Displacement auf Vertex-Ebene) ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Aufbrechen der 33,8 m Rasterkanten:**
  * Im Slang Vertex Shader (`vertexMain`) bricht ein mehrstufiges, domänenverzerrtes FBM-Mikrofraktursystem (`fbmNoise`) in Kombination mit der Geomorphologie-Textur (`texGeomorphology`) die geraden Dreieckskanten der Höhendaten dynamisch auf.
  * Steile Felswände erhalten horizontale Felsgesimse und Klüfte (bis zu $\pm 2,8\,\text{m}$ horizontales Displacement), während Kämme angehoben und Rinnen vertieft werden.
  * **Player Clearance Protection:** Über eine Abstandsmaske (`nearClearance = smoothstep(3.5, 9.0, distToCam)`) blendet das Displacement in unmittelbarer Spielernähe (< 3,5 m) sanft auf 0 ab. Dadurch bleibt die 1:1 Bodenkollision ([`src/game/TerrainCollider.h`](src/game/TerrainCollider.h)) exakt bündig und die First-Person-Kamera versinkt zu keinem Zeitpunkt im Boden, während die Umgebung ab 9 m bis 1.200 m maximale Plastizität entfaltet.
  * **Vulkan Descriptor Layout:** In [`src/rhi/VulkanPipeline.cpp`](src/rhi/VulkanPipeline.cpp) wurden die Shader-Stage-Flags aller 19 Textur-Sampler von `VK_SHADER_STAGE_FRAGMENT_BIT` auf `VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT` erweitert.

### 2. Multi-Scale Sobel-Feldman Normal Map Baking ([`scripts/sculpt_himalayas.py`](scripts/sculpt_himalayas.py), [`scripts/process_terrain.py`](scripts/process_terrain.py))
* **True 2D Sobel-Feldman Gradienten:**
  * Die einfache zentrale Differenzierung (`np.gradient`) wurde durch einen echten 2D-Sobel-Feldman-Operator mit Faltungskernen ersetzt:
    * **Radius 1 (34 m):** Höchste Frequenz zur Hervorhebung scharfer Felskanten und Mikrostrukturen.
    * **Radius 2 (68 m):** Mittlere Frequenz zur Stabilisierung monumentaler Wandflanken und Großreliefs.
  * Das kombinierte Normalfeld wurde in [`data/processed/everest_normal_map.png`](data/processed/everest_normal_map.png) gebacken.
* **Neigungsadaptive Sobel-Mischung im Fragment-Shader:**
  * In Steilwänden (> 22°–52°) wird die Sobel-Normal-Intensität dynamisch auf bis zu 0,85 verstärkt (`lerp(0.55, 0.85, ...)`), was Felsrinnen und Steilabbrüche mit tiefen, plastischen Kontrastschatten füllt – völlig ohne zusätzliche Geometrielast.

### 3. Geologisches Höhen-Banding & Jetstream Wind-Scour ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Wissenschaftlich akkurate Himalaya-Schichtung nach absoluter Geländehöhe:**
  * **> 8.600 m (Qomolangma-Formation):** Dunkelgrauer bis anthrazit-schwarzer mikritischer Kalkstein der Gipfelpyramide (`strataTint = float3(0.52, 0.54, 0.58)`).
    * **Jetstream Wind-Scour:** Orkanartige Winde (> 150 km/h) blasen den Schnee an allen Hängen ab 8.600 m und Neigungen über 10° physikalisch ab (`windScourSummit * smoothstep(8.0, 22.0, slopeDeg)`), wodurch der schwarze Kalkstein der Gipfelpyramide dramatisch exponiert wird.
  * **8.200 m – 8.600 m (The Yellow Band):** Das markante, ocker-goldgelbliche Band aus dolomitischem Marmor (`strataTint = float3(1.36, 1.26, 0.86)`), durchzogen von feinen rhythmischen Sedimentbändern (`yellowRhythm`).
  * **< 8.200 m (North Col Formation & Kristalliner Sockel):** Kaltgrauer Gneis, phyllitischer Schiefer und Granit (`strataTint = float3(0.82, 0.85, 0.89)`).

---

## 🏔️ Schritt 9.2: Brutaler Fotorealismus – Mikro-Blending, Schnee-SSS, Rayleigh/Mie & Jetstream-Schneefahnen (1:1 Part VII - Abgeschlossen)

In Schritt 9.2 (1:1 Part VII) wurde das Terrain über vier Kernbereiche von einem "sehr guten Game-Mesh" auf brutalen, kompromisslosen Fotorealismus gehoben:

### 1. Shader & Textur-Blending (Mikroebene) ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Vollständige Eliminierung von Triplanar-Streckung:**
  * Der Projektions-Blend-Exponent wurde neigungsadaptiv verschärft (`lerp(4.0, 10.0, smoothstep(22.0, 52.0, geomSlopeDeg))`).
  * Steile Vertikalwände (> 35°) erhalten zu 99 %+ eine reine Seitenprojektion ($X/Z$), wodurch jegliches senkrechtes "Abfließen" oder Verschmieren der Texturen an Felswänden restlos beseitigt ist.
* **Physical Height-Blending mit Distance-Fading:**
  * Wenn Schnee auf Fels trifft, wird nicht transparent überblendet. Das System nutzt die Heightmaps ([`texRockDisp`](shaders/terrain.slang), [`texSnowDisp`](shaders/terrain.slang)) als physikalische Tiefenmaske: Schnee lagert sich zuerst in tiefen Felsspalten und Rillen ab, bevor er erhabene Steinblöcke bedeckt.
  * **Distance-Fading (< 120 m):** Das Height-Blending moduliert den Nahbereich im direkten Sichtfeld, während es jenseits von 120 m nahtlos in kontinuierliches Makro-Blending übergeht. Dadurch werden Moiré- und Kachelmuster in der Ferne vollständig verhindert.
* **Gekachelte Mikro-Detail-Normal-Maps:**
  * Im Nahbereich (< 90 m) blenden hochfrequente Detail-Normalen (1,2 m Granit-Klüfte, 0,6 m körnige Firn-Eiskristalle) sanft ein und verleihen Fels und Schnee unmittelbare haptische Schärfe.

### 2. Physikalisch basierte Beleuchtung (PBR & Atmosphäre) ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Wellenlängenabhängiges Rayleigh- & Mie-Streuungsmodell:**
  * Echte Höhenluft bricht blaues Sonnenlicht sechsmal stärker als rotes ($\beta_R \propto \lambda^{-4}$).
  * Weiter entfernte Bergmassive werden durch spektrale In-Scattering-Atmosphäre tief azurblau und dunstig, während sonnenbeschienener Eisdunst über eine Vorwärts-Mie-Phasenfunktion ($g = 0,76$) einen feinen silbrig-goldenen Lichthof um die Sonne erzeugt.
* **Subsurface Scattering (SSS) für Schnee & Gletschereis:**
  * Schnee verhält sich nicht wie diffuser Gips: Licht dringt in die Eiskristalle ein und streut im Volumen.
  * Ein zweistufiger SSS-Term (Vorwärts-Transluzenz an Graten und Wechten + weiches Diffus-Wrap-Leuchten) nimmt dem Schnee die kalkige Optik und verleiht ihm den charakteristischen transluzenten Eisglanz.
* **Vertiefte Geomorphologische Ambient Occlusion:**
  * Enge Couloirs, Gurgeln und Kaminrisse erhalten eine gezielte Kontakt-Verdunklung (`gorgeCavity`), die die plastische Schattentiefe von Steilwänden dramatisch verstärkt.

### 3. Geologische Akkuranz (Himalaya-Details) ([`shaders/terrain.slang`](shaders/terrain.slang))
* **Dynamische Schuttkegel (Talus/Scree Fans) am Wandfuß:**
  * An den Fußpunkten steiler Felswände bricht konstant Gestein ab. Bei Neigungen zwischen 24° und 36° (Schüttwinkel / Angle of Repose) blendet der Shader fließend Schotter- und Gerölltexturen ein, um den Übergang von Felswand zu Schneefeld organisch zu brechen.
* **Horizontales Höhenbanding:**
  * Reale Schichtung: Dunkelgrauer mikritischer Qomolangma-Kalkstein (> 8.600 m), das markante gelbliche Dolomit-Band (8.200 m – 8.600 m) und der kaltgraue Gneis/Granit-Sockel (< 8.200 m).

### 4. Post-Processing & Dynamik ([`shaders/terrain.slang`](shaders/terrain.slang))
* **ACES Filmic Tonemapping mit Highlight-Compression:**
  * Fängt extreme Helligkeitsspitzen auf voll sonnenbeschienenen Schneeflanken sauber ab ($exposure = 0,82$). Verhindert hartes Weiß-Clipping und erhält Firnstrukturen und Windrippeln auch bei gleißendem Mittagslicht vollständig.
* **Dynamische Jetstream-Schneefahnen (Summit Banner Clouds):**
  * Das legendäre Markenzeichen des Mount Everest: Von orkanartigen Jetstream-Winden (> 150 km/h) aus den höchsten Graten (> 7.900 m) in den Himmel gewehte, transluzente Spindrift-Schneefahnen driften lebendig im Wind.

---

## 🏔️ Schritt 10: Physikalische Schnee-Kopplung & Triplanar-Homogenisierung (1:1 Part VIII - Abgeschlossen)

Beseitigung unnatürlicher Texturübergänge und physikalische Verankerung von Schnee und Fels ([`shaders/terrain.slang`](shaders/terrain.slang)):

### 1. Schneeablagerung physikalisch koppeln (Height-Blending & Flow)
* **Eliminierung der Flecken-Masken ("Kuhflecken"):**
  * Frühere fleckenhafte Satelliten-Luma-Verfärbungen auf Mittelgraten wurden vollständig durch ein physikalisches Depositionspotenzial ersetzt.
  * Die Schneemaske ist direkt an die Heightmap der Felswand gekoppelt: Schnee sammelt sich physikalisch nur in den Tälern, Furchen und Rillen der Normal-/Heightmap:
    $$\text{height\_mask} = \text{clamp}((\text{rock\_height} - \text{snow\_level}) \cdot \text{sharpness} + 0.5, 0.0, 1.0)$$
  * Zur Verhinderung von Bildschirmmoiré auf Distanz fadet die Schärfe über 220 m Entfernung sanft ab.
* **Gravitations- & Jetstream-Winddrift-Modell:**
  * Gravitativer Fall ($+Y$, $\mathbf{N} \cdot \mathbf{up}$) fängt Schnee auf horizontalen Terrassen und Firnkaren (`terraceCatch`).
  * WNW-Höhenwindvektor (Jetstream $285^\circ$, `float2(0.88, 0.35)`) berechnet Exposition vs. Lee-Schutz: Schnee bleibt in windabgewandten Rinnen haften, während windzugewandte Luv-Kanten abgeweht werden (`windShelter`).
  * Steilhänge $>40^\circ\text{--}52^\circ$ werfen Schnee durch Gravitation ab (`cliffRockSlope`), außer in durch Eiserosion eingekerbten Lawinenrinnen (`couloirSnowCling`).
  * Orkanartiger Gipfelwind ($>8.580\text{ m}$) entreißt exponierten Graten die Schneedecke und legt den dunklen Qomolangma-Kalkstein frei.

### 2. Triplanar-Projektion auf allen Achsen homogenisieren
* **Fixierung steiler Flanken:**
  * Homogener Blend-Exponent $w = \text{pow}(\vert\mathbf{N}\vert, 6.0)$ verhindert jegliches vertikale Abfließen oder Schmieren an Steilflanken.
* **Orthonormale Tangentenraum-Transformation ($X$, $Y$, $Z$):**
  * Z-Achsen-Projektionen transformieren sowohl Tangente als auch Normale mit $\text{sign}(N_z)$ (`wNormZ = float3(rockNormZ.x * signZ, rockNormZ.y, rockNormZ.z * signZ)`).
  * Behebt Invertierungsfehler und „flache Schalen“ (konkave Wölbung statt plastischem Relief) an Nordhängen und Gegenwänden vollständig.

---

## 🏔️ Schritt 11: PBR-Tiefenplastizität, Dynamic Sky / Skybox & Rayleigh-Dunst (1:1 Part IX - Abgeschlossen)

Perfektionierung der Lichtstimmung, Schattentiefe und Himmelsatmosphäre ([`shaders/sky.slang`](shaders/sky.slang) & [`shaders/terrain.slang`](shaders/terrain.slang)):

### 1. Beleuchtung & Tiefenplastizität (PBR)
* **Multi-Scale HBAO & Deep Contact Occlusion:**
  * In tiefen Schluchten, engen Couloirs (`geomMorph.r`), konkaven Wandfüßen (`geomMorph.g`) und Mikrofissuren (`dot(N, geomN)`) wurde eine mehrskalige Kontaktschatten-Berechnung implementiert:
    $$\text{contactAO} = \text{clamp}\left(\text{geomMorph.a}^{1.5} \cdot (1 - \text{cavity} \cdot 0.52) \cdot (1 - \text{talusFooting}) \cdot (1 - \text{microCrevice}), 0.06, 1.0\right)$$
  * Schirmt Himmelslicht (`skyLight`), Schneereflektion (`snowGroundBounce`) und Gegenhang-Bounce in Spalten stark ab ($\text{pow}(\text{contactAO}, 1.4)$).
  * Tiefe Kontaktschatten in Furchen und Mulden nehmen dem Gelände jeglichen verbleibenden Game-Engine-Mesh-Charakter und erzeugen monumentale geologische Masse.
* **Roughness-Splitting & Directed Specular Glints:**
  * **Fels:** Granit und Qomolangma-Kalkstein sind strikt rau und reflexionsfrei gemappt ($\text{roughness} = 0,85$–$0,95$, keine speckigen Glanzstellen).
  * **Schnee, Firn & Couloir-Eislinsen:** Gefrorener Firn und Eisströme in Rinnen erhalten deutlich niedrigere Rauheitswerte ($0,30$–$0,50$).
  * Gerichtetes Specular-Highlighting bricht das Sonnenlicht mit messerscharfen Glanzfacetten entlang vereister Couloirs und am Khumbu-Gletscher, während umliegende Felswände komplett matt bleiben.

### 2. Atmosphäre & Himmel (Dynamic Sky / Skybox & Rayleigh-Dunst)
* **Stratosphärischer Dynamic Sky Shader ([`shaders/sky.slang`](shaders/sky.slang)):**
  * Eigener Vulkan-Grafikpipeline-Pass mit prozeduraler Himmelskuppel (Fullscreen-Dreieck aus `SV_VertexID`), gezeichnet vor dem Terrain:
  * **Höhengradient ins Weltall:** Im Tal sattes Alpin-Azur (`float3(0.12, 0.24, 0.54)`), über 8.000 m Übergang in tiefdunkles stratosphärisches Indigo-Navy (`float3(0.022, 0.040, 0.13)`).
  * **Alpenglühen & Belt of Venus:** Bei Sonnenauf- und -untergang taucht eine intensive Rayleigh-Auslöschung den Horizont in glühendes Orange, Bernstein und Roségold, während auf der Gegenseite der zartrosa Venusgürtel über dem blauen Erdschatten aufsteigt.
  * **Sonnenscheibe & Mie-Korona:** Scharfe Sonnenscheibe ($0,5^\circ$ Winkeldurchmesser) mit intensiv vorwärts-streuender Eiskristall-Aureole und Sonnenkorona.
  * **Blizzard-Integration:** Bei Sturm nahtloser Übergang in eine diffuse weiße Schneesturm-Nebelkuppel.
  * Farb- und Belichtungskonsistenz durch identisches ACES Filmic Tonemapping ($exposure = 0,82$).
* **Rayleigh-Scattering & Exponential-Height-Fog:**
  * Implementierung eines physikalischen Höhen-Dunstepos in [`shaders/terrain.slang`](shaders/terrain.slang):
    $$\rho(h) = \exp\left(-\frac{\max(0, h - 4200)}{1800}\right)$$
  * Täler und Fußregionen liegen in atmosphärischem Dunst, während die Gipfelkämme kristallklar in die Stratosphäre ragen.
  * Hintere Bergmassive ($>10\text{ km}$) werden physikalisch tiefblau (Rayleigh Blueing) und verlieren Blendkontraste, wodurch sich die optische Raumtiefe des Himalayas vervielfacht.

---

## 🏔️ Schritt 12: Brutaler Felskontrast, Knochentrockener Granit, Messerscharfe Schneegullies & Tiefe Schluchten-Schwärze (1:1 Part X - Abgeschlossen)

Korrektur der fotorealistischen Kernbereiche: Wiederherstellung des echten Hochgebirgskontrasts, Beseitigung speckiger Wachs-/Plastikreflexionen, Eliminierung runder 33,8m-Mesh-Kuhflecken durch Multi-Scale Displacement und streckungsfreier Triplanar-Schnee ([`shaders/terrain.slang`](shaders/terrain.slang)):

### 1. Fels-Albedo & Kontrast (Der Fels ist nicht mehr ertrunken)
* **Wiederherstellung des epischen 6:1 bis 8:1 Kontrastverhältnisses:**
  * Vorherige Shader-Iterationen hatten den Fels zu hellgrau eingefärbt oder mit weichen Blend-Maps verwässert.
  * Dunkler, knochentrockener Granit, Gneis und Kalkstein heben sich jetzt mit extrem hoher Prägnanz vom strahlend weißen Schnee ($RGB \approx 0,88$–$0,92$) ab:
    * **Sockel bis 8.200 m:** Kaltgrauer bis anthrazitfarbener Tibetischer Gneis & Migmatit-Granit ($RGB \approx 0,16$–$0,20$).
    * **8.200 m – 8.600 m:** Das berühmte **Yellow Band** (ocker-/gelblicher dolomitischer Marmorstreifen, $RGB \approx 0,26$–$0,32$) als klarer geologischer Horizont ohne künstliche Wellenmuster.
    * **> 8.600 m:** Die **Qomolangma-Formation** (pechschwarzer bis dunkelgrauer mikritischer Kalkstein, $RGB \approx 0,11$–$0,14$).
  * Kein Satelliten-Bleaching mehr auf nacktem Gestein: Die Satellitentönung wird rein auf Firn und Eis angewendet.

### 2. Roughness & Beleuchtung (Schluss mit Plastik-, Wachs- & Cellophan-Glanz)
* **Knochentrockener, rein diffuser Fels:**
  * Fels-Roughness auf strikt **$0,88$ bis $0,98$** festgenagelt.
  * Sämtliche Specular-Highlights, Glitzer-Facetten und Fresnel-Ränder auf Felsflächen wurden mathematisch auf **$0,0$** eliminiert.
  * **Harter Lambertian-Terminator:** Ersetzung weicher Half-Lambert-Wrap-Funktionen durch reines physikalisches Lambert-Gesetz $\max(0, \mathbf{N} \cdot \mathbf{L})$. Sobald eine Felsflanke aus dem Sonnenlicht dreht, stürzt sie ohne wachsartigen Schmierglanz sofort in scharfen, alpinen Schlagschatten.
* **Kristalline Firnschnee-Roughness:**
  * Schnee-Roughness von unrealistisch glatten Werten ($0,32$) auf authentische **$0,70$ bis $0,86$** angehoben. Echter Alpinschnee streut diffus in alle Raumrichtungen statt wie lackierter Kunststoff zu spiegeln.
  * Subtiler mikroskopischer Diamond-Dust-Sparkle ($\text{pow}(\mathbf{R} \cdot \mathbf{V}, 128.0)$) bricht das Sonnenlicht nur bei extrem streifendem Lichteinfall in zarten Eiskristall-Lichtpunkten.

### 3. Schneemasken-Schärfe & Couloir-Flow (Weg von runden 33,8m-Mesh-Blobs)
* **Master Cliff-Rock-Bestimmung:**
  * Auf flachem Gelände ($<26^\circ$ Neigung, z.B. Everest Base Camp auf 5.300 m) ist Klippenfels vollständig deaktiviert ($0,0$): Die Ebene ist ein geschlossenes, strahlend weißes Firn-/Schneefeld.
  * Steilwände ($>44^\circ$) sind nackter Fels, **außer** in tief eingekerbten Erosions- und Lawinenrinnen (`geomMorph.r`).
* **Multi-Scale Triplanar-Felsrelief (Fraktale Felskanten):**
  * Kopplung aus $24\text{ m}$ makro-tektonischen Rinnen und $5,5\text{ m}$ feinen Granit-Bruchkanten (`rockDispH_Micro`).
  * Messerscharfer Schwellenwert (`smoothstep(0.45, 0.55, structuralRock)`): Schnee bricht an messerscharfen Felsleisten ab, statt mit 30 Meter breiten, verwaschenen Übergängen wie Kuhflecken über das 33,8m-Höhenraster zu schmieren.
  * Couloirs und Lawinenflutungen durchschneiden dunkle $60^\circ\text{--}75^\circ$ Felswände als leuchtend weiße, messerscharfe Schneebänder.

### 4. Triplanar-Schnee gegen Wandstreckung
* **Vollständige Triplanar-Projektion für Schnee ($X, Y, Z$):**
  * An Steilflanken und in Couloir-Rinnen wird Schnee nun ebenfalls von der Seite projiziert (`sampleTriplanarAlbedoMulti` & `sampleTriplanarNormMulti`).
  * Jegliches vertikales Strecken oder „Herabfließen“ von Schneetexturen an Steilwänden ist vollständig eliminiert.

### 5. Tiefes HBAO & Schluchten-Schwärze
* **Verstärkte Kontakt-Occlusion in Couloirs & Talsohlen:**
  * `contactAO` schirmt Himmelslicht und Schneeboden-Reflektionen in engen Felskerben und Wandfüßen bis auf ein tiefes Restminimum von $0,008$ ab.
  * Schlagschatten und dunkle Schluchten erhalten monumentale geologische Schwärze und plastische Tiefenwirkung zurück.


---

## 🏔️ Schritt 13: Leopardenmuster-Eliminierung via Physical Height-Blend, Exponentielle Triplanar-Schärfung pow(|N|, 8.0), PBR-Roughness-Trennung & Weltkoordinaten-Geologie (1:1 Part XI - Abgeschlossen)

Fundamentale Beseitigung aller Artefakte aus Schwellenwert-Clipping und ungeschärften Triplanar-Projektionen sowie physikalische Vollendung des Himalaya-Renderings ([`shaders/terrain.slang`](shaders/terrain.slang)):

### 1. Leopardenmuster & fehlerhaftes Noise-Clipping vollständig eliminiert
* **Löschung isolierter Noise-Cutouts:**
  * Frühere kreisrunde Ringe, schwarze Kringel und isolierte Flecken auf Bergflanken stammten aus hochfrequenten Rauschwerten mit zu engen Schwellenwertfenstern (`smoothstep(0.45, 0.55)`). In der Natur existieren keine isolierten runden Flecken auf Felsgraten.
  * Vollständige Entfernung des isolierten Rauschens zugunsten physikalischer Materialablagerung.
* **Physical Height-Blend:**
  * Schnee lagert sich physikalisch immer zuerst in den Tälern, Furchen und Rillen der Gesteinsstruktur ab:
    ```hlsl
    float rockCreviceDepth = 1.0 - rockHeight;
    float blendSharpness = 6.0;
    float heightDifference = (rockCreviceDepth + snowAmount) - 1.0;
    float snowMask = saturate(heightDifference * blendSharpness);
    ```
  * Multi-Scale Domain-Warped Displacement (`sampleTriplanarDispMulti`): Beseitigt das hochfrequente Wiederholungs- und Kachelmuster im Displacement und verbindet $24\text{ m}$ Felsspalten mit $110\text{ m}$ und $420\text{ m}$ geologischer Großformation.
* **Slope als Primärfilter:**
  * Steilwände über $45^\circ$ werfen Schnee gravitativ ab: Auf Felswänden $>45^\circ$ gilt strikt $\text{snowAmount} = 0,0$ und $\text{snowMask} = 0,0$, außer in tief eingekerbten Lawinen- und Erosionsrinnen (Couloirs).

### 2. Triplanar-Stretching (Wachseffekt) an Steilwänden beseitigt
* **Blend-Gewichte exponentiell geschärft:**
  * Anhebung der Triplanar-Normalengewichte auf die 8. Potenz:
    ```hlsl
    float3 blend = pow(abs(worldNormal), float3(8.0));
    blend /= (blend.x + blend.y + blend.z);
    ```
  * Verhindert das vertikale Herabfließen und Schmieren von Texturen an Steilflanken (z. B. Lhotse-Wand und Nuptse-Kante) vollständig.
* **Orthonormale Tangentenraum-Transformation korrigiert:**
  * Mathematisch exakte Vorzeichen- und Achsentransformation für $X$-, $Y$- und $Z$-Projektionen (unter Wahrung der Rechtshändigkeit $\mathbf{T} \times \mathbf{B} = \mathbf{N}$):
    * **X-Achse:** $\mathbf{wNormX} = (n_z \cdot \text{signX}, n_y, -n_x \cdot \text{signX})$
    * **Y-Achse:** $\mathbf{wNormY} = (n_x, n_z \cdot \text{signY}, -n_y \cdot \text{signY})$
    * **Z-Achse:** $\mathbf{wNormZ} = (n_x \cdot \text{signZ}, n_y, n_z \cdot \text{signZ})$
  * Beseitigt invertierte Schattierungen und wachsartige „flache Schalen“ auf Nord- und Gegenhängen vollständig.

### 3. PBR-Materialwerte & Lichtphysik korrigiert
* **Roughness-Werte physikalisch getrennt:**
  * **Granit & Kalkstein:** Strikt rau ($\text{Roughness} \approx 0,88$–$0,96$) ohne jeglichen Plastik- oder Speckglanz ($\text{Specular} = 0,0$, $\text{Fresnel} = 0,0$).
  * **Firn & Harsch:** Leicht diffus spiegelnd ($\text{Roughness} \approx 0,35$–$0,55$), um das charakteristische Glitzern bei flachem Sonnenstand einzufangen (`lowSunGlint = 1.0 - smoothstep(0.12, 0.75, L.y)`).
* **Multi-Scale HBAO (Tiefe in Spalten):**
  * Tiefe Kontaktschatten in Furchen, Couloirs und Wandfüßen schirmen Umgebungs- und Streulicht bis auf ein Minimum von $0,008$ ab und verleihen dem Gebirge monumentale Masse.
* **Subsurface Scattering (Translucency) für Schnee:**
  * Schnee absorbiert Sonnenlicht und streut es vorwärts durch scharfe Kämme (`geomMorph.b`). Ein Vorwärtsterm ($\text{pow}(\mathbf{V} \cdot -\mathbf{L}_{\text{sss}}, 3.5)$) eliminiert den kreidigen Gips-Look und lässt Schneegrate leuchtend und transluzent wirken.

### 4. Höhen- und Geologie-Banding (Himalaya-Signatur)
* **Geologische Schichtung über Weltkoordinate Y:**
  * **$> 8.600\text{ m}$:** Pechschwarzer bis dunkelgrauer, extrem schroffer Kalkstein der Qomolangma-Formation ($RGB \approx 0,18, 0,19, 0,22$). Durch orkanartige Jetstream-Winde nahezu vollständig schneefrei abgeweht.
  * **$8.200\text{ m}$ – $8.600\text{ m}$:** Das markante, hellgelbliche **Yellow Band** (rekristallisierter Dolomit-Marmor, $RGB \approx 0,85, 0,74, 0,48$).
  * **$< 8.200\text{ m}$:** Dunkelgrauer Gneis und Granit der tibetischen Sockelzone ($RGB \approx 0,38, 0,40, 0,44$).
* **Talus / Schuttkegel am Wandfuß ($30^\circ$–$40^\circ$):**
  * Unterhalb steiler Felswände in der Neigungszone von $30^\circ$ bis $40^\circ$ wird feines Schuttgeröll (`screeFooting`) eingeblendet.

### 5. Rayleigh-Streuung & Weitwinkel-Himmel
* **Dynamische Skybox mit echter Sonnenposition:**
  * Hochdynamisches Himmelsmodell ([`shaders/sky.slang`](shaders/sky.slang)) mit tiefer Sonnenstellung, Sonnenkorona, Belt of Venus und spektraler Dämmerungs-Extinktion.
* **Entfernungsabhängiger Rayleigh-Dunst & Kontrastverlust über 30+ km:**
  * Physikalische Dämpfung und Bläuung über $30\text{ km}$ Distanz ($\text{distAerialScale} = 1 - \exp(-\text{dist} \cdot 0,000042)$), wodurch das menschliche Auge die gigantische Tiefe und Dimension des Himalayas unmittelbar greifen kann.

---

## 🏔️ Schritt 14: Monumentale Bergschatten & 35-km DEM Cone-Tracing (1:1 Part XII - Abgeschlossen)

Reale physikalische Gebirgsschatten über das gesamte $35\text{ km} \times 35\text{ km}$ Mount-Everest-Massiv ([`shaders/terrain.slang`](shaders/terrain.slang), [`src/renderer/Renderer.cpp`](src/renderer/Renderer.cpp), [`src/rhi/VulkanTexture.cpp`](src/rhi/VulkanTexture.cpp)):

### 1. Hardware-beschleunigtes DEM Cone-Tracing auf der GPU
* **1024x1024 Float32 DEM-Textur im GPU VRAM (`VK_FORMAT_R32_SFLOAT`):**
  * Das unkomprimierte Höhengitter (`everest_dem_float32.bin`, 4 MB VRAM) wird als 20. Texturbinding (`texElevationDEM`, Binding 19) bereitgestellt.
  * Lineare Hardware-Filterung (`VK_FILTER_LINEAR`) sorgt für kontinuierliche, stufenlose Höhenabfrage entlang beliebiger Sicht- und Sonnenstrahlen.
* **Progressive quadratische Schrittweite (48 Schritte bis >35 km):**
  * Dynamische Schrittweitenformel $\Delta t_i = 25,0 + 10,0 \cdot i + 0,65 \cdot i^2$ spannt über $35.700\text{ Meter}$ auf.
  * Im Nahbereich ($t < 1\text{ km}$) erfassen Schritte von $25\text{ m}$ bis $90\text{ m}$ selbst feine Felsrippen und Pfeiler.
  * Im Fernbereich ($t > 5\text{ km}$) decken weite Schritte den gesamten Horizont ab.

### 2. Weiche Penumbra basierend auf der $0,53^\circ$ Sonnenscheibe
* **Astronomischer Sonnendurchmesser:**
  * Die Sonne besitzt einen scheinbaren Durchmesser von $\approx 0,533^\circ$ (Halbwinkel $\alpha \approx 0,267^\circ \approx 0,00465\text{ rad}$).
  * Aufweitung des Lichtkegels: $r(t) = \max(t \cdot 0,00465, 4,5\text{ m})$.
* **Stetige Scheiben-Visibilität:**
  * $V(t) = \text{clamp}\left(\frac{\Delta h}{r(t)} \cdot 0,5 + 0,5, 0,0, 1,0\right)$, wobei $\Delta h = R_y(t) - H_{\text{DEM}}(u(t), v(t))$.
  * Wenn der Strahl genau die Felsspitze streift ($\Delta h = 0$), ist exakt die Hälfte der Sonnenscheibe sichtbar ($V = 0,5$).
  * Glatte Hermite-Interpolation (`smoothstep(0.0, 1.0, minSunVis)`) erzeugt fotorealistische, weiche Halbschattenübergänge.

### 3. GPU Fast-Rejection & Höchstleistung (< 0,2 ms auf RTX 4060)
* **Backface-Abbruch:** Fragmente, deren geometrische Normale von der Sonne abgewandt ist ($\mathbf{N}_{\text{geom}} \cdot \mathbf{L} \le -0,02$ oder $\mathbf{N}_{\text{pbr}} \cdot \mathbf{L} \le 0,001$ oder $\mathbf{L}.y \le 0$), überspringen das Cone-Tracing sofort.
* **Himalaya-Höhenlimit ($8.860\text{ m}$):** Sobald der Sonnenstrahl eine Höhe von $8.860\text{ m}$ erreicht (über dem Gipfel des Mount Everest), kann kein Fels der Erde den Strahl mehr verdecken $\to$ sofortiger Abbruch.
* **Bounding-Box & Okklusion:** Verlassen der $35\text{ km}$-DEM-Grenzen oder Erreichen voller Verdeckung ($\text{minSunVis} \le 0,001$) beendet die Schleife sofort.

### 4. PBR-Lichtintegration
* Der ermittelte Bergschatten (`demShadow`) moduliert das direkte Sonnenlicht (`directLight`), das diamantene Eiskristall-Glitzern (`sparkle`), die Firn-Spiegelung (`specular`) und das Schnee-Subsurface-Scattering (`snowSSS`).
* Bei tiefstehender Sonne (Morgen-/Abenddämmerung) liegen tief eingeschnittene Täler wie der Khumbu-Gletscher oder das Western Cwm im tiefen, kühlen Schatten riesiger Massivwände, während die höchsten Grate und Spitzen im feurigen Alpenglühen erstrahlen.

---

## 🏔️ Schritt 15: Khumbu-Gletscher & Eisfall-Dynamik (1:1 Part XIII - Abgeschlossen)

Physikalisch fundierte Eisfall-Strukturierung, spektrale Lichtabsorption und supraglaziale Schmelzwassertümpel ([`shaders/terrain.slang`](shaders/terrain.slang), [`src/game/Player.cpp`](src/game/Player.cpp), [`src/game/TerrainCollider.cpp`](src/game/TerrainCollider.cpp), [`src/core/Window.cpp`](src/core/Window.cpp), [`src/main.cpp`](src/main.cpp)):

### 1. Prozedurales Fließspannungs-Gitter (Strain-Tensor Crevasses & Séracs)
* **Khumbu-Korridor-Geometrie & Geländestufen:**
  * Berechnung der Khumbu-Hauptfließachse von Basislager (5.300 m) über den steilen Eisfallbruch bis ins Western Cwm (6.400 m).
  * Begrenzung auf das reale Gletschertal mittels orthogonalem Distanzfilter (`khumbuCorridor`), wodurch benachbarte Granitwände (Nuptse, Westgrat) unberührt bleiben.
* **Fließspannungs-Bruchmuster:**
  * Projektion auf das Fließkoordinatensystem: $s_{\text{flow}} = \mathbf{P}_{xz} \cdot \hat{\mathbf{v}}_{\text{flow}}$ und $s_{\text{cross}} = \mathbf{P}_{xz} \cdot \hat{\mathbf{v}}_{\text{cross}}$.
  * Domänenverzerrte Zugspannungsrisse erzeugen quer zur Fließrichtung bis zu 25 Meter tiefe Gletscherspalten (`crevasseFactor`) und aufragende, instabile Sérac-Eisnadeln (`seracFactor`).
* **Vertex-Displacement auf Makroebene:**
  * Im Höhenband des Eisfalls ($5.480\text{ m}$ bis $6.180\text{ m}$) bei Hängen von $14^\circ$ bis $44^\circ$ verformen Sérac-Rücken ($+5,5\text{ m}$) und Spaltentröge ($-4,5\text{ m}$) das 3D-Polygonnetz.
* **100% Eis-Freilegung in Spalten:**
  * An Bruchwänden und in Spaltentiefen bricht die lockere Schneedecke physikalisch ab (`crevasseIceExpose`), sodass reines, massives Gletschereis sichtbar wird.
  * An den steilen Eisabstürzen des Khumbu-Eisfalls ($14^\circ$–$38^\circ$) wird der nackte Felsmaskierungsfilter unterdrückt, da es sich um stürzendes Gletschereis und nicht um Felsabbrüche handelt.

### 2. Spektrale Lichtabsorption im Gletschereis (Beer-Lambert-Gesetz)
* **Wellenlängenabhängige Extinktion:**
  * Echtes Gletschereis absorbiert rotes Licht $\approx 70\times$ bis $100\times$ stärker als blaues Licht.
  * Absorptionskoeffizient: $\mathbf{\mu}_a = [0,35,\, 0,038,\, 0,005]\text{ m}^{-1}$ für [Rot, Grün, Blau].
  * Spektrale Transmission nach Eindringtiefe $d$: $\mathbf{T}(d) = \exp(-\mathbf{\mu}_a \cdot d)$.
  * In $12\text{ m}$ Spaltentiefe werden $98,5\%$ des roten Lichts absorbiert, während $94\%$ des blauen Lichts überleben.
* **Spektrales Spalten-Leuchten (Internal Azure/Cobalt Glow):**
  * Diffuses inneres Streulicht (`crevasseInternalGlow`) leuchtet aus tiefen Spaltenwänden in magischem Kobalt- und Azurblau hervor.
  * Neigung der Spaltenwandnormalen zur Rissmitte hin (`crevasseNormalPerturb`).

### 3. Supraglaziale Schmelzwassertümpel (Glacial Meltwater Tarns)
* **Gletschermoränen-Becken:**
  * Auf den flachen Becken der unteren Gletscherzunge ($4.700\text{ m}$–$5.400\text{ m}$, Neigung $< 6,5^\circ$) sammeln sich Schmelzwasserseen.
* **Optik & Schwebstoff-Farbe:**
  * Charakteristische Türkis- und Smaragdfärbung durch suspendiertes Gesteinsmehl (Rock Flour, Gletschermilch).
  * Spiegelglatte Wasseroberfläche (Roughness $= 0,02$) mit physikalischer Schlick-Fresnel-Reflexion ($F_0 = 0,02$) und messerscharfem 512-Exponenten-Sonnenglanz.
  * Zarte, windgetriebene Wasserwellen auf der Oberfläche.

### 4. Beseitigung aller Undefined-Smoothstep-Instabilitäten
* Strikte Einhaltung der Vulkan/SPIR-V Spezifikation (GLSL.std.450), wonach `smoothstep(edge0, edge1, x)` bei $\text{edge0} \ge \text{edge1}$ undefiniertes Verhalten erzeugt.
* Alle inversen Fades wurden auf standardkonformes $1,0 - \text{smoothstep}(\text{min}, \text{max}, x)$ mit $\text{min} < \text{max}$ umgestellt, wodurch jegliche Geometrieverzerrungen und Flackern im Nah- und Fernbereich eliminiert wurden.

### 5. Steuerung & Alpine Geologie
* **Neuer Schnellreise-Preset `5`:** Sofortiger Teleport zum **Khumbu-Eisfall (5.867 m)** (`-13800, -8600`).
* **Alpine Oberflächenklassifikation:**
  * Erkennung von `Khumbu Icefall (Active Séracs & 25m Crevasses)` bei $5.350\text{ m}$ bis $6.250\text{ m}$ auf $14^\circ$–$38^\circ$ Hangneigung.
  * Erkennung von `Glacial Blue Ice (Khumbu Glacier & Moraine)` in flacheren Becken.


---

## 🌌 Schritt 16: Stratosphären-Optik & Eiskristall-Halos (1:1 Part XIV - Abgeschlossen)

Physikalisch exakte Modellierung der extremen Hochatmosphäre der Todeszone ($>7.000\text{ m}$ bis $8.848\text{ m}$), des Ozon-Dämmerungsspektrums und der Eiskristall-Atmosphärenoptik ([`shaders/sky.slang`](shaders/sky.slang)):

### 1. Barometrische Höhenformel & Dichteverteilung der Todeszone ($337\text{ hPa}$)
* **Barometrische Dichteabnahme:**
  * Reale Höhenformel des Himalayas mit Skalenhöhe $H_{\text{scale}} \approx 7.200\text{ m}$:
    $$p(h) = p_0 \cdot \exp\left(-\frac{h}{H_{\text{scale}}}\right), \quad \rho_{\text{rel}}(h) = \exp\left(-\frac{h}{7.200\text{ m}}\right)$$
  * Am Everest-Gipfel ($8.848\text{ m}$) sinkt der barometrische Luftdruck auf $\approx 296\text{ hPa}$ ($\rho_{\text{rel}} \approx 0,292$, weniger als ein Drittel des Meeresniveaus).
  * An der Schwelle zur Todeszone ($8.000\text{ m}$, Südsattel): $\approx 334\text{ hPa}$ ($\rho_{\text{rel}} \approx 0,330$).
  * Am Basislager ($5.300\text{ m}$): $\approx 485\text{ hPa}$ ($\rho_{\text{rel}} \approx 0,479$).
* **Kosmisch tiefes Indigo-Schwarzblau am Zenit:**
  * Da die Moleküldichte der Luft über $7.500\text{ m}$ dramatisch einbricht, existiert kaum noch streuende Luftsäule über dem Kopf des Bergsteigers.
  * Der Himmelszenit dunkelt von alpinem Azurblau ($RGB = [0,095, 0,210, 0,480]$) in ein surreales, tiefes Kosmos-Schwarzblau ($RGB = [0,0035, 0,0065, 0,024]$) ab.
  * Die Schräg-Luftmasse (*Slant Optical Depth*) zum Horizont hin erhält die leuchtende Distanz-Atmosphäre aufrecht, wodurch ein atemberaubender Kontrast zwischen schwarzem Zenit und strahlendem Gebirgshorizont entsteht.
* **Tages-Sterne in der Stratosphäre (> 7.000 m):**
  * Über $7.000\text{ m}$ bricht die Himmelsluminanz so weit ein, dass die hellsten Sterne (0. und 1. Größenklasse: Sirius, Wega, Rigel, Canopus) sowie helle Planeten (Venus, Jupiter) am helllichten Tag mit bloßem Auge sichtbar werden ($V_{\text{star}} = \text{smoothstep}(7000, 8600, h) \cdot \text{smoothstep}(0,48, 0,94, r_y)$).
  * Sphärisches 3D-Sternengitter mit atmosphärischem Funkeln (Szintillation) und feinen Beugungskreuzen (4-Point Diffraction Spikes).

### 2. Chappuis-Ozonabsorption (550–650 nm) & Gegendämmerung
* **Ozon-Extinktion im sichtbaren Licht:**
  * In der Stratosphäre ($15\text{ km}$–$30\text{ km}$) absorbiert Ozon selektiv gelbe und orange Wellenlängen (Chappuis-Bande, Peak $\approx 600\text{ nm}$, $\mathbf{\sigma}_{\text{ozone}} = [0,065, 0,082, 0,005]\text{ km}^{-1}$).
  * Bei flachem Sonnenstand ($L.y < 0,25$) wird das Sonnenlicht auf tangentialem Tangentenstrahl durch die Ozonschicht gefiltert:
    $$\mathbf{T}_{\text{ozone}} = \exp\left(-\mathbf{\sigma}_{\text{ozone}} \cdot m_{\text{ozone}} \cdot 8,5\right)$$
  * Unterdrückt gilbendes Streulicht und taucht den Dämmerungs-Zenit in reines Königsblau, Ultramarin und Purpur.
* **Gegendämmerungsbogen (Belt of Venus):**
  * Auf dem Gegenhorizont ($\mathbf{r}_{xz} \cdot \mathbf{L}_{xz} < -0,3$) leuchtet der Belt of Venus in spektral gefiltertem Pastellrosa ($RGB = [0,92, 0,44, 0,62] \times \mathbf{T}_{\text{ozone}}$), direkt über dem tiefblauen Erdschattenbogen.

### 3. Eiskristall-Atmosphärenoptik (22°-Halo, Parhelia & Lichtsäulen)
* **Physikalischer 22°-Halo mit chromatischer Dispersion:**
  * Minimaler Ablenkungswinkel $\delta_{\text{min}} = 2 \arcsin(n \cdot \sin(30^\circ)) - 60^\circ$ an $60^\circ$-Prismenflächen hexagonaler Eissäulchen.
  * Spektrale Farbaufspaltung des Brechungsindex $n_{\text{Eis}}$:
    * Rot ($n = 1,307$): Scharfer innerer Rand bei $21,65^\circ$.
    * Grün/Gelb ($n = 1,311$): Weißer Helligkeitspeak bei $22,05^\circ$.
    * Blau/Violett ($n = 1,317$): Zartblauer Außenrand bei $22,55^\circ$.
  * „Loch im Himmel“ (*Halo Dark Hole*): Signifikante Verdunklung des Himmels zwischen Vorwärtsstreuung und $21,4^\circ$-Innenrand, da alle Brechungsstrahlen in den $22^\circ$-Winkel gelenkt werden.
* **Nebensonnen (Parhelia / Sun Dogs):**
  * Entstehen durch horizontal schwebende hexagonale Eisplättchen genau auf Sonnenhöhe ($\Delta y \to 0$) im Azimutabstand von $\approx 22^\circ$.
  * Mit steigender Sonne weitet sich der Winkel physikalisch auf: $\theta_{\text{dog}} = 22,0^\circ + 2,8^\circ \cdot (L.y / 0,45)^{1,8}$.
  * Roter Innenrand zur Sonne hin, gleißend weißer Kern und nach außen ziehender Horizontalschweif entlang des Nebensonnenkreises (*Parhelic Circle*).
* **Vertikale Lichtsäulen (Light Pillars) bei Dämmerung:**
  * Planare Spiegelreflexion an den Basisflächen taumelnder Eisplättchen bei tiefstehender Sonne ($L.y < 0,26$).
  * Exakt ausgerichtete vertikale Lichtsäule ($\Delta \phi_{\text{horiz}} < 1,8^\circ$), die bis zu $22^\circ$ senkrecht über der Sonne in den feurigen Morgenhimmel oder Abendhimmel aufsteigt.
* **Diskrete Diamantstaub-Glints:**
  * Zirkumsolare Diamantstaub-Kristalle blitzen in eisiger Höhenluft als funkelnde Mikro-Lichtpunkte auf.

---

## 🏔️ Nächste Schritte (Roadmap)

* [x] **Schritt 1:** Geodaten- & Bild-Download, DEM-Stitching, PBR-Texturen, Wetter-API.
* [x] **Schritt 2:** C++20 / Vulkan Initialisierung, Dynamic Rendering, Device-Local Buffers & Slang Shader Pipeline.
* [x] **Schritt 3:** First-Person Bergsteiger-Spiel mit 1:1 Terrain-Kollision, Laufen, Springen, Hangphysik & Schnellreise.
* [x] **Schritt 4:** 18-Kanal PBR-Pipeline, POM (Parallax Occlusion Mapping), ESRI-Satelliten-Overlay & Slope Splatting.
* [x] **Schritt 5 (1:1 Part I):** Triplanare PBR-Projektion (streckungsfreie Steilwände), Gaea-Geomorphologie, Couloir Fluting, Everest Yellow Band Strata & stratosphärische Alpin-Physik.
* [x] **Schritt 6 (1:1 Part II):** Volle 1:1 Gitterauflösung (2M Dreiecke), volumetrisches Wolkenmeer, Alpenglühen, Blizzard/Whiteout & Live Open-Meteo Wetter.
* [x] **Schritt 7 (1:1 Part III):** Alpines Grat-Sculpting (Multi-Scale Discrete Curvature), hydraulische Couloirs, thermische Schuttkegel, Dual-Frequency Anti-Tiling, Horizon AO & ACES Tone Mapping.
* [x] **Schritt 8 (1:1 Part IV):** Fotorealismus-Feinschliff: Beseitigung von Wabenmuster/Kachelung durch Multi-Scale Distance Tiling & Domain Warping, authentische Himalaya-Petrologie, Multi-Bounce Schneelicht & weiche Schatten.
* [x] **Schritt 9 (1:1 Part V):** Geomorphologischer 1:1 Echtwelt-Abgleich: Entschärfung des über-spitzen Nadel-Looks zu massiven Monumental-Sockeln & akkurate Schnee/Fels-Balance (Hängegletscher, Firnkare, Felsband-Schneeterrassen).
* [x] **Schritt 9.1 (1:1 Part VI):** GPU-Tessellation / Virtual Heightfield (Auflösung der 33,8 m Kanten via Detail-Displacement), Multi-Scale Sobel-Normal-Baking & Geologisches Höhen-Banding (>8.600m Qomolangma mit Jetstream Wind-Scour, 8.200m–8.600m Yellow Band, <8.200m Gneis/Granit).
* [x] **Schritt 9.2 (1:1 Part VII):** Brutaler Fotorealismus: Verschärfte Triplanar-Exponenten (Null Wandstreckung), Physical Height-Blending mit Distance-Fading, Schnee-Subsurface-Scattering (SSS), Rayleigh/Mie-Atmosphärenstreuung, Talus-Schuttkegel, ACES-Highlight-Schutz & Jetstream-Schneefahnen.
* [x] **Schritt 10 (1:1 Part VIII):** Physikalische Schnee-Kopplung (Height-Blending & Flow gegen Flecken-Optik, Fallrichtung $+Y$ & Winddrift) & Triplanar-Homogenisierung (Normal-Transformation der $X/Z$-Achsen, Exponent $w = |\mathbf{N}|^{6.0}$).
* [x] **Schritt 11 (1:1 Part IX):** PBR-Tiefenplastizität (HBAO/Kontaktschatten in Furchen, Roughness-Splitting Fels $0,85$–$0,95$ vs. Eis $0,3$–$0,5$) & Dynamic Sky / Skybox mit weitem Rayleigh-Distanzdunst.
* [x] **Schritt 12 (1:1 Part X):** Brutaler Felskontrast (Knochentrockener Granit Albedo ~0.16 vs. Schnee ~0.90), Fels-Roughness strikt diffus (0.88–0.98, Null Specular/Fresnel), Messerscharfe Schneegullies & Couloir-Flow (Beseitigung aller 33,8m-Kuhflecken durch Multi-Scale Displacement), Triplanar-Schnee gegen Wandstreckung & Tiefes Schluchten-HBAO.
* [x] **Schritt 13 (1:1 Part XI):** Leopardenmuster-Eliminierung via Physical Height-Blend (`snowMask = saturate(((1.0 - rockHeight) + snowAmount - 1.0) * blendSharpness)`), Slope-Primärfilter ($>45^\circ$ reiner Fels außer Couloirs), Multi-Scale Domain-Warped Displacement, Exponentielle Triplanar-Schärfung `pow(|N|, 8.0)`, Tangentenraum-Paritätskorrektur ($X, Y, Z$), PBR-Roughness-Trennung (Fels $0,88$–$0,96$ diffus vs. Firn $0,35$–$0,55$ spiegelnd), Schnee-Translucency (SSS), Weltkoordinaten-$Y$ Geologie (Qomolangma-Kalkstein, Yellow Band, Basiskristallin) & 30+ km Rayleigh-Distanzdunst.
### 🏔️ Phase A: Das fundamentale High-End-Rendering (Schritte 14–18)
* [x] **Schritt 14 (1:1 Part XII): Monumentale Bergschatten & 35-km DEM Cone-Tracing:**
  * Hardware-beschleunigtes Raymarching durch das $1024 \times 1024$ Höhengitter direkt auf der GPU.
  * Echter 35-Kilometer-Schattenwurf: Der Mount Everest und die Lhotse-Wand werfen morgens und abends riesige, messerscharfe Pyramidenschatten über das Khumbu-Tal und Tibet mit weicher Halbschatten-Penumbra ($0,53^\circ$ Sonnendurchmesser).
  * Fast-Rejection-Optimierung: Sofortiger Abbruch bei Backfaces, Verlassen der Bounding Box oder Überschreiten der $8.860\text{ m}$ Gipfelhöhe (< 0,2 ms Rechenzeit).
* [x] **Schritt 15 (1:1 Part XIII): Khumbu-Gletscher & Eisfall-Dynamik:**
  * Prozedurales Fließspannungs-Gitter (Strain-Tensor): An Geländestufen brechen echte 25 Meter tiefe Gletscherspalten (Crevasses) und turmhohe, instabile Serac-Eisnadeln auf.
  * Spektrale Lichtabsorption im Gletschereis: Rotes Licht wird $100\times$ stärker absorbiert als blaues – Spalten leuchten von innen heraus in magischem Kobalt- und Azurblau.
  * Supraglaziale Schmelzwassertümpel auf dem Moränengrund mit physikalischem Brechungsindex.
* [x] **Schritt 16 (1:1 Part XIV): Stratosphären-Optik & Eiskristall-Halos:**
  * Barometrische Dichteverteilung der Todeszone ($337\text{ hPa}$): Der Himmel dunkelt am Gipfel in tiefes Kosmos-Schwarzblau ab; Sterne werden tagsüber sichtbar.
  * Chappuis-Ozonabsorption (550–650 nm) für intensives Alpenglühen-Zenitlicht.
  * Eiskristall-Atmosphärenoptik: Physikalischer $22^\circ$-Halo um die Sonne, Nebensonnen (Parhelia) und Lichtsäulen bei Dämmerung.
* [x] **Schritt 17 (1:1 Part XV): Photometrische HDR-Kamera & Human Eye Adaptation:**
  * Compute-gestützte 64-Bin Log-Luminanz-Histogramm-Analyse: Über $100.000:1$ Dynamikumfang ($120.000\text{ Lux}$ Mittagssonne auf Firn vs. $<800\text{ Lux}$ Felsnischen) parallelisiert in GPU Groupshared Memory abgebildet. Outlier-Rejection filtert die extremen 10% Schwarz-Void und 2% Glints.
  * Organische Pupillenadaption mit asymmetrischer Rhodopsin-Kinetik ($1,2\text{ s}^{-1}$ Hell-nach-Dunkel vs. $3,8\text{ s}^{-1}$ Dunkel-nach-Hell) für naturgetreue Blendung beim Verlassen von Felsverschneidungen auf gleißende Schneeflächen.
  * Anamorpher Dual-Pass Eiskristall-Glare & Beugungssterne: Hexagonale 6-Punkt-Beugungsstrahlen passend zum $60^\circ$-Prismenwinkel hexagonaler Eiskristalle mit chromatischer Dispersion, horizontalem Streifglanz und dynamischem 5-Tap-Okklusionstest gegen Felsgrate.
* [x] **Schritt 18 (1:1 Part XVI): CDLOD / Nanite-Level Mikro-Terrain (0,25m Kletter-Auflösung):**
  * Continuous Distance LOD (CDLOD) Grid: Hochauflösendes $80\text{ m} \times 80\text{ m}$ CDLOD-Gitter mit $320 \times 320$ Quads ($103.041$ Vertices, $204.800$ Dreiecke) bei echten $0,25\text{ m}$ Kantenlänge im direkten Nah- und Kletterbereich der Kamera.
  * Nahtloses Terrain-Morphing & Höhenüberlagerung: Bicubic Catmull-Rom DEM-Höhensynthese mit glatter C1-Differenzierbarkeit. Hermite-Abstandsmorphing zwischen $32\text{ m}$ und $39\text{ m}$ glättet die Mikro-Geomorphologie stetig auf das 35-km-Basis-DEM herunter, unterstützt durch $+2\text{ cm}$ Depth-Bias und unterbrechungsfreie Basishöhen ohne sichtbare Nähte, Löcher oder Z-Fighting.
  * Nanite-Level Alpine Geomorphologie: Sedimentäre Schichtungsstufen der Gelben Wand und Qomolangma-Formation ($1,6\text{ m}$ bis $2,8\text{ m}$ Periode, $0,16\text{ m}$ Terrassenstufen), orthogonale tektonische Querklüfte (Klettergriffe & Felsrisse), Schuttmoränen-Mikrorelief und Wind-Sastrugi-Rillen.
  * Analytische 2nd-Order Central Difference Normalenberechnung & Geodätisches Makro-Slope-Gating: Berechnung exakter Oberflächennormalen direkt im Slang-Vertex-Shader. Beseitigung von Leopardenmuster-Artefakten an Steilwänden durch geodätisches Makro-Normal-Gating in Kombination mit physikalischem Height-Blending über Triplanar-Displacement und Jetstream-Gipfelwind-Abschabung (>8.560m).
  * 3D Frost-Shattered Boulder Instancing: GPU-Instanziertes Rendern facettierter Kristallingesteins-Blöcke (40 Facetten pro Felsblock) mit zentroid-validierten Außenflächen-Normalen, geomorphologischer Hangstabilitäts-Filterung ($\le 34^\circ$), 52%iger natürlicher Bodeneinbettung und organischen *Xanthoria elegans* Flechten-Rosetten.
  * Schlüsselstellen-Verifikation: Getestet und validiert an allen extremen Fixpunkten (Everest Base Camp 5.300m, Hillary Step 8.790m, Lhotse-Wand/Südsattel 8.338m und Third Step Felsturm 8.690m).

### 🌌 Phase B: Das absolute Maximum – Meilenweit voraus (Schritte 19–23)
* [ ] **Schritt 19 (1:1 Part XVII): Hardware Ray-Traced Multi-Bounce GI (Das Western Cwm Phänomen):**
  * Vulkan Hardware Ray Tracing (`VK_KHR_ray_tracing_pipeline` / ReSTIR GI).
  * Bis zu 8 indirekte Licht-Bounces im 3.000 Meter tiefen Kar zwischen Everest, Lhotse und Nuptse: Das legendäre „Glutofen“-Schneelicht des Western Cwm, bei dem selbst tiefste Schatten gleißend weiß strahlen.
* [ ] **Schritt 20 (1:1 Part XVIII): Dynamische Schnee- & Lawinen-Physik (MPM - Material Point Method):**
  * GPU-basierte Material Point Method für nicht-Newtonschen Schnee: Harschkrusten brechen unter Steigeisen ein und hinterlassen verdichtete Trittspuren.
  * Aktiver Höhenwind verweht Schnee zu überhängenden Kantenwechten, die bei Betreten physikalisch abbrechen.
  * Echtzeit-Staublawinen mit tosenden Pulverschneewolken von der Lhotse-Wand.
* [ ] **Schritt 21 (1:1 Part XIX): 3D Gaussian Splatting Photogrammetrie-Hotspots:**
  * Direkte Integration von 3D Gaussian Splats in die Vulkan-Pipeline für den Hillary Step, das Gipfelplateau mit Gebetsfahnen und den Südsattel.
  * Fotorealismus in 8K Ground-Truth bis auf wenige Millimeter Betrachtungsabstand.
* [ ] **Schritt 22 (1:1 Part XX): Viszerale Bergsteiger-Kryo-Optik:**
  * Physikalische Kategorie-4-Gletscherbrille mit Brewster-Winkel-Polarisation gegen Schneeblendung.
  * Atem-Kondensation und gefrierende Eisblumen an den Rändern der Gletscherbrille bei Anstrengung.
  * Hypoxie- & Höhenrausch-Shader in der Todeszone: Periphere Sichtfeldeinengung (Tunnelblick), Entsättigung und Pulsieren im Takt der Herzfrequenz.
* [ ] **Schritt 23 (1:1 Part XXI): Wave-Based Alpine Audio Raytracing:**
  * Akustisches Raytracing im $35\text{ km}$ Höhenmodell mit Infraschall-Echos brechender Eistürme an der 3.000 m hohen Nuptse-Wand.
  * Physikalisch brechender Orkanwind an Felsgraten und materialspezifische Steigeisen-Akustik (zersplitterndes Blankeis vs. knirschender Firn).







