TARGET=scanner
OBJS=objs

CC=gcc

# -g for debugger
CCOPTS=-O -Wall -g -D_REENTRANT -DMYNAME=\"$(TARGET)\" -Wall

LIBS=-lm -lpthread -lbluetooth 

all: clean $(TARGET)

clean:
	rm -f $(OBJS)/*.o $(TARGET)


#####################################################################
# Linking

$(TARGET): $(OBJS)/$(TARGET).o $(OBJS)/$(TARGET)_util.o
	$(CC) $(CCOPTS) $(OBJS)/$(TARGET).o $(OBJS)/$(TARGET)_util.o $(LIBS) -o $(TARGET)

# Compiling

$(OBJS)/$(TARGET).o: $(TARGET).c $(TARGET).h
	$(CC) $(CCOPTS) $(TARGET).c -c -o $(OBJS)/$(TARGET).o

$(OBJS)/$(TARGET)_util.o: $(TARGET)_util.c $(TARGET)_util.h
	$(CC) $(CCOPTS) $(TARGET)_util.c -c -o $(OBJS)/$(TARGET)_util.o

