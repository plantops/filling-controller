#!/usr/bin/env bash
# Flash an SP01 ESP32-S3 bundle.
#
#   ./flash.sh /dev/ttyACM0
set -euo pipefail

PORT="${1:?usage: ./flash.sh <port>}"
cd "$(dirname "$0")"

[ -f GIT_SHA ] && echo "Bundle commit: $(cat GIT_SHA)"

python -m esptool --chip esp32s3 -p "$PORT" write-flash @flash_args

echo
echo "Flashed. Confirm the running image matches the commit above:"
echo "  python tools/g1_usb_probe.py --port $PORT --seconds 30"
echo "A successful flash report is not evidence that the app partition changed."
