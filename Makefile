
LIBNAME=libacq
SONAME=$(LIBNAME).so.1
LIBFILE=$(LIBNAME).so.1.0.1
LIBDIR=../lib

CFLAGS+=-I../include

#all: fs2xml proc-ints transpose demux bin2json checkramp lsa ob_calc_527

PROGS=fs2xml  ob_calc_527 daemon xiloader

all: $(PROGS)

fs2xml: fs2xml.o
	$(CC) -o fs2xml fs2xml.o -L ../lib -lpopt

ob_calc_527: ob_calc_527.o
	$(CC) -o ob_calc_527 ob_calc_527.o -L ../lib -lpopt

load: load.o
	$(CC) -o load load.o -L ../lib -lpopt

proc-ints: proc-ints.o
	$(CC) -o proc-ints proc-ints.o -L ../lib -lpopt

fungen: fungen.o
	$(CC) -o fungen fungen.o -L ../lib -lpopt -lm

checkramp: checkramp.o
	$(CC) -o checkramp checkramp.o -L ../lib -lpopt -lm

transpose: transpose.o
	$(CXX) -o transpose transpose.o -L ../lib -lpopt

demux: demux.o
	$(CXX) -o demux demux.o -L ../lib -lpopt -lm

xiloader: xiloader.o
	$(CXX) -o xiloader xiloader.o -L ../lib -lpopt

xiloader.x86: xiloader.c
	$(CXX) -o xiloader.x86 xiloader.c -lpopt

lsa: lsa.o ptypair.o usc.o
	$(CC) -o lsa $^ -L ../lib -lpopt

bin2json: bin2json.o
	$(CXX) -o bin2json bin2json.o -L ../lib -lpopt -lm

lib: acq-util.c
	$(CC) -I../include -fPIC acq-util.c \
		-shared -Wl,-soname,$(SONAME) -o $(LIBDIR)/$(LIBFILE) -lc
	cd $(LIBDIR); \
	ln -s $(LIBFILE) $(SONAME); \
        ln -s $(LIBFILE) $(LIBNAME).so

clean:
	rm -f *.o $(PROGS)


