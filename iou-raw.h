#include <sys/types.h>

// Get interface index of Ethernet device
int raw_get_dev_index(char *name);

// Initialize an Ethernet raw socket
int raw_init_socket(int dev_id);

// Send an Ethernet frame to the Ethernet device
ssize_t raw_send(int socket, int dev_id, char *buffer, size_t len);

// Receive an Ethernet frame from device
ssize_t raw_recv(int socket, char *buffer, size_t len);


