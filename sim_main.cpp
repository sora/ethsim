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


#define pr_info(S, ...)   printf("\x1b[1m\x1b[94minfo:\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_err(S, ...)    fprintf(stderr, "\x1b[1m\x1b[31merror:\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_warn(S, ...)   if(warn) fprintf(stderr, "\x1b[1m\x1b[33mwarn :\x1b[0m " S "\n", ##__VA_ARGS__)
#define pr_debug(S, ...)  if (debug) fprintf(stderr, "\x1b[1m\x1b[90mdebug:\x1b[0m " S "\n", ##__VA_ARGS__)

#define N            2
#define MAXNUMDEV    4

static int debug = 0;
static int caught_signal = 0;

struct eth10g_axis {
	uint8_t *tvalid;
	uint8_t *tkeep;
	uint8_t *tdata;
	uint8_t *tlast;
	uint8_t *tuser;
	uint8_t *tready;
};

struct tx {
	unsigned int pos;
	unsigned char buf[2048];
	struct eth10g_axis axis;
};

struct rx {
	unsigned int rdy;
	unsigned int len;
	unsigned int pos;
	unsigned int gap;
	unsigned char buf[2048];
	struct eth10g_axis axis;
};

struct phy {
	char dev[IFNAMSIZ];
	int n;
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

