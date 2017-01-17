#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/route.h>

#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>


#define MAXNUMDEV    8

#define TAP_PATH     "/dev/net"
#define TAP_MAJOR    10
#define TAP_MINOR    200


/*
 * tap_init
 */
int tap_release(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	err = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if (err < 0) {
		perror("TUNSETIFF");
		close(fd);
		return err;
	}

	/* disable persistent mode */
	if (ioctl(fd, TUNSETPERSIST, 0) != 0) {
		perror("ioctl(TUNSETPERSIST)");
	}

	return fd;
}

void usage(void)
{
	fprintf(stderr, "Usage:tapdev {number_of_devices}\n");
}


/*
 * main
 */
int main(int argc, char **argv)
{
	int i, ndev, fd[MAXNUMDEV];
	char devpath[IFNAMSIZ + 9];
	char dev[IFNAMSIZ];

	if (argc != 2) {
		usage();
		return 1;
	}

	ndev = atoi(argv[1]);
	if (ndev < 1 || ndev > MAXNUMDEV) {
		usage();
		return 1;
	}

	/* tap device release */
	for (i = 0; i < ndev; i++) {
		sprintf(dev, "phy%d", i);

		fd[i] = tap_release(dev);
		if (fd[i] < 0) {
			perror("tap_release");
			return 1;
		}
	}
	
	/* char device release */
	for (i = 0; i < ndev; i++) {
		sprintf(devpath, "%s/phy%d", TAP_PATH, i);
		unlink(devpath);
	}

	for (i = 0; i < ndev; i++) {
		close(fd[i]);
	}

	return 0;
}

