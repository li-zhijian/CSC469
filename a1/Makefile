CC = gcc
CCFLAGS = -Wall -g -std=c99 -pedantic
TARGETS = inactive

## Project Build
.PHONY: all clean

all: $(TARGETS)

%: %.c
	$(CC) -o $@ $(CCFLAGS) $^

clean:
	rm -rf $(TARGETS)