	struct pollfd poll_fds[MAXNUMDEV];  // same value of phy.fd

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

void phy_setup(struct sim *s)
{
	struct phy *phy;
	int ret, i;

	for (i = 0; i < s->ndev; i++) {
		phy = &s->phy[i];

		sprintf(phy->dev, "%s%d", TAP_NAME, i);

		phy->n = i;

		phy->tx.pos = 0;
		phy->rx.pos = 0;
		phy->rx.rdy = 0;
		phy->rx.len = 0;
		phy->rx.gap = 3;

		ret = tap_open(phy);
		if (ret < 0) {
			perror("tap_open");
			exit(1);
		}
	}
}

void axis_setup(struct sim *s)
{
	// phy0
	s->phy[0].tx.axis.tvalid = (uint8_t *)&s->top->phy0_tx_tvalid;
	s->phy[0].tx.axis.tkeep  = (uint8_t *)&s->top->phy0_tx_tkeep;
	s->phy[0].tx.axis.tdata  = (uint8_t *)&s->top->phy0_tx_tdata;
	s->phy[0].tx.axis.tlast  = (uint8_t *)&s->top->phy0_tx_tlast;
	s->phy[0].tx.axis.tuser  = (uint8_t *)&s->top->phy0_tx_tuser;
	s->phy[0].tx.axis.tready = (uint8_t *)&s->top->phy0_tx_tready;
	s->phy[0].rx.axis.tvalid = (uint8_t *)&s->top->phy0_rx_tvalid;
	s->phy[0].rx.axis.tkeep  = (uint8_t *)&s->top->phy0_rx_tkeep;
	s->phy[0].rx.axis.tdata  = (uint8_t *)&s->top->phy0_rx_tdata;
	s->phy[0].rx.axis.tlast  = (uint8_t *)&s->top->phy0_rx_tlast;
	s->phy[0].rx.axis.tuser  = (uint8_t *)&s->top->phy0_rx_tuser;
	s->phy[0].rx.axis.tready = (uint8_t *)&s->top->phy0_rx_tready;

	// phy1
	s->phy[1].tx.axis.tvalid = (uint8_t *)&s->top->phy1_tx_tvalid;
	s->phy[1].tx.axis.tkeep  = (uint8_t *)&s->top->phy1_tx_tkeep;
	s->phy[1].tx.axis.tdata  = (uint8_t *)&s->top->phy1_tx_tdata;
	s->phy[1].tx.axis.tlast  = (uint8_t *)&s->top->phy1_tx_tlast;
	s->phy[1].tx.axis.tuser  = (uint8_t *)&s->top->phy1_tx_tuser;
	s->phy[1].tx.axis.tready = (uint8_t *)&s->top->phy1_tx_tready;
	s->phy[1].rx.axis.tvalid = (uint8_t *)&s->top->phy1_rx_tvalid;
	s->phy[1].rx.axis.tkeep  = (uint8_t *)&s->top->phy1_rx_tkeep;
	s->phy[1].rx.axis.tdata  = (uint8_t *)&s->top->phy1_rx_tdata;
	s->phy[1].rx.axis.tlast  = (uint8_t *)&s->top->phy1_rx_tlast;
	s->phy[1].rx.axis.tuser  = (uint8_t *)&s->top->phy1_rx_tuser;
	s->phy[1].rx.axis.tready = (uint8_t *)&s->top->phy1_rx_tready;

	// phy2
	s->phy[2].tx.axis.tvalid = (uint8_t *)&s->top->phy2_tx_tvalid;
	s->phy[2].tx.axis.tkeep  = (uint8_t *)&s->top->phy2_tx_tkeep;
	s->phy[2].tx.axis.tdata  = (uint8_t *)&s->top->phy2_tx_tdata;
	s->phy[2].tx.axis.tlast  = (uint8_t *)&s->top->phy2_tx_tlast;
	s->phy[2].tx.axis.tuser  = (uint8_t *)&s->top->phy2_tx_tuser;
	s->phy[2].tx.axis.tready = (uint8_t *)&s->top->phy2_tx_tready;
	s->phy[2].rx.axis.tvalid = (uint8_t *)&s->top->phy2_rx_tvalid;
	s->phy[2].rx.axis.tkeep  = (uint8_t *)&s->top->phy2_rx_tkeep;
	s->phy[2].rx.axis.tdata  = (uint8_t *)&s->top->phy2_rx_tdata;
	s->phy[2].rx.axis.tlast  = (uint8_t *)&s->top->phy2_rx_tlast;
	s->phy[2].rx.axis.tuser  = (uint8_t *)&s->top->phy2_rx_tuser;
	s->phy[2].rx.axis.tready = (uint8_t *)&s->top->phy2_rx_tready;

	// phy3
	s->phy[3].tx.axis.tvalid = (uint8_t *)&s->top->phy3_tx_tvalid;
	s->phy[3].tx.axis.tkeep  = (uint8_t *)&s->top->phy3_tx_tkeep;
	s->phy[3].tx.axis.tdata  = (uint8_t *)&s->top->phy3_tx_tdata;
	s->phy[3].tx.axis.tlast  = (uint8_t *)&s->top->phy3_tx_tlast;
	s->phy[3].tx.axis.tuser  = (uint8_t *)&s->top->phy3_tx_tuser;
	s->phy[3].tx.axis.tready = (uint8_t *)&s->top->phy3_tx_tready;
	s->phy[3].rx.axis.tvalid = (uint8_t *)&s->top->phy3_rx_tvalid;
	s->phy[3].rx.axis.tkeep  = (uint8_t *)&s->top->phy3_rx_tkeep;
	s->phy[3].rx.axis.tdata  = (uint8_t *)&s->top->phy3_rx_tdata;
	s->phy[3].rx.axis.tlast  = (uint8_t *)&s->top->phy3_rx_tlast;
	s->phy[3].rx.axis.tuser  = (uint8_t *)&s->top->phy3_rx_tuser;
	s->phy[3].rx.axis.tready = (uint8_t *)&s->top->phy3_rx_tready;
}

void poll_setup(struct sim *s)
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

static inline void rxsim_idle(struct rx *rx)
{
	*rx->axis.tvalid = 0;
	*rx->axis.tkeep  = 0;
	*rx->axis.tdata  = 0;
	*rx->axis.tuser  = 0;
}

static inline void rxsim(struct rx *rx)
{
	uint8_t *p = rx->axis.tdata;
	uint8_t tkeep;
	int i;

	*rx->axis.tvalid = 1;
	*rx->axis.tuser = 0;

	// s_axis_rx_tkeep
	if ((rx->len - rx->pos) > 8) {
		tkeep = 8;
	} else {
		tkeep = (rx->len - rx->pos) % 8;
		if (tkeep == 0) {
			tkeep = 8;
		}
	}
	*rx->axis.tkeep = tkeep_bit(tkeep);

	// s_axis_rx_tdata
	for (i = 0; i < tkeep; i++) {
		*(p+7-i) = *(uint8_t *)&rx->buf[rx->pos++];
	}

	pr_debug("rxsim: rdy=%d, rx->pos=%d, rx->len=%d, tkeep=%d, tkeep=%02X",
			rx->rdy, rx->pos, rx->len, tkeep, *rx->axis.tkeep);

	if (rx->pos == rx->len) {
		*rx->axis.tlast = 1;
		rx->rdy = 0;
		rx->len = 0;
		rx->pos = 0;
		rx->gap = 3;
		pr_debug("-------------------------");
	} else {
		*rx->axis.tlast = 0;
	}
}

static inline void pkt_recv(struct sim *s, struct phy *phy)
{
	int count;

	if ((s->poll_fds[phy->n].revents & POLLIN) == POLLIN) {
		count = eth_recv(phy);
		if (count < 0) {
			perror("eth_recv");
			exit(1);
		}
		pr_debug("Receiving a packet: phy%d, count=%d", phy->n, count);

		phy->rx.len = count;
		phy->rx.rdy = 1;
	}
}


static inline int eth_xmit(struct phy *phy)
{
	return write(phy->fd, phy->tx.buf, phy->tx.pos);
}


static inline void txsim(struct tx *tx)
{
	uint8_t *p = tx->axis.tdata;
	int i;

	pr_info("txsim: tx->pos=%d, tkeep=%02X, tlast=%02X",
			tx->pos, *tx->axis.tkeep, *tx->axis.tlast);

	for (i = 0; i < 8; i++) {
		*(uint8_t *)&tx->buf[tx->pos++] = *(p+7-i);
	}
}

static inline void pkt_send(struct phy *phy)
{
	int count;

	if (*phy->tx.axis.tlast) {
		count = eth_xmit(phy);
		if (count < 0) {
			perror("eth_write");
			exit(1);
		}
		pr_debug("Sending a packet: fd=%d, count=%d", phy->fd, count);

		phy->tx.pos = 0;
	}
}

int main(int argc, char** argv)
{
	struct sim sim;
	struct phy *phy;
	int i, ret, do_sim; //, timeout = 2000;

	sim.ndev = N;  // todo

	sim.phy = (struct phy *)malloc(sizeof(struct phy) * sim.ndev);
	if (sim.phy == NULL) {
		perror("malloc");
		return -1;
	}


	phy_setup(&sim);
	axis_setup(&sim);
	poll_setup(&sim);

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

		poll(sim.poll_fds, sim.ndev, 0);

		for (i = 0; i < sim.ndev; i++) {
			phy = &sim.phy[i];

			// RX simulation
			if (phy->rx.gap > 0) {
				// insert a gap when receiving a packet
				if ((sim.main_time % SFP_CLK) == 0) {
					--phy->rx.gap;
				}
				do_sim = 1;
			} else {
				if (phy->rx.rdy) {
					rxsim(&phy->rx);
					do_sim = 1;
				} else {
					rxsim_idle(&phy->rx);
					pkt_recv(&sim, phy);
				}
			}

			pr_info("log: t=%u, n=%d, tx->pos=%d, tkeep=%02X, tlast=%02X",
				(unsigned int)sim.main_time, i, phy->tx.pos, *phy->tx.axis.tkeep, *phy->tx.axis.tlast);

			// TX simulation
			if (*phy->tx.axis.tvalid) {
				txsim(&phy->tx);
				do_sim = 1;

				pkt_send(phy);
			} else {
				phy->tx.pos = 0;
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

