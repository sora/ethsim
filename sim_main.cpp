#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtestbench.h"

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <signal.h>

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

int caught_signal = 0;

struct port {
	unsigned int rdy;
	unsigned int len;
	unsigned int pos;
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

	int do_sim;

	int gap_budget;

	int txpackets;
	int rxpackets;
};


void sig_handler(int sig) {
	if (sig == SIGINT)
		caught_signal = 1;
}

void set_signal(int sig) {
	if ( signal(sig, sig_handler) == SIG_ERR ) {
		fprintf( stderr, "Cannot set signal\n" );
		exit(1);
	}
}

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
		s->poll_fds[i].events = POLLIN | POLLERR;
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
	int budget = 10;

	while (--budget) {
		clock(s);
		tick(s);
	}
	s->top->cold_reset = 0;
}

static inline int eth_recv(struct phy *phy)
{
	return (phy->rx.len = read(phy->fd, phy->rx.buf, sizeof(phy->rx.buf)));
}

static inline int eth_send(struct phy *phy)
{
	return write(phy->fd, phy->tx.buf, phy->tx.pos);
}

static inline void rxsim(struct sim *s, int n)
{
	struct port *rx = &s->phy[n].rx;

	printf("rxsim\n");

	s->top->s_axis_rx_tvalid = 1;
	s->top->s_axis_rx_tkeep = 0xFF;
	s->top->s_axis_rx_tuser = 0;

	// s_axis_rx_tdata
	*(uint64_t *)s->top->m_axis_tx_tdata = *(uint64_t *)&rx->buf[rx->pos];

	if (rx->pos == rx->len) {
		s->top->s_axis_rx_tlast = 1;
		s->gap_budget = 3;
	} else {
		s->top->s_axis_rx_tlast = 0;
	}

	++rx->pos;
	s->do_sim = 1;
}

static inline void txsim(struct sim *s, int n)
{
	struct port *tx = &s->phy[n].tx;

	printf("txsim\n");

	printf("tx_pos=%d\ttdata=%lu\n",
		tx->pos, s->top->m_axis_tx_tdata);
	*(uint64_t *)&tx->buf[tx->pos] = *(uint64_t *)s->top->m_axis_tx_tdata;

	++tx->pos;
	s->do_sim = 1;
}

int main(int argc, char** argv)
{
	struct sim sim;
	int i, ret; //, timeout = 2000;

	sim.ndev = N;  // todo

	sim.phy = (struct phy *)malloc(sizeof(struct phy) * sim.ndev);
	if (sim.phy == NULL) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < sim.ndev; i++) {
		sprintf(sim.phy[i].dev, "%s/%s%d", TAP_PATH, TAP_NAME, i);

		sim.phy[i].tx.pos = 0;
		sim.phy[i].rx.pos = 0;
		sim.phy[i].tx.rdy = 0;    // unused
		sim.phy[i].rx.rdy = 0;
		sim.phy[i].tx.len = 0;    // unused
		sim.phy[i].rx.len = 0;

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
	sim.top->cold_reset = 1;

	cold_reset(&sim);

	sim.txpackets = 0;
	sim.rxpackets = 0;
	sim.gap_budget = 3;

	set_signal(SIGINT);

	// run
//	while (--timeout) {
	while (1) {
		if (caught_signal)
			break;


		if (sim.rxpackets > 1) {
			printf("Simulation finished. rxpackets=%d\n", sim.rxpackets);
			break;
		}


		sim.do_sim = 0;

		poll(sim.poll_fds, sim.ndev, 0);

		for (i = 0; i < sim.ndev; i++) {

			printf("main_time=%u\tcold_reset=%d\ttx_tvalid=%d\n",
				(uint32_t)sim.main_time, sim.top->cold_reset,
				sim.top->m_axis_tx_tvalid);

			// RX simulation
			if (sim.gap_budget > 0) {
				// insert a gap when receiving a packet
				if ((sim.main_time % SFP_CLK) == 0) {
					--sim.gap_budget;
				}
				sim.do_sim = 1;
				break;
			} else {
				if (sim.phy[i].rx.rdy) {
					rxsim(&sim, i);
				} else {
					sim.phy[i].rx.rdy = 0;


					// packet received
					if (sim.poll_fds[i].revents & POLLIN) {
						sim.phy[i].rx.rdy = 1;
						sim.phy[i].rx.pos = 0;

						printf("rx.dry: i=%d\n", i);

						ret = eth_recv(&sim.phy[i]);
						if (ret < 0) {
							perror("eth_recv");
							break;
						}

						sim.phy[i].rx.len = ret;
					}
				}
			}

			// TX simulation
			if (sim.top->m_axis_tx_tvalid) {
				txsim(&sim, i);

				// packet send
				if (sim.top->m_axis_tx_tlast) {
					ret = eth_send(&sim.phy[i]);
					if (ret < 0) {
						perror("eth_write");
						break;
					}
				}
			} else {
				sim.phy[i].tx.pos = 0;
			}
		}

		if (sim.do_sim) {
			clock(&sim);
			tick(&sim);
		}
	}

	sim.tfp->close();
	sim.top->final();

	free(sim.phy);

	return 0;
}

