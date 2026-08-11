#!/usr/bin/env python3
"""Build the compact PhyzBox Voyager ephemeris from official NAIF SPICE kernels.

The generated runtime file contains geometric J2000 states relative to the solar
system barycenter. It is deterministic for a fixed set of source kernel hashes.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import struct
import sys
import urllib.request
from pathlib import Path

try:
    import spiceypy as spice
except ImportError as error:
    raise SystemExit(
        "spiceypy is required. Install it with: "
        "python -m pip install --target .tools/python spiceypy"
    ) from error


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CACHE = ROOT / ".runtime" / "voyager-source"
DEFAULT_OUTPUT = ROOT / "godot" / "data" / "voyager_ephemeris.phyz"
DEFAULT_METADATA = ROOT / "godot" / "data" / "voyager_ephemeris.json"

KERNELS = {
    "de440s.bsp": "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp",
    "naif0012.tls": "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls",
    "Voyager_1.a54206u_V0.2_merged.bsp": (
        "https://naif.jpl.nasa.gov/pub/naif/VOYAGER/kernels/spk/"
        "Voyager_1.a54206u_V0.2_merged.bsp"
    ),
    "Voyager_2.m05016u.merged.bsp": (
        "https://naif.jpl.nasa.gov/pub/naif/VOYAGER/kernels/spk/"
        "Voyager_2.m05016u.merged.bsp"
    ),
}

# Giant planets use system barycenters because the compact DE440 kernel does not
# contain every giant-planet center. At game rendering scale this distinction is
# negligible; encounter spacecraft states still use their native SPK centers.
BODIES = [
    (10, "Sun"),
    (199, "Mercury"),
    (299, "Venus"),
    (399, "Earth"),
    (301, "Moon"),
    (499, "Mars"),
    (5, "Jupiter"),
    (6, "Saturn"),
    (7, "Uranus"),
    (8, "Neptune"),
    (9, "Pluto"),
    (-31, "Voyager 1"),
    (-32, "Voyager 2"),
]

ENCOUNTERS = [
    "1979-03-05T00:00:00",
    "1979-07-09T00:00:00",
    "1980-11-12T00:00:00",
    "1981-08-25T00:00:00",
    "1986-01-24T00:00:00",
    "1989-08-25T00:00:00",
]

TRAJECTORY_BOUNDARIES = [
    "1977-08-20T15:32:32.182",
    "1977-08-23T11:29:10.841",
    "1977-09-05T13:59:24.383",
    "1977-09-08T09:08:16.593",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download_sources(cache: Path) -> list[Path]:
    cache.mkdir(parents=True, exist_ok=True)
    paths: list[Path] = []
    for name, url in KERNELS.items():
        path = cache / name
        if not path.exists():
            print(f"Downloading {url}")
            request = urllib.request.Request(url, headers={"User-Agent": "PhyzBox ephemeris importer"})
            with urllib.request.urlopen(request) as response, path.open("wb") as output:
                while chunk := response.read(1024 * 1024):
                    output.write(chunk)
        paths.append(path)
    return paths


def build_epochs() -> list[float]:
    epochs: set[float] = set()

    def add_range(start: str, stop: str, seconds: float) -> None:
        current = spice.str2et(start)
        end = spice.str2et(stop)
        while current <= end + 1.0e-6:
            epochs.add(round(current, 6))
            current += seconds

    # Daily samples cover the complete planetary tour. Weekly samples retain the
    # interstellar cruise without inflating the shipped asset.
    add_range("1977-08-20T00:00:00", "1990-01-01T00:00:00", 86400.0)
    add_range("1990-01-01T00:00:00", "2030-12-31T00:00:00", 7.0 * 86400.0)
    # Hourly states around each flyby keep cubic Hermite interpolation accurate
    # where trajectories bend most rapidly.
    for encounter in ENCOUNTERS:
        center = spice.str2et(encounter)
        for hour in range(-15 * 24, 15 * 24 + 1):
            epochs.add(round(center + hour * 3600.0, 6))

    for boundary in TRAJECTORY_BOUNDARIES:
        epochs.add(round(spice.str2et(boundary), 6))
    return sorted(epochs)


def state_at(naif_id: int, epoch: float) -> tuple[float, ...]:
    try:
        state, _light_time = spice.spkezr(str(naif_id), epoch, "J2000", "NONE", "0")
        return tuple(float(component) for component in state)
    except spice.utils.exceptions.SpiceyError:
        return (math.nan,) * 6


def write_ephemeris(output: Path, epochs: list[float]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(b"PHYZEPH2")
        stream.write(struct.pack("<III", 2, len(BODIES), len(epochs)))
        for naif_id, name in BODIES:
            encoded = name.encode("utf-8")
            stream.write(struct.pack("<iH", naif_id, len(encoded)))
            stream.write(encoded)
        for index, epoch in enumerate(epochs):
            if index % 1000 == 0:
                print(f"Sampling {index}/{len(epochs)}")
            utc_text = spice.et2utc(epoch, "ISOC", 3)
            utc_value = dt.datetime.fromisoformat(utc_text).replace(tzinfo=dt.UTC)
            utc_millis = round(utc_value.timestamp() * 1000.0)
            stream.write(struct.pack("<dq", epoch, utc_millis))
            for naif_id, _name in BODIES:
                stream.write(struct.pack("<6d", *state_at(naif_id, epoch)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--metadata", type=Path, default=DEFAULT_METADATA)
    arguments = parser.parse_args()

    source_paths = download_sources(arguments.cache)
    spice.kclear()
    # Load leap seconds first, then historical spacecraft kernels. DE440s is
    # loaded last so planetary states use the modern JPL planetary ephemeris.
    load_order = [source_paths[1], source_paths[2], source_paths[3], source_paths[0]]
    for path in load_order:
        spice.furnsh(str(path))

    epochs = build_epochs()
    write_ephemeris(arguments.output, epochs)
    metadata = {
        "format": "PHYZEPH2",
        "reference_frame": "J2000",
        "observer": "SOLAR SYSTEM BARYCENTER",
        "aberration_correction": "NONE",
        "position_unit": "km",
        "velocity_unit": "km/s",
        "coverage_utc": ["1977-08-20T00:00:00", "2030-12-31T00:00:00"],
        "sample_count": len(epochs),
        "runtime_sha256": sha256(arguments.output),
        "bodies": [{"naif_id": body_id, "name": name} for body_id, name in BODIES],
        "sources": [
            {"file": path.name, "url": KERNELS[path.name], "sha256": sha256(path)}
            for path in source_paths
        ],
        "generated_utc": dt.datetime.now(dt.UTC).replace(microsecond=0).isoformat(),
        "generator": "tools/import_voyager_ephemeris.py",
    }
    arguments.metadata.parent.mkdir(parents=True, exist_ok=True)
    arguments.metadata.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {arguments.output} ({arguments.output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
