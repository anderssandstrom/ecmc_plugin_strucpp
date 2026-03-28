#!/usr/bin/env python3

import argparse
import pathlib
import subprocess
import sys


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
NEW_IOC_SCRIPT = SCRIPT_DIR / "strucpp_new_ioc.py"
DECLGEN_SCRIPT = SCRIPT_DIR / "strucpp_declgen.py"


def run(cmd, cwd=None):
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Front-door helper for common ecmc_plugin_strucpp app workflows."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    new_ioc = subparsers.add_parser("new-ioc", help="Create a minimal IOC project scaffold")
    new_ioc.add_argument("output_dir", help="Directory to create")
    new_ioc.add_argument("--ioc-name")
    new_ioc.add_argument("--program", default="main")
    new_ioc.add_argument("--logic-name", default="main")
    new_ioc.add_argument("--slave", default="14")
    new_ioc.add_argument("--plugin-root")
    new_ioc.add_argument("--strucpp-root")
    new_ioc.add_argument("--force", action="store_true")

    declgen = subparsers.add_parser("declgen", help="Generate ST declarations from a small manifest")
    declgen.add_argument("--input", required=True)
    declgen.add_argument("--output", required=True)
    declgen.add_argument("--program", default="MAIN")
    declgen.add_argument("--var-block-only", action="store_true")

    build = subparsers.add_parser("build", help="Build an app or IOC project with make")
    build.add_argument("--project", default=".", help="Project directory, default: .")
    build.add_argument("--target", default="all", help="Make target, default: all")
    build.add_argument("--src", action="store_true", help="Run make in <project>/src instead of <project>")
    build.add_argument("--make", dest="make_cmd", default="make", help="Make command, default: make")
    build.add_argument("--dry-run", action="store_true", help="Pass -n to make")
    build.add_argument(
        "--make-arg",
        action="append",
        default=[],
        help="Extra make variable assignment, for example STRUCPP=/path/to/strucpp",
    )

    validate = subparsers.add_parser(
        "validate",
        help="Run the helper's dry-run validation target",
    )
    validate.add_argument("--project", default=".", help="Project directory, default: .")
    validate.add_argument("--src", action="store_true", help="Run make in <project>/src instead of auto-detect")
    validate.add_argument("--make", dest="make_cmd", default="make", help="Make command, default: make")
    validate.add_argument("--dry-run", action="store_true", help="Pass -n to make")
    validate.add_argument(
        "--make-arg",
        action="append",
        default=[],
        help="Extra make variable assignment, for example STRUCPP=/path/to/strucpp",
    )

    return parser.parse_args()


def resolve_project_dir(project_arg: str, use_src: bool, prefer_src: bool = False) -> pathlib.Path:
    project_dir = pathlib.Path(project_arg).resolve()
    if use_src:
        return project_dir / "src"
    if prefer_src and (project_dir / "src" / "Makefile").exists():
        return project_dir / "src"
    return project_dir


def main():
    args = parse_args()

    if args.command == "new-ioc":
        cmd = [sys.executable, str(NEW_IOC_SCRIPT), args.output_dir]
        if args.ioc_name:
            cmd += ["--ioc-name", args.ioc_name]
        if args.program:
            cmd += ["--program", args.program]
        if args.logic_name:
            cmd += ["--logic-name", args.logic_name]
        if args.slave:
            cmd += ["--slave", args.slave]
        if args.plugin_root:
            cmd += ["--plugin-root", args.plugin_root]
        if args.strucpp_root:
            cmd += ["--strucpp-root", args.strucpp_root]
        if args.force:
            cmd.append("--force")
        run(cmd)
        return

    if args.command == "declgen":
        cmd = [
            sys.executable,
            str(DECLGEN_SCRIPT),
            "--input",
            args.input,
            "--output",
            args.output,
            "--program",
            args.program,
        ]
        if args.var_block_only:
            cmd.append("--var-block-only")
        run(cmd)
        return

    if args.command == "build":
        cwd = resolve_project_dir(args.project, args.src)
        cmd = [args.make_cmd]
        if args.dry_run:
            cmd.append("-n")
        cmd.extend(args.make_arg)
        cmd.append(args.target)
        run(cmd, cwd=cwd)
        return

    if args.command == "validate":
        cwd = resolve_project_dir(args.project, args.src, prefer_src=True)
        cmd = [args.make_cmd]
        if args.dry_run:
            cmd.append("-n")
        cmd.extend(args.make_arg)
        cmd.append("validate")
        run(cmd, cwd=cwd)
        return


if __name__ == "__main__":
    main()
