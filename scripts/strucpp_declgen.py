#!/usr/bin/env python3

import argparse
import pathlib
import sys


TYPE_LAYOUTS = {
    "BOOL": ("X", 1, True),
    "SINT": ("B", 1, False),
    "USINT": ("B", 1, False),
    "BYTE": ("B", 1, False),
    "INT": ("W", 2, False),
    "UINT": ("W", 2, False),
    "WORD": ("W", 2, False),
    "DINT": ("D", 4, False),
    "UDINT": ("D", 4, False),
    "DWORD": ("D", 4, False),
    "REAL": ("D", 4, False),
    "LINT": ("L", 8, False),
    "ULINT": ("L", 8, False),
    "LWORD": ("L", 8, False),
    "LREAL": ("L", 8, False),
}

AREAS = {"I", "Q", "M", "VAR"}


class AreaAllocator:
    def __init__(self):
        self.byte_offset = 0
        self.bit_offset = 0

    def allocate(self, area: str, type_name: str) -> str:
        layout = TYPE_LAYOUTS.get(type_name)
        if layout is None:
            raise RuntimeError(f"Unsupported IEC type '{type_name}'")
        size_code, width_bytes, is_bool = layout
        if is_bool:
            return self._allocate_bool(area)
        return self._allocate_scalar(area, size_code, width_bytes)

    def _allocate_bool(self, area: str) -> str:
        address = f"%{area}X{self.byte_offset}.{self.bit_offset}"
        self.bit_offset += 1
        if self.bit_offset >= 8:
            self.byte_offset += 1
            self.bit_offset = 0
        return address

    def _allocate_scalar(self, area: str, size_code: str, width_bytes: int) -> str:
        if self.bit_offset != 0:
            self.byte_offset += 1
            self.bit_offset = 0
        if width_bytes > 1 and self.byte_offset % width_bytes != 0:
            self.byte_offset += width_bytes - (self.byte_offset % width_bytes)
        address = f"%{area}{size_code}{self.byte_offset}"
        self.byte_offset += width_bytes
        return address


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate ST declarations with automatic %I/%Q/%M addresses from a small manifest."
    )
    parser.add_argument("--input", required=True, help="Input manifest path")
    parser.add_argument("--output", required=True, help="Output ST file path")
    parser.add_argument("--program", default="MAIN", help="PROGRAM name, default: MAIN")
    parser.add_argument(
        "--var-block-only",
        action="store_true",
        help="Emit only the VAR ... END_VAR block instead of a full PROGRAM",
    )
    return parser.parse_args()


def read_manifest(path: pathlib.Path):
    entries = []
    for line_no, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 3)
        if len(parts) < 3:
            raise RuntimeError(
                f"Invalid manifest line {line_no} in {path}: expected AREA TYPE NAME [ECMC_ITEM]"
            )
        area = parts[0].upper()
        type_name = parts[1].upper()
        name = parts[2]
        item_name = parts[3].strip() if len(parts) > 3 else ""

        if area not in AREAS:
            raise RuntimeError(
                f"Invalid area '{parts[0]}' on line {line_no} in {path}: expected I, Q, M, or VAR"
            )
        if type_name not in TYPE_LAYOUTS:
            raise RuntimeError(
                f"Unsupported type '{parts[1]}' on line {line_no} in {path}"
            )
        if area in ("I", "Q") and not item_name:
            raise RuntimeError(
                f"Missing ecmc item name on line {line_no} in {path} for area {area}"
            )
        if area in ("M", "VAR") and item_name.startswith("@"):
            raise RuntimeError(
                f"Unexpected annotation-like suffix on line {line_no} in {path}; "
                "use a plain ecmc item name only for I/Q entries"
            )

        entries.append(
            {
                "area": area,
                "type_name": type_name,
                "name": name,
                "item_name": item_name,
                "line_no": line_no,
            }
        )
    if not entries:
        raise RuntimeError(f"No declarations found in {path}")
    return entries


def generate_st(entries, program_name: str, manifest_name: str, var_block_only: bool) -> str:
    allocators = {area: AreaAllocator() for area in ("I", "Q", "M")}
    lines = []

    if not var_block_only:
        lines.append(f"PROGRAM {program_name}")
    lines.append("VAR")

    rendered = []
    for entry in entries:
        area = entry["area"]
        if area == "VAR":
            declaration = f"  {entry['name']} : {entry['type_name']};"
        else:
            address = allocators[area].allocate(area, entry["type_name"])
            declaration = f"  {entry['name']} AT {address} : {entry['type_name']};"
            if area in ("I", "Q"):
                declaration += f"  // @ecmc {entry['item_name']}"
        rendered.append(declaration)

    width = max(len(line.split("//", 1)[0].rstrip()) for line in rendered)
    for line in rendered:
        if "//" in line:
            left, right = line.split("//", 1)
            lines.append(f"{left.rstrip():<{width}}  //{right}")
        else:
            lines.append(line)

    lines.append("END_VAR")
    if not var_block_only:
        lines.append("")
        lines.append(f"// Generated from {manifest_name}. Add logic below.")
        lines.append("END_PROGRAM")
    lines.append("")
    return "\n".join(lines)


def main():
    args = parse_args()
    input_path = pathlib.Path(args.input)
    output_path = pathlib.Path(args.output)

    entries = read_manifest(input_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        generate_st(entries, args.program.upper(), input_path.name, args.var_block_only),
        encoding="utf-8",
    )


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
