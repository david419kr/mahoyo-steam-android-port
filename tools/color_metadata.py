"""Recover and reference the native pixel transform, separate from layer blend.

The small mode3 LUT contains numerical transform metadata, not game imagery.
Source bytes are supplied by the owner and remain outside version control.
"""


import argparse


import hashlib


import json


from pathlib import Path


import pefile


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


TABLE_RVA = 0x3B4040


def transform(rgb, mode, brightness=0, contrast=0, solid=(0, 0, 0), lut=None):
    """Reference byte RGB output of FUN1400a2d70; alpha is unchanged."""
    if contrast > 256:
        raise ValueError("Native positive-contrast table exceeds its checked256-entry range")
    if mode == 1:
        result = solid
    elif mode in (2, 3):
        gray = int((161 * rgb[0] + 548 * rgb[1] + 56 * rgb[2] + 382.5) / 765.0)
        if mode == 2:
            result = (gray, gray, gray)
        else:
            if lut is None or len(lut) != 768:
                raise ValueError("Mode3 requires the exact 256-entry RGB LUT")
            result = tuple(lut[gray * 3:gray * 3 + 3])
    elif mode == 4:
        result = tuple(255 - value for value in rgb)
    elif 8 <= mode <= 82:
        if lut is None or len(lut) != 768:
            raise ValueError("Channel color modes require the exact 256-entry RGB LUT")
        result = tuple(lut[value * 3 + channel] for channel, value in enumerate(rgb))
    elif mode == 0:
        result = rgb
    else:
        raise ValueError("This reference implements native modes 0..4 and8..82; gray-palette modes5..7 remain unsupported")
    result = tuple(max(0, min(255, value + brightness)) for value in result)
    if contrast > 0:
        return tuple(0 if value < contrast else
                     int((value - contrast) * 255 / (255 - 2 * contrast))
                     if value < 255 - contrast else 255 for value in result)
    if contrast < 0:
        # C++ signed integer division truncates toward zero.
        return tuple((int(value * (255 + 2 * contrast) / 255) - contrast) & 255
                     for value in result)
    return result


def extract(binary: Path, output: Path, metadata: Path,
            channel_output: Path = Path("build/work/android-package/meta")):
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("The native PE differs; fixed palette RVAs cannot be reused")
    pe = pefile.PE(data=raw)
    channels = [pe.get_data(TABLE_RVA + (3 * 3 + channel) * 256, 256)
                for channel in range(3)]
    lut = bytes(value for index in range(256) for value in
                (channels[0][index], channels[1][index], channels[2][index]))
    # Native mode2 is identity after weighted grayscale; mode4 inverts channels.
    for mode in (2, 4):
        expected = bytes(index if mode == 2 else 255 - index for index in range(256))
        for channel in range(3):
            if pe.get_data(TABLE_RVA + (mode * 3 + channel) * 256, 256) != expected:
                raise ValueError(f"Unexpected native palette at mode{mode}/channel{channel}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(lut)
    channel_output.mkdir(parents=True, exist_ok=True)
    channel_modes = []
    for mode in range(8, 83):
        source_rvas = [TABLE_RVA + (mode * 3 + channel) * 256 for channel in range(3)]
        tables = [pe.get_data(rva, 256) for rva in source_rvas]
        if any(len(table) != 256 for table in tables):
            raise ValueError(f"Truncated native color table for mode {mode}")
        palette = bytes(value for index in range(256) for value in
                        (tables[0][index], tables[1][index], tables[2][index]))
        path = channel_output / f"color{mode}-rgb.bin"
        path.write_bytes(palette)
        channel_modes.append({"mode": mode, "path": str(path), "bytes": len(palette),
                              "layout": "256 interleaved RGB entries; each channel indexes its original channel byte",
                              "sha256": hashlib.sha256(palette).hexdigest(),
                              "source_channel_rvas": [hex(rva) for rva in source_rvas]})
    data = {
        "target_sha256": digest,
        "procedure_va": "0x1400a2d70",
        "mode3_lut": {"path": output.name, "bytes": len(lut), "layout": "256 interleaved RGB byte entries",
                      "sha256": hashlib.sha256(lut).hexdigest(),
                      "source_channel_rvas": [hex(TABLE_RVA + (9 + channel) * 256) for channel in range(3)]},
        "defaults": {"mode": 0, "brightness": 0, "contrast": 0},
        "channel_modes": channel_modes,
        "operation_order": ["native mode palette/solid/grayscale", "brightness byte clamp", "contrast LUT"],
        "grayscale": "floor((161*R+548*G+56*B+382.5)/765); integer equivalent=(161*R+548*G+56*B+382)/765",
        "contrast_positive": "0 if x<c; floor((x-c)*255/(255-2*c)) if x<255-c; otherwise255",
        "contrast_negative": "trunc_toward_zero(x*(255+2*c)/255)-c, cast tobyte",
        "brightness": "clamp(channel+brightness,0,255)",
        "alpha": "preserve original byte; transform happens before image resampling",
    }
    metadata.parent.mkdir(parents=True, exist_ok=True)
    metadata.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


