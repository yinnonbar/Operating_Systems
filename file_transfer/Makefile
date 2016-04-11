CC=g++

PROGSRC=clftp.cpp srftp.cpp
PROGOBJ=$(PROGSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -g  $(INCS)

OUTPUT = clftp srftp
TARGETS = $(OUTPUT)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS=$(PROGSRC) Makefile README performance.jpg

all: $(TARGETS)

clftp: clftp.o
	$(CC) $(CFLAGS) $^ -o $@

srftp: srftp.o
	$(CC) $(CFLAGS)  $^ -o $@ -lpthread

clean:
	$(RM) $(TARGETS) $(PROGOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(PROGSRC)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $(INC) $(FUSE_COMPILE) $< -o $@

tar:
	$(TAR) $(TARFLAGS)  $(TARNAME) $(TARSRCS)

force:
	touch $(PROGSRC)
	make

.PHONY: tar clean force all
