#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>


#define MAXNUMDEV    8

#define TAP_PATH     "/dev/net"
#define TAP_NAME     "tap"
#define TAP_MAJOR    10
#define TAP_MINOR    200

struct tap {
	char path[MAXPATHLEN];

	char dev[IFNAMSIZ];

	int mode;
	
	int ndev;

	uid_t uid;

	gid_t gid;
};


int tap_mkchardev(struct tap *tap)
{
	char devpath[IFNAMSIZ + MAXPATHLEN];
	dev_t devnum = 0;
	int ret;

	sprintf(devpath, "%s/%s", tap->path, tap->dev);

	devnum = makedev(TAP_MAJOR, TAP_MINOR);

	ret = mknod(devpath, S_IFCHR|0666, devnum);
	if (ret < 0) {
		perror("mknod");
		return -1;
	}

	ret = chown(devpath, tap->uid, tap->gid);
	if (ret < 0) {
		perror("chown");
		return -1;
	}

	return 0;
}

/*
 * tap_init
 */
int tap_init(struct tap *tap)
{
	char devpath[IFNAMSIZ + MAXPATHLEN];
	struct ifreq ifr;
	int fd, err;

	sprintf(devpath, "%s/%s", tap->path, tap->dev);

	//fd = open(devpath, O_RDWR);
	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (tap->dev) {
		strncpy(ifr.ifr_name, tap->dev, IFNAMSIZ);
	}

	err = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if (err < 0) {
		perror("TUNSETIFF");
		close(fd);
		return err;
	}

	err = ioctl(fd, TUNSETOWNER, tap->uid);
	if (err < 0) {
		perror("TUNSETOWNER");
		close(fd);
		return err;
	}

	err = ioctl(fd, TUNSETGROUP, tap->gid);
	if (err < 0) {
		perror("TUNESETGROUP");
		close(fd);
		return err;
	}

	/* enable persistent mode */
	if (ioctl(fd, TUNSETPERSIST, tap->mode) != 0) {
		perror("ioctl(TUNSETPERSIST)");
	}

	return fd;
}

int tap_config(struct tap *tap, int n)
{
	struct sockaddr_in *s_in;
	struct ifreq ifr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return -1;
	}

	strncpy(ifr.ifr_name, tap->dev, IFNAMSIZ);

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

int tap_release(struct tap *tap)
{
	char devpath[IFNAMSIZ + MAXPATHLEN];

	sprintf(devpath, "%s/%s", tap->path, tap->dev);
	unlink(devpath);

	return 0;
}

void usage(void)
{
	fprintf(stderr, "Usage:tapdev {add or del} {number_of_devices} {uid} {groupid}\n");
}


/*
 * main
 */
int main(int argc, char **argv)
{
	char mode_ascii[4];
	struct tap tap;
	int i, ret;

	if (argc != 5) {
		usage();
		return 1;
	}

	strncpy(mode_ascii, argv[1], 4);
	if ((ret = strcmp(mode_ascii, "add")) == 0) {
		tap.mode = 1;
	} else if ((strcmp(mode_ascii, "del")) == 0) {
		tap.mode = 0;
	} else {
		printf("unknown command: %s\n", mode_ascii);
		return 1;
	}

	tap.ndev = atoi(argv[2]);
	if (tap.ndev < 1 || tap.ndev > MAXNUMDEV) {
		usage();
		return 1;
	}

	tap.uid = atoi(argv[3]);
	if (tap.uid < 0) {
		usage();
		return 1;
	}

	tap.gid = atoi(argv[4]);
	if (tap.gid < 0) {
		usage();
		return 1;
	}

	strncpy(tap.path, TAP_PATH, sizeof(tap.path));

	/* tap device setup */
	for (i = 0; i < tap.ndev; i++) {
		sprintf(tap.dev, "%s%d", TAP_NAME, i);

		if (tap.mode == 1) {
			ret = tap_mkchardev(&tap);
			if (ret < 0) {
				perror("tap_mkchardev");
				return 1;
			}

			ret = tap_init(&tap);
			if (ret < 0) {
				perror("tap_init");
				return 1;
			}

			ret = tap_config(&tap, i);
			if (ret < 0) {
				perror("tap_config");
				return 1;
			}
		} else if (tap.mode == 0) {
			ret = tap_init(&tap);
			if (ret < 0) {
				perror("tap_init");
				return 1;
			}

			ret = tap_release(&tap);
			if (ret < 0) {
				perror("tap_release");
				return 1;
			}
		}
	}
	
	return 0;
}

