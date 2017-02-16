TARGET = ipu_tester

CC = mipsel-gcw0-linux-uclibc-gcc
LD = mipsel-gcw0-linux-uclibc-gcc

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CONFIG  := $(SYSROOT)/usr/bin/sdl-config
SDL_CFLAGS  := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS    := $(shell $(SDL_CONFIG) --libs)

CFLAGS = -g -O2 $(SDL_CFLAGS)

LDFLAGS = $(SDL_LIBS) -lSDL_ttf -lSDL_gfx

all: $(TARGET)

OBJS = ipu_tester.o

$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm $(OBJS) $(TARGET)
