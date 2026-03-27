#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stlib", required=True)
    parser.add_argument("--header", action="append", default=[])
    args = parser.parse_args()

    stlib_path = Path(args.stlib)
    archive = json.loads(stlib_path.read_text())
    manifest = archive.setdefault("manifest", {})
    headers = manifest.setdefault("headers", [])

    for header in args.header:
        if header not in headers:
            headers.append(header)

    stlib_path.write_text(json.dumps(archive, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
