#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


FORWARD_RE = re.compile(
    r"//\s*Forward:\s+([A-Za-z_][A-Za-z0-9_]*)\s+AT\s+(%[IQM][XBWDL]\d+(?:\.\d+)?)\s+in\s+"
)
LOCATED_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+AT\s+(%[IQM][XBWDL]\d+(?:\.\d+)?)\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*(?://\s*(.*))?$",
    re.IGNORECASE,
)
VAR_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:AT\s+%[IQM][XBWDL]\d+(?:\.\d+)?)?\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*(?://\s*(.*))?$",
    re.IGNORECASE,
)
PROGRAM_RE = re.compile(r"^\s*PROGRAM\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", re.IGNORECASE)
QUOTED_RE = re.compile(r'"([^"]*)"')
PLACEHOLDER_RE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)(?:=([^}]*))?\}")

WIDTH_BYTES = {"B": 1, "W": 2, "D": 4, "L": 8}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a build summary and dry-run validation report for ecmc_plugin_strucpp artifacts."
    )
    parser.add_argument("--st-source", required=True, help="ST source file")
    parser.add_argument("--header", required=True, help="Generated STruCpp header")
    parser.add_argument("--map", dest="map_path", help="Generated mapping file")
    parser.add_argument("--substitutions", help="Generated substitutions file")
    parser.add_argument("--logic-lib", help="Logic library path to mention in the report")
    parser.add_argument("--output", help="Summary output path")
    parser.add_argument("--validate-only", action="store_true", help="Validate and print summary to stdout")
    parser.add_argument(
        "--define",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Placeholder definition used in @ecmc annotations, for example AXIS_INDEX=1",
    )
    return parser.parse_args()


def normalize_name(text):
    return text.strip().upper()


def parse_definitions(items):
    definitions = {}
    for item in items:
        if "=" not in item:
            raise RuntimeError(f"Invalid --define '{item}', expected KEY=VALUE")
        key, value = item.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            raise RuntimeError(f"Invalid --define '{item}', expected KEY=VALUE")
        definitions[key] = value
    return definitions


def expand_placeholders(value, definitions, source_path, line_no, annotation_name):
    def replace(match):
        key = match.group(1)
        default_value = match.group(2)
        if key in definitions:
            return definitions[key]
        if default_value is not None:
            return default_value
        if key not in definitions:
            raise RuntimeError(
                f"Undefined placeholder '{key}' in {annotation_name} annotation at {source_path}:{line_no}"
            )

    return PLACEHOLDER_RE.sub(replace, value)


def parse_program_name(text, path):
    for line in text.splitlines():
        match = PROGRAM_RE.match(line)
        if match:
            return match.group(1)
    raise RuntimeError(f"Failed to find PROGRAM declaration in {path}")


def parse_st_source(st_path, definitions):
    text = st_path.read_text(encoding="utf-8")
    program_name = parse_program_name(text, st_path)
    located = []
    exports = []
    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        located_match = LOCATED_RE.match(raw_line)
        var_match = VAR_RE.match(raw_line)

        if "@ecmc" in raw_line:
            if not located_match:
                raise RuntimeError(f"Malformed @ecmc annotation in {st_path}:{line_no}")
            annotation = raw_line.split("@ecmc", 1)[1].strip()
            if not annotation:
                raise RuntimeError(f"Empty @ecmc annotation in {st_path}:{line_no}")
        if "@epics" in raw_line:
            if not var_match:
                raise RuntimeError(f"Malformed @epics annotation in {st_path}:{line_no}")

        if located_match:
            name = located_match.group(1)
            address = located_match.group(2)
            type_name = located_match.group(3).upper()
            comment = located_match.group(4) or ""
            ecmc_item = None
            if "@ecmc" in comment:
                ecmc_item = expand_placeholders(
                    comment.split("@ecmc", 1)[1].strip(),
                    definitions,
                    st_path,
                    line_no,
                    "@ecmc",
                )
            located.append(
                {
                    "name": name,
                    "norm_name": normalize_name(name),
                    "address": address,
                    "area": address[1],
                    "type_name": type_name,
                    "ecmc_item": ecmc_item,
                    "line_no": line_no,
                }
            )

        if var_match and "@epics" in (var_match.group(3) or ""):
            name = var_match.group(1)
            type_name = var_match.group(2).upper()
            annotation = var_match.group(3).split("@epics", 1)[1].strip()
            writable = False
            record_name = None
            export_tokens = []
            for token in annotation.split():
                lower = token.lower()
                if lower in ("rw", "ro"):
                    writable = lower == "rw"
                elif token.startswith("rec_w_prefix="):
                    record_name = token[len("rec_w_prefix="):]
                    if not record_name:
                        raise RuntimeError(f"Empty @epics record name override in {st_path}:{line_no}")
                elif token.startswith("rec_wo_prefix="):
                    suffix = token[len("rec_wo_prefix="):]
                    if not suffix:
                        raise RuntimeError(f"Empty @epics record name suffix override in {st_path}:{line_no}")
                    record_name = f"Plg-ST0-{suffix}"
                elif token.startswith("rec="):
                    record_name = token[4:]
                    if not record_name:
                        raise RuntimeError(f"Empty @epics record name override in {st_path}:{line_no}")
                else:
                    export_tokens.append(token)
            export_name = " ".join(export_tokens).strip() or name
            exports.append(
                {
                    "name": name,
                    "norm_name": normalize_name(name),
                    "type_name": type_name,
                    "export_name": export_name,
                    "record_name": record_name,
                    "writable": writable,
                    "line_no": line_no,
                }
            )
    return program_name, located, exports


