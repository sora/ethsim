#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtestbench.h"

#include <fcntl.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>

#define SFP_CLK           (64/2)        // 6.4 ns (156.25 MHz)

#define WAVE_FILE_NAME        "wave.vcd"
#define SIM_TIME_RESOLUTION   "100 ps"

#define IFNAMSIZ     256
#define TAP_PATH     "/tmp"
#define TAP_NAME     "phy"

#define MAXNUMDEV    8
#define N    1

struct port {
	unsigned int ready;
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

	int ndev;

	struct phy *phy;

	struct pollfd poll_fds[MAXNUMDEV];
};

int tap_open(struct phy *phy)
{
	phy->fd = open(phy->dev, O_RDWR);
	if (phy->fd < 0) {
		perror("open");
		goto err;
	}

err:
	return phy->fd;
}

void poll_init(struct sim *s)
{
	int i, ret;

	memset(&s->poll_fds, 0, sizeof(s->poll_fds));

	for (i = 0; i < s->ndev; i++) {
		s->poll_fds[i].fd = s->phy[i].fd;
		s->poll_fds[i].events = POLLIN;
	}
}

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

static inline int eth_recv(struct phy *phy)
{
	int ret;

	ret = read(phy->fd, phy->rx.buf, sizeof(phy->rx.buf));
	if (ret < 0) {
		perror("read");
		return -1;
	}

	return 0;
}

static inline int eth_send(struct phy *phy)
{
	int ret;

	ret = write(phy->fd, phy->tx.buf, sizeof(phy->tx.buf));
	if (ret < 0) {
		perror("read");
		return -1;
	}

	return 0;
}

int main(int argc, char** argv)
{
	struct sim sim;
	int i, ret;

	sim.ndev = N;  // todo

	sim.phy = (struct phy *)malloc(sizeof(struct phy) * sim.ndev);
	if (sim.phy == NULL) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < sim.ndev; i++) {
		sprintf(sim.phy[i].dev, "%s/%s%d", TAP_PATH, TAP_NAME, i);

		sim.phy[i].tx.ready = 0;

		sim.phy[i].rx.ready = 0;

		ret = tap_open(&sim.phy[i]);
		if (ret < 0) {
			perror("tap_open");
			return -1;
		}
	}

	poll_init(&sim);

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

		poll(sim.poll_fds, sim.ndev, 0);

		for (i = 0; i < sim.ndev; i++) {

			if (sim.poll_fds[i].revents & POLLIN) {
				ret = eth_recv(&sim.phy[i]);
				if (ret < 0) {
					perror("eth_recv()");
					break;
				}
			}

			if (sim.phy[i].tx.ready) {
				ret = eth_send(&sim.phy[i]);
				if (ret < 0) {
					perror("eth_send()");
					break;
				}
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

