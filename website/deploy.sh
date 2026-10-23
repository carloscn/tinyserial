#!/usr/bin/env bash
# Build the site (one page per language) and upload it to vpsde, served as https://tinyserial.mltz.tech.
# nginx config and DNS live in the private vps repo: vpsde/tinyserial/.
set -euo pipefail

cd "$(dirname "$0")"

HOST="${DEPLOY_HOST:-vpsde}"
DEST="/home/carlos/tinyserial-site/public/"

python3 build.py

rsync -az --delete -e "ssh -C -o KexAlgorithms=curve25519-sha256 -o ConnectTimeout=10 -o BatchMode=yes" \
  _site/ "$HOST:$DEST"

echo "deployed to $HOST:$DEST"
echo "verify: curl -sI https://tinyserial.mltz.tech/ | head -1"