def parse_header_forwards(header_path):
    forwards = {}
    for line in header_path.read_text(encoding="utf-8").splitlines():
        match = FORWARD_RE.search(line)
        if not match:
            continue
        name = normalize_name(match.group(1))
        address = match.group(2)
        if name in forwards and forwards[name] != address:
            raise RuntimeError(
                f"Conflicting generated header forwards for {match.group(1)} in {header_path}"
            )
        forwards[name] = address
    return forwards


def parse_map_file(map_path):
    mappings = {}
    if not map_path:
        return mappings
    for line_no, raw_line in enumerate(pathlib.Path(map_path).read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        if "=" not in line:
            raise RuntimeError(f"Malformed mapping line in {map_path}:{line_no}")
        address, item_name = [part.strip() for part in line.split("=", 1)]
        if address in mappings and mappings[address] != item_name:
            raise RuntimeError(
                f"Conflicting mappings for {address} in {map_path}: {mappings[address]} vs {item_name}"
            )
        mappings[address] = item_name
    return mappings


def parse_substitutions(subst_path):
    exports = []
    if not subst_path:
        return exports
    for raw_line in pathlib.Path(subst_path).read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line.startswith("{"):
            continue
        quoted = QUOTED_RE.findall(line)
        if len(quoted) >= 5:
            exports.append(quoted[4])
    return exports


def byte_bit_region(address):
    size_code = address[2]
    rest = address[3:]
    if size_code == "X":
        byte_text, bit_text = rest.split(".", 1)
        byte_index = int(byte_text, 10)
        bit_index = int(bit_text, 10)
        return byte_index, byte_index, bit_index, bit_index
    byte_index = int(rest, 10)
    width = WIDTH_BYTES[size_code]
    return byte_index, byte_index + width - 1, None, None


def detect_overlaps(located):
    warnings = []
    for idx, left in enumerate(located):
        for right in located[idx + 1 :]:
            if left["area"] != right["area"]:
                continue
            left_start, left_end, left_bit_start, left_bit_end = byte_bit_region(left["address"])
            right_start, right_end, right_bit_start, right_bit_end = byte_bit_region(right["address"])
            byte_overlap = not (left_end < right_start or right_end < left_start)
            if not byte_overlap:
                continue
            if left_bit_start is not None and right_bit_start is not None:
                if left_start == right_start and left_bit_start == right_bit_start:
                    warnings.append(
                        f"Exact bit overlap in %{left['area']} between {left['name']} ({left['address']}) and "
                        f"{right['name']} ({right['address']})"
                    )
            else:
                warnings.append(
                    f"Overlapping located addresses in %{left['area']} between {left['name']} ({left['address']}) and "
                    f"{right['name']} ({right['address']})"
                )
    return warnings


def validate(program_name, located, exports, forwards, mappings, subst_exports, paths):
    errors = []
    warnings = detect_overlaps(located)

    seen_located_names = set()
    seen_located_addresses = set()
    for entry in located:
        if entry["norm_name"] in seen_located_names:
            errors.append(f"Duplicate located variable name in ST: {entry['name']}")
        seen_located_names.add(entry["norm_name"])
        if entry["address"] in seen_located_addresses:
            errors.append(f"Duplicate located address in ST: {entry['address']}")
        seen_located_addresses.add(entry["address"])

        forward_address = forwards.get(entry["norm_name"])
        if forward_address is None:
            errors.append(f"Located variable {entry['name']} missing from generated header forwards")
        elif forward_address != entry["address"]:
            errors.append(
                f"Generated header address mismatch for {entry['name']}: ST {entry['address']} vs header {forward_address}"
            )

        if entry["area"] in ("I", "Q") and entry["ecmc_item"]:
            mapped_item = mappings.get(entry["address"])
            if mapped_item is None:
                errors.append(f"Missing mapping for {entry['address']} ({entry['name']})")
            elif mapped_item != entry["ecmc_item"]:
                errors.append(
                    f"Mapping mismatch for {entry['address']}: ST '{entry['ecmc_item']}' vs map '{mapped_item}'"
                )

    seen_export_names = set()
    seen_record_names = set()
    for export in exports:
        if export["export_name"] in seen_export_names:
            errors.append(f"Duplicate @epics export name in ST: {export['export_name']}")
        seen_export_names.add(export["export_name"])
        if export["record_name"]:
            if export["record_name"] in seen_record_names:
                errors.append(f"Duplicate @epics record name in ST: {export['record_name']}")
            seen_record_names.add(export["record_name"])
        if paths["substitutions"] and export["export_name"] not in subst_exports:
            errors.append(f"Export missing from substitutions file: {export['export_name']}")

    for address in mappings:
        if address not in seen_located_addresses:
            errors.append(f"Map contains address not present in ST/header: {address}")

    return errors, warnings


def build_summary(program_name, located, exports, mappings, errors, warnings, paths):
    lines = []
    lines.append("ecmc_plugin_strucpp build summary")
    lines.append("")
    lines.append(f"program: {program_name}")
    if paths["logic_lib"]:
        lines.append(f"logic_lib: {paths['logic_lib']}")
    lines.append(f"st_source: {paths['st_source']}")
    lines.append(f"header: {paths['header']}")
    if paths["map"]:
        lines.append(f"map: {paths['map']}")
    if paths["substitutions"]:
        lines.append(f"substitutions: {paths['substitutions']}")
    lines.append("")
    lines.append(f"located_vars: {len(located)}")
    lines.append(f"mapped_io: {sum(1 for entry in located if entry['area'] in ('I', 'Q') and entry['ecmc_item'])}")
    lines.append(f"epics_exports: {len(exports)}")
    lines.append("")

    if located:
        lines.append("Located Variables:")
        for entry in located:
            suffix = f" -> {entry['ecmc_item']}" if entry["ecmc_item"] else ""
            lines.append(f"  {entry['address']} {entry['name']} : {entry['type_name']}{suffix}")
        lines.append("")

    if mappings:
        lines.append("Map Bindings:")
        for address in sorted(mappings):
            lines.append(f"  {address} = {mappings[address]}")
        lines.append("")

    if exports:
        lines.append("EPICS Exports:")
        for export in exports:
            mode = "rw" if export["writable"] else "ro"
            rec_suffix = f", rec={export['record_name']}" if export["record_name"] else ""
            lines.append(f"  {export['export_name']} ({export['name']}, {export['type_name']}, {mode}{rec_suffix})")
        lines.append("")

    if warnings:
        lines.append("Warnings:")
        for warning in warnings:
            lines.append(f"  - {warning}")
        lines.append("")

    if errors:
        lines.append("Validation: FAILED")
        for error in errors:
            lines.append(f"  - {error}")
    else:
        lines.append("Validation: OK")

    lines.append("")
    return "\n".join(lines)


def main():
    args = parse_args()
    st_path = pathlib.Path(args.st_source)
    header_path = pathlib.Path(args.header)
    map_path = pathlib.Path(args.map_path) if args.map_path else None
    subst_path = pathlib.Path(args.substitutions) if args.substitutions else None

    definitions = parse_definitions(args.define)
    program_name, located, exports = parse_st_source(st_path, definitions)
    forwards = parse_header_forwards(header_path)
    mappings = parse_map_file(map_path)
    subst_exports = parse_substitutions(subst_path)

    paths = {
        "st_source": str(st_path),
        "header": str(header_path),
        "map": str(map_path) if map_path else "",
        "substitutions": str(subst_path) if subst_path else "",
        "logic_lib": args.logic_lib or "",
    }
    errors, warnings = validate(program_name, located, exports, forwards, mappings, subst_exports, paths)
    summary = build_summary(program_name, located, exports, mappings, errors, warnings, paths)

    if args.output:
        output_path = pathlib.Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(summary, encoding="utf-8")

    if args.validate_only:
        print(summary)

    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
