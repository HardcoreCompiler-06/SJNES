# SJNES

<p align="center">
  <b>My first project: NES Emulator</b><br>
  Developed by <b>Chiến</b>
</p>

## Introduction

SJNES is my first emulator project, built to simulate the Nintendo Entertainment System/Famicom. The project focuses on learning how the NES works internally, including the CPU, PPU, APU, cartridge system, memory bus, controllers, and mappers.

The goal of SJNES is not only to run NES games, but also to help me understand low-level programming, emulator architecture, graphics rendering, audio generation, memory mapping, and software debugging.

## Features

- Open and run NES ROM files.
- Emulate the NES CPU based on the 6502 processor.
- Emulate PPU graphics rendering.
- Emulate APU audio channels.
- Support keyboard input for controller controls.
- Support multiple NES mappers.
- Qt-based graphical user interface.
- Basic actions such as reset, pause, and fast forward.
- Audio debug tools and waveform display.
- Per-channel oscilloscope for NES and expansion audio (VRC6, VRC7, N163, FDS, MMC5, Sunsoft 5B).
- Channel mute options for testing audio.

## Main Components

### CPU

SJNES emulates the NES CPU, including registers, status flags, addressing modes, instruction execution, interrupts, stack behavior, and memory access.

### PPU

The PPU handles the visual output of the NES. SJNES implements background rendering, sprite rendering, palettes, nametables, pattern tables, scrolling, and NMI behavior.

### APU

The APU is responsible for sound generation. SJNES supports the main NES audio channels, including Pulse 1, Pulse 2, Triangle, Noise, and DMC.

### Mapper

NES cartridges use mappers to extend the console's capabilities. SJNES supports several common mappers such as NROM, MMC1, UxROM, CNROM, MMC3, MMC5, AxROM, MMC2, VRC6, and others.

### User Interface

SJNES uses Qt for the graphical interface. The interface includes ROM loading, reset, pause, audio options, debug information, and waveform visualization.

## Technologies Used

- C++
- Qt
- Visual Studio
- Git/GitHub

## Screenshots

![SJNES Screenshot](images/sjnes.png)

## Author

<p align="center">
  <img src="images/avatar.jpg" alt="Author photo" width="140"/><br>
  <b>Chiến</b><br>
  Developer of SJNES
</p>

This is my first project, a NES emulator created for learning, research, and software development practice.

## Acknowledgements

SJNES was written for learning. I studied the resources and emulators below to
understand how the NES works. The core skeleton (CPU, PPU, bus, cartridge and mapper base classes) follows
javidx9's olcNES (OLC-3 license) and was then heavily extended by me. A few other
parts (the FDS mapper, FDS disk loading, and the CPU interrupt timing model) are ported from Mesen, which is
licensed under GPL-3.0. Those places are marked in the source comments.

- [OneLoneCoder / javidx9](https://github.com/OneLoneCoder/olcNES) – NES emulator tutorial series
- [Mesen (SourMesen)](https://github.com/SourMesen/Mesen2) – reference for accurate NES behavior
- [NESdev Wiki and community](https://www.nesdev.org/wiki/Nesdev_Wiki) – hardware documentation
- [Corrscope](https://github.com/corrscope/corrscope) – inspiration for the correlation-based
  oscilloscope triggering used for some audio channels

emu2413 https://github.com/digital-sound-antiques/emu2413 for VRC7 audio expansion

miniz https://github.com/richgel999/miniz

## Project Status

SJNES is still in development. Some games can run, but accuracy may depend on the game, mapper, CPU timing, PPU behavior, and APU implementation.

## Future Improvements

- Improve CPU, PPU, and APU accuracy.
- Add more mapper support.
- Improve audio synchronization.
- Add save states for more mappers.
- Optimize performance for low-end computers.
- Improve debug tools.
- Add controller support.
- Add CRT shader or visual effects.

## License

Copyright (C) Chiến

Third-party code and its licenses (olc6502 under OLC-3, Mesen under GPL-3.0) are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

SJNES is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version. See [LICENSE](LICENSE).

SJNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Conclusion

SJNES is a learning-focused NES emulator project. Through this project, I learned more about C++, Qt, computer architecture, graphics processing, audio processing, memory management, and emulator development.
