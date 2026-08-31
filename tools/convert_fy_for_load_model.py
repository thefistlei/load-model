#!/usr/bin/env python3
"""Convert milicon/RaysFusion fy assets to load-model import layout.

Enhanced version: exports ALL scene elements with transforms and materials,
generates scene.json config for multi-model OpenGL loading.

Requires Python 3.6+.
"""

import argparse
import json
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = PROJECT_ROOT / "new-resources" / "fy"
DEFAULT_DST = PROJECT_ROOT / "resources" / "objects" / "fy"


def find_assimp_exporter():
    # type: () -> Optional[List[str]]
    candidates = [
        PROJECT_ROOT / "bin" / "fbx2obj",
        PROJECT_ROOT / "bin" / "Debug" / "fbx2obj.exe",
        PROJECT_ROOT / "bin" / "fbx2obj.exe",
    ]
    for path in candidates:
        if path.is_file():
            return [str(path)]

    for name in ("assimp", "assimp.exe"):
        if shutil.which(name):
            return [name]
    return None


def export_fbx_to_obj(exporter_cmd, fbx, obj):
    # type: (List[str], Path, Path) -> bool
    obj.parent.mkdir(parents=True, exist_ok=True)
    if exporter_cmd[0].endswith("fbx2obj") or exporter_cmd[0].endswith("fbx2obj.exe"):
        cmd = exporter_cmd + [str(fbx), str(obj)]
    else:
        cmd = exporter_cmd + ["export", str(fbx), str(obj)]
    try:
        subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        return obj.is_file()
    except subprocess.CalledProcessError as exc:
        err = (exc.stderr or exc.stdout or str(exc)).strip()
        if err:
            print("  export failed: {}".format(err.splitlines()[-1]), file=sys.stderr)
        return False
    except (FileNotFoundError, OSError) as exc:
        print("  export failed: {}".format(exc), file=sys.stderr)
        return False


def load_json(path):
    # type: (Path) -> dict
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def material_hash_from_node(node):
    # type: (str) -> str
    tail = node.rsplit("/", 1)[-1]
    return tail[:8]


def build_material_index(material_dir):
    # type: (Path) -> Dict[str, Path]
    index = {}  # type: Dict[str, Path]
    for path in material_dir.glob("*.material.json"):
        m = re.search(r"\.([0-9a-f]{8})\.material\.json$", path.name)
        if m:
            index[m.group(1)] = path
    return index


def pick_base_texture(material):
    # type: (dict) -> Optional[str]
    textures = material.get("textures") or []
    for tex in textures:
        if tex.get("name") == "BaseMap" and tex.get("path"):
            return tex["path"]
    if textures and textures[0].get("path"):
        return textures[0]["path"]
    return None


def parse_mesh_json(mesh_json):
    # type: (Path) -> Optional[Tuple[str, List[Tuple[str, str]]]]
    data = load_json(mesh_json)
    for action in data.get("actions", []):
        if action.get("action") != "import-via-assimp":
            continue
        cfg = action.get("config") or {}
        fbx_name = cfg.get("path")
        if not fbx_name:
            return None
        slots = []  # type: List[Tuple[str, str]]
        opts = cfg.get("import_options") or {}
        for item in opts.get("material_assignments") or []:
            name = item.get("name")
            node = item.get("node")
            if name and node:
                slots.append((name, node))
        return fbx_name, slots
    return None


def write_mtl(path, slots, slot_texture):
    # type: (Path, List[Tuple[str, str]], Dict[str, str]) -> None
    lines = []  # type: List[str]
    seen = set()  # type: Set[str]
    for slot_name, _node in slots:
        if slot_name in seen:
            continue
        seen.add(slot_name)
        tex_name = slot_texture.get(slot_name)
        lines.append("newmtl {}".format(slot_name))
        if tex_name:
            lines.append("map_Kd {}".format(tex_name))
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def patch_exported_mtl(obj_path, mtl_path):
    # type: (Path, Path) -> None
    assimp_mtl = obj_path.with_suffix(".mtl")
    if assimp_mtl.is_file() and assimp_mtl != mtl_path:
        try:
            assimp_mtl.unlink()
        except OSError:
            pass


