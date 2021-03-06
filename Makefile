TARGET=scanner
OBJS=objs

CC=gcc

# -g for debugger
#CCOPTS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-1.2 -I/usr/lib64/glib/include
CCOPTS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-2.0
#CCOPTS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -I/usr/include/glib-2.0 -I/usr/lib64/glib/include

LIBS=-lm -lpthread -lbluetooth -lcurses

all: clean $(TARGET)

clean:
	rm -f $(OBJS)/*.o $(TARGET)


#####################################################################
# Linking

$(TARGET): $(OBJS)/$(TARGET).o $(OBJS)/$(TARGET)_util.o $(OBJS)/$(TARGET)_parser.o $(OBJS)/$(TARGET)_connect.o $(OBJS)/example.o $(OBJS)/advertisetest.o
	$(CC) $(CCOPTS) $(OBJS)/$(TARGET).o $(OBJS)/$(TARGET)_parser.o $(OBJS)/$(TARGET)_connect.o $(OBJS)/$(TARGET)_util.o $(LIBS) -o $(TARGET)
	$(CC) $(CCOPTS) $(OBJS)/example.o $(LIBS) -o example
	$(CC) $(CCOPTS) $(OBJS)/advertisetest.o $(LIBS) -o advertisetest

# Compiling

$(OBJS)/$(TARGET).o: $(TARGET).c $(TARGET).h
	$(CC) $(CCOPTS) $(TARGET).c -c -o $(OBJS)/$(TARGET).o

$(OBJS)/$(TARGET)_parser.o: $(TARGET)_parser.c $(TARGET)_parser.h
	$(CC) $(CCOPTS) $(TARGET)_parser.c -c -o $(OBJS)/$(TARGET)_parser.o

$(OBJS)/$(TARGET)_connect.o: $(TARGET)_connect.c $(TARGET)_connect.h
	$(CC) $(CCOPTS) $(TARGET)_connect.c -c -o $(OBJS)/$(TARGET)_connect.o

$(OBJS)/$(TARGET)_util.o: $(TARGET)_util.c $(TARGET)_util.h
	$(CC) $(CCOPTS) $(TARGET)_util.c -c -o $(OBJS)/$(TARGET)_util.o

$(OBJS)/example.o: example.c
	$(CC) $(CCOPTS) example.c -c -o $(OBJS)/example.o

$(OBJS)/advertisetest.o: advertisetest.c
	$(CC) $(CCOPTS) advertisetest.c -c -o $(OBJS)/advertisetest.o

