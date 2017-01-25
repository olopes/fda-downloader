# GPL v3
#  * fda-downloader - Simple reader for FlyDream Altimeter or Hobbyking Altimeter

CC=gcc
CFLAGS=-Isrc -g -Wall -pedantic 
LDFLAGS=-lm -g
ODIR=obj

EXEFILE=fda-downloader
# if windows OUTFILE=fda-downloader.exe
OBJ_IMPL=fda-downloader-linux.o
# if windows OUTFILE=fda-downloader-win.o
# else OUTFILE=fda-downloader-dummy.o

# take a look at this:
# http://stackoverflow.com/questions/714100/os-detecting-makefile
# $OSTYPE in freebsd
ifeq ($(OS),Windows_NT)
	EXEFILE=fda-downloader.exe
	OBJ_IMPL=fda-downloader-win.o
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



_DEPS = src/fda-downloader.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

.PHONY: clean all

all: $(EXEFILE)

_OBJ = fda-downloader.o $(OBJ_IMPL)
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: src/%.c $(DEPS) $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEFILE): $(OBJ)
	gcc -o $@ $^ $(LDFLAGS)

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -f $(ODIR)/*.o *~ core src/*~ $(EXEFILE)
