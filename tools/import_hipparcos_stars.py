#!/usr/bin/env python3
"""Build the compact runtime star catalogue used by the Voyager sky renderer."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import ssl
import struct
import urllib.parse
from datetime import datetime, timezone
from pathlib import Path


SOURCE_URL = "https://heasarc.gsfc.nasa.gov/xamin/vo/tap/sync"
MAGNITUDE_LIMIT = 9.0
MAGIC = b"PHYZSTAR1"
CATALOG_EPOCH = 1991.25
OUTPUT_EPOCH = 2000.0
ADQL_QUERY = (
    "select ra_deg,dec_deg,vmag,bv_color,pm_ra,pm_dec "
    f"from hipparcos where vmag <= {MAGNITUDE_LIMIT}"
)
SOURCE_QUERY_URL = SOURCE_URL + "?" + urllib.parse.urlencode(
    {"request": "doQuery", "lang": "ADQL", "format": "text", "query": ADQL_QUERY}
)


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        return
    try:
        import certifi
        import requests

        response = requests.get(url, timeout=90, verify=certifi.where())
        response.raise_for_status()
        destination.write_bytes(response.content)
        return
    except ImportError:
        import urllib.request

        request = urllib.request.Request(url, headers={"User-Agent": "PhyzBox-data-import/1.0"})
        context = ssl.create_default_context()
        with urllib.request.urlopen(request, timeout=90, context=context) as response:
            destination.write_bytes(response.read())


def optional_float(fields: list[str], index: int, fallback: float) -> float:
    try:
        value = fields[index].strip()
        return float(value) if value else fallback
    except (IndexError, ValueError):
        return fallback


def convert_row(line: str) -> tuple[float, float, float, float] | None:
    fields = line.rstrip("\n").split("|")
    if len(fields) != 6:
        return None
    try:
        right_ascension = float(fields[0].strip())
        declination = float(fields[1].strip())
        magnitude = float(fields[2].strip())
    except ValueError:
        return None
    if magnitude > MAGNITUDE_LIMIT:
        return None

    years = OUTPUT_EPOCH - CATALOG_EPOCH
    proper_motion_ra = optional_float(fields, 4, 0.0)
    proper_motion_dec = optional_float(fields, 5, 0.0)
    cos_dec = max(1.0e-6, abs(math.cos(math.radians(declination))))
    right_ascension += proper_motion_ra * years / (3_600_000.0 * cos_dec)
    declination += proper_motion_dec * years / 3_600_000.0
    color_index = max(-0.35, min(2.2, optional_float(fields, 3, 0.65)))
    return right_ascension % 360.0, declination, magnitude, color_index


def build(source: Path, output: Path, metadata_path: Path) -> int:
    stars: list[tuple[float, float, float, float]] = []
    with source.open("rt", encoding="ascii", errors="replace") as stream:
        for line in stream:
            star = convert_row(line)
            if star is not None:
                stars.append(star)
    stars.sort(key=lambda star: star[2])

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(MAGIC)
        stream.write(struct.pack("<I", len(stars)))
        for star in stars:
            stream.write(struct.pack("<ffff", *star))

    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    output_hash = hashlib.sha256(output.read_bytes()).hexdigest()
    metadata = {
        "format": "PHYZSTAR1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source": {
            "catalogue": "The Hipparcos Main Catalogue, ESA SP-1200 (1997)",
            "url": SOURCE_QUERY_URL,
            "sha256": source_hash,
            "credit": "ESA",
            "archive": "NASA HEASARC TAP service, table hipparcos",
        },
        "selection": {
            "maximum_v_magnitude": MAGNITUDE_LIMIT,
            "coordinate_frame": "ICRS / J2000 equator",
            "position_epoch": OUTPUT_EPOCH,
            "color": "Johnson B-V colour index",
        },
        "star_count": len(stars),
        "runtime_sha256": output_hash,
    }
    metadata_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return len(stars)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", type=Path, default=Path(".runtime/hipparcos-source"))
    parser.add_argument("--output", type=Path, default=Path("godot/data/hipparcos_bright.phyzstars"))
    parser.add_argument("--metadata", type=Path, default=Path("godot/data/hipparcos_bright.json"))
    args = parser.parse_args()
    source = args.cache / "heasarc_hipparcos_vmag9.txt"
    download(SOURCE_QUERY_URL, source)
    count = build(source, args.output, args.metadata)
    print(f"wrote {count:,} real stars to {args.output}")


if __name__ == "__main__":
    main()
