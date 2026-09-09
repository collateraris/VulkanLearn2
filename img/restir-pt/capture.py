"""Capture the README's ReSTIR PT comparisons from the actual Windows renderer.

Requires a built bin/Release/vulkan_guide.exe, scene assets, NumPy and Pillow.
Run from the repository root: python img/restir-pt/capture.py
The user's assets/config.xml is only read. Temporary configurations, HDR files
and logs are kept under win64/readme-gallery; only PNGs and a manifest go here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, PngImagePlugin


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path(__file__).resolve().parent
MODES = {
    "no-accumulation": (False, False),
    "accumulation": (True, False),
    "accumulation-denoiser": (True, True),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"Not an RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        data = np.fromfile(stream, dtype="<f4" if scale < 0 else ">f4")
    if data.size != width * height * 3:
        raise ValueError(f"Incomplete capture: {path}")
    image = data.reshape(height, width, 3)[::-1] * abs(scale)
    if not np.isfinite(image).all() or np.any(image < 0):
        raise ValueError(f"Invalid or negative HDR values: {path}")
    return image


def save_display_png(hdr: np.ndarray, path: Path) -> None:
    # Match gbuffer_shading.frag.slang and the swapchain's sRGB encoding.
    # No per-image exposure, normalization, white balance, cropping or resizing.
    mapped = hdr.astype(np.float64) / (1.0 + hdr.astype(np.float64))
    mapped = np.power(mapped, 1.0 / 2.2)
    srgb = np.where(mapped <= 0.0031308, 12.92 * mapped,
                    1.055 * np.power(mapped, 1.0 / 2.4) - 0.055)
    pixels = np.rint(np.clip(srgb, 0.0, 1.0) * 255.0).astype(np.uint8)
    info = PngImagePlugin.PngInfo()
    info.add(b"sRGB", b"\x00")
    Image.fromarray(pixels).save(path, pnginfo=info, optimize=True)


def junction(link: Path, target: Path) -> None:
    if link.exists():
        if link.resolve() != target.resolve():
            raise RuntimeError(f"Existing path points elsewhere: {link}")
        return
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)],
                   check=True, capture_output=True)


def prepare_case(scene_id: int, original: bytes, view: str = "") -> tuple[Path, dict]:
    case_name = f"scene-{scene_id}" + (f"-{view}" if view else "")
    case = ROOT / "win64" / "readme-gallery" / case_name
    assets = case / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    (case / "bin" / "Release").mkdir(parents=True, exist_ok=True)
    config = ET.fromstring(original)
    scene = config.find(f"./scene_configs/scene[@id='{scene_id}']")
    if scene is None:
        raise ValueError(f"Unknown scene ID: {scene_id}")
    scene_settings = dict(scene.attrib)
    model = ROOT / "assets" / scene.get("path", "")
    if not model.is_file():
        raise FileNotFoundError(model)
    config.find("current_scene").set("id", str(scene_id))
    config.find("render_mode").set("name", "RESTIR")
    config.find("window").set("width", "1200")
    config.find("window").set("height", "800")
    # Config prepends its own relative assets directory to model/HDR paths.
    for entry in config.findall("./scene_configs/scene"):
        for attribute in ("path", "hdr"):
            value = entry.get(attribute)
            if value:
                entry.set(attribute, Path(os.path.relpath(ROOT / "assets" / value, assets)).as_posix())
    ET.indent(config)
    ET.ElementTree(config).write(assets / "config.xml", encoding="utf-8", xml_declaration=True)
    for directory in ("shaders", "shaders_slang", "third_party"):
        junction(case / directory, ROOT / directory)
    return case, scene_settings


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenes", type=int, nargs="+", default=[1, 2, 3, 5])
    parser.add_argument("--view", default="", help="Name a new view, e.g. view-2, to keep previous camera captures.")
    parser.add_argument("--modes", choices=list(MODES), nargs="+", default=list(MODES))
    parser.add_argument("--frame", type=int, default=512)
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    if os.name != "nt" or not 1 <= args.frame <= 10_000_000:
        parser.error("This capture runner requires Windows and a frame number in [1, 10000000].")
    if args.view and not re.fullmatch(r"[a-zA-Z0-9_-]+", args.view):
        parser.error("View names may contain only letters, digits, underscores and hyphens.")
    executable = ROOT / "bin" / "Release" / "vulkan_guide.exe"
    if not executable.is_file():
        raise FileNotFoundError(executable)
    original_path = ROOT / "assets" / "config.xml"
    original = original_path.read_bytes()
    manifest_path = OUTPUT / "captures.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    # Keep the original provenance when a later run adds a different camera.
    source_fields = ("config_sha256", "executable_sha256", "shader_sha256")
    for capture in manifest.get("captures", {}).values():
        for field in source_fields:
            if field in manifest:
                capture.setdefault(field, manifest[field])
    # Check the whole request before rendering so a changed camera never
    # silently replaces an existing gallery view under its old filename.
    config = ET.fromstring(original)
    for scene_id in args.scenes:
        scene = config.find(f"./scene_configs/scene[@id='{scene_id}']")
        if scene is None:
            parser.error(f"Unknown scene ID: {scene_id}")
        prefix = f"scene-{scene_id}" + (f"-{args.view}" if args.view else "")
        for mode in args.modes:
            previous = manifest.get("captures", {}).get(f"{prefix}-{mode}.png")
            if previous and previous["scene_settings"] != dict(scene.attrib):
                parser.error(f"{prefix} has different saved scene/camera settings; use --view with a new name.")
    manifest.update({
        "render_mode": "RESTIR", "resolution": [1200, 800],
        "frame": args.frame, "indirect_bounce_limit": 3, "jitter_seed": 0,
        "config_sha256": hashlib.sha256(original).hexdigest(),
        "executable_sha256": sha256(executable),
        "display_transfer": "linear HDR -> Reinhard x/(1+x) -> pow(1/2.2) -> sRGB",
        "no_accumulation_note": "One output frame after reservoir warmup; temporal/spatial reservoir reuse remains enabled.",
        "captures": manifest.get("captures", {}),
        "provenance_note": "Top-level hashes describe the latest run; each capture retains its own settings and source hashes.",
    })
    shader_names = ["restir_di_start.rgen.slang.spv", "restir_start.rgen.slang.spv",
                    "restirPTTemporal.comp.slang.spv", "restirPTSpacial.rgen.slang.spv",
                    "restirShade.comp.slang.spv", "denoise_prefilter.comp.slang.spv",
                    "spatial_denoise.comp.slang.spv", "denoise_temporal.comp.slang.spv",
                    "gbuffer_shading.frag.slang.spv"]
    manifest["shader_sha256"] = {name: sha256(ROOT / "shaders_slang" / name) for name in shader_names}
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    for scene_id in args.scenes:
        case, settings = prepare_case(scene_id, original, args.view)
        for mode in args.modes:
            accumulation, denoiser = MODES[mode]
            capture_dir = case / mode
            capture_dir.mkdir(exist_ok=True)
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("RESTIR_")}
            env.update({
                "RESTIR_ACCUMULATION": "1" if accumulation else "0",
                "RESTIR_DENOISER": "1" if denoiser else "0",
                "RESTIR_DIAGNOSTICS_FRAMES": str(args.frame),
                "RESTIR_DIAGNOSTICS_MAX_FRAMES": str(args.frame),
                "RESTIR_DIAGNOSTICS_OUTPUT": str(capture_dir),
            })
            print(f"Scene {scene_id}, {mode}, frame {args.frame}: rendering...", flush=True)
            with (case / f"{mode}.stdout.log").open("w") as out, (case / f"{mode}.stderr.log").open("w") as err:
                process = subprocess.Popen([str(executable)], cwd=case / "bin" / "Release",
                                           env=env, stdout=out, stderr=err, startupinfo=startup)
                try:
                    result = process.wait(timeout=args.timeout)
                except BaseException:
                    process.terminate()
                    process.wait()
                    raise
            if result:
                raise RuntimeError(f"Renderer exited with {result}; inspect {case / (mode + '.stderr.log')}")
            pfm = capture_dir / f"frame_{args.frame:06d}.pfm"
            hdr = read_pfm(pfm)
            if hdr.shape != (800, 1200, 3):
                raise ValueError(f"Unexpected capture size: {hdr.shape}")
            prefix = f"scene-{scene_id}" + (f"-{args.view}" if args.view else "")
            png = OUTPUT / f"{prefix}-{mode}.png"
            save_display_png(hdr, png)
            luminance = hdr.astype(np.float64) @ np.array([0.2126, 0.7152, 0.0722])
            manifest["captures"][png.name] = {
                "scene_id": scene_id, "view": args.view, "scene_settings": settings,
                **{field: manifest[field] for field in source_fields},
                "frame": args.frame, "frame_accumulation": accumulation, "denoiser": denoiser,
                "averaged_frames": args.frame if accumulation else 1,
                "pfm": pfm.relative_to(ROOT).as_posix(), "pfm_sha256": sha256(pfm),
                "png_sha256": sha256(png), "mean_luminance": float(luminance.mean()),
                "finite_pixels": 1200 * 800,
            }
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            print(f"Saved {png.relative_to(ROOT)} ({png.stat().st_size} bytes)", flush=True)
    if original_path.read_bytes() != original:
        raise RuntimeError("The user's configuration changed during capture; it was not overwritten.")


if __name__ == "__main__":
    main()