def convert_part(src_root, dst_root, mesh_json, material_index, exporter_cmd):
    # type: (Path, Path, Path, Dict[str, Path], Optional[List[str]]) -> Optional[Path]
    parsed = parse_mesh_json(mesh_json)
    if not parsed:
        return None

    fbx_name, slots = parsed
    part_name = re.sub(r"\.[0-9a-f]{8}(\.mesh)?$", "", mesh_json.stem, flags=re.I)

    fbx_src = src_root / "mesh" / fbx_name
    if not fbx_src.is_file():
        print("  skip {}: missing {}".format(part_name, fbx_src))
        return None

    out_dir = dst_root / "parts" / part_name
    out_dir.mkdir(parents=True, exist_ok=True)

    slot_texture = {}  # type: Dict[str, str]
    for slot_name, node in slots:
        mat_hash = material_hash_from_node(node)
        mat_path = material_index.get(mat_hash)
        if not mat_path:
            continue
        tex_rel = pick_base_texture(load_json(mat_path))
        if not tex_rel:
            continue
        tex_src = (mat_path.parent / tex_rel).resolve()
        if not tex_src.is_file():
            print("  warn {}: missing texture {}".format(part_name, tex_src))
            continue
        tex_dst_name = tex_src.name
        shutil.copy2(str(tex_src), str(out_dir / tex_dst_name))
        slot_texture[slot_name] = tex_dst_name

    obj_path = out_dir / "{}.obj".format(part_name)
    mtl_path = out_dir / "{}.mtl".format(part_name)
    write_mtl(mtl_path, slots, slot_texture)

    if exporter_cmd:
        if export_fbx_to_obj(exporter_cmd, fbx_src, obj_path):
            patch_exported_mtl(obj_path, mtl_path)
            obj_text = obj_path.read_text(encoding="utf-8", errors="ignore")
            mtl_line = "mtllib {}\n".format(mtl_path.name)
            if "mtllib " not in obj_text:
                obj_path.write_text(mtl_line + obj_text, encoding="utf-8")
            print("  ok {}.obj".format(part_name))
            return obj_path

    shutil.copy2(str(fbx_src), str(out_dir / fbx_name))
    print("  partial {}: fbx copied (build fbx2obj or install assimp-utils)".format(part_name))
    return None


# ---------------------------------------------------------------------------
# Element JSON parsing (transforms + material mapping)
# ---------------------------------------------------------------------------

def parse_element_json(element_json):
    # type: (Path) -> Optional[dict]
    """Extract title, transform, and material_mapping from an element JSON."""
    data = load_json(element_json)
    title = None
    transform = None
    material_mapping = None

    for action in data.get("actions", []):
        act = action.get("action", "")
        cfg = action.get("config") or {}

        if act == "retitle":
            raw_title = cfg.get("title", "")
            # title may be a variable reference like "@title"
            # Check variables section for the actual value
            variables = action.get("variables", {})
            if raw_title.startswith("@") and raw_title in variables:
                title = variables[raw_title].lstrip("@. ")
            else:
                title = raw_title.lstrip("@. ")

        elif act == "ext-fx-standard-pipeline/transform":
            transform = {
                "position": cfg.get("position", [0, 0, 0]),
                "scaling": cfg.get("scaling", [1, 1, 1]),
                "euler_degrees": cfg.get("euler_angles_in_degrees", [0, 0, 0]),
            }
            if cfg.get("use_quaternion"):
                transform["quaternion"] = cfg.get("quaternion", [0, 0, 0, 1])

        elif act == "ext-fx-standard-pipeline/add-model-from-template":
            material_mapping = []
            for item in cfg.get("material_mapping", []):
                name = item.get("name")
                override_node = item.get("override_node")
                if name and override_node:
                    material_mapping.append({"name": name, "node": override_node})

    if not title:
        title = element_json.stem.split(".")[0]

    return {
        "title": title,
        "transform": transform,
        "material_mapping": material_mapping or [],
    }


def parse_material_info(material_json):
    # type: (Path) -> dict
    """Extract blend_mode, textures, and values from a material JSON."""
    data = load_json(material_json)
    textures = {}
    for tex in data.get("textures", []):
        name = tex.get("name")
        path = tex.get("path")
        if name and path:
            textures[name] = path

    values = {}
    for v in data.get("values", []):
        name = v.get("name")
        val = v.get("value")
        if name:
            values[name] = val

    return {
        "blend_mode": data.get("blend_mode", "opaque"),
        "textures": textures,
        "values": values,
        "two_sided": data.get("two_sided", False),
        "shader": data.get("material_function_include", ""),
    }


# ---------------------------------------------------------------------------
# Quaternion to rotation matrix (4x4 as flat list, row-major)
# ---------------------------------------------------------------------------

