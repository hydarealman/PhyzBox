"""Fetch only the Windows files from Godot's multi-platform template archive.

Godot publishes one archive larger than 1 GB. This reader uses HTTP byte ranges and
the ZIP central directory so a Windows checkout downloads only the two needed files.
"""

from __future__ import annotations

import argparse
import struct
import urllib.request
import zlib
from pathlib import Path


def fetch_range(url: str, start: int, end: int) -> bytes:
    request = urllib.request.Request(url, headers={"Range": f"bytes={start}-{end}"})
    with urllib.request.urlopen(request) as response:
        data = response.read()
    expected = end - start + 1
    if len(data) != expected:
        raise RuntimeError(f"HTTP range returned {len(data)} bytes; expected {expected}")
    return data


def remote_size(url: str) -> int:
    request = urllib.request.Request(url, method="HEAD")
    with urllib.request.urlopen(request) as response:
        value = response.headers.get("Content-Length")
    if not value:
        raise RuntimeError("Template server did not provide Content-Length")
    return int(value)


def central_directory(url: str) -> bytes:
    size = remote_size(url)
    tail_size = min(size, 1024 * 1024)
    tail = fetch_range(url, size - tail_size, size - 1)
    marker = tail.rfind(b"PK\x05\x06")
    if marker < 0:
        raise RuntimeError("ZIP end-of-central-directory record not found")
    _, _, _, _, directory_size, directory_offset, _ = struct.unpack_from(
        "<4H2IH", tail, marker + 4
    )
    return fetch_range(url, directory_offset, directory_offset + directory_size - 1)


def entries(directory: bytes):
    position = 0
    while position + 46 <= len(directory):
        values = struct.unpack_from("<4s6H3I5H2I", directory, position)
        if values[0] != b"PK\x01\x02":
            raise RuntimeError("Invalid ZIP central-directory entry")
        name_length, extra_length, comment_length = values[10], values[11], values[12]
        name_start = position + 46
        name = directory[name_start : name_start + name_length].decode("utf-8")
        yield {
            "name": name,
            "method": values[4],
            "compressed_size": values[8],
            "size": values[9],
            "offset": values[16],
        }
        position = name_start + name_length + extra_length + comment_length


def extract(url: str, entry: dict, destination: Path) -> None:
    header = fetch_range(url, entry["offset"], entry["offset"] + 29)
    values = struct.unpack("<4s5H3I2H", header)
    if values[0] != b"PK\x03\x04":
        raise RuntimeError(f"Invalid local header for {entry['name']}")
    data_offset = entry["offset"] + 30 + values[9] + values[10]
    compressed = fetch_range(
        url, data_offset, data_offset + entry["compressed_size"] - 1
    )
    if entry["method"] == 0:
        content = compressed
    elif entry["method"] == 8:
        content = zlib.decompress(compressed, -zlib.MAX_WBITS)
    else:
        raise RuntimeError(f"Unsupported ZIP compression method {entry['method']}")
    if len(content) != entry["size"]:
        raise RuntimeError(f"Size mismatch while extracting {entry['name']}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(content)
    print(f"Extracted {destination.name} ({len(content)} bytes)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("url")
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    wanted = {"windows_debug_x86_64.exe", "windows_release_x86_64.exe"}
    found = set()
    for entry in entries(central_directory(arguments.url)):
        filename = Path(entry["name"]).name
        if filename in wanted:
            extract(arguments.url, entry, arguments.output / filename)
            found.add(filename)
    missing = wanted - found
    if missing:
        raise RuntimeError(f"Templates not found in archive: {sorted(missing)}")


if __name__ == "__main__":
    main()
