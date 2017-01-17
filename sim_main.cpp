#include <verilated.h>
#include "Vtestbench.h"

vluint64_t main_time = 0;

double sc_time_stamp () {
	return main_time;
}

int main(int argc, char** argv) {
	Verilated::commandArgs(argc, argv);

	Vtestbench *top = new Vtestbench;

	top->cold_reset = 0;

	while (!Verilated::gotFinish()) {
		if (main_time > 200) {
			break;
		}

		if (main_time > 10) {
			top->cold_reset = 1;
		}

		if ((main_time % 10) == 1) {
			top->clk156 = 1;
		}

		if ((main_time % 10) == 6) {
			top->clk156 = 0;
		}

		top->eval();
		main_time++;
	}

	top->final();

	delete top;
}

