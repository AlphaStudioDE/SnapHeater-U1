#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
out="${TMPDIR:-/tmp}/snapheater-control-arbiter-test"
cc -std=c11 -Wall -Wextra -Werror \
  -I"$root/tests/stubs" -I"$root/main" \
  "$root/main/control_lease.c" "$root/tests/control_arbiter_host_test.c" \
  -o "$out"
"$out"
