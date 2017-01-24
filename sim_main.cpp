#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtestbench.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SFP_CLK           (64/2)        // 6.4 ns (156.25 MHz)

#define WAVE_FILE_NAME        "wave.vcd"
#define SIM_TIME_RESOLUTION   "100 ps"

#define IFNAMSIZ     256
#define TAP_PATH     "/dev/net"
#define TAP_NAME     "phy"

#define N    1

struct port {
	unsigned int pos;
	unsigned int len;
	unsigned char buf[2048];
};

struct phy {
	char dev[IFNAMSIZ];
	int fd;
	struct port tx;
	struct port rx;
};

struct sim {
	vluint64_t main_time;

	Vtestbench *top;
	
	VerilatedVcdC *tfp;

	struct phy *phy;
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
		s->top->cold_reset = 1;
}

static inline int tap_open(struct phy *phy)
{
	phy->fd = open(phy->dev, O_RDWR);
	if (phy->fd < 0) {
		perror("open");
		goto err;
	}

err:
	return phy->fd;
}

static inline int eth_recv(struct phy *phy)
{
	return 0;
}

static inline int eth_send(struct phy *phy)
{
	return 0;
}

int main(int argc, char** argv)
{
	struct sim sim;
	int i, ret;

	sim.phy = (struct phy *)malloc(sizeof(struct phy) * N);
	if (sim.phy == NULL) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < N; i++) {
		sprintf(sim.phy[i].dev, "%s/%s%d", TAP_PATH, TAP_NAME, i);

		ret = tap_open(&sim.phy[i]);
		if (ret < 0) {
			perror("tap_open");
			return -1;
		}
	}

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
		// timeout
		if (sim.main_time > 200) {
			break;
		}

		cold_reset(&sim);

		for (i = 0; i < N; i++) {
			ret = eth_recv(&sim.phy[i]);
			if (ret < 0) {
				perror("eth_recv()");
				break;
			}

			ret = eth_send(&sim.phy[i]);
			if (ret < 0) {
				perror("eth_send()");
				break;
			}
		}

		clock(&sim);
		tick(&sim);
	}

	sim.tfp->close();
	sim.top->final();

	free(sim.phy);

	return 0;
}

