"""Prepare an incremental, lossless first-play asset subset from owned HFA files.

No copyrighted asset or external decoder is distributed with this tool. The
source installation is opened read-only and never becomes the output directory.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import struct
import sys
import time
from typing import Iterable

from asset_codecs import (AssetError, decode_cbg, decode_lenzu, decode_mzp,
                          read_ccit, unwrap_audio)
from hfa import Entry, FormatError, read_at, read_index

CONVERTER_VERSION = 2
TITLE_ASSETS = (
    "title_bg1.cbg", "title_bg2.cbg", "title1st_b.cbg", "title1st_w.cbg",
    "title_menu_ja.cbg", "title_titlelogo_ja.cbg", "title_titlelogo_b_ja.cbg",
    "titlecursor_a.cbg", "titlecursor_b.cbg", "title_since.cbg", "logo.mp4",
)
METADATA_EXTENSIONS = (".chs", ".ctd", ".ccit")


@dataclass(frozen=True)
class Source:
    archive: Path
    index: int
    entry: Entry


def canonical_name(name: str) -> str:
    normalized = name.replace("\\", "/").lower()
    path = PurePosixPath(normalized)
    if (not name or path.is_absolute() or len(path.parts) != 1 or
            normalized in (".", "..") or ":" in normalized or
            any(ord(char) < 32 for char in normalized)):
        raise AssetError(f"unsafe/unsupported asset filename {name!r}")
    return normalized


def build_index(source: Path) -> tuple[dict[str, Source], list[dict]]:
    """Apply last match in an archive; later sorted archives override earlier."""
    index, archives = {}, []
    for path in sorted(source.glob("*.hfa")):
        size = path.stat().st_size
        with path.open("rb") as file:
            entries, meta = read_index(file, size)
        for number, entry in enumerate(entries):
            index[canonical_name(entry.name)] = Source(path, number, entry)
        archives.append({"name": path.name, "size": size,
                         "index_sha256": meta["index_sha256"]})
    if not archives:
        raise AssetError("no HFA archives in source directory")
    return index, archives


def _hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _atomic_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_bytes(data)
    temp.replace(path)


def _json_bytes(data) -> bytes:
    return (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def _convert(raw: bytes, name: str, output: Path, *, namespace: str = "") -> dict:
    if namespace and namespace != "ko":
        raise AssetError("unsupported resource namespace")
    prefix = namespace + "/" if namespace else ""
    suffix = Path(name).suffix
    if suffix in (".mzp", ".cbg"):
        if raw.startswith(b"mrgd00"):
            image, metadata = decode_mzp(raw)
        elif raw.startswith(b"CompressedBG_MT\0"):
            image, metadata = decode_cbg(raw)
        else:
            raise AssetError("unsupported image payload: " + name)
        relative = f"images/{prefix}{name}.png"
        path = output / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        temp = path.with_suffix(".png.tmp")
        image.save(temp, format="PNG", compress_level=6)
        temp.replace(path)
        return {"kind": "image", "path": relative, **metadata}
    if suffix == ".hw":
        data, metadata = unwrap_audio(raw)
        relative = f"audio/{prefix}{name}.ogg"
        _atomic_bytes(output / relative, data)
        return {"kind": "audio", "path": relative, **metadata}
    if suffix in METADATA_EXTENSIONS:
        # The Korean overlay ships already-decoded CHS/CTD/CCIT, unlike base HFA metadata.
        decoded = decode_lenzu(raw) if raw.startswith(b"LenZuCompressor\0") else raw
        if suffix == ".ccit":
            relative = f"fonts/{prefix}{name}.bin"
            metadata_path = f"fonts/{prefix}{name}.json"
            info = read_ccit(decoded)
            _atomic_bytes(output / relative, decoded)
            _atomic_bytes(output / metadata_path, _json_bytes(info))
            return {"kind": "font-map", "path": relative,
                    "metadata_path": metadata_path,
                    "character_count": info["character_count"],
                    "coordinate_count": info["coordinate_count"],
                    "decoded_sha256": hashlib.sha256(decoded).hexdigest()}
        if suffix == ".ctd":
            # Validate UTF-8 without changing the original CRLF indexing.
            decoded.decode("utf-8", "strict")
            relative, kind = f"text/{prefix}{name}", "text"
            lines = decoded.count(b"\r\n")
            extra = {"encoding": "utf-8", "crlf_count": lines}
        else:
            if not decoded.startswith(b"HunexCompiledScriptVer1.00\0"):
                raise AssetError("decoded CHS has an unsupported signature")
            relative, kind, extra = f"scripts/{prefix}{name}", "script", {}
        _atomic_bytes(output / relative, decoded)
        return {"kind": kind, "path": relative, **extra}
    raise AssetError(f"unsupported requested asset type: {name}")


def _video(source: Source, name: str, output: Path, *, namespace: str = "") -> dict:
    if namespace and namespace != "ko":
        raise AssetError("unsupported resource namespace")
    relative = f"video/{namespace + '/' if namespace else ''}{name}"
    path = output / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    with source.archive.open("rb") as src:
        prefix = read_at(src, source.entry.offset, 12, source.archive.stat().st_size)
        if prefix[4:8] != b"ftyp":
            raise AssetError("video does not start with ISO BMFF ftyp")
        src.seek(source.entry.offset)
        remaining = source.entry.size
        with temp.open("wb") as dst:
            while remaining:
                chunk = src.read(min(remaining, 1024 * 1024))
                if not chunk:
                    raise AssetError("truncated video segment")
                dst.write(chunk)
                remaining -= len(chunk)
    temp.replace(path)
    return {"kind": "video", "path": relative, "container": "isobmff"}


def prepare(source: Path, output: Path, names: Iterable[str], *,
            all_metadata: bool = False, title: bool = False,
            checkpoint_every: int = 1, checkpoint_seconds: float = 0.0) -> dict:
    if not isinstance(checkpoint_every, int) or checkpoint_every < 1:
        raise ValueError("checkpoint_every must be a positive integer")
    if (not isinstance(checkpoint_seconds, (int, float)) or
            not math.isfinite(checkpoint_seconds) or checkpoint_seconds < 0):
        raise ValueError("checkpoint_seconds must be finite and nonnegative")
    source, output = source.resolve(), output.resolve()
    if output == source or output.is_relative_to(source):
        raise AssetError("output must not be inside the original installation")
    index, archives = build_index(source)
    requested = {canonical_name(name) for name in names}
    if title:
        requested.update(TITLE_ASSETS)
    if all_metadata:
        requested.update(name for name in index if Path(name).suffix in METADATA_EXTENSIONS)
    missing = sorted(requested - index.keys())
    if missing:
        raise AssetError(f"requested assets not found: {missing}")
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "assets.json"
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    else:
        manifest = {"schema_version": 1, "design_size": [1920, 1080], "assets": {}}
    manifest["source_archives"] = archives
    manifest["archive_precedence"] = "last row wins; later sorted HFA wins (cross-HFA order provisional)"
    converted = reused = pending = 0
    last_checkpoint = time.monotonic()

    def checkpoint(force: bool = False) -> None:
        nonlocal pending, last_checkpoint
        if not force and (not pending or
                (pending < checkpoint_every and (not checkpoint_seconds or
                 time.monotonic() - last_checkpoint < checkpoint_seconds))):
            return
        _atomic_bytes(manifest_path, _json_bytes(manifest))
        pending = 0
        last_checkpoint = time.monotonic()

    name = "<final manifest>"
    try:
        for name in sorted(requested):
            item = index[name]
            stat = item.archive.stat()
            identity = {"archive": item.archive.name, "index": item.index,
                        "offset": item.entry.offset, "size": item.entry.size,
                        "archive_size": stat.st_size, "archive_mtime_ns": stat.st_mtime_ns}
            cached = manifest["assets"].get(name)
            if (cached and cached.get("source") == identity and
                    cached.get("converter_version") == CONVERTER_VERSION):
                cached_path = output / cached["path"]
                # Only this tool's safe relative names are accepted from the cache.
                if (cached_path.resolve().is_relative_to(output) and cached_path.is_file()
                        and _hash(cached_path) == cached["sha256"]):
                    reused += 1
                    checkpoint()
                    continue
            if Path(name).suffix == ".mp4":
                record = _video(item, name, output)
            else:
                if item.entry.size > 256 * 1024 * 1024:
                    raise AssetError("nonvideo member exceeds explicit 256 MiB input bound")
                with item.archive.open("rb") as src:
                    raw = read_at(src, item.entry.offset, item.entry.size, stat.st_size)
                record = _convert(raw, name, output)
                record["source_sha256"] = hashlib.sha256(raw).hexdigest()
            record.update(source=identity, converter_version=CONVERTER_VERSION)
            destination = output / record["path"]
            record.update(sha256=_hash(destination), bytes=destination.stat().st_size)
            manifest["assets"][name] = record
            converted += 1
            pending += 1
            checkpoint()
            print(f"prepared {name}: {record['kind']} {record['bytes']} bytes", flush=True)
        name = "<final manifest>"
        checkpoint(force=True)
    except BaseException as error:
        flush_error = None
        if pending:
            try:
                checkpoint(force=True)
            except Exception as failed:
                flush_error = failed
        detail = f"{name}: {error}"
        if flush_error is not None:
            detail += f"; completed-record checkpoint also failed: {flush_error}"
        if isinstance(error, KeyboardInterrupt):
            print(f"asset preparation interrupted at {detail}", file=sys.stderr, flush=True)
            raise
        if not isinstance(error, Exception):
            if flush_error is not None:
                print(detail, file=sys.stderr, flush=True)
            raise
        raise AssetError(detail) from error
    return {"converted": converted, "reused": reused,
            "asset_count": len(manifest["assets"]),
            "asset_bytes": sum(record["bytes"] for record in manifest["assets"].values()),
            "manifest": str(manifest_path)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--name", action="append", default=[], help="exact original asset filename; repeatable")
    parser.add_argument("--names-file", type=Path, help="UTF-8 JSON string list or one filename per line")
    parser.add_argument("--all-metadata", action="store_true", help="all CHS, CTD and CCIT only")
    parser.add_argument("--metadata-only", action="store_true", help="alias for --all-metadata")
    parser.add_argument("--title", action="store_true", help="initial Japanese title image candidates and logo movie")
    parser.add_argument("--native-bitmap-recipes", type=Path,
                        help="explicit immutable-prepared-input mode: pinned Native recipes; creates a new private candidate")
    parser.add_argument("--native-bitmap-workers", type=int, default=4, help="Native candidate mode workers (1..4)")
    parser.add_argument("--checkpoint-every", type=int, default=1,
                        help="atomic manifest checkpoint after this many completed records (default: 1)")
    parser.add_argument("--checkpoint-seconds", type=float, default=0.0,
                        help="also checkpoint at asset boundaries after this interval; 0 disables the timer")
    args = parser.parse_args()
    names = args.name
    if args.names_file:
        content = args.names_file.read_text(encoding="utf-8-sig")
        names += json.loads(content) if content.lstrip().startswith("[") else [
            line.strip() for line in content.splitlines() if line.strip() and not line.startswith("#")]
    try:
        if args.native_bitmap_recipes:
            if args.all_metadata or args.metadata_only or args.title:
                raise ValueError("Native prepared-input mode cannot be mixed with HFA selection flags")
            from bitmap_composition import materialize
            result = materialize(args.source, args.native_bitmap_recipes, args.output,
                                 names=names, workers=args.native_bitmap_workers)
        else:
            result = prepare(args.source, args.output, names,
                             all_metadata=args.all_metadata or args.metadata_only, title=args.title,
                             checkpoint_every=args.checkpoint_every,
                             checkpoint_seconds=args.checkpoint_seconds)
    except (AssetError, FormatError, OSError, ValueError, struct.error) as exc:
        parser.exit(1, f"asset preparation failed: {exc}\n")
    except KeyboardInterrupt:
        parser.exit(130)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
