#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include "iou-raw.h"

// Front-end for IOUlive
// Displays command line usage and parses command line
// (likely very inefficiently, too!)

void print_usage(void);

int parse_netmap(char *s, int *iouinst, int *iouintf, int *iouliveinst);
void create_temppath(char *p);
extern void ioulive_main(char *IOULIVE_NAME, char *IOU_NAME, char *intf, int IOUINTF, int IOUINST, int IOULIVEINST);


int main(int argc, char **argv)
{
	char intf[20];
	char netmap_path[255];
	unsigned int instance;
	int i, j, raw_dev_id;
	FILE *infp;
	char linein[255];
	unsigned char ch;
	int result;
	int IOUINST, IOUINTF, IOULIVEINST;
	char tempPath[40], IOULIVE_NAME[40], IOU_NAME[40];

	printf("\nIOUlive86\n\n");

	if (argc < 4 || argc > 6) {
		print_usage();
		exit(0);
	}

	intf[0] = '\0';
	strcpy(netmap_path, "NETMAP");
	instance = 0;

	for (i=1; i<argc; i++){
		if (!strcmp(argv[i], "-i")) {
			if(i < argc - 1) {
				strcpy(intf, argv[i+1]);
				i++;
				continue;
			} else {
				print_usage();
				exit(0);
			}
		} else if (!strcmp(argv[i], "-n")) {
			if(i < argc - 1) {
				strcpy(netmap_path, argv[i+1]);
				i++;
				continue;
			} else { 
				print_usage();
				exit(0);
			}
		} else if (i == argc - 1 && atoi(argv[i])){
			instance = atoi(argv[i]);
			break;
		} else {
			print_usage();
			exit(0);
		}
	}

	if (instance == 0) {
		print_usage();
	}

	raw_dev_id = raw_get_dev_index(intf);

	if (raw_dev_id == -1) {
		printf("Error: Invalid Ethernet device specified\n");
		exit(0);
	}

	// printf("Interface = %s\n", intf);
	// printf("Netmap path = %s\n", netmap_path);
	// printf("Instance = %d\n\n", instance);

	// open NETMAP file and verify details

	infp = fopen(netmap_path, "rt");

	if (!infp) {
		printf("Cannot open %s\n\n", netmap_path);
		exit(0);
	}

	// Read each line in and verify which instance talks to us

	j = 1;

	do {

		linein[0] = '\0';
		i = 0;

		while (i < 255){
			ch = fgetc(infp);
			if (feof(infp)) break;
			if (ch != '\n') linein[i++] = ch;
			else {
				j++;
				break;
			}
		}
		linein[i] = '\0';

		if (i == 255){
			printf("Error: NETMAP file contains lines > 255 characters\n\n");
			fclose(infp);
			exit(0);
		}

		// Parse line of NETMAP file

		// result contains instance we are attached to
		// or '0' if not present on this line

		if (strlen(linein)) {
			result = parse_netmap(linein, &IOUINST, &IOUINTF, &IOULIVEINST);
			if (result) {
				fclose(infp);
				printf("ERROR: Line %d of NETMAP is invalid!\n");
				exit(0);
			}
			// printf("IOU Instance = %03d, IOU Interface = %03d, IOULIVE Instance = %03d\n", IOUINST, IOUINTF, IOULIVEINST);
			if (IOULIVEINST == instance) break;
		}
	} while (!feof(infp));

	fclose(infp);

	if (IOULIVEINST != instance) {
		printf("Error: Check NETMAP file - no mapped IOUlive86 instance found\n\n");
		exit(0);
	}

	// printf("Found!  We are instance %03d, mapping to IOU instance %03d on interface %03d\n", IOULIVEINST, IOUINST, IOUINTF);

	// Create path to /tmp/netioX

	create_temppath(tempPath);
	sprintf(IOU_NAME, "%s%d", tempPath, IOUINST);
	sprintf(IOULIVE_NAME, "%s%d", tempPath, IOULIVEINST);

	// printf("Path to IOU instance: %s\n", IOU_NAME);
	// printf("Path to IOULIVE instance: %s\n", IOULIVE_NAME);

	// Call IOULIVE main program

	// printf("Calling IOULIVE main loop...\n");

	ioulive_main(IOULIVE_NAME, IOU_NAME, intf, IOUINTF, IOUINST, IOULIVEINST);
}

void print_usage(void)
{
	printf("Usage: ioulive86 -i <interface> [-n NETMAP] <instance ID>\n");
}

int parse_netmap(char *s, int *iouinst, int *iouintf, int *iouliveinst)
{
	// format of NETMAP file for the purposes of parsing
	// <router instance>  <IOUlive86 instance>
	// where:
	// <router instance> = INSTANCE:IntfMajor/IntfMinor
	// <IOUlive86 instance) = IOU_INSTANCE
	//
	// first, parse for two fields (router instance and IOUlive instance)
	// then, parse for INSTANCE and INTERFACE (using : as token delimeter)
	// then, parse for INTERFACE (Int major and Int minor with / as delimeter)

	char Left[100], Right[100];
	char LeftInstance[20], RightInstance[20];
	char LeftIntf[20], IntMajor[20], IntMinor[20];
	char temps[30];
	int interface;
	char *t;
	int i;

	*iouinst = 0;
	*iouintf = 0;
	*iouliveinst = 0;

	t = strtok(s, " ");
	if (t != NULL) strcpy(Left, t);
	else return 1;

	t = strtok(NULL, " ");
	if (t != NULL) strcpy(Right, t);
	else return 1;

	// parse left and right strings
	t = strtok(Left, ":");
	if (t != NULL) strcpy(LeftInstance, t);
	else return 1;

	t = strtok(NULL, " ");
	if (t != NULL) strcpy(LeftIntf, t);
	else return 1;

	strcpy(temps, LeftIntf);
	t = strtok(temps, "@");
	if (t != NULL) strcpy(LeftIntf, t);

	t = strtok(LeftIntf, "/");
	if (t != NULL) strcpy(IntMajor, t);
	else return 1;

	t = strtok(NULL, "/");
	if (t != NULL) strcpy(IntMinor, t);
	else return 1;

	interface = atoi(IntMajor) + ( atoi(IntMinor) * 16);

	// Parse right side of NETMAP line
	// Strip off everything AFTER the numeric instance ID

	i = 0;
	while ( Right[i] != '\0' ){
		if ( isdigit(Right[i])) i++;
		else {
			Right[i] = '\0';
			break;
		}
	}

	*iouinst = atoi(LeftInstance);
	*iouintf = interface;
	*iouliveinst = atoi(Right);
	return 0;
}

void create_temppath(char *p)
{
	unsigned int user_id;
	char tmp[20];

	user_id = getuid();
	strcpy(p, "/tmp/netio");
	sprintf(tmp, "%u", user_id);
	strcat(p, tmp);
	strcat(p, "/");
}

