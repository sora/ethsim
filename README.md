## Install

```bash
$ make
$ sudo ./add.sh 4 `whoami`
$ ls /dev/net
$ ip addr
$ make sim
# Ctrl-c
$ sudo ./del.sh 4 `whoami`
$ gtkwave ./wave.vcd
$ make clean
```
## debug

sim_main.cpp

```c
// debug message: ON
static int debug = 1;

// debug message: OFF
static int debug = 0;
```