def quat_to_mat4(q):
    # type: (List[float]) -> List[float]
    """Convert quaternion [x, y, z, w] to 4x4 rotation matrix (row-major)."""
    x, y, z, w = q
    x2, y2, z2 = x + x, y + y, z + z
    xx, xy, xz = x * x2, x * y2, x * z2
    yy, yz, zz = y * y2, y * z2, z * z2
    wx, wy, wz = w * x2, w * y2, w * z2

    return [
        1.0 - yy - zz, xy + wz,       xz - wy,       0.0,
        xy - wz,       1.0 - xx - zz, yz + wx,       0.0,
        xz + wy,       yz - wx,       1.0 - xx - yy, 0.0,
        0.0,           0.0,           0.0,           1.0,
    ]


def euler_to_mat4(euler_deg):
    # type: (List[float]) -> List[float]
    """Convert Euler angles [x, y, z] in degrees to 4x4 rotation matrix (row-major)."""
    rx, ry, rz = [math.radians(a) for a in euler_deg]

    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)

    # R = Rz * Ry * Rx
    r00 = cy * cz
    r01 = sx * sy * cz - cx * sz
    r02 = cx * sy * cz + sx * sz
    r10 = cy * sz
    r11 = sx * sy * sz + cx * cz
    r12 = cx * sy * sz - sx * cz
    r20 = -sy
    r21 = sx * cy
    r22 = cx * cy

    return [
        r00, r01, r02, 0.0,
        r10, r11, r12, 0.0,
        r20, r21, r22, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]


# ---------------------------------------------------------------------------
# Full scene export
# ---------------------------------------------------------------------------

def build_mesh_to_element_map(scene_dir):
    # type: (Path) -> Dict[str, List[Path]]
    """Map mesh.json filenames to the element JSONs that reference them."""
    mesh_to_elements = {}  # type: Dict[str, List[Path]]
    for element_path in sorted(scene_dir.glob("*.element.json")):
        data = load_json(element_path)
        for action in data.get("actions", []):
            if action.get("action") == "include":
                cfg = action.get("config") or {}
                inc_path = cfg.get("path", "")
                if inc_path.endswith(".mesh.json"):
                    mesh_name = Path(inc_path).name
                    mesh_to_elements.setdefault(mesh_name, []).append(element_path)
    return mesh_to_elements


