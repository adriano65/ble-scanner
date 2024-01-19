TARGET=scanner
CC=gcc

CFLAGS:=-std=gnu99 -DMYNAME=\"$(TARGET)\" -Wno-unknown-pragmas
SOFTREL :=	1
SUBSREL :=  00
CFLAGS += -DSOFTREL=$(SOFTREL) -DSUBSREL=$(SUBSREL)


# -g for debugger
#CFLAGS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-1.2 -I/usr/lib64/glib/include
CFLAGS += -O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-2.0
#CFLAGS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-2.0 -I/usr/lib64/glib/include

LLOPTS=-lm -lpthread -lbluetooth -lcurses

all: $(TARGET)

clean:
	rm -f *.o
	rm $(TARGET)

$(TARGET): *.c *.h
	@echo "..."
	$(CC) $(CFLAGS) $(TARGET)_parser.c $(TARGET)_connect.c $(TARGET)_util.c $(TARGET).c $(LLOPTS) -o $(TARGET)
	$(CC) $(CFLAGS) example.c $(LLOPTS) -o example
	$(CC) $(CFLAGS) advertisetest.c $(LLOPTS) -o advertisetest
