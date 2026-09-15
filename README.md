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

## 🏔️ Schritt 11: PBR-Tiefenplastizität, Dynamic Sky / Skybox & Rayleigh-Dunst (1:1 Part IX - Geplant)

Zur Perfektionierung der Lichtstimmung, Schattentiefe und Himmelsatmosphäre:

### 1. Beleuchtung & Tiefenplastizität (PBR)
* **HBAO / Screen-Space & Contact Occlusion:**
  * In den engen Falten zwischen den Graten und in tiefen Schluchten fehlt das tiefe Umgebungsdunkel, in das kein Himmelslicht vordringt.
  * Tiefe Kontaktschatten in Furchen und Mulden nehmen dem Gelände das verbleibende Polygon-Gefühl und erzeugen monumentale Masse.
* **Roughness-Splitting:**
  * Fels benötigt hohe Rauheit ($0,85$–$0,95$), während gefrorener Schnee, Firn und Eislinsen in Rinnen niedrigere Werte ($0,3$–$0,5$) mit gerichtetem Specular-Highlight erhalten, an denen das Sonnenlicht bricht.

### 2. Atmosphäre & Himmel (Dynamic Sky / Skybox)
* **Skybox / Dynamic Sky:**
  * Das neutrale Grau-Blau des Himmels wird durch einen dynamischen atmosphärischen Gradienten mit Sonnenstand oder eine hochaufgelöste HDR-Skybox ersetzt.
  * Liefert physikalisch fundierte Einstrahlung und farbig nuanciertes Umgebungslicht (Ambient Light).
* **Rayleigh-Scattering & Distanzdunst:**
  * Die hintersten Achttausender-Massive müssen über die Distanz messbar bläulicher, weicher und kontrastärmer werden.
  * Ein Exponential-Height-Fog, der mit der Distanz zur Kamera zunimmt, verdoppelt optisch die Weite des Himalayas.

---

## 🔭 Nächste Schritte (Roadmap)

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
* [ ] **Schritt 11 (1:1 Part IX):** PBR-Tiefenplastizität (HBAO/Kontaktschatten in Furchen, Roughness-Splitting Fels $0,85$–$0,95$ vs. Eis $0,3$–$0,5$) & Dynamic Sky / Skybox mit weitem Rayleigh-Distanzdunst.





