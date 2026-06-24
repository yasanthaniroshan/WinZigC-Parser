#!/usr/bin/env bash
# One-time vendoring of header-only/third-party deps (not run by `make all`).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="${ROOT}/third_party"

mkdir -p "${TP}"

clone_if_missing() {
  local dir="$1"
  local url="$2"
  local tag="$3"
  if [[ -f "${dir}/.vendor_ok" ]]; then
    echo "ok: ${dir}"
    return
  fi
  echo "==> cloning ${tag} into ${dir}"
  rm -rf "${dir}"
  git clone --depth 1 --branch "${tag}" "${url}" "${dir}"
  rm -rf "${dir}/.git"
  touch "${dir}/.vendor_ok"
}

clone_if_missing "${TP}/CLI11" https://github.com/CLIUtils/CLI11.git v2.6.2
clone_if_missing "${TP}/spdlog" https://github.com/gabime/spdlog.git v1.17.0

echo "==> third_party ready (commit or include in submission tarball)"
