#!/usr/bin/env bash
# Downloads the models used by the default scene into objs/.
#
# - dragon.obj, buddha.obj: Stanford 3D Scanning Repository, Stanford Computer
#   Graphics Laboratory (https://graphics.stanford.edu/data/3Dscanrep/).
#   Stanford distributes ASCII PLY; this script extracts the "res2" decimated
#   reconstructions and converts them to OBJ.
# - teapot.obj: the Utah teapot (Martin Newell, University of Utah, 1975), as an
#   OBJ from Alec Jacobson's common-3d-test-models
#   (https://github.com/alecjacobson/common-3d-test-models).
# - torus.obj: from matheusgadelha/3dviewer (MIT License)
#   (https://github.com/matheusgadelha/3dviewer).
#
# GitHub downloads are pinned to specific commits so the files can't change.
#
# Usage: ./fetch_models.sh [--force]   (--force re-downloads existing files)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/objs"
FORCE=0
[[ "${1:-}" == "--force" ]] && FORCE=1

STANFORD="https://graphics.stanford.edu/pub/3Dscanrep"
TEST_MODELS="https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/8a4f8642acaf43f9cd7b67858a1502e1055ef202/data"
VIEWER="https://raw.githubusercontent.com/matheusgadelha/3dviewer/240a4372023cc9c73a813d1c10d327600cc61771/objs"

for tool in curl tar awk; do
    command -v "$tool" >/dev/null || { echo "error: $tool is required" >&2; exit 1; }
done

mkdir -p "$OUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

MODELS=(dragon.obj buddha.obj teapot.obj torus.obj)

needs() {  # needs <file>: true if the file should be (re)created
    [[ $FORCE -eq 1 || ! -s "$OUT/$1" ]]
}

# Nothing to do if every model is already present
missing=0
for model in "${MODELS[@]}"; do needs "$model" && missing=1; done
if [[ $missing -eq 0 ]]; then
    echo "Models: all present in objs/ (use --force to re-download)"
    exit 0
fi

# Converts an ASCII PLY (x y z vertices, polygon faces) to OBJ on stdout.
ply_to_obj() {
    awk '
        BEGIN { in_header = 1 }
        in_header {
            if ($1 == "format" && $2 != "ascii") { print "error: only ASCII PLY is supported" > "/dev/stderr"; exit 1 }
            if ($1 == "element" && $2 == "vertex") nv = $3
            if ($1 == "element" && $2 == "face")   nf = $3
            if ($1 == "end_header") { in_header = 0; print "# converted from PLY by fetch_models.sh" }
            next
        }
        nv > 0 { print "v", $1, $2, $3; nv--; next }
        nf > 0 {
            n = $1; line = "f"
            for (i = 2; i <= n + 1; i++) line = line " " ($i + 1)   # PLY is 0-based, OBJ 1-based
            print line; nf--
        }
    ' "$1"
}

# fetch_stanford <output name> <archive path> <ply path inside archive>
fetch_stanford() {
    local name="$1" archive="$2" ply="$3"
    if ! needs "$name"; then echo "  $name already present, skipping"; return; fi
    echo "  downloading $STANFORD/$archive"
    curl -fL --retry 3 --progress-bar -o "$TMP/archive.tar.gz" "$STANFORD/$archive"
    tar -xzf "$TMP/archive.tar.gz" -C "$TMP" "$ply"
    ply_to_obj "$TMP/$ply" > "$OUT/$name.tmp"
    mv "$OUT/$name.tmp" "$OUT/$name"
    rm -f "$TMP/archive.tar.gz"
    echo "  wrote objs/$name"
}

# fetch_obj <output name> <url>
fetch_obj() {
    local name="$1" url="$2"
    if ! needs "$name"; then echo "  $name already present, skipping"; return; fi
    curl -fL --retry 3 -s -o "$OUT/$name.tmp" "$url"
    mv "$OUT/$name.tmp" "$OUT/$name"
    echo "  wrote objs/$name"
}

echo "Fetching models into $OUT"
fetch_stanford dragon.obj dragon/dragon_recon.tar.gz dragon_recon/dragon_vrip_res2.ply
fetch_stanford buddha.obj happy/happy_recon.tar.gz happy_recon/happy_vrip_res2.ply
fetch_obj teapot.obj "$TEST_MODELS/teapot.obj"
fetch_obj torus.obj "$VIEWER/torus.obj"
echo "Done."
