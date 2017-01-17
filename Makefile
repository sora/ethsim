CFLAGS += -Wall -O2

all: tap

tap: tapdev.c
	gcc $(CFLAGS) -o tapdev  tapdev.c

.PHONY: clean
clean:
	rm -f tapdev

