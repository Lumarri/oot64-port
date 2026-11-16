#!/usr/bin/env python3
"""Verify a decrypted Majora's Mask 3D dump and extract Forge inputs.

Writes only to a local output directory (default: extracted/). Never copies
the cartridge image. The decompressed code.bin hash is the Moonfall Curse
identity (CUE/ctrtool backwards LZSS, displacement +3).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

MEDIA_UNIT = 0x200
EXPECTED_PRODUCT = "CTR-P-AJRP"
EXPECTED_PROGRAM_ID = "0004000000125600"
EXPECTED_NCCH_VERSION = 2
EXPECTED_EXHEADER_NAME = "Joker"
EXPECTED_CODE_SHA256 = (
    "6eec27b1827eab2c87744ac38066d569ffe6fe75790b1eb3300bd91f5099038d"
)
EXPECTED_CODE_CRC32 = 0xFFDF1EE8


class DumpError(RuntimeError):
    pass


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decompress_code(compressed: bytes) -> bytes:
    if len(compressed) < 8:
        raise DumpError("ExeFS .code is too small")
    extra = _u32(compressed, len(compressed) - 4)
    encinfo = _u32(compressed, len(compressed) - 8)
    header_len = encinfo >> 24
    enc_size = encinfo & 0xFFFFFF
    out_len = len(compressed) + extra
    if extra > 32 * 1024 * 1024 or out_len < len(compressed):
        raise DumpError("ExeFS .code declares an invalid decompressed size")
    if header_len < 8 or enc_size < header_len or enc_size > len(compressed):
        raise DumpError("ExeFS .code has an invalid BLZ footer")

    out = bytearray(out_len)
    out[: len(compressed)] = compressed
    src = len(compressed) - header_len
    dst = out_len
    end = len(compressed) - enc_size
    while src > end and dst > 0:
        src -= 1
        flags = compressed[src]
        for _ in range(8):
            if src <= end or dst <= 0:
                break
            if flags & 0x80:
                if src < 2:
                    raise DumpError("ExeFS .code back-reference is truncated")
                src -= 2
                info = compressed[src] | (compressed[src + 1] << 8)
                length = (info >> 12) + 3
                disp = (info & 0xFFF) + 3
                for _ in range(length):
                    dst -= 1
                    out[dst] = out[dst + disp]
            else:
                src -= 1
                dst -= 1
                out[dst] = compressed[src]
            flags = (flags << 1) & 0xFF
    return bytes(out)


def _read(stream, offset: int, size: int, label: str) -> bytes:
    stream.seek(offset)
    data = stream.read(size)
    if len(data) != size:
        raise DumpError(f"truncated ROM while reading {label}")
    return data


def inspect_dump(rom_path: Path) -> dict:
    size = rom_path.stat().st_size
    with rom_path.open("rb") as stream:
        ncsd = _read(stream, 0, 0x200, "NCSD header")
        if ncsd[0x100:0x104] != b"NCSD":
            raise DumpError("file is not an NCSD .3ds/.cci image")
        part0 = _u32(ncsd, 0x120) * MEDIA_UNIT
        part0_size = _u32(ncsd, 0x124) * MEDIA_UNIT
        if part0 == 0 or part0 + part0_size > size:
            raise DumpError("partition 0 is missing or truncated")

        ncch = _read(stream, part0, 0x200, "NCCH header")
        if ncch[0x100:0x104] != b"NCCH":
            raise DumpError("partition 0 is not NCCH")
        flags = ncch[0x188:0x190]
        if not (flags[7] & 0x4):
            raise DumpError("dump is still encrypted (NoCrypto flag is clear)")

        product = ncch[0x150:0x160].split(b"\x00", 1)[0].decode("ascii", "replace")
        program_id = ncch[0x118:0x120][::-1].hex()
        ncch_version = struct.unpack_from("<H", ncch, 0x112)[0]
        exefs_off = part0 + _u32(ncch, 0x1A0) * MEDIA_UNIT
        romfs_off = part0 + _u32(ncch, 0x1B0) * MEDIA_UNIT
        romfs_size = _u32(ncch, 0x1B4) * MEDIA_UNIT

        exheader = _read(stream, part0 + 0x200, 0x800, "ExHeader")
        name = exheader[0:8].split(b"\x00", 1)[0].decode("ascii", "replace")
        compressed = bool(exheader[0x0D] & 1)
        text_addr, _, text_size = struct.unpack_from("<III", exheader, 0x10)
        ro_addr, _, _ = struct.unpack_from("<III", exheader, 0x20)
        data_addr, _, _ = struct.unpack_from("<III", exheader, 0x30)

        exefs_hdr = _read(stream, exefs_off, 0x200, "ExeFS header")
        code_rel = code_size = None
        for index in range(8):
            entry = exefs_hdr[index * 16 : (index + 1) * 16]
            fname = entry[0:8].split(b"\x00", 1)[0]
            if fname == b".code":
                code_rel, code_size = struct.unpack_from("<II", entry, 8)
                break
        if code_rel is None:
            raise DumpError("no readable ExeFS .code; dump may still be encrypted")

        code_raw = _read(
            stream, exefs_off + 0x200 + code_rel, code_size, "ExeFS .code"
        )
        code = decompress_code(code_raw) if compressed else code_raw
        if b"joker-1_1_0" not in code:
            raise DumpError("code.bin is not MM3D build 1.1.0")

        romfs_magic = _read(stream, romfs_off, 4, "RomFS magic")
        if romfs_magic != b"IVFC":
            raise DumpError("RomFS is not decrypted IVFC")

        info = {
            "rom_path": str(rom_path),
            "product_code": product,
            "program_id": program_id,
            "ncch_version": ncch_version,
            "exheader_name": name,
            "no_crypto": True,
            "compressed_code": compressed,
            "text_address": text_addr,
            "text_size": text_size,
            "rodata_address": ro_addr,
            "data_address": data_addr,
            "code_sha256": _sha256(code),
            "code_crc32": format(zlib.crc32(code) & 0xFFFFFFFF, "08x"),
            "code_bytes": len(code),
            "exheader_sha256": _sha256(exheader),
            "exheader_offset": part0 + 0x200,
            "code_offset": exefs_off + 0x200 + code_rel,
            "code_raw_size": code_size,
            "romfs_offset": romfs_off,
            "romfs_size": romfs_size,
        }

        mismatches = []
        if product != EXPECTED_PRODUCT:
            mismatches.append(f"product {product}")
        if program_id != EXPECTED_PROGRAM_ID:
            mismatches.append(f"program id {program_id}")
        if ncch_version != EXPECTED_NCCH_VERSION:
            mismatches.append(f"NCCH version {ncch_version}")
        if name != EXPECTED_EXHEADER_NAME:
            mismatches.append(f"ExHeader name {name}")
        if info["code_sha256"] != EXPECTED_CODE_SHA256:
            mismatches.append("code.bin SHA-256")
        if zlib.crc32(code) & 0xFFFFFFFF != EXPECTED_CODE_CRC32:
            mismatches.append("code.bin CRC32")
        if mismatches:
            raise DumpError("dump is not the Moonfall Curse EUR 1.1.0 image: " + ", ".join(mismatches))
        info["moonfall_match"] = True
        return info


def _copy_region(stream, offset: int, size: int, dest: Path) -> str:
    digest = hashlib.sha256()
    remaining = size
    stream.seek(offset)
    with dest.open("wb") as out:
        while remaining:
            chunk = stream.read(min(remaining, 8 * 1024 * 1024))
            if not chunk:
                raise DumpError(f"ROM ended while writing {dest.name}")
            out.write(chunk)
            digest.update(chunk)
            remaining -= len(chunk)
    return digest.hexdigest()


def extract(rom_path: Path, output: Path, *, include_romfs: bool) -> dict:
    info = inspect_dump(rom_path)
    output.mkdir(parents=True, exist_ok=True)
    with rom_path.open("rb") as stream:
        exheader = _read(stream, info["exheader_offset"], 0x800, "ExHeader")
        raw = _read(stream, info["code_offset"], info["code_raw_size"], "ExeFS .code")
        code = decompress_code(raw) if info["compressed_code"] else raw
        (output / "exheader.bin").write_bytes(exheader)
        (output / "code.bin").write_bytes(code)
        info["exheader_path"] = "exheader.bin"
        info["code_path"] = "code.bin"
        if include_romfs:
            info["romfs_sha256"] = _copy_region(
                stream, info["romfs_offset"], info["romfs_size"], output / "romfs.bin"
            )
            info["romfs_path"] = "romfs.bin"
    manifest = {k: v for k, v in info.items() if not k.endswith("_offset") and k != "code_raw_size"}
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "rom",
        nargs="?",
        type=Path,
        help="Path to the decrypted .3ds/.cci dump",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("extracted/mm3d-eur-rev1"),
        help="Directory for exheader.bin, code.bin and romfs.bin",
    )
    parser.add_argument(
        "--no-romfs",
        action="store_true",
        help="Verify and extract code/ExHeader only",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Check identifiers and hashes without writing files",
    )
    args = parser.parse_args(argv)

    rom = args.rom
    if rom is None:
        roms = Path("roms")
        candidates = sorted(roms.glob("*.3ds")) + sorted(roms.glob("*.cci"))
        if len(candidates) != 1:
            raise DumpError("pass the ROM path, or put exactly one .3ds/.cci in roms/")
        rom = candidates[0]
    if not rom.is_file():
        raise DumpError(f"ROM not found: {rom}")

    if args.verify_only:
        info = inspect_dump(rom)
        json.dump(info, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    manifest = extract(rom, args.output, include_romfs=not args.no_romfs)
    json.dump(manifest, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except DumpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
