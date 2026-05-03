#!/usr/bin/env python3
"""
Render the latest screenshot as a voxel plane through the HackMatrix API.

Requires:
  pip install pillow
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable

from PIL import Image

from hackMatrix.api import Client


def find_latest_screenshot(screenshot_dir: Path) -> Path:
    candidates = []
    for path in screenshot_dir.iterdir():
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".png", ".jpg", ".jpeg"}:
            continue
        candidates.append(path)

    if not candidates:
        raise FileNotFoundError(f"no screenshots found in {screenshot_dir}")

    return max(candidates, key=lambda path: path.stat().st_mtime)


def batched(
    iterable: list[tuple[tuple[float, float, float], tuple[float, float, float]]],
    batch_size: int,
) -> Iterable[list[tuple[tuple[float, float, float], tuple[float, float, float]]]]:
    for index in range(0, len(iterable), batch_size):
        yield iterable[index:index + batch_size]


def build_plane_pixels(image_path: Path,
                       plane_width: int,
                       plane_height: int,
                       origin_x: float,
                       origin_y: float,
                       origin_z: float,
                       voxel_edge: float,
                       voxel_gap: float) -> list[tuple[tuple[float, float, float], tuple[float, float, float]]]:
    spacing = voxel_edge + voxel_gap
    image = Image.open(image_path).convert("RGB")
    image = image.resize((plane_width, plane_height))

    pixels = []
    for y in range(plane_height):
        for x in range(plane_width):
            r, g, b = image.getpixel((x, y))
            pos = (
                origin_x + x * spacing,
                origin_y - y * spacing,
                origin_z,
            )
            color = (r / 255.0, g / 255.0, b / 255.0)
            pixels.append((pos, color))

    return pixels


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", default=None)
    parser.add_argument("--image", type=Path, default=None)
    parser.add_argument("--screenshot-dir", type=Path, default=Path("screenshots"))
    parser.add_argument("--plane-width", type=int, default=640)
    parser.add_argument("--plane-height", type=int, default=480)
    parser.add_argument("--origin-x", type=float, default=0.0)
    parser.add_argument("--origin-z", type=float, default=0.0)
    parser.add_argument("--origin-y", type=float, default=1.0)
    parser.add_argument("--voxel-edge", type=float, default=0.01)
    parser.add_argument("--voxel-gap", type=float, default=0.0025)
    parser.add_argument("--batch-size", type=int, default=4096)
    parser.add_argument("--ids-out", type=Path, default=Path("/tmp/hackmatrix-screenshot-voxel-ids.json"))
    parser.add_argument("--replace", action="store_true", default=True)
    parser.add_argument("--no-replace", dest="replace", action="store_false")
    args = parser.parse_args()

    image_path = args.image or find_latest_screenshot(args.screenshot_dir)
    spacing = args.voxel_edge + args.voxel_gap
    origin_y = args.origin_y
    if origin_y is None:
        origin_y = 5.0 + 0.5 * (args.plane_height - 1) * spacing

    pixels = build_plane_pixels(
        image_path=image_path,
        plane_width=args.plane_width,
        plane_height=args.plane_height,
        origin_x=args.origin_x,
        origin_y=origin_y,
        origin_z=args.origin_z,
        voxel_edge=args.voxel_edge,
        voxel_gap=args.voxel_gap,
    )

    client = Client(args.address)
    created_ids: list[int] = []
    first_batch = True

    for chunk in batched(pixels, args.batch_size):
        positions = [pos for pos, _color in chunk]
        colors = [color for _pos, color in chunk]
        ids = client.add_voxels(
            positions,
            replace=args.replace and first_batch,
            size=args.voxel_edge,
            colors=colors,
        )
        created_ids.extend(ids)
        first_batch = False

    args.ids_out.write_text(json.dumps({
        "image": str(image_path),
        "voxel_ids": created_ids,
    }))

    print(f"image: {image_path}")
    print(f"voxels: {len(created_ids)}")
    print(f"ids written to: {args.ids_out}")


if __name__ == "__main__":
    main()
