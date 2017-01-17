DEVNUM = 2

CFLAGS += -Wall -O2

all: tap

run: tap
	./run.sh 

tap: mktap.c rmtap.c
	gcc $(CFLAGS) -o mktap  mktap.c
	gcc $(CFLAGS) -o rmtap  rmtap.c

.PHONY: clean
clean:
	rm -f mktap rmtap

