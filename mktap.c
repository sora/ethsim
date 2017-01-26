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
#define TAP_NAME     "phy"
#define TAP_MAJOR    10
#define TAP_MINOR    200


int tap_mkchardev(char *dev)
{
	char devpath[IFNAMSIZ + 9];
	dev_t devnum = 0;
	int ret;

	sprintf(devpath, "%s/%s", TAP_PATH, dev);

	devnum = makedev(TAP_MAJOR, TAP_MINOR);

	ret = mknod(devpath, S_IFCHR|0666, devnum);
	if (ret < 0) {
		perror("mknod");
		return -1;
	}

	return 0;
}

/*
 * tap_init
 */
int tap_init(char *dev, int mode)
{
	char devpath[IFNAMSIZ + 9];
	struct ifreq ifr;
	int fd, err;

	sprintf(devpath, "%s/%s", TAP_PATH, dev);

	fd = open(devpath, O_RDWR);
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

	/* enable persistent mode */
	if (ioctl(fd, TUNSETPERSIST, mode) != 0) {
		perror("ioctl(TUNSETPERSIST)");
	}

	return fd;
}

int tap_config(char *dev, int n)
{
	struct sockaddr_in *s_in;
	struct ifreq ifr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return -1;
	}

	strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl(SIOCGIFFLAGS)");
		return -1;
	}

	/* ifup */
	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl(SIOCSIFFLAGS)");
		return -1;
	}

	s_in = (struct sockaddr_in *)&ifr.ifr_addr;

	/* set IP address */
	s_in->sin_family = AF_INET;
	s_in->sin_addr.s_addr = htonl(10 << 24 | 10 << 16 | 10 << 8 | (n + 1));
	s_in->sin_port = 0;
	if (ioctl(fd, SIOCSIFADDR, &ifr) != 0) {
		perror("ioctl(SIOCSIFADDR)");
		return -1;
	}

	/* set subnet mask */
	s_in->sin_addr.s_addr = inet_addr("255.255.255.0");
	if (ioctl(fd, SIOCSIFNETMASK, &ifr) != 0) {
		perror("ioctl(SIOCSIFNETMASK)");
		return -1;
	}

	close(fd);

	return 0;
}

int tap_release(char *dev)
{
	char devpath[IFNAMSIZ + 9];

	sprintf(devpath, "%s/phy%d", TAP_PATH, i);
	unlink(devpath);

	return 0;
}

void usage(void)
{
	fprintf(stderr, "Usage:tapdev {add or del} {number_of_devices}\n");
}


/*
 * main
 */
int main(int argc, char **argv)
{
	char dev[IFNAMSIZ];
	int mode, i, ndev, ret;
	char mode_ascii[4];

	if (argc != 3) {
		usage();
		return 1;
	}

	strncpy(mode, argv[1], 4);
	if ((ret = strcmp(mode_ascii, "add")) == 0) {
		mode = 1;
	} else if ((strcmp(mode_ascii, "del")) == 0) {
		mode = 0;
	} else {
		printf("unknown command: %s\n", mode_ascii);
		return 1;
	}

	ndev = atoi(argv[2]);
	if (ndev < 1 || ndev > MAXNUMDEV) {
		usage();
		return 1;
	}

	/* tap device setup */
	for (i = 0; i < ndev; i++) {
		sprintf(dev, "%s%d", TAP_NAME, i);

		if (mode == make) {
			ret = tap_mkchardev(dev);
			if (ret < 0) {
				perror("tap_mkchardev");
				return 1;
			}

			ret = tap_init(dev, mode);
			if (ret < 0) {
				perror("tap_init");
				return 1;
			}

			ret = tap_config(dev, i);
			if (ret < 0) {
				perror("tap_config");
				return 1;
			}
		} else if (mode == release) {
			ret = tap_init(dev, mode);
			if (ret < 0) {
				perror("tap_init");
				return 1;
			}

			ret = tap_release(dev);
			if (ret < 0) {
				perror("tap_release");
				return 1;
			}
		}
	}
	
	return 0;
}

