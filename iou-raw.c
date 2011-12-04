#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <linux/if.h>
#include <linux/if_packet.h>

#include "iou-raw.h"

// This file holds the functions that deal with low-level packet I/O

// Get interface index of Ethernet device
int raw_get_dev_index(char *name)
{
	struct ifreq if_req;
	int fd;

	// Dummy fd
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "raw_get_dev_index: socket: %s\n", strerror(errno));
		return (-1);
	}

	memset((void *) &if_req, 0, sizeof(if_req));
	strcpy(if_req.ifr_name, name);

	if (ioctl(fd, SIOCGIFINDEX, &if_req) < 0) {
		fprintf(stderr, "raw_get_dev_index: SIOCGIFINDEX: %s\n", strerror(errno));
		close(fd);
		return (-1);
	}

	close(fd);
	return (if_req.ifr_ifindex);
}

// Initialize an Ethernet raw socket
int raw_init_socket(int dev_id)
{
	struct sockaddr_ll sa;
	struct packet_mreq mreq;
	int sock;

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
		fprintf(stderr, "raw_init_socket: socket: %s\n", strerror(errno));
		return (-1);
	}

	memset(&sa, 0, sizeof(struct sockaddr_ll));
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_ALL);
	sa.sll_hatype = ARPHRD_ETHER;
	sa.sll_halen = ETH_ALEN;
	sa.sll_ifindex = dev_id;

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = sa.sll_ifindex;
	mreq.mr_type = PACKET_MR_PROMISC;

	if (bind(sock, (struct sockaddr *) &sa, sizeof(struct sockaddr_ll)) == -1) {
		fprintf(stderr, "raw_init_socket: bind: %s\n", strerror(errno));
		return (-1);
	}

	if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
		fprintf(stderr, "raw_init_socket: setsockopt: %s\n", strerror(errno));
		return (-1);
	}

	return (sock);
}

// Send an Ethernet frame to the Ethernet device
ssize_t raw_send(int sock, int dev_id, char *buffer, size_t len)
{
	struct sockaddr_ll sa;

	memset(&sa, 0, sizeof(struct sockaddr_ll));
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_ALL);
	sa.sll_hatype = ARPHRD_ETHER;
	sa.sll_halen = ETH_ALEN;
	sa.sll_ifindex = dev_id;

	return( sendto(sock, buffer, len, 0, (struct sockaddr *) &sa, sizeof(sa)));
}

// Receive an Ethernet frame from device
ssize_t raw_recv(int sock, char *buffer, size_t len)
{
	return( recv(sock, buffer, len, 0));
}