def convert_fy_full(src_root, dst_root, exporter_cmd):
    # type: (Path, Path, Optional[List[str]]) -> int
    """Full scene export: all models with transforms + materials -> scene.json."""
    if not src_root.is_dir():
        print("Source not found: {}".format(src_root), file=sys.stderr)
        return 1

    # Find scene directory (fs.*.scene/)
    scene_dirs = list(src_root.glob("fs.*.scene"))
    if not scene_dirs:
        print("No scene directory found", file=sys.stderr)
        return 1
    scene_dir = scene_dirs[0]
    print("Scene dir: {}".format(scene_dir.name))

    material_index = build_material_index(src_root / "material")
    mesh_jsons = sorted((src_root / "mesh").glob("*.mesh.json"))
    if not mesh_jsons:
        print("No mesh.json files found", file=sys.stderr)
        return 1

    # Build mesh -> element mapping
    mesh_to_elements = build_mesh_to_element_map(scene_dir)

    if dst_root.exists():
        shutil.rmtree(str(dst_root))
    dst_root.mkdir(parents=True)

    # Copy all textures to a shared textures/ dir
    tex_dst_dir = dst_root / "textures"
    tex_dst_dir.mkdir(exist_ok=True)
    for tex in (src_root / "texture").glob("*.png"):
        shutil.copy2(str(tex), str(tex_dst_dir / tex.name))

    scene_models = []  # type: List[dict]

    for mesh_json in mesh_jsons:
        parsed = parse_mesh_json(mesh_json)
        if not parsed:
            continue

        fbx_name, slots = parsed
        part_name = re.sub(r"\.[0-9a-f]{8}(\.mesh)?$", "", mesh_json.stem, flags=re.I)

        # Find element JSONs that reference this mesh
        elements = mesh_to_elements.get(mesh_json.name, [])

        # Convert FBX to OBJ
        fbx_src = src_root / "mesh" / fbx_name
        if not fbx_src.is_file():
            print("  skip {}: missing {}".format(part_name, fbx_src))
            continue

        out_dir = dst_root / "parts" / part_name
        out_dir.mkdir(parents=True, exist_ok=True)

        # Copy textures for this part's materials
        slot_texture = {}  # type: Dict[str, str]
        for slot_name, node in slots:
            mat_hash = material_hash_from_node(node)
            mat_path = material_index.get(mat_hash)
            if not mat_path:
                continue
            tex_rel = pick_base_texture(load_json(mat_path))
            if not tex_rel:
                continue
            tex_src = (mat_path.parent / tex_rel).resolve()
            if tex_src.is_file():
                tex_dst_name = tex_src.name
                shutil.copy2(str(tex_src), str(out_dir / tex_dst_name))
                slot_texture[slot_name] = tex_dst_name

        obj_path = out_dir / "{}.obj".format(part_name)
        mtl_path = out_dir / "{}.mtl".format(part_name)
        write_mtl(mtl_path, slots, slot_texture)

        obj_ok = False
        if exporter_cmd:
            obj_ok = export_fbx_to_obj(exporter_cmd, fbx_src, obj_path)
            if obj_ok:
                patch_exported_mtl(obj_path, mtl_path)
                obj_text = obj_path.read_text(encoding="utf-8", errors="ignore")
                mtl_line = "mtllib {}\n".format(mtl_path.name)
                if "mtllib " not in obj_text:
                    obj_path.write_text(mtl_line + obj_text, encoding="utf-8")
                print("  ok {}.obj".format(part_name))
        if not obj_ok:
            shutil.copy2(str(fbx_src), str(out_dir / fbx_name))
            print("  partial {}: fbx copied".format(part_name))

        # Build model entry from element JSONs (or fallback with no transform)
        if elements:
            for elem_path in elements:
                elem_info = parse_element_json(elem_path)
                model_entry = build_model_entry(
                    part_name, elem_info, slots, material_index,
                    src_root, dst_root, obj_ok
                )
                if model_entry:
                    scene_models.append(model_entry)
        else:
            # No element JSON -> use defaults
            model_entry = build_model_entry(
                part_name, None, slots, material_index,
                src_root, dst_root, obj_ok
            )
            if model_entry:
                scene_models.append(model_entry)

    # Write scene.json
    scene_config = {"models": scene_models}
    scene_json_path = dst_root / "scene.json"
    with scene_json_path.open("w", encoding="utf-8") as f:
        json.dump(scene_config, f, indent=2, ensure_ascii=False)
    print("Wrote scene.json with {} models".format(len(scene_models)))

    # Also keep backward-compat: largest OBJ as fy.obj
    converted_objs = list((dst_root / "parts").rglob("*.obj"))
    if converted_objs:
        default_obj = max(converted_objs, key=lambda p: p.stat().st_size)
        fy_obj = dst_root / "fy.obj"
        shutil.copy2(str(default_obj), str(fy_obj))
        src_mtl = default_obj.with_suffix(".mtl")
        if src_mtl.is_file():
            shutil.copy2(str(src_mtl), str(dst_root / "fy.mtl"))
        for tex in default_obj.parent.glob("*.png"):
            shutil.copy2(str(tex), str(dst_root / tex.name))

    print("Converted {} parts -> {}".format(len(scene_models), dst_root))
    return 0


def build_model_entry(part_name, elem_info, slots, material_index,
                      src_root, dst_root, obj_ok):
    # type: (str, Optional[dict], List[Tuple[str, str]], Dict[str, Path], Path, Path, bool) -> Optional[dict]
    """Build one model entry for scene.json."""
    title = part_name
    transform = None
    materials = []

    if elem_info:
        title = elem_info.get("title") or part_name
        transform = elem_info.get("transform")
        mat_mapping = elem_info.get("material_mapping", [])

        # Build material info from the mapping
        for mm in mat_mapping:
            mat_name = mm["name"]
            mat_node = mm["node"]
            mat_hash = material_hash_from_node(mat_node)
            mat_path = material_index.get(mat_hash)
            if mat_path:
                mat_info = parse_material_info(mat_path)
                # Resolve texture paths relative to dst_root
                resolved_textures = {}
                for tex_name, tex_rel in mat_info["textures"].items():
                    tex_src = (mat_path.parent / tex_rel).resolve()
                    tex_in_shared = dst_root / "textures" / tex_src.name
                    if tex_in_shared.is_file():
                        resolved_textures[tex_name] = "textures/{}".format(tex_src.name)
                    else:
                        resolved_textures[tex_name] = tex_rel

                materials.append({
                    "name": mat_name,
                    "blend_mode": mat_info["blend_mode"],
                    "two_sided": mat_info["two_sided"],
                    "textures": resolved_textures,
                    "values": mat_info["values"],
                    "shader": mat_info["shader"],
                })
    else:
        # Fallback: use slots
        for slot_name, node in slots:
            mat_hash = material_hash_from_node(node)
            mat_path = material_index.get(mat_hash)
            if mat_path:
                mat_info = parse_material_info(mat_path)
                materials.append({
                    "name": slot_name,
                    "blend_mode": mat_info["blend_mode"],
                    "textures": mat_info["textures"],
                    "values": mat_info["values"],
                })

    model_path = "parts/{}/{}.obj".format(part_name, part_name)
    if not obj_ok:
        # FBX was copied instead
        mesh_jsons = list((src_root / "mesh").glob("{}.*.mesh.fbx".format(part_name)))
        if mesh_jsons:
            model_path = "parts/{}/{}".format(part_name, mesh_jsons[0].name)

    entry = {
        "name": title,
        "model_path": model_path,
    }

    if transform:
        entry["transform"] = transform
        # Pre-compute 4x4 model matrix for convenience
        t = transform
        pos = t.get("position", [0, 0, 0])
        scale = t.get("scaling", [1, 1, 1])

        if "quaternion" in t:
            rot = quat_to_mat4(t["quaternion"])
        else:
            rot = euler_to_mat4(t.get("euler_degrees", [0, 0, 0]))

        # M = T * R * S
        mat = [0.0] * 16
        for r in range(3):
            for c in range(3):
                mat[r * 4 + c] = rot[r * 4 + c] * scale[c]
        mat[12] = pos[0]
        mat[13] = pos[1]
        mat[14] = pos[2]
        mat[15] = 1.0
        entry["matrix"] = mat

    if materials:
        entry["materials"] = materials

    return entry


