#include <errno.h>
#include <memory.h> 
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "unpifiplus.h"

#define RET_FAILURE -1
#define RET_SUCCESS 0

#define BUFFER_SIZE 1024

#define DEBUG 1
#define TURNOFF 0

/* Structure for storing arguments of server */ 
typedef struct
{
	int port_number;
	int window_size;
}server_args;

/* Structure for storing arugments of client */ 
typedef struct
{
	char ipAddress[BUFFER_SIZE];
	int port_number;
	char file_name[BUFFER_SIZE];
	int window_size;
	int seed;
	float probability;
	int mean_time;
}client_args;

typedef struct client_info
{
	struct sockaddr_in client_ip_address;
	pid_t child_pid; 
	unsigned short client_port;
	struct client_info *next;
}client_info;

/* Structure for storing interface information */
typedef struct Node
{
	int sockfd;
	struct sockaddr_in *ip_address;
	struct sockaddr_in *network_mask;
	struct sockaddr_in subnet_address;
	struct client_info *client_info_head;
	struct Node *next;
}node;

node *for_purging_head;
client_info *heads_of_all_connections[BUFFER_SIZE];

int read_server_input_file();
int read_client_input_file();

