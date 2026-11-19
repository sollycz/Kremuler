# Project: IFJ26 compiler
# Makefile
#
# AI-generated (Claude Code, Claude Opus 5.5 Medium), reviewed by the team.
# Prompt: "write a makefile for the IFJ26 compiler
#   plain 'make' must build exactly one executable from all .c files in src/, using gcc with -std=c11 -Wall -Wextra
#   add a 'clean' target that removes the binary and object files"

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDFLAGS =

TARGET  = ifj26

# sources may be in src/ (repository layout) or in the archive root
SRCS    = $(wildcard src/*.c) $(wildcard *.c)
OBJS    = $(SRCS:.c=.o)
HDRDIRS = -Isrc -I.

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# every object depends on all headers, simple and safe for a small project
%.o: %.c $(wildcard src/*.h) $(wildcard *.h)
	$(CC) $(CFLAGS) $(HDRDIRS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
