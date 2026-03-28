#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


TYPE_LAYOUTS = {
    "BOOL",
    "SINT",
    "USINT",
    "BYTE",
    "INT",
    "UINT",
    "WORD",
    "DINT",
    "UDINT",
    "DWORD",
    "REAL",
    "LINT",
    "ULINT",
    "LWORD",
    "LREAL",
}
AREAS = {"I", "Q", "M", "VAR"}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a first-draft declaration manifest from ecmc item names."
    )
    parser.add_argument("--input", help="Input file with one item name per line")
    parser.add_argument(
        "--item",
        action="append",
        default=[],
        help="Item name to include; may be used multiple times",
    )
    parser.add_argument("--output", required=True, help="Output manifest path")
    return parser.parse_args()


def read_lines(input_path: str | None, items: list[str]) -> list[str]:
    lines = []
    if input_path:
        path = pathlib.Path(input_path)
        for raw_line in path.read_text(encoding="utf-8").splitlines():
            line = raw_line.split("#", 1)[0].strip()
            if line:
                lines.append(line)
    for item in items:
        line = item.strip()
        if line:
            lines.append(line)
    if not lines:
        raise RuntimeError("No item names provided. Use --input or --item.")
    return lines


def sanitize_leaf(item_name: str) -> str:
    leaf = item_name.split(".")[-1]
    leaf = re.sub(r"\d+$", "", leaf)
    leaf = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", leaf)
    leaf = leaf.replace("-", "_")
    return leaf.lower()


def infer_name(item_name: str) -> str:
    lower = item_name.lower()
    if lower.endswith(".enc.actpos"):
        return "actual_position"
    if lower.endswith(".traj.targetpos"):
        return "target_position"
    if lower.endswith(".traj.targetvel"):
        return "target_velocity"
    if lower.endswith(".traj.targetacc"):
        return "target_acceleration"
    if lower.endswith(".traj.targetdec"):
        return "target_deceleration"
    if lower.endswith(".drv.enable"):
        return "drive_enable"
    if lower.endswith(".drv.enabled"):
        return "drive_enabled"

    leaf = sanitize_leaf(item_name)
    replacements = {
        "position_actual": "actual_position",
        "positionactual": "actual_position",
        "velocity_setpoint": "velocity_setpoint",
        "velocitysetpoint": "velocity_setpoint",
        "drive_control": "drive_control",
        "drivecontrol": "drive_control",
        "status_word": "status_word",
        "statusword": "status_word",
        "control_word": "control_word",
        "controlword": "control_word",
    }
    return replacements.get(leaf, leaf)


def infer_area_and_type(item_name: str) -> tuple[str, str]:
    lower = item_name.lower()
    leaf = sanitize_leaf(item_name)

    if ".enc." in lower:
        return "I", "LREAL"
    if ".traj.target" in lower:
        return "Q", "LREAL"
    if lower.endswith(".drv.enable"):
        return "Q", "BOOL"
    if lower.endswith(".drv.enabled"):
        return "I", "BOOL"

    if leaf in {"drive_control", "drivecontrol", "control_word", "controlword"}:
        return "Q", "WORD"
    if leaf in {"status_word", "statusword"}:
        return "I", "WORD"
    if leaf in {"velocity_setpoint", "velocitysetpoint"}:
        return "Q", "INT"
    if leaf in {"position_actual", "positionactual"}:
        return "I", "INT"
    if "enable" in leaf:
        return ("Q", "BOOL") if "enabled" not in leaf else ("I", "BOOL")
    if "status" in leaf:
        return "I", "WORD"
    if "control" in leaf:
        return "Q", "WORD"
    if "target" in leaf or "setpoint" in leaf:
        return "Q", "INT"
    if "actual" in leaf or "feedback" in leaf:
        return "I", "INT"

    raise RuntimeError(
        f"Could not infer AREA/TYPE for '{item_name}'. "
        "Use explicit manifest syntax: AREA TYPE NAME ECMC_ITEM"
    )


def parse_entry(line: str) -> dict:
    parts = line.split(None, 3)
    if len(parts) >= 4 and parts[0].upper() in AREAS and parts[1].upper() in TYPE_LAYOUTS:
        return {
            "area": parts[0].upper(),
            "type_name": parts[1].upper(),
            "name": parts[2],
            "item_name": parts[3].strip(),
        }

    item_name = line
    area, type_name = infer_area_and_type(item_name)
    return {
        "area": area,
        "type_name": type_name,
        "name": infer_name(item_name),
        "item_name": item_name,
    }


def render_manifest(entries: list[dict]) -> str:
    area_width = max(len(entry["area"]) for entry in entries)
    type_width = max(len(entry["type_name"]) for entry in entries)
    name_width = max(len(entry["name"]) for entry in entries)

    lines = ["# AREA TYPE   NAME              ECMC_ITEM"]
    for entry in entries:
        lines.append(
            f"{entry['area']:<{area_width}}  "
            f"{entry['type_name']:<{type_width}}  "
            f"{entry['name']:<{name_width}}  "
            f"{entry['item_name']}"
        )
    lines.append("")
    return "\n".join(lines)


def main():
    args = parse_args()
    lines = read_lines(args.input, args.item)
    entries = [parse_entry(line) for line in lines]

    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render_manifest(entries), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
