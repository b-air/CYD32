#!/usr/bin/env bash
# Self-test for the scripts/guard.sh rule that keeps the unsalted bearing /
# grid cell out of serial, log and file output. Runs on a scratch copy.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cp -r "$root/scripts" "$root/src" "$root/include" "$tmp/"
fails=0
probe() {  # $1 expected rc (0 allowed / 1 refused), $2 source line
  echo "$2" > "$tmp/src/zz_probe.cpp"
  bash "$tmp/scripts/guard.sh" > /dev/null; local rc=$?
  if [[ $rc != "$1" ]]; then echo "FAIL guard rc=$rc (want $1): $2"; fails=$((fails + 1)); fi
}
probe 1 'Serial.printf("%u\n", e.bearing);'
probe 1 'log_i("cell %u", e.cell);'
probe 1 'ESP_LOGI(TAG, "%u", bearing::cellFromMac(m));'
probe 1 'f.write((const uint8_t *)&r.bearing, 2);'
probe 1 'fprintf(fp, "%u", rec.bearing);'
probe 0 'snprintf(t, sizeof t, "ANGLE %03u", e.bearing);'
probe 0 'Serial.printf("id %08x\n", e.id);'
echo "guard self-test: $fails failed"
exit $((fails > 0))
