# Blocksmith 2

A block building game for the Nintendo 3DS, written from scratch in C with
libctru and its own PICA200 graphics code.

Version 0.1 - early setup. Nothing playable yet.

## Build

Needs devkitPro (3DS: devkitARM, libctru, tools), a host `gcc` for the PC
tests, and Python 3 with Pillow for the icon and banner.

From Git Bash:

    sh tools/dkp-make.sh          # PC tests, then blocksmith2.3dsx and blocksmith2.cia
    sh tools/dkp-make.sh test     # PC tests only

Inside devkitPro's own MSYS2 shell, plain `make` works the same way.
