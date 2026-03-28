#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


VAR_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:AT\s+%[IQM][XBWDL]\d+(?:\.\d+)?)?\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*//\s*@epics\s+(.+?)\s*$"
)

TEMPLATE_MAP = {
    ("BOOL", False): "ecmcStrucppBi.template",
    ("BOOL", True): "ecmcStrucppBo.template",
    ("SINT", False): "ecmcStrucppLongIn.template",
    ("SINT", True): "ecmcStrucppLongOut.template",
    ("USINT", False): "ecmcStrucppLongIn.template",
    ("USINT", True): "ecmcStrucppLongOut.template",
    ("BYTE", False): "ecmcStrucppLongIn.template",
    ("BYTE", True): "ecmcStrucppLongOut.template",
    ("INT", False): "ecmcStrucppLongIn.template",
    ("INT", True): "ecmcStrucppLongOut.template",
    ("UINT", False): "ecmcStrucppLongIn.template",
    ("UINT", True): "ecmcStrucppLongOut.template",
    ("WORD", False): "ecmcStrucppLongIn.template",
    ("WORD", True): "ecmcStrucppLongOut.template",
    ("DINT", False): "ecmcStrucppLongIn.template",
    ("DINT", True): "ecmcStrucppLongOut.template",
    ("UDINT", False): "ecmcStrucppLongIn.template",
    ("UDINT", True): "ecmcStrucppLongOut.template",
    ("DWORD", False): "ecmcStrucppLongIn.template",
    ("DWORD", True): "ecmcStrucppLongOut.template",
    ("REAL", False): "ecmcStrucppAi.template",
    ("REAL", True): "ecmcStrucppAo.template",
    ("LREAL", False): "ecmcStrucppAi.template",
    ("LREAL", True): "ecmcStrucppAo.template",
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate macro-based EPICS substitutions from ST @epics annotations."
    )
    parser.add_argument("--st-source", required=True, help="ST source file with // @epics annotations")
    parser.add_argument("--output", required=True, help="Output .substitutions file")
    return parser.parse_args()


def parse_exports(st_path: pathlib.Path):
    exports = []
    seen_source_names = set()
    seen_export_names = set()
    for line_no, line in enumerate(st_path.read_text(encoding="utf-8").splitlines(), start=1):
        match = VAR_RE.match(line)
        if not match:
            if "@epics" in line:
                raise RuntimeError(f"Malformed @epics annotation in {st_path}:{line_no}")
            continue

        source_name = match.group(1)
        type_name = match.group(2).upper()
        annotation = match.group(3).strip()
        tokens = annotation.split()

        writable = False
        if tokens and tokens[-1].lower() in ("rw", "ro"):
            writable = tokens[-1].lower() == "rw"
            tokens = tokens[:-1]

        export_name = " ".join(tokens).strip()
        if not export_name:
            export_name = source_name
        if source_name in seen_source_names:
            raise RuntimeError(
                f"Duplicate @epics source variable '{source_name}' in {st_path}:{line_no}"
            )
        if export_name in seen_export_names:
            raise RuntimeError(
                f"Duplicate @epics export name '{export_name}' in {st_path}:{line_no}"
            )
        seen_source_names.add(source_name)
        seen_export_names.add(export_name)

        template = TEMPLATE_MAP.get((type_name, writable))
        if not template:
            raise RuntimeError(
                f"Unsupported @epics type '{type_name}' for variable {source_name} in {st_path}:{line_no}"
            )

        exports.append(
            {
                "source_name": source_name,
                "type_name": type_name,
                "export_name": export_name,
                "template": template,
                "writable": writable,
            }
        )

    return exports


def substitution_block(template_name, rows):
    lines = [f'file "{template_name}" {{']
    if template_name in ("ecmcStrucppAi.template", "ecmcStrucppAo.template"):
        lines.append("pattern { P, PORT, ADDR, TIMEOUT, REC, ASYN, DESC, EGU, PREC }")
        for row in rows:
            lines.append(
                '{ "$(P=)", "$(PORT=PLUGIN.STRUCPP0)", "$(ADDR=0)", "$(TIMEOUT=1000)", '
                f'"{row["export_name"]}", "{row["export_name"]}", "{row["source_name"]}", "", "3" }}'
            )
    elif template_name in ("ecmcStrucppBi.template", "ecmcStrucppBo.template"):
        lines.append("pattern { P, PORT, ADDR, TIMEOUT, REC, ASYN, DESC, ZNAM, ONAM }")
        for row in rows:
            lines.append(
                '{ "$(P=)", "$(PORT=PLUGIN.STRUCPP0)", "$(ADDR=0)", "$(TIMEOUT=1000)", '
                f'"{row["export_name"]}", "{row["export_name"]}", "{row["source_name"]}", "FALSE", "TRUE" }}'
            )
    else:
        lines.append("pattern { P, PORT, ADDR, TIMEOUT, REC, ASYN, DESC }")
        for row in rows:
            lines.append(
                '{ "$(P=)", "$(PORT=PLUGIN.STRUCPP0)", "$(ADDR=0)", "$(TIMEOUT=1000)", '
                f'"{row["export_name"]}", "{row["export_name"]}", "{row["source_name"]}" }}'
            )
    lines.append("}")
    return lines


def main():
    args = parse_args()
    st_path = pathlib.Path(args.st_source)
    output_path = pathlib.Path(args.output)
    exports = parse_exports(st_path)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        "# Auto-generated by strucpp_epics_substgen.py",
        "# Use with dbLoadTemplate(<this-file>, \"P=<prefix>,PORT=<plugin-port>\")",
        "",
    ]

    grouped = {}
    for export in exports:
        grouped.setdefault(export["template"], []).append(export)

    for template_name in (
        "ecmcStrucppBi.template",
        "ecmcStrucppBo.template",
        "ecmcStrucppLongIn.template",
        "ecmcStrucppLongOut.template",
        "ecmcStrucppAi.template",
        "ecmcStrucppAo.template",
    ):
        rows = grouped.get(template_name, [])
        if not rows:
            continue
        lines.extend(substitution_block(template_name, rows))
        lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
