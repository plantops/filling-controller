#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/hmi"

echo "SP01 host HMI preview"
echo "Open: http://<linux-node-ip>:${PORT}/"
exec python3 -m http.server "${PORT}" --bind 0.0.0.0 --directory "${ROOT}"
