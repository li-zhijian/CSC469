CC = gcc

CC_FLAGS = -c -Wall -fmessage-length=0 -pipe -O3 -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer -DNDEBUG -I. -I$(TOPDIR)/include -D_REENTRANT=1

CC_DBG_FLAGS = -c -Wall -fmessage-length=0 -pipe -g -finline-limit=65000 -fkeep-inline-functions -finline-functions -ffast-math -fomit-frame-pointer -I. -I$(TOPDIR)/include -D_REENTRANT=1 -D_HOARD_DEBUG

all: libkheap libcmumalloc libmmlibc libhoard

alloclibs:
	mkdir alloclibs

# Library containing mm_malloc and mm_free for OS/161 kheap-based allocator

libkheap: alloclibs
	cd kheap; $(CC) $(CC_FLAGS) kheap.c; ar rs ../alloclibs/libkheap.a kheap.o

libkheap_dbg: alloclibs
	cd kheap; $(CC) $(CC_DBG_FLAGS) kheap.c; ar rs ../alloclibs/libkheap_dbg.a kheap.o 

# Library containing mm_malloc and mm_free for CMU CS213 malloc

libcmumalloc:	alloclibs cmu_malloc/malloc.c
	cd cmu_malloc; $(CC) $(CC_FLAGS) malloc.c; ar rs ../alloclibs/libcmumalloc.a malloc.o

libcmumalloc_dbg:	alloclibs cmu_malloc/malloc.c
	cd cmu_malloc; $(CC) $(CC_DBG_FLAGS) malloc.c; ar rs ../alloclibs/libcmumalloc_dbg.a malloc.o


# Library containing mm_malloc and mm_free wrappers for libc allocator
libmmlibc:	alloclibs
	cd libc; $(CC) $(CC_FLAGS) libc_wrapper.c; ar rs ../alloclibs/libmmlibc.a libc_wrapper.o

# Simplified Hoard Allocator
libhoard: alloclibs	hoard/malloc.c
	cd hoard; $(CC) $(CC_FLAGS) malloc.c; ar rs ../alloclibs/libhoard.a malloc.o

libhoard_dbg: alloclibs	hoard/malloc.c
	cd hoard; $(CC) $(CC_DBG_FLAGS) malloc.c; ar rs ../alloclibs/libhoard.a malloc.o

clean:
	rm -rf alloclibs; rm -f */*.o; rm -f *~
