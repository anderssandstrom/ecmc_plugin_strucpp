#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


PROGRAM_RE = re.compile(r"^\s*PROGRAM\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", re.IGNORECASE)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a minimal ecmc_plugin_strucpp logic wrapper from an ST source file."
    )
    parser.add_argument("--st-source", required=True, help="ST source file containing PROGRAM <Name>")
    parser.add_argument("--output", required=True, help="Output C++ wrapper path")
    parser.add_argument(
        "--logic-name",
        help="Exported logic library name, default <st-stem>_logic",
    )
    parser.add_argument(
        "--header-include",
        help='Generated program header include, default "generated/<stem>.hpp"',
    )
    parser.add_argument(
        "--exports-include",
        help='Generated exports header include, default "generated/<stem>_epics_exports.hpp"',
    )
    return parser.parse_args()


def parse_program_name(st_path: pathlib.Path) -> str:
    for line in st_path.read_text(encoding="utf-8").splitlines():
        match = PROGRAM_RE.match(line)
        if match:
            return match.group(1)
    raise RuntimeError(f"Failed to find PROGRAM declaration in {st_path}")


def generate_wrapper(program_name: str,
                     logic_name: str,
                     header_include: str,
                     exports_include: str) -> str:
    class_name = f"Program_{program_name}"
    export_init = f"ecmcStrucppExports::initProgram_{program_name}Exports"
    lines = [
        '#include "ecmcStrucppLogicWrapper.hpp"',
        f'#include "{header_include}"',
        "",
        f'#if __has_include("{exports_include}")',
        f'#include "{exports_include}"',
        f"#define ECMC_STRUCPP_EXPORT_INIT_FN {export_init}",
        "#endif",
        "",
        "#ifdef ECMC_STRUCPP_EXPORT_INIT_FN",
        f'ECMC_STRUCPP_DECLARE_LOGIC_API_WITH_EXPORTS("{logic_name}",',
        f"                                            strucpp::{class_name},",
        "                                            strucpp::locatedVars,",
        "                                            ECMC_STRUCPP_EXPORT_INIT_FN);",
        "#else",
        f'ECMC_STRUCPP_DECLARE_LOGIC_API("{logic_name}",',
        f"                               strucpp::{class_name},",
        "                               strucpp::locatedVars);",
        "#endif",
        "",
    ]
    return "\n".join(lines)


def main():
    args = parse_args()
    st_path = pathlib.Path(args.st_source)
    output_path = pathlib.Path(args.output)
    stem = st_path.stem
    program_name = parse_program_name(st_path)
    logic_name = args.logic_name or f"{stem}_logic"
    header_include = args.header_include or f"generated/{stem}.hpp"
    exports_include = args.exports_include or f"generated/{stem}_epics_exports.hpp"

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        generate_wrapper(program_name, logic_name, header_include, exports_include),
        encoding="utf-8",
    )


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
