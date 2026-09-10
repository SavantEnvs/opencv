#!/usr/bin/env bash
#
# mayhem/test.sh — behavioral oracle for OpenCV (5.x): RUNS the known-answer probe that mayhem/build.sh
# compiled with the project's NORMAL flags (/mayhem/opencv_kat, dynamically linked, against the
# unsanitized build/clean static libs) on the committed fixture mayhem/kat/templ.png and asserts EXACT
# computed values via grep. Every value below was recorded once from the clean build of the committed
# tree and is a function of the library's behavior only: decoded pixels, encoder output, parsed
# persistence documents, matrix algebra, HarfBuzz glyph rasterization, color conversion, resampling.
# If the program is neutered to exit(0) (the verify-repo sabotage shim) the probe prints nothing,
# every assertion misses and this script FAILS — the behavioral property the gate requires.
# Emits a CTRF summary. Does NOT compile anything; a missing probe or fixture is a FAILURE, not a skip.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

PROBE=/mayhem/opencv_kat
FIXTURE="$SRC/mayhem/kat/templ.png"
EXPECTED="$SRC/mayhem/kat/expected.txt"

# Unconditional: a missing binary or fixture is a FAILURE, never a skip.
if [ ! -x "$PROBE" ];   then echo "FATAL: $PROBE missing (build.sh bug)" >&2; emit_ctrf "opencv-kat" 0 1; exit 1; fi
if [ ! -f "$FIXTURE" ]; then echo "FATAL: $FIXTURE missing" >&2;           emit_ctrf "opencv-kat" 0 1; exit 1; fi
if [ ! -f "$EXPECTED" ]; then echo "FATAL: $EXPECTED missing" >&2;         emit_ctrf "opencv-kat" 0 1; exit 1; fi

OUT="$("$PROBE" "$FIXTURE" 2>/dev/null || true)"
echo "---- probe output ----"; echo "$OUT"; echo "----------------------"

# mayhem/kat/expected.txt holds one exact KEY=VALUE line per assertion (comments/blank lines ignored).
passed=0; failed=0
while IFS= read -r want; do
  case "$want" in ''|'#'*) continue ;; esac
  if printf '%s\n' "$OUT" | grep -qxF -- "$want"; then
    echo "PASS: $want"; passed=$((passed+1))
  else
    got="$(printf '%s\n' "$OUT" | grep -F -- "${want%%=*}=" | head -1)"
    echo "FAIL: expected '$want' (got '${got:-<nothing>}')"; failed=$((failed+1))
  fi
done < "$EXPECTED"

echo "opencv-kat: passed=$passed failed=$failed"
emit_ctrf "opencv-kat" "$passed" "$failed"
