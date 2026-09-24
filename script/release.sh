#!/bin/bash
# Publish every dist/tinyserial_<version>_ubuntu*.deb as one GitHub release.
# A new release starts .github/workflows/publish-apt.yml. An existing release
# receives the new assets and that workflow is started explicitly.
# Usage: ./script/release.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(awk -F': ' '/^Version:/{print $2; exit}' tinyserial-deb-prj/DEBIAN/control)"
VERSION="${VERSION%%+*}"
TAG="v${VERSION}"
mapfile -t DEBS < <(find "$ROOT/dist" -maxdepth 1 -type f -name "tinyserial_${VERSION}_ubuntu*.deb" | sort)

if [ "${#DEBS[@]}" -eq 0 ]; then
    echo "No packages matching dist/tinyserial_${VERSION}_ubuntu*.deb" >&2
    echo "Run ./script/build-docker.sh or ./script/build-deb.sh first." >&2
    exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
    echo "gh was not found." >&2
    exit 1
fi

dirty="$(git status --porcelain | grep -v -e ' tinyserial-deb-prj/opt/tinyserial/tinyserial$' -e '^?? dist/' -e '^?? tinyserial.deb$' || true)"
if [ -n "$dirty" ]; then
    echo "Commit or stash these changes before releasing:" >&2
    echo "$dirty" >&2
    exit 1
fi

if ! git merge-base --is-ancestor HEAD "@{upstream}" 2>/dev/null; then
    echo "Push this commit before releasing. The release tag has to point at a commit already on GitHub." >&2
    exit 1
fi

NOTES="$(cat <<EOF
Packages for Ubuntu 20.04 (focal), 22.04 (jammy), 24.04 (noble), and 26.04 (resolute).

Install on Ubuntu 26.04:

\`\`\`bash
sudo curl -fsSL https://apt.mltz.tech/key.gpg -o /usr/share/keyrings/mltz.gpg
echo "deb [signed-by=/usr/share/keyrings/mltz.gpg] https://apt.mltz.tech resolute main" | sudo tee /etc/apt/sources.list.d/mltz.list
sudo apt-get update
sudo apt-get install tinyserial
\`\`\`

Use jammy on 22.04, noble on 24.04, and focal on 20.04. The same key and host apply.
EOF
)"

if gh release view "$TAG" >/dev/null 2>&1; then
    gh release upload "$TAG" "${DEBS[@]}" --clobber
    gh workflow run "Publish apt package" --ref "$(git rev-parse --abbrev-ref HEAD)" -f "tag=${TAG}"
    echo "Uploaded packages to existing release ${TAG} and started the apt workflow."
else
    gh release create "$TAG" "${DEBS[@]}" \
        --title "TinySerial ${TAG}" \
        --notes "$NOTES"
    echo "Release ${TAG} created. The apt publish workflow starts from this release."
fi
