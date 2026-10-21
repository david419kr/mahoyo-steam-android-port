"""Extract reviewed first-play animation keyframes from the owned native PE.

No game executable is run. Missing numeric fields retain original sentinels.
Only explicitly requested effects are emitted. Loop accessors use the complete
hash-bound integer interpreter; this does not authorize unsupported render data.
"""


import argparse


import hashlib


import json


from pathlib import Path


import struct


import weakref


import pefile


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


INDEX_TABLE_RVA = 0xB8DC8


DESCRIPTORS_RVA = 0x26CAE0


FRAMES_RVA = 0x3C68A0


EFFECT_COUNT = 8372


FRAME_COUNT = 28141


WINDOW_COLUMNS = ("effect", "localFrame", "globalFrame", "descriptorBits",
                  "windowX", "windowY", "windowWidth", "windowHeight",
                  "windowColorMode", "softEdgeWidth")


PROPERTIES_COLUMNS = ("effect", "localFrame", "globalFrame", "descriptorBits",
                      "originX", "originY", "blendMode", "colorMode", "colorR",
                      "colorG", "colorB", "variantX", "variantY", "brightness",
                      "contrast", "auxResourceAndScale", "priority")


PROPERTIES_FIELD_IDS = (6, 7, *range(17, 28))


PROPERTIES_OFFSETS = (0x0e, 0x10, *range(0x28, 0x3e, 2))


WINDOW_FIELD_EVIDENCE = None


WINDOW_CONSUMER_EVIDENCE = None


PROPERTIES_SETUP_EVIDENCE = None


_LOOP_MACHINES = weakref.WeakKeyDictionary()


def _read_exact(pe, rva, size):
    data = pe.get_data(rva, size)
    if len(data) != size:
        raise ValueError(f"Truncated native data at RVA{rva:#x}")
    return data


def loop_start(pe, effect):
    if type(effect) is not int or not 1 <= effect <= EFFECT_COUNT:
        raise ValueError("Loop accessor requires actual effect ID1..8372")
    # Complete A44D0 entry/return paths were independently checked against two
    # fresh native emulators for every actual ID. No unlisted/default-zero case.
    # Cache is object-scoped: a new PE object must pass the whole-file identity.
    if pe not in _LOOP_MACHINES:
        from effect_accessor import IntegerMachine, BASE
        if hashlib.sha256(pe.__data__).hexdigest() != TARGET_SHA256 or \
                pe.OPTIONAL_HEADER.ImageBase != BASE:
            raise ValueError("Native PE identity differs from full loop-accessor proof")
        _LOOP_MACHINES[pe] = IntegerMachine(pe.get_memory_mapped_image())
    machine = _LOOP_MACHINES[pe]
    from effect_accessor import LOOP_RVA
    value, path = machine.call(LOOP_RVA, effect)
    # Preserve the evidence API: nonzero points to the last literal assignment;
    # zero points to the final EAX<-R8D return instruction, not a guessed default.
    assignments = [rva for rva in path if pe.get_data(rva, 2) == b"\x41\xb8"]
    return value, assignments[-1] if assignments else path[-2]


