# GPL v3
#  * fda-downloader - Simple reader for FlyDream Altimeter or Hobbyking Altimeter

CC=gcc
CFLAGS=-Isrc -g -Wall -pedantic 
LDFLAGS=-lm -g
ODIR=obj

EXEFILE=fda-downloader
OBJ_IMPL=fda-downloader-dummy.o

# take a look at this:
# http://stackoverflow.com/questions/714100/os-detecting-makefile
# $OSTYPE in freebsd
ifeq ($(OS),Windows_NT)
    EXEFILE=fda-downloader.exe
endif

ifeq ($(OS),dummy)
    OBJ_IMPL=fda-downloader-dummy.o
    EXEFILE=fda-dummy
else 
    ifeq ($(OS),Windows_NT)
        OBJ_IMPL=fda-downloader-win.o
        EXEFILE=fda-downloader.exe
    else
        # assume unix
        UNAME_S := $(shell uname -s)
        ifeq ($(UNAME_S),Linux)
            # use linux implementation
            OBJ_IMPL=fda-downloader-linux.o
        else
            # not implemented :-(
            OBJ_IMPL=fda-downloader-dummy.o
        endif
    endif
endif



_DEPS = src/fda-downloader.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

.PHONY: clean all

_OBJ = fda-downloader.o $(OBJ_IMPL)
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: src/%.c $(DEPS) $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEFILE): $(OBJ)
	gcc -o $@ $^ $(LDFLAGS)

all: $(EXEFILE)

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -fr $(ODIR) *~ core src/*~ fda-downloader fda-downloader.exe fda-dummy fda-dummy.exe
