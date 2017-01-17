DEVNUM = 2

CFLAGS += -Wall -O2

all: run

run: tap
	./run.sh

tap: tapdev.c
	gcc $(CFLAGS) -o tapdev  tapdev.c

.PHONY: clean
clean:
	rm -f tapdev