def extract(binary, output, effects, evidence, window_output=None, properties_output=None):
    requested = list(effects)
    if not requested or any(type(effect) is not int or not 1 <= effect <= EFFECT_COUNT
                            for effect in requested):
        raise ValueError("Effect IDs must be integers within the reviewed table")
    requested = sorted(set(requested))
    destinations = [output.resolve(), evidence.resolve()]
    if window_output is not None:
        destinations.append(window_output.resolve())
    if properties_output is not None:
        destinations.append(properties_output.resolve())
    if binary.resolve() in destinations or len(set(destinations)) != len(destinations):
        raise ValueError("Native input and output paths must all be distinct")
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("Native PE identity differs; fixed RVAs cannot be reused")
    pe = pefile.PE(data=raw)
    targets = struct.unpack(f"<{EFFECT_COUNT}I", _read_exact(pe, INDEX_TABLE_RVA, EFFECT_COUNT * 4))
    ends = [0]
    for target in targets:
        code = _read_exact(pe, target, 8)
        if code[:1] != b"\xba" or code[5:] != b"\x8b\xc2\xc3":
            raise ValueError(f"Unexpected index accessor at RVA{target:#x}")
        ends.append(struct.unpack_from("<I", code, 1)[0])
    if ends[-1] != FRAME_COUNT or any(a > b for a, b in zip(ends, ends[1:])):
        raise ValueError("Animation boundaries are not monotonic or have wrong final index")
    lines = ["# effect\ttime\tx\ty\talpha\tscaleX\tscaleY\trotation\tcurve\tmode\tloopStart\tduration\n"]
    window_lines = ["# " + "\t".join(WINDOW_COLUMNS) + "\n"]
    properties_lines = ["# " + "\t".join(PROPERTIES_COLUMNS) + "\n"]
    proofs = []
    for effect in requested:
        begin, end = ends[effect - 1:effect + 1]
        if not 0 <= begin < end <= FRAME_COUNT:
            raise ValueError(f"Empty or out-of-bounds native frame range for effect{effect}")
        duration = struct.unpack("<I", _read_exact(pe, FRAMES_RVA + (end - 1) * 64, 4))[0]
        loop, loop_rva = loop_start(pe, effect)
        descriptor = struct.unpack("<Q", _read_exact(pe, DESCRIPTORS_RVA + effect * 8, 8))[0]
        row_proofs = []
        previous_time = -1
        for index in range(begin, end):
            frame = _read_exact(pe, FRAMES_RVA + index * 64, 64)
            time = struct.unpack_from("<I", frame, 0)[0]
            curve, mode, x, y, alpha = struct.unpack_from("<5h", frame, 4)
            rotation = struct.unpack_from("<i", frame, 20)[0]
            scale_x, scale_y = struct.unpack_from("<2h", frame, 24)
            if time < previous_time:
                raise ValueError(f"Effect{effect} has descending frame times")
            previous_time = time
            lines.append("\t".join(map(str, [effect, time, x, y, alpha, scale_x, scale_y,
                                                rotation, curve, mode, loop, duration])) + "\n")
            row_proofs.append({"index": index, "rva": hex(FRAMES_RVA + index * 64),
                               "raw_hex": frame.hex()})
            if window_output is not None:
                # Native field11..16 are signed XYWH/color/softness, not LTRB.
                window = struct.unpack_from("<6h", frame, 0x1c)
                window_lines.append("\t".join(map(str, [effect, index - begin, index,
                                                       hex(descriptor), *window])) + "\n")
            if properties_output is not None:
                # Fields6/7 are image-origin coordinates, not host259 offsets.
                properties = (struct.unpack_from("<2h", frame, 0x0e) +
                              struct.unpack_from("<11h", frame, 0x28))
                properties_lines.append("\t".join(map(str, [effect, index - begin,
                    index, hex(descriptor), *properties])) + "\n")
        proofs.append({"effect": effect, "descriptor": hex(descriptor), "frame_begin": begin,
                       "frame_end_exclusive": end, "duration_ms": duration,
                       "loop_start_ms": loop, "loop_accessor_rva": hex(loop_rva),
                       "frames": row_proofs})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(lines), encoding="utf-8")
    evidence.parent.mkdir(parents=True, exist_ok=True)
    proof = {"target_sha256": digest, "index_table_rva": hex(INDEX_TABLE_RVA),
             "frames_rva": hex(FRAMES_RVA), "effects": proofs}
    if window_output is not None:
        payload = "".join(window_lines).encode("utf-8")
        window_output.parent.mkdir(parents=True, exist_ok=True)
        window_output.write_bytes(payload)
        proof["window_sidecar"] = {"schema_version": 1, "columns": WINDOW_COLUMNS,
            "original_sha256": digest, "global_frame_index_base": 0,
            "local_frame_index_base": 0, "descriptor_type": "raw uint64 bitmask, not pointer",
            "field_type": "signed int16, original sentinel retained",
            "field_offsets": ["0x1c", "0x1e", "0x20", "0x22", "0x24", "0x26"],
            "field_evidence_id": WINDOW_FIELD_EVIDENCE,
            "consumer_evidence_id": WINDOW_CONSUMER_EVIDENCE,
            "row_count": len(window_lines) - 1, "requested_effect_ids": requested,
            "output_sha256": hashlib.sha256(payload).hexdigest(), "output_bytes": len(payload)}
    if properties_output is not None:
        payload = "".join(properties_lines).encode("utf-8")
        properties_output.parent.mkdir(parents=True, exist_ok=True)
        properties_output.write_bytes(payload)
        proof["properties_sidecar"] = {"schema_version": 1, "columns": PROPERTIES_COLUMNS,
            "original_sha256": digest, "global_frame_index_base": 0,
            "local_frame_index_base": 0, "descriptor_type": "raw uint64 bitmask, not pointer",
            "field_type": "signed int16, original sentinel retained",
            "field_ids": PROPERTIES_FIELD_IDS,
            "field_offsets": [hex(offset) for offset in PROPERTIES_OFFSETS],
            "field_evidence_id": WINDOW_FIELD_EVIDENCE,
            "setup_evidence_id": PROPERTIES_SETUP_EVIDENCE,
            "consumer_evidence_id": WINDOW_CONSUMER_EVIDENCE,
            "origin_coordinates": "resource image anchor, distinct from host259 target offsets",
            "runtime_support_claim": False,
            "row_count": len(properties_lines) - 1, "requested_effect_ids": requested,
            "output_sha256": hashlib.sha256(payload).hexdigest(), "output_bytes": len(payload)}
    evidence.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    return len(lines) - 1


