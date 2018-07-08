LGS     = ../../src/simulator
CC      = arm-linux-gnueabi-gcc-7
CFLAGS  = -static -I $(LGS)/modules/spin/clientlib -I $(LGS)/modules/gem5/clientlib  -I $(LGS)
RM      = rm -f


default: all

all: handlers

handlers: simple_send/handlers.c
	$(CC) $(CFLAGS) -o simple_send/handlers simple_send/handlers.c $(LGS)/modules/gem5/clientlib/lgslib.c $(LGS)/modules/spin/clientlib/spin.c

clean veryclean:
	$(RM) Hello