# ---------------------------------------------------------------------------
# Legacy single-model export (backward compat)
# ---------------------------------------------------------------------------

def convert_fy(src_root, dst_root, exporter_cmd):
    # type: (Path, Path, Optional[List[str]]) -> int
    if not src_root.is_dir():
        print("Source not found: {}".format(src_root), file=sys.stderr)
        return 1

    material_index = build_material_index(src_root / "material")
    mesh_jsons = sorted((src_root / "mesh").glob("*.mesh.json"))
    if not mesh_jsons:
        print("No mesh.json files found", file=sys.stderr)
        return 1

    if dst_root.exists():
        shutil.rmtree(str(dst_root))
    dst_root.mkdir(parents=True)

    converted = []  # type: List[Path]
    for mesh_json in mesh_jsons:
        obj = convert_part(src_root, dst_root, mesh_json, material_index, exporter_cmd)
        if obj:
            converted.append(obj)

    if not converted:
        print("No OBJ parts exported. Build tools/fbx2obj or install assimp-utils.", file=sys.stderr)
        return 2

    default_obj = max(converted, key=lambda p: p.stat().st_size)
    fy_obj = dst_root / "fy.obj"
    fy_mtl = dst_root / "fy.mtl"
    shutil.copy2(str(default_obj), str(fy_obj))
    src_mtl = default_obj.with_suffix(".mtl")
    if src_mtl.is_file():
        shutil.copy2(str(src_mtl), str(fy_mtl))

    for tex in default_obj.parent.glob("*.png"):
        shutil.copy2(str(tex), str(dst_root / tex.name))

    obj_text = fy_obj.read_text(encoding="utf-8", errors="ignore")
    obj_text = re.sub(
        r"^mtllib\s+.*$",
        "mtllib {}".format(fy_mtl.name),
        obj_text,
        count=1,
        flags=re.M,
    )
    if "mtllib " not in obj_text:
        obj_text = "mtllib {}\n".format(fy_mtl.name) + obj_text
    fy_obj.write_text(obj_text, encoding="utf-8")

    if fy_mtl.is_file():
        mtl_text = fy_mtl.read_text(encoding="utf-8")
        for tex in dst_root.glob("*.png"):
            mtl_text = mtl_text.replace("../{}".format(tex.name), tex.name)
        fy_mtl.write_text(mtl_text, encoding="utf-8")

    print("Converted {} parts -> {}".format(len(converted), dst_root))
    print("Default model: {} (from {})".format(fy_obj, default_obj.parent.name))
    return 0


def main():
    # type: () -> int
    if sys.version_info < (3, 6):
        print("Python 3.6+ required (found {}.{}.{})".format(*sys.version_info[:3]), file=sys.stderr)
        return 1

    parser = argparse.ArgumentParser(description="Convert milicon fy assets for load-model")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC)
    parser.add_argument("--dst", type=Path, default=DEFAULT_DST)
    parser.add_argument("--legacy", action="store_true",
                        help="Use legacy single-model export mode")
    args = parser.parse_args()

    exporter = find_assimp_exporter()
    if not exporter:
        print("Warning: no fbx2obj/assimp found; only partial conversion.", file=sys.stderr)
    else:
        print("Using exporter: {}".format(" ".join(exporter)))

    if args.legacy:
        return convert_fy(args.src, args.dst, exporter)
    return convert_fy_full(args.src, args.dst, exporter)


if __name__ == "__main__":
    sys.exit(main())
