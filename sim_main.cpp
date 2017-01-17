#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtestbench.h"

#define SFP_CLK           (64/2)        // 6.4 ns (156.25 MHz)

#define WAVE_FILE_NAME        "wave.vcd"
#define SIM_TIME_RESOLUTION   "100 ps"


struct sim {
	vluint64_t main_time;

	Vtestbench *top;
	
	VerilatedVcdC *tfp;
};

static inline void tick(struct sim *s)
{
	s->top->eval();
	s->tfp->dump(s->main_time);
	++s->main_time;
}

static inline void clock(struct sim *s)
{
	if ((s->main_time % SFP_CLK) == 0)
		s->top->clk156 = !s->top->clk156;
}

static inline void cold_reset(struct sim *s)
{
	if ((s->main_time % 20) == 0)
		sim.top->cold_reset = 1;
}

int main(int argc, char** argv)
{
	struct sim sim;

	Verilated::commandArgs(argc, argv);
	Verilated::traceEverOn(true);

	sim.main_time = 0;
	sim.top = new Vtestbench;
	sim.tfp = new VerilatedVcdC;

	sim.top->trace(sim.tfp, 99);
	sim.tfp->spTrace()->set_time_resolution(SIM_TIME_RESOLUTION);
	sim.tfp->open(WAVE_FILE_NAME);


	// init
	sim.top->clk156 = 0;
	sim.top->cold_reset = 0;

	// run
	for (;;) {
		if (sim.main_time > 200) {
			break;
		}

		cold_reset(&sim);

		clock(&sim);
		tick(&sim);
	}

	sim.tfp->close();
	sim.top->final();

	return 0;
}

