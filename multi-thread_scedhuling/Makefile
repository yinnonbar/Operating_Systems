CC=g++
RANLIB=ranlib

LIBSRC=uthreads.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -std=c++11 -Wall -g $(INCS)

OSMLIB = libuthreads.a
TARGETS = $(OSMLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex2.tar
TARSRCS=$(LIBSRC) Makefile README

all: $(TARGETS) 

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS)  $@ $^
	$(RANLIB) $@
	
clean:
	$(RM) $(TARGETS) $(OSMLIB) $(LIBOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)
	
%.o: %.cpp
	$(CC) -c $(CFLAGS) $(INC) $< -o $@

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS) uthreads.h
