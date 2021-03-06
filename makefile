CC=gcc
CFLAGS=-O2 -Wall -Wextra -DHAVE_SSE2 -DDSFMT_MEXP=4253 -fomit-frame-pointer -DHAVE_SSE2 -msse -msse2 -msse3 -msse4 -msse4.1 -march='native'
CLIBS=-I $(HOME)/include -L $(HOME)/lib -lutils -lm -lGL -lGLEW  -lpng -DSDL
SDL_LIBS=-lSDL2 -lSDL2main
EXEC=patchy2d
INSTALL=install -m 111
BINDIR=$(HOME)/bin/

MAIN=main.c dSFMT.c
SOURCES=program_gl.c draw.c mySDL.c zargs.c params.c hash.c alloc.c init.c mm_math.c energy.c lists.c patches.c optimize.c run.c canonical.c grand_canonical.c npt.c restricted.c cluster.c io.c rnd.c colors.c
OBJS=$(SOURCES:.c=.o)
DEPS=$(SOURCES:.c=.h)

TARGET=main

all: $(TARGET)

main: $(MAIN) $(OBJS)
	$(CC) $(CFLAGS) $(MAIN) $(OBJS) -o $(EXEC) $(CLIBS) $(SDL_LIBS)
	ctags *.{c,h}

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(CLIBS) $(SDL_LIBS)

install:
	$(INSTALL) $(EXEC) $(BINDIR)
