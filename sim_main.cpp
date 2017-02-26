#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtestbench.h"

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define SFP_CLK           (64/2)        // 6.4 ns (156.25 MHz)

#define WAVE_FILE_NAME        "wave.vcd"
#define SIM_TIME_RESOLUTION   "100 ps"

//#define IFNAMSIZ     256
#define TAP_PATH     "/dev/net"
#define TAP_NAME     "tap"

#define MAXNUMDEV    8
#define N    2

#define pr_info(S, ...)   printf("\x1b[1m\x1b[94minfo:\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_err(S, ...)    fprintf(stderr, "\x1b[1m\x1b[31merror:\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_warn(S, ...)   if(warn) fprintf(stderr, "\x1b[1m\x1b[33mwarn :\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_debug(S, ...)  if (debug) fprintf(stderr, "\x1b[1m\x1b[90mdebug:\x1b[0m " S "\n", ##__VA_ARGS__)

static int debug = 0;
static int caught_signal = 0;


struct tx {
	unsigned int pos;
	unsigned char buf[2048];
};

struct rx {
	unsigned int rdy;
	unsigned int len;
	unsigned int pos;
	unsigned int gap;
	unsigned char buf[2048];
};

struct phy {
	char dev[IFNAMSIZ];
	int fd;
	struct tx tx;
	struct rx rx;
};

struct sim {
	vluint64_t main_time;

	Vtestbench *top;
	
	VerilatedVcdC *tfp;

	int ndev;

	struct phy *phy;

