CC = gcc
CFLAGS = -g -Wall -Wno-unused-function

src_files = main.c ssu.c eeprom.c lcd.c accel.c rtc.c interrupts.c portb.c
libs      = -lSDL2
bin_name  = powar

ifeq ($(OS),Windows_NT)
	CC = x86_64-w64-mingw32-gcc
	libs = $(shell /usr/x86_64-w64-mingw32/sys-root/mingw/bin/sdl2-config --static-libs)
	CFLAGS += -static
	bin_name = powar.exe
endif

all:
	$(CC) $(src_files) $(CFLAGS) $(libs) -o $(bin_name)

debug: CFLAGS := $(CFLAGS) -DPKW_DEBUG=1
debug: all

clean:
	rm -f pw
