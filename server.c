#include "udp_utils.h"
#include "myrtt.h"

void sig_chld(int signo)
{
    pid_t pid;
    int status;
    printf("SIGNAL: SIGCHILD handler in parent client.\n");
    while((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    	purge_client_connection(for_purging_head, pid);
        printf("SIGNAL: Child %d Terminated\n", pid);
    }
    return;
}

int read_server_input_file(server_args* s_args)
{
	char buffer[BUFFER_SIZE];
	FILE *fp;
	int line_number;

	line_number = 0;
	memset(buffer, 0, sizeof(buffer));

	fp = fopen("server.in","r");
	if(fp == NULL) 
	{
		perror("ERROR!! server.in  ");
		return RET_FAILURE;
	}

	while ((fgets(buffer, BUFFER_SIZE, fp) != NULL) && (line_number <= 1))
	{
		if (0 == line_number)
			s_args->port_number = atoi(buffer);
		if (1 == line_number)
			s_args->window_size = atoi(buffer);

		line_number++;
		memset(buffer, 0, sizeof(buffer));
	}

	fclose(fp);

	return RET_SUCCESS;
}

int build_interface_list(struct ifi_info *ifi, struct ifi_info *ifihead, int port_number, 
							node *head, int *max_sockfd, int *total_interfaces, fd_set *read_fds)
{

	int i, sock_fd;
	u_char *ptr;
	struct sockaddr_in *serv_addr;
	const int optval = 1;	

	*total_interfaces = 0;

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) // HARD-CODED AF_INET, 1. SEE LATER
	{
		if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			perror("ERROR!! On socket()  ");
			return RET_FAILURE;
		}

		if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
		{
			perror("ERROR!! On setsockopt()  ");
			return RET_FAILURE;
		}

		serv_addr = (struct sockaddr_in *) ifi->ifi_addr;
		serv_addr->sin_family = AF_INET;
		serv_addr->sin_port = htons(port_number);

		if((bind(sock_fd, (struct sockaddr *)serv_addr, sizeof(*serv_addr))) < 0)
		{
			perror("ERROR!! On bind()  ");
			return RET_FAILURE;
		}

		if (sock_fd > *max_sockfd)
			*max_sockfd = sock_fd + 1;

		insert(head, sock_fd, (struct sockaddr_in *)ifi->ifi_addr, (struct sockaddr_in *)ifi->ifi_ntmaddr);

		*total_interfaces = *total_interfaces + 1;
		FD_SET(sock_fd, read_fds);

	}
}

