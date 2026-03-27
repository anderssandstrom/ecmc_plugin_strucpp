#!/usr/bin/env python3

import argparse
import pathlib
import re


FORWARD_RE = re.compile(
    r"//\s*Forward:\s+([A-Za-z_][A-Za-z0-9_]*)\s+AT\s+(%[IQM][XBWDL]\d+(?:\.\d+)?)\s+in\s+"
)
ST_ECMC_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+AT\s+%[IQM][XBWDL]\d+(?:\.\d+)?\s*:\s*[^;]+;\s*//\s*@ecmc\s+(.+?)\s*$",
    re.IGNORECASE,
)
ADDRESS_RE = re.compile(r"^(%[IQM][XBWDL]\d+(?:\.\d+)?)$")


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Generate an ecmc_plugin_strucpp mapping file from a generated "
            "STruCpp header plus either inline ST @ecmc annotations or a "
            "variable binding file."
        )
    )
    parser.add_argument("--header", required=True, help="Generated STruCpp header to inspect")
    parser.add_argument("--st-source", help="ST source file with // @ecmc <ecmcDataItem> annotations")
    parser.add_argument(
        "--bindings",
        help="Optional variable-name binding file: VAR_NAME=ecmcDataItem",
    )
    parser.add_argument("--output", required=True, help="Output mapping file path")
    parser.add_argument(
        "--allow-partial",
        action="store_true",
        help="Allow missing bindings for some used %I/%Q variables",
    )
    return parser.parse_args()


def read_text(path_str):
    path = pathlib.Path(path_str)
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise SystemExit(f"error: could not read {path}: {exc}")


def normalize_name(name):
    return name.strip().upper()


def parse_forwards(header_text):
    located = []
    seen_names = set()
    for match in FORWARD_RE.finditer(header_text):
        source_name = match.group(1)
        name = normalize_name(source_name)
        address = match.group(2)
        area = address[1]
        if name in seen_names:
            raise SystemExit(f"error: duplicate located variable name in generated header: {source_name}")
        seen_names.add(name)
        located.append((name, address, area, source_name))
    return located


def parse_binding_file(text, source_path):
    bindings = {}
    line_no = 0
    for raw_line in text.splitlines():
        line_no += 1
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        if "=" not in line:
            raise SystemExit(
                f"error: invalid binding line {line_no} in {source_path}: expected VAR_NAME=ecmcDataItem"
            )
        key, value = line.split("=", 1)
        key = normalize_name(key)
        value = value.strip()
        if not key or not value:
            raise SystemExit(
                f"error: invalid binding line {line_no} in {source_path}: expected VAR_NAME=ecmcDataItem"
            )
        if ADDRESS_RE.match(key):
            raise SystemExit(
                f"error: binding line {line_no} in {source_path} uses a located address as key; expected an ST variable name"
            )
        if key in bindings:
            raise SystemExit(f"error: duplicate binding for variable {key} in {source_path}")
        bindings[key] = value
    return bindings


def parse_st_source_annotations(text, source_path):
    bindings = {}
    line_no = 0
    for raw_line in text.splitlines():
        line_no += 1
        match = ST_ECMC_RE.match(raw_line)
        if not match:
            continue
        key = normalize_name(match.group(1))
        value = match.group(2).strip()
        if key in bindings:
            raise SystemExit(
                f"error: duplicate @ecmc annotation for variable {key} in {source_path} line {line_no}"
            )
        if not value:
            raise SystemExit(
                f"error: empty @ecmc annotation for variable {key} in {source_path} line {line_no}"
            )
        bindings[key] = value
    return bindings


def merge_bindings(st_bindings, file_bindings):
    bindings = dict(st_bindings)
    for key, value in file_bindings.items():
        if key in bindings and bindings[key] != value:
            raise SystemExit(
                f"error: conflicting bindings for variable {key}: "
                f"ST source='{bindings[key]}' bindings file='{value}'"
            )
        bindings[key] = value
    return bindings


def address_sort_key(address):
    area_order = {"I": 0, "Q": 1, "M": 2}
    area = address[1]
    size = address[2]
    rest = address[3:]
    if "." in rest:
        byte_text, bit_text = rest.split(".", 1)
        bit_index = int(bit_text, 10)
    else:
        byte_text = rest
        bit_index = -1
    return (area_order.get(area, 99), int(byte_text, 10), bit_index, size, address)


def main():
    args = parse_args()
    if not args.st_source and not args.bindings:
        raise SystemExit("error: one of --st-source or --bindings is required")

    header_text = read_text(args.header)
    st_text = read_text(args.st_source) if args.st_source else ""
    binding_text = read_text(args.bindings) if args.bindings else ""

    located = parse_forwards(header_text)
    if not located:
        raise SystemExit(f"error: found no located variable forwards in {args.header}")

    st_bindings = parse_st_source_annotations(st_text, args.st_source) if args.st_source else {}
    file_bindings = parse_binding_file(binding_text, args.bindings) if args.bindings else {}
    bindings = merge_bindings(st_bindings, file_bindings)

    used_names = [name for name, address, area, source_name in located if area in ("I", "Q")]

    missing = [name for name in used_names if name not in bindings]
    if missing and not args.allow_partial:
        raise SystemExit(
            "error: missing bindings for located variables: " + ", ".join(sorted(missing))
        )

    located_names = {name for name, _, _, _ in located}
    extras = [name for name in bindings if name not in located_names]
    if extras:
        raise SystemExit(
            "error: binding source contains variables not present in generated header: "
            + ", ".join(sorted(extras))
        )

    mappings = []
    for name, address, area, source_name in located:
        if area not in ("I", "Q"):
            continue
        item_name = bindings.get(name)
        if item_name is None:
            continue
        mappings.append((address, item_name, source_name))

    mappings.sort(key=lambda row: address_sort_key(row[0]))

    out_path = pathlib.Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as handle:
        handle.write(
            "# Generated by strucpp_mapgen.py from STruCpp forwards and binding metadata.\n"
        )
        handle.write(f"# header={pathlib.Path(args.header).name}\n")
        if args.st_source:
            handle.write(f"# st_source={pathlib.Path(args.st_source).name}\n")
        if args.bindings:
            handle.write(f"# bindings={pathlib.Path(args.bindings).name}\n")
        handle.write("\n")
        for address, item_name, source_name in mappings:
            handle.write(f"{address}={item_name}  # {source_name}\n")


if __name__ == "__main__":
    main()