	struct pollfd poll_fds[MAXNUMDEV];

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

#if 0
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
#endif

int tap_open(struct phy *phy)
{
	struct ifreq ifr;
	int err;

	phy->fd = open("/dev/net/tun", O_RDWR);
	if (phy->fd < 0) {
		perror("open");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	printf("dev: %s\n", phy->dev);
	if (phy->dev) {
		strncpy(ifr.ifr_name, phy->dev, IFNAMSIZ);
	}

	err = ioctl(phy->fd, TUNSETIFF, (void *)&ifr);
	if (err < 0) {
		perror("TUNSETIFF");
		close(phy->fd);
		return err;
	}

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

static inline int eth_xmit(struct phy *phy)
{
	return write(phy->fd, phy->tx.buf, phy->tx.pos);
}

static inline unsigned int tkeep_bit(uint8_t n)
{
	switch (n) {
		case 1: return 0x80;
		case 2: return 0xc0;
		case 3: return 0xe0;
		case 4: return 0xf0;
		case 5: return 0xf8;
		case 6: return 0xfc;
		case 7: return 0xfe;
		case 8: return 0xff;
		default:
			pr_err("tkeep_bit: unknown value");
			exit(1);
	}
}

static inline void rxsim_idle(struct sim *s, int n)
{
	s->top->s_axis_rx_tvalid = 0;
	s->top->s_axis_rx_tuser = 0;
	s->top->s_axis_rx_tkeep = 0;
	s->top->s_axis_rx_tdata = 0;
	s->top->s_axis_rx_tlast = 0;
}

static inline void rxsim(struct sim *s, int n)
{
	struct rx *rx = &s->phy[n].rx;
	uint8_t *p = (uint8_t *)&s->top->s_axis_rx_tdata;
	uint8_t tkeep;
	int i;

	s->top->s_axis_rx_tvalid = 1;
	s->top->s_axis_rx_tuser = 0;

	// s_axis_rx_tkeep
	if ((rx->len - rx->pos) > 8) {
		tkeep = 8;
	} else {
		tkeep = (rx->len - rx->pos) % 8;
		if (tkeep == 0) {
			tkeep = 8;
		}
	}
	s->top->s_axis_rx_tkeep = tkeep_bit(tkeep);

	// s_axis_rx_tdata
	for (i = 0; i < tkeep; i++) {
		*(p+7-i) = *(uint8_t *)&rx->buf[rx->pos++];
	}

	pr_debug("rxsim: n=%d, rdy=%d, rx->pos=%d, rx->len=%d, tkeep=%d, tkeep=%02X",
			n, rx->rdy, rx->pos, rx->len, tkeep, s->top->s_axis_rx_tkeep);

	if (rx->pos == rx->len) {
		s->top->s_axis_rx_tlast = 1;
		rx->rdy = 0;
		rx->len = 0;
		rx->pos = 0;
		rx->gap = 3;
		pr_debug("-------------------------");
	} else {
		s->top->s_axis_rx_tlast = 0;
	}
}

static inline void txsim(struct sim *s, int n)
{
	uint8_t *p = (uint8_t *)&s->top->m_axis_tx_tdata;
	struct tx *tx = &s->phy[n].tx;
	int i;

	pr_info("txsim: n=%d, tx->pos=%d, tkeep=%02X",
			n, tx->pos, s->top->s_axis_rx_tkeep);

	for (i = 0; i < 8; i++) {
		*(uint8_t *)&tx->buf[tx->pos++] = *(p+7-i);
	}
}

static inline void pkt_recv(struct sim *s, int n)
{
	int count;

	if ((s->poll_fds[n].revents & POLLIN) == POLLIN) {
		count = eth_recv(&s->phy[n]);
		if (count < 0) {
			perror("eth_recv");
			exit(1);
		}
		pr_debug("Receiving a packet: phy%d, count=%d", n, count);

		s->phy[n].rx.len = count;
		s->phy[n].rx.rdy = 1;
	}
}

static inline void pkt_send(struct sim *s, int n)
{
	int count;

	if (s->top->m_axis_tx_tlast) {
		count = eth_xmit(&s->phy[n]);
		if (count < 0) {
			perror("eth_write");
			exit(1);
		}
		pr_debug("Sending a packet: phy%d, count=%d", n, count);

		s->phy[n].tx.pos = 0;
	}
}


int main(int argc, char** argv)
{
	struct sim sim;
	int i, ret, do_sim; //, timeout = 2000;

	sim.ndev = N;  // todo

	sim.phy = (struct phy *)malloc(sizeof(struct phy) * sim.ndev);
	if (sim.phy == NULL) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < sim.ndev; i++) {
		sprintf(sim.phy[i].dev, "%s%d", TAP_NAME, i);

		sim.phy[i].tx.pos = 0;
		sim.phy[i].rx.pos = 0;
		sim.phy[i].rx.rdy = 0;
		sim.phy[i].rx.len = 0;
		sim.phy[i].rx.gap = 3;

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

	set_signal(SIGINT);

	// run
	while (1) {
		if (caught_signal)
			break;

		if (sim.rxpackets > 1) {
			printf("Simulation finished. rxpackets=%d\n", sim.rxpackets);
			break;
		} else {
			//printf("rxpackets=%d\n", sim.rxpackets);
			;
		}


		do_sim = 0;

		poll(sim.poll_fds, sim.ndev, -1);

		for (i = 0; i < sim.ndev; i++) {

			// RX simulation
			if (sim.phy[i].rx.gap > 0) {
				// insert a gap when receiving a packet
				if ((sim.main_time % SFP_CLK) == 0) {
					--sim.phy[i].rx.gap;
				}
				do_sim = 1;
			} else {
				if (sim.phy[i].rx.rdy) {
					rxsim(&sim, i);
					do_sim = 1;
				} else {
					rxsim_idle(&sim, i);
					pkt_recv(&sim, i);
				}
			}

			// TX simulation
			if (sim.top->m_axis_tx_tvalid) {
				txsim(&sim, i);
				do_sim = 1;

				pkt_send(&sim, i);
			} else {
				sim.phy[i].tx.pos = 0;
			}
		}

		if (do_sim) {
			clock(&sim);
			tick(&sim);
		}
	}

	printf("finish\n");

	sim.tfp->close();
	sim.top->final();

	free(sim.phy);

	return 0;
}

