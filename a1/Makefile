CC = gcc
CCFLAGS = -Wall -g -std=c99 -pedantic
LIBS = -lrt
TARGETS = trace trace_context

## Project Build
.PHONY: all clean

all: $(TARGETS)

%: %.c
	$(CC) -o $@ $(CCFLAGS) $^ $(LIBS)

clean:
	rm -rf $(TARGETS)
