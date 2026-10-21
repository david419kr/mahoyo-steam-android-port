"""Materialize exact Native primary/patch recipes into a NEW private prepared tree.

Inputs are immutable. This tool neither decodes HFA nor promotes its output to Stage.
"""


from __future__ import annotations


from concurrent.futures import ThreadPoolExecutor, as_completed


from copy import deepcopy


import hashlib


import io


import json


import os


from pathlib import Path, PurePosixPath


import re


import shutil


import uuid


import numpy as np


from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


OUTPUT_ROOT = ROOT / "build/work" / "native-bitmap-assets"


AUTHORITY = ROOT / "data" / "bitmap-composition-profile.json"


AUTHORITY_SHA = "96e0657557c322ee290fa607af8a5fd7f9d2fee5072d0e73a80ddfb381f5b548"


NAME = re.compile(r"[a-z0-9_]+\.mzp\Z")


GEOMETRY_FIELDS = ("width", "height", "storage_width", "storage_height", "tile_width",
                   "tile_height", "tile_border", "bitmap_type", "bitmap_depth")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_relative(value: str) -> Path:
    p = PurePosixPath(value)
    if not value or "\\" in value or p.is_absolute() or any(x in ("", ".", "..") for x in value.split("/")) or ":" in value:
        raise ValueError(f"unsafe prepared path: {value!r}")
    return Path(*p.parts)


def key_replace(primary: np.ndarray, patch: np.ndarray) -> np.ndarray:
    """Native 055F90: source A!=0 replaces RGBA, clipped at top-left minWH."""
    if primary.dtype != np.uint8 or patch.dtype != np.uint8 or primary.ndim != 3 or patch.ndim != 3 or primary.shape[2] != 4 or patch.shape[2] != 4:
        raise ValueError("RGBA uint8 arrays required")
    result = primary.copy()
    h, w = min(primary.shape[0], patch.shape[0]), min(primary.shape[1], patch.shape[1])
    dst, src = result[:h, :w], patch[:h, :w]
    mask = src[:, :, 3] != 0
    dst[mask] = src[mask]
    return result


def load_recipes(path: Path) -> tuple[dict, str]:
    authority_bytes = AUTHORITY.read_bytes()
    if sha(authority_bytes.replace(b'\r\n',b'\n')) != AUTHORITY_SHA:
        raise ValueError("Composition profile checksum mismatch")
    authority = json.loads(authority_bytes)
    raw = path.read_bytes()
    if sha(raw) != authority["generatedTable"]["sha256"]:
        raise ValueError("Native recipe table pin mismatch")
    table = json.loads(raw)
    if table.get("schema") != "woh.native.bitmap-records.v1" or table.get("targetSHA256") != authority["targetSHA256"]:
        raise ValueError("Native recipe identity mismatch")
    records = table["records"]
    keys = [r["key"] for r in records]
    if len(keys) != 10821 or keys != sorted(set(keys)) or sum(r["alias"] is not None for r in records) != 3818:
        raise ValueError("Native recipe inventory incomplete")
    for r in records:
        if not NAME.fullmatch(r["key"]) or not NAME.fullmatch(r["primary"]) or (r["alias"] is not None and (r["alias"] != r["key"] or not NAME.fullmatch(r["alias"]))):
            raise ValueError("invalid Native recipe name")
    return table, AUTHORITY_SHA


def _read_image(source: Path, manifest: dict, name: str) -> tuple[np.ndarray, dict, str]:
    record = manifest["assets"].get(name)
    if not record or record.get("kind") != "image" or "nativeComposite" in record:
        raise ValueError(f"missing/ambiguous RAW image metadata: {name}")
    if record.get("path") != f"images/{name}.png":
        raise ValueError(f"ambiguous RAW image allocation path: {name}")
    raw = (source / safe_relative(record["path"])).read_bytes()
    digest = sha(raw)
    if digest != record.get("sha256") or len(raw) != record.get("bytes"):
        raise ValueError(f"RAW image fingerprint mismatch: {name}")
    with Image.open(io.BytesIO(raw)) as image:
        image.load()
        if image.format != "PNG" or image.size != (record["width"], record["height"]):
            raise ValueError(f"RAW PNG geometry mismatch: {name}")
        pixels = np.array(image.convert("RGBA"), dtype=np.uint8)
    return pixels, record, digest


