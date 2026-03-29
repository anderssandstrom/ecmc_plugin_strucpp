#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


VAR_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:AT\s+%[IQM][XBWDL]\d+(?:\.\d+)?)?\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*//\s*@epics(?:\s+(.+?))?\s*$"
)
PROGRAM_RE = re.compile(r"^class\s+(Program_[A-Z0-9_]+)\s*:\s*public\s+ProgramBase\s*\{")

TYPE_MAP = {
    "BOOL": "ECMC_STRUCPP_EXPORT_BOOL",
    "SINT": "ECMC_STRUCPP_EXPORT_S8",
    "USINT": "ECMC_STRUCPP_EXPORT_U8",
    "BYTE": "ECMC_STRUCPP_EXPORT_U8",
    "INT": "ECMC_STRUCPP_EXPORT_S16",
    "UINT": "ECMC_STRUCPP_EXPORT_U16",
    "WORD": "ECMC_STRUCPP_EXPORT_U16",
    "DINT": "ECMC_STRUCPP_EXPORT_S32",
    "UDINT": "ECMC_STRUCPP_EXPORT_U32",
    "DWORD": "ECMC_STRUCPP_EXPORT_U32",
    "REAL": "ECMC_STRUCPP_EXPORT_F32",
    "LREAL": "ECMC_STRUCPP_EXPORT_F64",
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate ecmc_plugin_strucpp EPICS export header from ST @epics annotations."
    )
    parser.add_argument("--st-source", required=True, help="ST source file with // @epics annotations")
    parser.add_argument("--header", required=True, help="Generated STruCpp header")
    parser.add_argument("--header-include", required=True, help="Include path to use in generated export header")
    parser.add_argument("--output", required=True, help="Output header path")
    return parser.parse_args()


def parse_program_class(header_path: pathlib.Path):
    for line in header_path.read_text(encoding="utf-8").splitlines():
        match = PROGRAM_RE.search(line.strip())
        if match:
            return match.group(1)
    raise RuntimeError(f"Failed to find Program_* class in {header_path}")


def derive_export_name(program_class: str, var_name: str):
    program_name = program_class.removeprefix("Program_").lower()
    return f"plugin.strucpp0.{program_name}.{var_name}"


def normalize_group_name(group_name: str):
    normalized = re.sub(r"[^A-Za-z0-9]+", "_", group_name.strip().lower()).strip("_")
    if not normalized:
        raise RuntimeError(f"Invalid empty @epics group name '{group_name}'")
    return normalized


def parse_epics_annotation(annotation: str):
    tokens = annotation.split()
    writable = "0"
    export_tokens = []
    group_name = ""
    bit_index = None
    for token in tokens:
        lower = token.lower()
        if lower in ("rw", "ro"):
            writable = "1" if lower == "rw" else "0"
        elif token.startswith("group="):
            group_name = token[len("group="):]
            if not group_name:
                raise RuntimeError("Empty @epics group name")
        elif token.startswith("bit="):
            bit_text = token[len("bit="):]
            if not bit_text:
                raise RuntimeError("Empty @epics bit index")
            try:
                bit_index = int(bit_text, 10)
            except ValueError as exc:
                raise RuntimeError(f"Invalid @epics bit index '{bit_text}'") from exc
            if bit_index < 0 or bit_index > 31:
                raise RuntimeError(f"Invalid @epics bit index '{bit_text}', expected 0..31")
        elif token.startswith("prefix="):
            if token == "prefix=":
                raise RuntimeError("Empty @epics record prefix override")
        elif token.startswith("rec_full="):
            raise RuntimeError("Unsupported @epics token 'rec_full=', use 'rec=' with optional 'prefix='")
        elif token.startswith("rec_suffix="):
            raise RuntimeError("Unsupported @epics token 'rec_suffix=', use 'rec=' instead")
        elif token.startswith("rec="):
            if token == "rec=":
                raise RuntimeError("Empty @epics record name override")
        else:
            export_tokens.append(token)
    export_name = " ".join(export_tokens).strip()
    return export_name, writable, group_name, bit_index


