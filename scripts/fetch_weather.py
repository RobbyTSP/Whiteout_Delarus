#!/usr/bin/env python3
"""
Fetches real-time weather and atmospheric profiles for the Everest/Khumbu region
using the Open-Meteo API.
"""

import os
import json
import argparse
from pathlib import Path
import requests

HIMALAYA_STATIONS = {
    "everest_summit": {
        "name": "Mount Everest Summit",
        "latitude": 27.9881,
        "longitude": 86.9250,
        "elevation": 8848
    },
    "everest_base_camp": {
        "name": "Everest Base Camp (South)",
        "latitude": 28.0044,
        "longitude": 86.8528,
        "elevation": 5364
    },
    "namche_bazaar": {
        "name": "Namche Bazaar (Gateway to Everest)",
        "latitude": 27.8069,
        "longitude": 86.7140,
        "elevation": 3440
    }
}

def fetch_weather(output_dir="data/weather"):
    out_path = Path(output_dir)
    out_path.mkdir(parents=True, exist_ok=True)
    
    print("=== Fetching Real-Time Himalayan Weather (Open-Meteo API) ===")
    
    weather_report = {}
    
    for station_id, info in HIMALAYA_STATIONS.items():
        print(f"Querying station: {info['name']} ({info['elevation']}m)...")
        url = (
            f"https://api.open-meteo.com/v1/forecast?"
            f"latitude={info['latitude']}&longitude={info['longitude']}&"
            f"current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,"
            f"weather_code,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,cloud_cover"
        )
        
        try:
            r = requests.get(url, timeout=15)
            if r.status_code == 200:
                data = r.json()
                curr = data.get("current", {})
                weather_report[station_id] = {
                    "station": info["name"],
                    "coordinates": {"lat": info["latitude"], "lon": info["longitude"]},
                    "elevation_m": info["elevation"],
                    "timestamp": curr.get("time"),
                    "temperature_celsius": curr.get("temperature_2m"),
                    "apparent_temperature_celsius": curr.get("apparent_temperature"),
                    "relative_humidity_percent": curr.get("relative_humidity_2m"),
                    "surface_pressure_hpa": curr.get("surface_pressure"),
                    "wind_speed_kmh": curr.get("wind_speed_10m"),
                    "wind_direction_deg": curr.get("wind_direction_10m"),
                    "wind_gusts_kmh": curr.get("wind_gusts_10m"),
                    "cloud_cover_percent": curr.get("cloud_cover"),
                    "weather_code": curr.get("weather_code")
                }
                print(f"  Temp: {curr.get('temperature_2m')}°C | Wind: {curr.get('wind_speed_10m')} km/h | Pressure: {curr.get('surface_pressure')} hPa")
            else:
                print(f"  Error: HTTP {r.status_code}")
        except Exception as e:
            print(f"  Failed: {e}")
            
    out_file = out_path / "everest_current.json"
    with open(out_file, "w") as f:
        json.dump(weather_report, f, indent=2)
    print(f"\n✓ Realtime weather data written to {out_file}")
    return weather_report

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Fetch Himalayan Weather")
    parser.add_argument("--output", default="data/weather", help="Destination folder")
    args = parser.parse_args()
    fetch_weather(args.output)
