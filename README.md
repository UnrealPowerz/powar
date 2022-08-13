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
Make sure SDL2 is installed.

## Linux

```
$ make
```

### Cross-compiling for Windows
Make sure `x86_64-w64-mingw32-gcc`, SDL2-static for mingw, and basic libraries like pthreads for Windows are available, 

```
$ OS=Windows_NT make
```

## Windows
Probably really similarly to the cross-compiling step on Linux.

# Contributing
Help is always welcome.

Would not be possible without [DmitryGR's work](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker) on hacking and documenting the Pokéwalker, or [PoroCYon's dumper](https://git.titandemo.org/PoroCYon/pokewalker-rom-dumper).
