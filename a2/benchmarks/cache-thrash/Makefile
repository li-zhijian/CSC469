
INCLUDES = $(TOPDIR)/include
LIBDIR = $(TOPDIR)/util
LIBS = -lmmutil -lpthread
LIBS_DBG = -lmmutil_dbg -lpthread
DEPENDS = cache-thrash.c $(LIBDIR)/libmmutil.a $(INCLUDES)/tsc.h $(INCLUDES)/mm_thread.h $(INCLUDES)/timer.h
DEPENDS_DBG = cache-thrash.c $(LIBDIR)/libmmutil_dbg.a $(INCLUDES)/tsc.h $(INCLUDES)/mm_thread.h $(INCLUDES)/timer.h

CC = gcc
CC_FLAGS = -O3 -I$(INCLUDES) -L $(LIBDIR)
CC_DBG_FLAGS = -g -I$(INCLUDES) -L $(LIBDIR)

all: cache-thrash-kheap cache-thrash-cmu cache-thrash-libc cache-thrash-hoard

# Allocator based on OS/161 kheap

cache-thrash-kheap: $(DEPENDS) $(TOPDIR)/allocators/alloclibs/libkheap.a
	$(CC) $(CC_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libkheap.a -lmmutil -lpthread

cache-thrash-kheap-dbg: $(DEPENDS_DBG) $(TOPDIR)/allocators/alloclibs/libkheap_dbg.a 
	$(CC) $(CC_DBG_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libkheap_dbg.a -lmmutil_dbg -lpthread

#Allocator based on CMU CS213 malloc/free

cache-thrash-cmu: $(DEPENDS) $(TOPDIR)/allocators/alloclibs/libcmumalloc.a
	$(CC) $(CC_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libcmumalloc.a -lmmutil -lpthread

cache-thrash-cmu-dbg: $(DEPENDS_DBG) $(TOPDIR)/allocators/alloclibs/libcmumalloc_dbg.a 
	$(CC) $(CC_DBG_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libcmumalloc_dbg.a -lmmutil_dbg -lpthread



# Allocator using libc malloc/free inside mm_malloc/mm_free wrappers

cache-thrash-libc: $(DEPENDS) $(TOPDIR)/allocators/alloclibs/libmmlibc.a
	$(CC) $(CC_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libmmlibc.a -lmmutil -lpthread

cache-thrash-libc-dbg: $(DEPENDS_DBG) $(TOPDIR)/allocators/alloclibs/libmmlibc.a
	$(CC) $(CC_DBG_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libmmlibc.a -lmmutil_dbg -lpthread

# Hoard
cache-thrash-hoard: $(DEPENDS) $(TOPDIR)/allocators/alloclibs/libhoard.a
	$(CC) $(CC_FLAGS) -o $(@) cache-thrash.c $(TOPDIR)/allocators/alloclibs/libhoard.a -lmmutil -lpthread

# Cleanup
clean:
	rm -f cache-thrash-* *~
