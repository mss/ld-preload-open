default: all

CFLAGS += -O2 -std=gnu11 -Wall

libpathmunger.so: pathmunger.c
	$(CC) $(CFLAGS) -s -shared -fPIC "$<" -o "$@" -ldl

clean:
	$(RM) *.so

all: libpathmunger.so

.PHONY: default all clean

