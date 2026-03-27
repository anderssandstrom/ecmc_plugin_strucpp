#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
strucpp_dir="${STRUCPP_DIR:-$repo_dir/../strucpp}"
node_image="${NODE_IMAGE:-docker.io/library/node:20}"
podman_bin="${PODMAN:-podman}"

if [[ ! -d "$strucpp_dir" ]]; then
  echo "STRUCPP_DIR not found: $strucpp_dir" >&2
  exit 1
fi

mkdir -p "$repo_dir/libs"

"$podman_bin" run --rm \
  -v "$strucpp_dir:/src/strucpp:ro" \
  -v "$repo_dir:/src/plugin" \
  "$node_image" \
  bash -lc "set -euo pipefail; cp -R /src/strucpp /tmp/strucpp; cd /tmp/strucpp; npm ci --ignore-scripts; npm run build; node dist/cli.js --compile-lib /src/plugin/lib/ecmc_motion.st -o /src/plugin/libs --lib-name ecmc-motion --lib-version 0.1.0 --lib-namespace ecmc_motion --no-default-libs"

python3 "$repo_dir/scripts/patch_stlib_headers.py" \
  --stlib "$repo_dir/libs/ecmc-motion.stlib" \
  --header ecmcMcApi.h
