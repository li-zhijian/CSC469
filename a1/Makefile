CC = gcc
CCFLAGS = -Wall -g -std=c99 -pedantic
TARGETS = trace

## Project Build
.PHONY: all clean

all: $(TARGETS)

%: %.c
	$(CC) -o $@ $(CCFLAGS) $^

clean:
	rm -rf $(TARGETS)
