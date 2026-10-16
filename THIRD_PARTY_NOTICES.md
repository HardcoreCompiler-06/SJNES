# Third-party notices

SJNES is licensed under the GNU General Public License v3.0 (see `LICENSE`).
It contains code derived from the projects below.

## olc6502 / olcNES (javidx9, OneLoneCoder)

Used in (structure/skeleton derived, then heavily extended):
`CPU6502.cpp/.h`, `PPU.cpp/.h`, `Bus.h`, `Cartridge.cpp/.h`, `Mapper.cpp/.h`, `Mapper_000.cpp`
Source: https://github.com/OneLoneCoder/olcNES

```
License (OLC-3)
~~~~~~~~~~~~~~~

Copyright 2018-2019 OneLoneCoder.com

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions or derivations of source code must retain the above
copyright notice, this list of conditions and the following disclaimer.

2. Redistributions or derivative works in binary form must reproduce
the above copyright notice. This list of conditions and the following
disclaimer must be reproduced in the documentation and/or other
materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Mesen / Mesen2 (SourMesen and contributors)

Used in: `Mapper_020.cpp`, `Mapper_020.h` (Famicom Disk System), `Cartridge.cpp` (FDS disk
image loading), `CPU6502.h` and `CPU6502_CycleAccurate.inc` (IRQ/NMI timing model), and other
places marked in the source comments.
Source: https://github.com/SourMesen/Mesen2
License: GNU General Public License v3.0. Copyright remains with the Mesen authors.

## NESdev Wiki

Hardware documentation used for reference. No code copied.
https://www.nesdev.org/wiki/Nesdev_Wiki

## Corrscope

Inspiration for the correlation-based oscilloscope triggering. No code copied.
https://github.com/corrscope/corrscope

## emu2413 (Mitsutaka Okazaki)

Used in: `3rd_party/emu2413.c`, `3rd_party/emu2413.h` (YM2413 / OPLL sound synthesis emulation for VRC7 expansion audio).
Source: https://github.com/digital-sound-antiques/emu2413
License: MIT License. Copyright (C) 2020 Mitsutaka Okazaki.

## miniz (Rich Geldreich)

Used in: `3rd_party/miniz.c`, `3rd_party/miniz.h` (Deflate/ZIP compression and decompression library).
Source: https://github.com/richgel999/miniz
License: MIT License (or Public Domain). Copyright (c) 2010-2014 Rich Geldreich and contributors.