int main(int argc, char** argv)
{
	server_args s_arguments;
	struct ifi_info *ifi, *ifihead;
	node *head, *temp, *another_temp;
	int max_sockfd, total_interfaces, nready, bytes_received, connection_sock_fd;
	fd_set read_fds, temp_set;
	char data_buffer[BUFFER_SIZE], ip_buffer[BUFFER_SIZE];
	struct sockaddr_in from_addr;
	struct sockaddr_in *conn_addr;
	int addr_len, already_exists;
	pid_t pid;
	struct sockaddr_in child_server;
   	int child_server_len, flag_same, interface_counter;


	// Initialization of some values (Prankur)
	char fileName[BUFFER_SIZE];
	char buffer_recv[BUFFER_SIZE];
	char ACKBuffer[BUFFER_SIZE];
	memset(ACKBuffer, 0, BUFFER_SIZE);
	memset(buffer_recv, 0, BUFFER_SIZE);
	memset(fileName, 0, BUFFER_SIZE);
	int clientWindowSize;
	
	fd_set rset;
	int maxfdp1;
	FD_ZERO(&rset);

	struct sockaddr_in tempCli;
	memset(&tempCli, 0, sizeof(tempCli));

	int retransmitCounter=0;

	//End

	/* Initialization and memset stuff below. */
	max_sockfd = 0;
	addr_len = 0;
	flag_same = -1;
	interface_counter = 0;
	already_exists = 0;
	memset(&s_arguments, 0, sizeof(s_arguments));
	head = (node *)malloc(sizeof(node)); 
	memset(head, 0, sizeof(node));
	memset(data_buffer, 0, sizeof(data_buffer));
	memset(ip_buffer, 0, sizeof(ip_buffer));
	memset(&from_addr, 0, sizeof(from_addr));
	memset(&child_server, 0, sizeof(child_server));
	memset(&heads_of_all_connections, 0, sizeof(heads_of_all_connections));
	FD_ZERO(&read_fds);
	FD_ZERO(&temp_set);
	temp = head;
	for_purging_head = head;

	if(RET_FAILURE == read_server_input_file(&s_arguments))
	{
		printf("ERROR: Failed reading server.in. Terminating.\n");
		exit(RET_FAILURE);
	}

#if TURNOFF
	printf("Port number: %d\n", s_arguments.port_number);
	printf("Window size: %d\n", s_arguments.window_size);
#endif

	if(RET_FAILURE == build_interface_list(ifi, ifihead, s_arguments.port_number, head, &max_sockfd, &total_interfaces, &read_fds))
	{
		printf("ERROR: Failed binding sockets to interfaces. Terminating.\n");
		exit(RET_FAILURE);
	}


	printf("**********INTERFACE LIST**********\n");
	printf("Total Interfaces = %d\n", total_interfaces);
	printf("----------------------------------\n");	
	print(head->next);

	while(1)
	{
		temp_set = read_fds;
		nready = select(max_sockfd + 1, &temp_set, NULL, NULL, NULL);
		if(nready < 0 && errno == EINTR) 
			continue;

		temp = head;
		interface_counter = 0;
		while(temp->next != NULL)
		{
			if (FD_ISSET(temp->sockfd, &temp_set)) 
			{
				addr_len = sizeof(from_addr);
				bytes_received = recvfrom(temp->sockfd, data_buffer, sizeof(data_buffer), 0, (struct sockaddr *)&from_addr, &addr_len);
				//bytes_received = read(temp->sockfd, data_buffer, sizeof(data_buffer));
				already_exists = find_client(interface_counter, from_addr, from_addr.sin_port);
#if DEBUG
				printf("Find returned : %d\n", already_exists); 
#endif
				
				/* ******************************************************************** */
				/* Printing client ip address, port whether it is local/loopback/remote */
				/* ******************************************************************** */
				printf("Client from IP Address : %s and Port : %d\n",inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));

				/* Check if the client is local/remote/loopback */
				if((from_addr.sin_addr.s_addr & temp->network_mask->sin_addr.s_addr) && 
					(temp->ip_address->sin_addr.s_addr & temp->network_mask->sin_addr.s_addr))
				{

					if(from_addr.sin_addr.s_addr == temp->ip_address->sin_addr.s_addr)
					{
						printf("Client is loopback address.\n");
						flag_same = 1;
					}
					else
						printf("Client is on local network.\n");

				}
				else
					printf("Client is on remote network.\n");
				/* ******************************************************************** */

				if(0 == already_exists)
				{
					pid = fork();

					if (-1 == pid)
					{
						perror("ERROR! On fork()  ");
						return RET_FAILURE;
					}
					if (0 == pid)
					{
						/* Child process*/
						
						/* Close all other inherited socket descriptors except the current one */
						another_temp = head;
						while(another_temp->next != NULL)
						{
							if(temp->sockfd != another_temp->sockfd)
								close(another_temp->sockfd);
							another_temp = another_temp->next;
						}

						printf("Recieved file-name : %s\n", data_buffer);
						//Prankur
						sprintf(fileName, "%s", data_buffer);
						//end
						if((connection_sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
						{
							perror("ERROR!! On socket()  ");
							return RET_FAILURE;
						}

						if(flag_same == 1)
						{
							int optval =1;
							setsockopt(connection_sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_DONTROUTE, &optval, sizeof optval);
						}

						conn_addr = (struct sockaddr_in *) temp->ip_address;
						conn_addr->sin_family = AF_INET;
						conn_addr->sin_port = htons(0);

						if((bind(connection_sock_fd, (struct sockaddr *)conn_addr, sizeof(*conn_addr))) < 0)
						{
							perror("ERROR!! On bind()  ");
							return RET_FAILURE;
						}

						child_server_len = sizeof(child_server);
						if (getsockname(connection_sock_fd, (struct sockaddr *)&child_server, &child_server_len) == -1) 
						{
							perror("ERROR! On getsockname()  ");
							return RET_FAILURE;
						}

						printf("Connection socket created successfully %s:%u \n", inet_ntoa(child_server.sin_addr), ntohs(child_server.sin_port));

		//Now will send the ephermal port number to client
		struct timeval time_count;
		struct sockaddr_in newCliSock;
		socklen_t newCliSocklen;
		bzero(&newCliSock, sizeof(newCliSock));
		newCliSocklen = sizeof(newCliSock);
		
		tempCli.sin_family = AF_INET;
		tempCli.sin_addr = from_addr.sin_addr;
		tempCli.sin_port = child_server.sin_port;
		retransmitCounter=0;

send_port:
		retransmitCounter++;
		if(retransmitCounter > 3)
		{
			printf("Client never sent an ACK for my Port number message\n");
			printf("Now will terminate the child\n");
			exit(1);
		}
		time_count.tv_sec = 3;
		time_count.tv_usec = 0;

		memset(data_buffer, 0, sizeof(data_buffer));
		memset(buffer_recv,0, sizeof(buffer_recv));
		sprintf(data_buffer,"%d",ntohs(child_server.sin_port));
		sprintf(ACKBuffer, "%s", "ACK");
#if 1		
		if(sendto(temp->sockfd, data_buffer, strlen(data_buffer), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)) <0)
		{
			perror("SendTo failed:");
			exit(1);
		}
#endif
		if(sendto(connection_sock_fd, ACKBuffer, strlen(ACKBuffer), 0, (struct sockaddr *)&from_addr, sizeof(struct sockaddr)) <0)
		{
			perror("SendTo failed:");
			exit(1);
		}
		
		FD_SET(connection_sock_fd, &rset);
		maxfdp1=connection_sock_fd+1;
		if(select(maxfdp1, &rset, NULL, NULL, &time_count) < 0)
		{
			if(errno == EINTR)
				goto send_port;
			else
			{
				perror("Select Error :");
				exit(1);
			}
		}
		if(FD_ISSET(connection_sock_fd, &rset))
		{
			//printf("Hi I'm here");
			recvfrom(connection_sock_fd, buffer_recv, sizeof(buffer_recv), 0, (struct sockaddr *)&newCliSock, &newCliSocklen);
			//goto send_ACK;
		
		}
		else
		{
			goto send_port;
		}	
		
	
		
		
	if(sendto(connection_sock_fd, buffer_recv, strlen(buffer_recv), 0, (const struct sockaddr *)&newCliSock, sizeof(newCliSock)) < 0)
	{
		perror("Send error :");
		exit(1);	
	}	

	if(connect(connection_sock_fd,(struct sockaddr*)&newCliSock,sizeof(struct sockaddr))< 0)
	{
		perror("Server Connection Failed:");
		exit(1);
	}
	
	close(temp->sockfd);		

 /*       	
	FD_ZERO(&rset);
	retransmitCounter =0;
	
send_ACK:	
	retransmitCounter++;
	if(retransmitCounter > 3)
	{
		printf("Client never sent an ACK for my Port number message\n");
		printf("Now will terminate the child\n");
		exit(1);
	}

	time_count.tv_sec = 3;
	time_count.tv_usec = 0;
					
	
	if(sendto(connection_sock_fd, ACKBuffer, strlen(ACKBuffer), 0, (const struct sockaddr *)&newCliSock, sizeof(newCliSock))<0)
	{
		perror("Sendto falied:");
		exit(1);
	}

	FD_SET(connection_sock_fd, &rset);
	maxfdp1=connection_sock_fd+1;

	if(select(maxfdp1, &rset, NULL, NULL, &time_count) < 0)
	{
		if(errno == EINTR)
			goto send_ACK;
		else
		{
			perror("Select Error :");
			exit(1);
		}
	}
	if(FD_ISSET(connection_sock_fd, &rset))
	{
		//printf("Hi I'm here");
		
	}
	else
	{
		goto send_ACK;
	}

*/
			printf("New Client address %s:%u\n",inet_ntoa(newCliSock.sin_addr), ntohs(newCliSock.sin_port));
			

                    	
                    	printf("The initial window size received from client: %s\n", buffer_recv);
			clientWindowSize = atoi(buffer_recv);
			printf("File name = %s and window size = %d\n\n", fileName, clientWindowSize);
//			sendFile(connection_sock_fd, fileName, clientWindowSize,s_arguments.window_size) ;
			sendFile(connection_sock_fd, fileName, (struct sockaddr*)&newCliSock, newCliSocklen, clientWindowSize, s_arguments.window_size);
                    	exit(1);
					}
					else 
					{ /* Parent process*/
						signal(SIGCHLD, sig_chld);
						insert_client_connections(temp->client_info_head, from_addr, pid, from_addr.sin_port);
						heads_of_all_connections[interface_counter] = (client_info *)malloc(sizeof(client_info)); 
						memset(heads_of_all_connections[interface_counter], 0, sizeof(client_info));
						memcpy(heads_of_all_connections[interface_counter], temp->client_info_head, sizeof(client_info));
						//print_client_connections(interface_counter);
					}
				}
			}
			temp = temp->next;
			interface_counter++;
		}
	}
	
	/* Close sockets and free ifi's*/
	cleanup(head);
	free_ifi_info_plus(ifihead);	

	exit(RET_SUCCESS);
}
