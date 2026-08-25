#!/usr/bin/env python3
"""Convert image textures under bin/Debug to KTX2 + ASTC and update .mtl references.

Idempotent: skips textures that already have a newer/equal KTX2 output.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DEFAULT_TEXTURE_DIR = PROJECT_ROOT / "bin" / "Debug"
REPORT_PATH = SCRIPT_DIR / "astc-conversion-report.csv"

IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg"}

# filename stem / pattern -> (block_dim, colorspace, layout)
DEBUG_FILENAME_RULES = [
    (re.compile(r"(^|/)(diffuse|albedo|basecolor|color)(\.|$)", re.I), ("6x6", "sRGB", "astc_6x6_sRGB")),
    (re.compile(r"(^|/)(normal|bump|nrm)(\.|$)", re.I), ("6x6", "linear", "astc_6x6")),
    (re.compile(r"(^|/)(ao|ambientocclusion|occlusion)(\.|$)", re.I), ("8x8", "linear", "astc_8x8")),
    (re.compile(r"(^|/)(roughness|metallic|metalness|specular|height)(\.|$)", re.I), ("8x8", "linear", "astc_8x8")),
]

DEFAULT_CONFIG = ("6x6", "sRGB", "astc_6x6_sRGB")

MTL_MAP_KEYS = (
    "map_Kd",
    "map_Ks",
    "map_Ka",
    "map_Bump",
    "map_bump",
    "map_d",
    "map_Ns",
    "bump",
    "disp",
    "decal",
    "refl",
)


def find_toktx(explicit: Optional[str] = None) -> str:
    if explicit:
        p = Path(explicit)
        if not p.exists():
            raise FileNotFoundError(f"toktx not found: {explicit}")
        return str(p)

    env = os.environ.get("TOKTX")
    if env and Path(env).exists():
        return env

    which = shutil.which("toktx")
    if which:
        return which

    candidates = [
        PROJECT_ROOT / "tools" / "KTX-Software" / "bin" / "toktx.exe",
        PROJECT_ROOT / "tools" / "bin" / "toktx.exe",
        SCRIPT_DIR / "tools" / "KTX-Software" / "bin" / "toktx.exe",
        Path(r"C:\Program Files\KTX-Software\bin\toktx.exe"),
        Path(r"C:\KTX-Software\bin\toktx.exe"),
    ]
    for c in candidates:
        if c.exists():
            return str(c)

    raise FileNotFoundError(
        "toktx not found. Install KTX-Software and ensure toktx is on PATH, "
        "or pass --toktx path, or set TOKTX env var."
    )


def collect_image_files(texture_dir: Path) -> list[Path]:
    files: list[Path] = []
    for path in sorted(texture_dir.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix.lower() not in IMAGE_EXTENSIONS:
            continue
        files.append(path)
    return files


def resolve_config(image_path: Path, texture_dir: Path) -> tuple[str, str, str]:
    rel = str(image_path.relative_to(texture_dir)).replace("\\", "/")
    stem = image_path.stem.lower()
    haystack = f"{rel}/{stem}"
    for pattern, config in DEBUG_FILENAME_RULES:
        if pattern.search(haystack):
            return config
    return DEFAULT_CONFIG


def convert_texture(
    toktx: str,
    image_path: Path,
    ktx2_path: Path,
    block_dim: str,
    colorspace: str,
    force: bool = False,
) -> str:
    """Returns status: converted|skipped_exists|error"""
    if ktx2_path.exists() and not force:
        if ktx2_path.stat().st_mtime >= image_path.stat().st_mtime and ktx2_path.stat().st_size > 0:
            return "skipped_exists"

    if ktx2_path.exists():
        ktx2_path.unlink()

    oetf = "srgb" if colorspace.lower() == "srgb" else "linear"
    cmd = [
        toktx,
        "--t2",
        "--assign_oetf",
        oetf,
        "--encode",
        "astc",
        "--astc_blk_d",
        block_dim,
        "--astc_quality",
        "medium",
        "--genmipmap",
        str(ktx2_path),
        str(image_path),
    ]

    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        return "converted"
    except subprocess.CalledProcessError as e:
        stderr = (e.stderr or "") + (e.stdout or "")
        print(f"ERROR converting {image_path}: {stderr}", file=sys.stderr)
        return "error"


def update_mtl_files(texture_dir: Path, converted: dict[Path, Path]) -> int:
    """Rewrite map_* lines to point at .ktx2 when conversion succeeded."""
    if not converted:
        return 0

    by_dir: dict[Path, dict[str, str]] = {}
    for src, ktx2 in converted.items():
        by_dir.setdefault(src.parent, {})[src.name.lower()] = ktx2.name

    updated = 0
    for mtl_path in sorted(texture_dir.rglob("*.mtl")):
        mapping = by_dir.get(mtl_path.parent, {})
        if not mapping:
            continue

        original = mtl_path.read_text(encoding="utf-8")
        lines = original.splitlines(keepends=True)
        changed = False
        new_lines: list[str] = []

        for line in lines:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                new_lines.append(line)
                continue

            parts = stripped.split(None, 1)
            if len(parts) != 2 or parts[0] not in MTL_MAP_KEYS:
                new_lines.append(line)
                continue

            key, tex_ref = parts
            tex_name = Path(tex_ref.replace("\\", "/")).name
            replacement = mapping.get(tex_name.lower())
            if replacement is None:
                new_lines.append(line)
                continue

            prefix = line[: line.index(stripped)]
            new_lines.append(f"{prefix}{key} {replacement}\n")
            changed = True

        if changed:
            mtl_path.write_text("".join(new_lines), encoding="utf-8", newline="\n")
            updated += 1
            print(f"Updated MTL: {mtl_path.relative_to(texture_dir)}")

    return updated


def validate(texture_dir: Path, converted: dict[Path, Path]) -> list[str]:
    issues: list[str] = []
    for src, ktx2 in converted.items():
        if not ktx2.exists():
            issues.append(f"Missing KTX2 output: {ktx2.relative_to(texture_dir)}")
    return issues


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert Debug textures to ASTC KTX2")
    parser.add_argument(
        "--texture-dir",
        type=Path,
        default=DEFAULT_TEXTURE_DIR,
        help=f"Root directory to scan (default: {DEFAULT_TEXTURE_DIR})",
    )
    parser.add_argument("--toktx", help="Path to toktx executable")
    parser.add_argument("--force", action="store_true", help="Reconvert even if KTX2 exists")
    parser.add_argument("--dry-run", action="store_true", help="Plan only, no conversion")
    parser.add_argument("--skip-mtl", action="store_true", help="Skip .mtl reference updates")
    args = parser.parse_args()

    texture_dir = args.texture_dir.resolve()
    if not texture_dir.is_dir():
        print(f"Texture directory not found: {texture_dir}", file=sys.stderr)
        return 1

    toktx = None if args.dry_run else find_toktx(args.toktx)
    if toktx:
        print(f"Using toktx: {toktx}")
        try:
            subprocess.run([toktx, "--version"], capture_output=True, text=True, check=False)
        except OSError as e:
            print(f"Cannot run toktx: {e}", file=sys.stderr)
            return 1

    image_files = collect_image_files(texture_dir)
    if not image_files:
        print(f"No image files found under {texture_dir}")
        return 0

    report_rows: list[dict] = []
    converted: dict[Path, Path] = {}
    stats = {"convert": 0, "skip_exists": 0, "error": 0}

    for image_path in image_files:
        rel = image_path.relative_to(texture_dir)
        filesize = image_path.stat().st_size
        ktx2_path = image_path.with_suffix(".ktx2")
        block_dim, colorspace, layout = resolve_config(image_path, texture_dir)

        if args.dry_run:
            status = "planned"
            ktx2_size = ktx2_path.stat().st_size if ktx2_path.exists() else ""
        else:
            status = convert_texture(toktx, image_path, ktx2_path, block_dim, colorspace, force=args.force)
            if status == "converted":
                stats["convert"] += 1
                converted[image_path] = ktx2_path
            elif status == "skipped_exists":
                stats["skip_exists"] += 1
                converted[image_path] = ktx2_path
            else:
                stats["error"] += 1
            ktx2_size = ktx2_path.stat().st_size if ktx2_path.exists() else ""

        ratio = ""
        if isinstance(ktx2_size, int) and ktx2_size > 0:
            ratio = f"{filesize / ktx2_size:.2f}x"

        report_rows.append(
            {
                "texture": str(rel),
                "status": status,
                "block_dim": block_dim,
                "colorspace": colorspace,
                "layout": layout,
                "source_bytes": filesize,
                "ktx2_bytes": ktx2_size,
                "ratio": ratio,
            }
        )
        print(f"[{status}] {rel} -> {ktx2_path.name} ({block_dim} {colorspace})", flush=True)

    if not args.dry_run and not args.skip_mtl:
        n = update_mtl_files(texture_dir, converted)
        print(f"Updated {n} .mtl file(s)")

    with open(REPORT_PATH, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "texture",
                "status",
                "block_dim",
                "colorspace",
                "layout",
                "source_bytes",
                "ktx2_bytes",
                "ratio",
            ],
        )
        writer.writeheader()
        writer.writerows(report_rows)

    converted_png_bytes = 0
    converted_ktx_bytes = 0
    for row in report_rows:
        if row["status"] in ("error",):
            continue
        if row["ktx2_bytes"] == "" or row["ktx2_bytes"] is None:
            continue
        converted_png_bytes += int(row["source_bytes"])
        converted_ktx_bytes += int(row["ktx2_bytes"])

    print("---")
    print(f"Texture dir: {texture_dir}")
    print(f"Report: {REPORT_PATH}")
    print(f"Stats: {stats}")
    if converted_ktx_bytes:
        print(
            f"Converted size: source {converted_png_bytes / 1024 / 1024:.1f} MB -> "
            f"KTX2 {converted_ktx_bytes / 1024 / 1024:.1f} MB "
            f"({converted_png_bytes / converted_ktx_bytes:.2f}x)"
        )

    if not args.dry_run:
        issues = validate(texture_dir, converted)
        if issues:
            print(f"Validation issues ({len(issues)}):")
            for issue in issues[:30]:
                print(f"  - {issue}")
            return 2
        print("Validation: OK")

    return 0 if stats["error"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
