# Powar

Powar is a **prototype** Pokéwalker emulator. It is still missing tons of features and cannot connect to DS emulator (yet).

![Pokéwalker home screen](/pics/home.png)
![Pokéwalker battle screen](/pics/battle.png)

# Usage

Get a Pokéwalker ROM and EEPROM image, you can dump these yourself using [PoroCYon's dumper for DSi](https://git.titandemo.org/PoroCYon/pokewalker-rom-dumper) or
[DmitryGR's PalmOS app](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker#_TOC_377b8050cfd1e60865685a4ca39bc4c0).

Name these images `rom.bin` and `eeprom.bin` respectively and put them in the same folder as the emulator.

Start the emulator. The buttons are mapped to the arrow keys (left, down, right) and WSD.

# Features

## Supported

- All CPU instructions the walker uses (so far)
- EEPROM
- LCD
- Buttons
- Some Timer W functions

## Not yet supported

- RTC
- Accelerometer
- Most interrupts
- Sleep mode
- IR communication
- Sound
- Many more :')

# Compiling

## Linux

Make sure SDL2 and CMake are installed.

```
$ cmake .
$ cmake --build .
```

## Windows

Make sure Visual Studio is installed, along with the "Desktop development with C++" workload. Additionally, you'll need to install [vcpkg](https://vcpkg.io/en/getting-started.html), activate the Visual Studio integration by running `vcpkg integrate install` from an elevated prompt, and install SDL2 by running `vcpkg install sdl2`.

From Visual Studio use File > Open > CMake to open the CMakeLists.txt file. Use the Select Startup Item menu to select powar.exe, and click Run. This will result in an error, but will create the `out\build\x64-Debug` directory. Place your `rom.bin` and `eeprom.bin` in that folder and click Run again.

## Emscripten (WebAssembly)

Make sure CMake is installed, and then install Emscripten [using the emsdk](https://emscripten.org/docs/getting_started/downloads.html). After installation, activate PATH and other environment variables using the provided scripts (`emsdk_env.sh`, `emsdk_env.bat`, or `emsdk_env.ps1` based on your shell). On Windows, you'll also need to put [Ninja](https://ninja-build.org/) in your `PATH`.

```
$ emcmake cmake .
$ cmake --build .
```

You can then run the web interface by starting a web server in the `static` folder.

# Contributing

Help is always welcome.

Would not be possible without [DmitryGR's work](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker) on hacking and documenting the Pokéwalker, or [PoroCYon's dumper](https://git.titandemo.org/PoroCYon/pokewalker-rom-dumper).
