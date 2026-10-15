#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
gcc -std=gnu11 -O2 -Wall -Wextra -Wno-misleading-indentation -Icommon -Istm32/include tests/core_test.c common/protocol.c stm32/src/config.c stm32/src/stage.c -o "$tmpdir/core_test"
"$tmpdir/core_test"
# Compile the real gateway bus logic with a simulated UART; no copied algorithm.
sed '/#include "secrets.h"/c\#include <secrets.h>' esp32/main/bus.c > "$tmpdir/bus.c"
gcc -std=gnu11 -O2 -Wall -Wextra -Wno-misleading-indentation -Itests/stubs -Iesp32/main -Icommon tests/assignment_test.c "$tmpdir/bus.c" common/protocol.c -o "$tmpdir/assignment_test"
"$tmpdir/assignment_test"
gcc -std=gnu11 -O2 -Wall -Wextra -Wno-misleading-indentation -DNODE_SENSOR_TYPE=2 -Icommon -Istm32/include tests/node_address_test.c stm32/src/node.c common/protocol.c -o "$tmpdir/node_address_test"
"$tmpdir/node_address_test"
python3 -m unittest discover -s tests -p 'test_*.py' -v
gcc -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -fsyntax-only -I"$tmpdir" -Icommon -Istm32/include stm32/src/*.c common/*.c
