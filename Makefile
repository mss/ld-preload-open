default: all

CFLAGS += -O2 -std=gnu11 -Wall

libpath-mapping.so: path-mapping.c
	$(CC) $(CFLAGS) -s -shared -fPIC "$<" -o "$@" -ldl

clean:
	$(RM) *.so

all: libpath-mapping.so

.PHONY: default all clean

