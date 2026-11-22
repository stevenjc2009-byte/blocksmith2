#!/bin/sh
# Run make inside devkitPro's own MSYS2 shell, so DEVKITPRO/DEVKITARM reach it.
# (Git Bash and devkitPro's MSYS2 are different runtimes; only PATH crosses over,
# and the login shell resets PATH, so the host gcc folder is passed in by hand.)
# Usage from Git Bash:  sh tools/dkp-make.sh [make targets]
here="$(cd "$(dirname "$0")/.." && pwd -W)"
hostgcc_dir="$(dirname "$(command -v gcc)")"
hostgcc_win="$(cd "$hostgcc_dir" && pwd -W)"
exec /c/devkitPro/msys2/usr/bin/bash.exe -lc "export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; export PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin:\$PATH:\$(cygpath -u '$hostgcc_win'); cd \"\$(cygpath -u '$here')\" && make $*"
