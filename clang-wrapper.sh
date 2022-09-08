#!/usr/bin/env bash

set -euo pipefail

exec "$CLANG_UNWRAPPED" -fuse-ld="$LLD_UNWRAPPED" -nostdlib -Wl,-dynamic-linker=$(realpath musl/lib/libc.so) -Lmusl/obj/lib -nostartfiles -nostdlib -nostdinc -nodefaultlibs musl/lib/crt1.o musl/lib/crti.o musl/lib/crtn.o -lc -isystem musl/obj/include -isystem musl/include -isystem musl/arch/x86_64 -isystem musl/arch/generic $@