def parse_exports(st_path: pathlib.Path, program_class: str):
    exports = []
    seen_source_names = set()
    seen_export_names = {}
    grouped_members = {}
    for line_no, line in enumerate(st_path.read_text(encoding="utf-8").splitlines(), start=1):
        match = VAR_RE.match(line)
        if not match:
            if "@epics" in line:
                raise RuntimeError(f"Malformed @epics annotation in {st_path}:{line_no}")
            continue

        var_name = match.group(1)
        type_name = match.group(2).upper()
        annotation = (match.group(3) or "").strip()
        if type_name not in TYPE_MAP:
            raise RuntimeError(
                f"Unsupported @epics type '{type_name}' for variable {var_name} in {st_path}:{line_no}"
            )

        export_name, writable, group_name, bit_index = parse_epics_annotation(annotation)
        effective_group_name = group_name
        record_name = ""
        for token in annotation.split():
            if token.startswith("rec="):
                record_name = token[4:]
                break
        if not effective_group_name and bit_index is not None and record_name:
            effective_group_name = record_name
        grouped_bool = bool(effective_group_name)
        if grouped_bool:
            if type_name != "BOOL":
                raise RuntimeError(
                    f"@epics group is only supported for BOOL variables, not '{type_name}' in {st_path}:{line_no}"
                )
            if bit_index is None:
                raise RuntimeError(
                    f"Grouped @epics BOOL '{var_name}' in {st_path}:{line_no} requires bit=<0..31>"
                )
        elif bit_index is not None:
            raise RuntimeError(
                f"@epics bit= requires group= or rec= for variable '{var_name}' in {st_path}:{line_no}"
            )

        if not export_name:
            export_name = derive_export_name(
                program_class,
                normalize_group_name(effective_group_name) if grouped_bool else var_name,
            )
        if var_name in seen_source_names:
            raise RuntimeError(
                f"Duplicate @epics source variable '{var_name}' in {st_path}:{line_no}"
            )

        seen_meta = seen_export_names.get(export_name)
        group_key = (export_name, writable, effective_group_name)
        if seen_meta is None:
            seen_export_names[export_name] = group_key
        elif seen_meta != group_key:
            raise RuntimeError(
                f"Conflicting @epics export name '{export_name}' in {st_path}:{line_no}"
            )
        seen_source_names.add(var_name)

        flags = "ECMC_STRUCPP_EXPORT_FLAG_NONE"
        emitted_bit_index = "0"
        if grouped_bool:
            flags = "ECMC_STRUCPP_EXPORT_FLAG_GROUPED_BOOL"
            emitted_bit_index = str(bit_index)
            if group_key not in grouped_members:
                grouped_members[group_key] = set()
            if bit_index in grouped_members[group_key]:
                raise RuntimeError(
                    f"Duplicate @epics group bit {bit_index} for export '{export_name}' in {st_path}:{line_no}"
                )
            grouped_members[group_key].add(bit_index)

        exports.append(
            {
                "source_name": var_name,
                "member_name": var_name.upper(),
                "export_name": export_name,
                "type_name": TYPE_MAP[type_name],
                "writable": writable,
                "flags": flags,
                "bit_index": emitted_bit_index,
            }
        )

    return exports


def generate_header(program_class: str, header_include: str, exports):
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <vector>")
    lines.append("")
    lines.append('#include "ecmcStrucppLogicIface.hpp"')
    lines.append(f'#include "{header_include}"')
    lines.append("")
    lines.append("namespace ecmcStrucppExports {")
    lines.append("")
    func_name = f"init{program_class}Exports"
    lines.append(
        f"inline void {func_name}(strucpp::{program_class}& program, std::vector<ecmcStrucppExportedVar>& out) {{"
    )
    lines.append("  out.clear();")
    for export in exports:
        lines.append("  out.push_back({")
        lines.append(f'    "{export["export_name"]}",')
        lines.append(f"    program.{export['member_name']}.raw_ptr(),")
        lines.append(f"    {export['type_name']},")
        lines.append(f"    {export['writable']},")
        lines.append(f"    {export['flags']},")
        lines.append(f"    {export['bit_index']},")
        lines.append("  });")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace ecmcStrucppExports")
    lines.append("")
    return "\n".join(lines)


def main():
    args = parse_args()
    st_path = pathlib.Path(args.st_source)
    header_path = pathlib.Path(args.header)
    output_path = pathlib.Path(args.output)

    program_class = parse_program_class(header_path)
    exports = parse_exports(st_path, program_class)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        generate_header(program_class, args.header_include, exports),
        encoding="utf-8",
    )


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