def _atomic_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        with temporary.open("xb") as out:
            out.write(data)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def materialize(source: Path, recipes: Path, output_parent: Path, names=(), workers: int = 4) -> dict:
    if not 1 <= workers <= 4:
        raise ValueError("workers must be in 1..4")
    source, output_parent = source.resolve(strict=True), output_parent.resolve()
    allowed = OUTPUT_ROOT.resolve()
    if not output_parent.is_relative_to(allowed) or output_parent.is_relative_to(source) or source.is_relative_to(output_parent):
        raise ValueError("output parent must be separate and under build/work/native-bitmap-assets")
    table, authority_sha = load_recipes(recipes)
    raw_manifest = (source / "assets.json").read_bytes()
    manifest = json.loads(raw_manifest)
    if manifest.get("nativeBitmapCandidate"):
        raise ValueError("input must be immutable RAW prepared source, not a composite candidate")
    pairs = {r["key"]: r for r in table["records"] if r["alias"] is not None}
    input_manifest = deepcopy(manifest)
    requested = set(names)
    if any(not NAME.fullmatch(n) for n in requested):
        raise ValueError("only exact lowercase full .mzp keys are accepted")
    selected = sorted(requested & pairs.keys()) if requested else sorted(pairs)
    for name in requested:
        if name not in manifest["assets"]:
            raise ValueError(f"requested input missing: {name}")
    # Never hardlink a selected alias; HOLD cannot silently fall back to its raw patch.
    excluded = {f"images/{n}.png" for n in selected}
    excluded.update(safe_relative(manifest["assets"][n]["path"]).as_posix() for n in selected if n in manifest["assets"])
    output_parent.mkdir(parents=True, exist_ok=True)
    output = output_parent / ("candidate-" + uuid.uuid4().hex)
    output.mkdir()
    linked, copied = 0, 0
    for directory, dirs, files in os.walk(source, followlinks=False):
        base = Path(directory)
        if any((base / d).is_symlink() or (base / d).is_junction() for d in dirs):
            raise ValueError("input tree must not contain directory links")
        relative = base.relative_to(source)
        (output / relative).mkdir(exist_ok=True)
        for filename in files:
            src = base / filename
            rel = (relative / filename).as_posix()
            if src.is_symlink():
                raise ValueError("input tree must not contain symbolic files")
            if rel == "assets.json" or rel in excluded or rel == "meta/native-composite-geometry.tsv":
                continue
            dst = output / relative / filename
            try:
                os.link(src, dst)
                linked += 1
            except OSError:
                shutil.copyfile(src, dst)
                copied += 1
    results, holds = [], []
    def convert(name):
        recipe = pairs[name]
        primary, primary_record, primary_sha = _read_image(source, input_manifest, recipe["primary"])
        patch, patch_record, patch_sha = _read_image(source, input_manifest, recipe["alias"])
        pixels = key_replace(primary, patch)
        buffer = io.BytesIO()
        Image.fromarray(pixels).save(buffer, format="PNG", compress_level=3, optimize=False)
        raw = buffer.getvalue()
        provenance = {"logicalKey": name, "rowIndex": recipe["rowIndex"],
                      "primary": recipe["primary"], "patch": recipe["alias"],
                      "primaryInputSHA256": primary_sha, "patchInputSHA256": patch_sha,
                      "primaryInputSource": primary_record.get("source"), "patchInputSource": patch_record.get("source"),
                      "primaryRawSHA256": primary_record.get("source_sha256"), "patchRawSHA256": patch_record.get("source_sha256"),
                      "oldPatchWH": [patch.shape[1], patch.shape[0]], "newPrimaryWH": [primary.shape[1], primary.shape[0]],
                      "outputPixelSHA256": sha(pixels.tobytes()), "outputFileSHA256": sha(raw),
                      "nativeAuthoritySHA256": authority_sha, "nativeTargetSHA256": table["targetSHA256"]}
        record = deepcopy(patch_record)
        for field in GEOMETRY_FIELDS:
            record.pop(field, None)
            if field in primary_record:
                record[field] = primary_record[field]
        record.update(sha256=sha(raw), bytes=len(raw), nativeComposite=provenance)
        _atomic_bytes(output / safe_relative(record["path"]), raw)
        return name, record, provenance
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(convert, n): n for n in selected}
        for future in as_completed(futures):
            name = futures[future]
            try:
                name, record, provenance = future.result()
                manifest["assets"][name] = record
                results.append(provenance)
            except Exception as error:
                manifest["assets"].pop(name, None)
                holds.append({"logicalKey": name, "status": "HOLD", "reason": str(error)})
            completed = len(results) + len(holds)
            if completed % 100 == 0 or completed == len(selected):
                print(f"native bitmap {completed}/{len(selected)} composed={len(results)} HOLD={len(holds)}", flush=True)
    results.sort(key=lambda r: r["logicalKey"])
    holds.sort(key=lambda r: r["logicalKey"])
    geometry = f"WOHCOMPOSITEGEOMETRY1 {table['targetSHA256']} {authority_sha} {len(results)}\n"
    geometry += "".join(f'"{r["logicalKey"]}" {r["oldPatchWH"][0]} {r["oldPatchWH"][1]} {r["newPrimaryWH"][0]} {r["newPrimaryWH"][1]}\n' for r in results)
    _atomic_bytes(output / "meta/native-composite-geometry.tsv", geometry.encode("ascii"))
    ready = len(selected) == len(pairs) and not holds
    candidate = {"schema": 1, "sourceImmutableRoot": str(source), "sourceManifestSHA256": sha(raw_manifest),
                 "nativeAuthoritySHA256": authority_sha, "recipeTableSHA256": sha(recipes.read_bytes()),
                 "selectedPairs": len(selected), "composed": len(results), "holds": holds,
                 "stageReady": ready, "workers": workers, "pngCompression": 3,
                 "hardlinkedFiles": linked, "copiedFiles": copied}
    manifest["nativeBitmapCandidate"] = candidate
    _atomic_bytes(output / "assets.json", (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    _atomic_bytes(output / "native-composite-provenance.jsonl", "".join(json.dumps(r, sort_keys=True) + "\n" for r in results).encode("utf-8"))
    # Verify source manifest unchanged; image files were only opened for reading.
    if sha((source / "assets.json").read_bytes()) != candidate["sourceManifestSHA256"]:
        raise ValueError("immutable input manifest changed during materialization")
    report = dict(candidate, output=str(output), manifestSHA256=sha((output / "assets.json").read_bytes()),
                  geometrySHA256=sha(geometry.encode("ascii")), outputAssetBytes=sum(r["bytes"] for r in manifest["assets"].values()))
    _atomic_bytes(output / "native-composite-report.json", (json.dumps(report, indent=2) + "\n").encode())
    return report


