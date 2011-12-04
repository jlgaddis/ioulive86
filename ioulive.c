#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "iou-raw.h"

// IOUlive86
// Hastily written to bridge IOU to a real network interface
// 
// This is the actual IOUlive core

#define IOULIVE_INTF_ID		0

void ioulive_main(char *IOULIVE_NAME, char *IOU_NAME, char *IOULIVE_INTF, int IOU_INTF_ID, int IOU_INST, int IOULIVE_INST)
{
    int ioulive_sock, iou_sock, raw_sock, raw_fd, raw_dev_id, rval, i;
    struct sockaddr_un router; 
    struct sockaddr_un server;
    char buf[1522];
    char outbuf[1514];
    pid_t task;

	unlink(IOULIVE_NAME);

beginning:

	printf("Press CTRL-C to exit IOUlive86...\n");
    // open "client" socket to router
    iou_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (iou_sock < 0) {
	perror("opening socket to router\n");
	exit(1);
    }
    router.sun_family = AF_UNIX;
    strcpy(router.sun_path, IOU_NAME);
    while (connect(iou_sock, (struct sockaddr *) &router, sizeof(struct sockaddr_un)) < 0);
    
	// open "server" socket (IOUlive instance)

    unlink(IOULIVE_NAME);
    ioulive_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (ioulive_sock < 0) {
        perror("opening raw socket");
        exit(1);
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, IOULIVE_NAME);
    if (bind(ioulive_sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un))) {
        perror("binding raw socket");
        exit(1);
    }
    printf("Connected to IOU instance: %u\n", IOU_INST);
    listen(ioulive_sock, 5);

    // open "raw" Ethernet socket to bridge IOU packets
    raw_dev_id = raw_get_dev_index(IOULIVE_INTF);

    if (raw_dev_id == -1) {
	perror("Invalid Ethernet device specified");
	close(ioulive_sock);
	close(iou_sock);
	exit(1);
    }

    raw_fd = raw_init_socket(raw_dev_id);

    if (raw_fd < 0) {
	perror("Invalid file handle for Ethernet device");
	close(ioulive_sock);
	close(iou_sock);
	exit(1);
    }

    // here's where we fork into two separate processes
    // one receives from IOU and sends to real LAN, the other
    // receives from real LAN and sends to IOU

    task = fork();

    if (task == 0) {

	while (1) {

		bzero(buf, sizeof(buf));
		bzero(outbuf, sizeof(outbuf));

		if ((rval = read(ioulive_sock, buf, 1522)) < 0)
			perror("reading frame from IOU instance");
		else if (rval == 0)
			perror("IOU instance ending connection");
		else {
			// copy packet from IOU without 8-leading bytes
			memcpy(outbuf, &buf[8], rval-8);

			// output to local eth interface
			raw_send(raw_fd, raw_dev_id, outbuf, 1514);
		}
	}

    } else {

	while (1) {

		bzero(buf, sizeof(buf));
		bzero(outbuf, sizeof(outbuf));

		if ((rval = raw_recv(raw_fd, outbuf, 1522)) < 0)
			perror("reading raw Ethernet frame");
		else if (rval == 0)
			perror("ending raw Ethernet connection");
		else {
			// calculate IOU header

			buf[0] = IOU_INST >> 8;
			buf[1] = IOU_INST & 255;
			buf[2] = IOULIVE_INST >> 8;
			buf[3] = IOULIVE_INST & 255;
			buf[4] = IOU_INTF_ID;
			buf[5] = IOULIVE_INTF_ID;
			buf[6] = 1;
			buf[7] = 0;

			memcpy(&buf[8], outbuf, rval);

			// send to IOU instance
			if (write(iou_sock, buf, 1522) <0){
				printf("IOU instance terminated: waiting for re-connection.\n");
				close(ioulive_sock);
				close(iou_sock);
				unlink(IOULIVE_NAME);
				goto beginning;
			}	
		}
	}
    }

    close(ioulive_sock);
    close(iou_sock);
    unlink(IOULIVE_NAME);
}

