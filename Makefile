DEVNUM = 2

CFLAGS += -Wall -O2

WFLAGS = -Wall
sim_top = testbench
VERILATOR_SRC := sim_main.cpp
SIM_SRC := testbench.sv
RTL_SRC := dut.v

all: tapctl

run: tapctl sim
	./run.sh $(DEVNUM)

tapctl: tapctl.c
	gcc $(CFLAGS) -o tapctl tapctl.c

sim: $(VERILATOR_SRC) $(SIM_SRC) $(RTL_SRC)
	verilator $(WFLAGS) -DSIMULATION --cc --trace --top-module testbench -sv $(SIM_SRC) $(RTL_SRC) --exe $(VERILATOR_SRC)
	make -j -C obj_dir/ -f V$(sim_top).mk V$(sim_top)
	./obj_dir/Vtestbench

.PHONY: clean
clean:
	rm -f tapctl
	rm -f wave.vcd
	rm -rf obj_dir
