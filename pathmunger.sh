#!/bin/bash
set -o errexit
set -o pipefail
test "${0//[ :]/INVALID}" = "${0}"
export LD_PRELOAD=$(dirname $0)/lib$(basename $0 .sh).so${LD_PRELOAD:+:${LD_PRELOAD}}
exec "$@"
