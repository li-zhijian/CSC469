
INCLUDES = $(TOPDIR)/include
LIBDIR = $(TOPDIR)/util
LIBS = -lmmutil -pthread -lgtest
LIBS_DBG = -lmmutil_dbg -pthread -lgtest
DEPENDS = runner.cc $(LIBDIR)/libmmutil.a $(INCLUDES)/tsc.h $(INCLUDES)/mm_thread.h $(INCLUDES)/timer.h
DEPENDS_DBG = runner.cc $(LIBDIR)/libmmutil_dbg.a $(INCLUDES)/tsc.h $(INCLUDES)/mm_thread.h $(INCLUDES)/timer.h

CC = g++
CC_FLAGS = -O3 -I$(INCLUDES) -L $(LIBDIR)
CC_DBG_FLAGS = -g -I$(INCLUDES) -L $(LIBDIR) -DDEBUG

all: tests-hoard

# Simplified Hoard Allocator
tests-hoard: $(DEPENDS) $(TOPDIR)/allocators/alloclibs/libhoard.a hoard.cc
	$(CC) $(CC_DBG_FLAGS) -o $(@) runner.cc hoard.cc $(TOPDIR)/allocators/alloclibs/libhoard.a $(LIBS)

# Cleanup
clean:
	rm -f tests-* *~
