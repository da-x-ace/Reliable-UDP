#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include "unpifiplus.h"
#include "udp_utils.h"

#define BUFSIZE 1024
#define LISTENQ 1024


void recieveFile(int sockfd, char* fileName, int windowSize, int seed, float probability, int meanTime);

int read_client_input_file(client_args* c_args)
{
	char buffer[BUFFER_SIZE];
	FILE *fp;
	int line_number;

	line_number = 0;
	memset(buffer, 0, sizeof(buffer));

	fp = fopen("client.in","r");
	if(fp == NULL) 
	{
		perror("ERROR!! client.in  ");
		return RET_FAILURE;
	}

	while ((fgets(buffer, BUFFER_SIZE, fp) != NULL) && (line_number <= 6))
	{
		if (0 == line_number)
			memcpy(c_args->ipAddress, buffer, strlen(buffer) - 1);
		if (1 == line_number)
			c_args->port_number = atoi(buffer);
		if (2 == line_number)
			memcpy(c_args->file_name, buffer, strlen(buffer) - 1);
		if (3 == line_number)
			c_args->window_size = atoi(buffer);
		if (4 == line_number)
			c_args->seed = atoi(buffer);					
		if (5 == line_number)
		{
			c_args->probability = atof(buffer);
			if(c_args->probability > 1.0 || c_args->probability < 0.0)
			{
				printf("ERROR!! Probability not in range [0.0, 1.0]. Terminating.\n");
				fclose(fp);
				return RET_FAILURE;
			}
		}
		if (6 == line_number)
			c_args->mean_time = atoi(buffer);

		line_number++;
		memset(buffer, 0, sizeof(buffer));
	}

	fclose(fp);

	return RET_SUCCESS;
}



char * my_sock_ntop(const struct sockaddr *sa, socklen_t salen)
{
    char portstr[8];
    static char str[128];

	switch (sa->sa_family) 
	{
		case AF_INET: 
		{
			struct sockaddr_in	*sin = (struct sockaddr_in *) sa;

			if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
				return(NULL);
			if (ntohs(sin->sin_port) != 0) 
			{
				snprintf(portstr, sizeof(portstr), ":%d", ntohs(sin->sin_port));
				strcat(str, portstr);
			}
			return(str);
		}
	}
}


void fileClient(int sockfd,struct sockaddr_in* servaddr, socklen_t servlen, client_args* c_arguments)
{
		//printf("Filename : %s\n", c_arguments->file_name);
	struct sockaddr_in newServAddr; //The address got when the child sends the port number
	struct sockaddr_in changeServAddr; //New server address to which I'll do my calling
	socklen_t newLen, socklen;

	char buffer[BUFSIZE];
	char recvBuffer[BUFSIZE];
	char str[BUFSIZE];
	int serverPort;
	memset(str,0,BUFSIZE);
	memset(recvBuffer, 0, BUFSIZE);
	memset(buffer, 0, BUFSIZE);
	bzero(&newServAddr, sizeof(newServAddr));
	bzero(&changeServAddr, sizeof(changeServAddr));

	fd_set rset;
	int maxfdp1;
	FD_ZERO(&rset);
	
	int retransmitCounter =0;

	inet_ntop(AF_INET,&servaddr->sin_addr, str, sizeof(str));
	//printf("Server Address : %s\n", str);
	//printf("Server Port : %u\n", servaddr->sin_port);

	struct timeval time_count;

send_filename:	

	retransmitCounter++;
	if(retransmitCounter > 3)
	{
		printf("Server never sent a Port number message\n");
		printf("Now will terminate the client\n");
		exit(1);
	}


	time_count.tv_sec = 3;
	time_count.tv_usec = 0;

	//printf("Length of filename = %d\n", strlen(c_arguments->file_name));
	sprintf(buffer,"%s",c_arguments->file_name);
	//printf("Filename : %s\n", buffer);	
	//if(sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*) servaddr, sizeof(struct sockaddr)) < 0)
	if(write(sockfd, buffer, strlen(buffer)) < 0)
	{
		perror("SendTo failed:");
		exit(1);
	}
	

	FD_SET(sockfd, &rset);
	maxfdp1=sockfd+1;

	if(select(maxfdp1, &rset, NULL, NULL, &time_count) < 0)
	{
		if(errno == EINTR)
			goto send_filename;
		else
		{
			perror("Select Error :");
			exit(1);
		}
	}
	if(FD_ISSET(sockfd, &rset))
	{
		//printf("Hi I'm here");
		newLen = sizeof(newServAddr);
		if(recvfrom(sockfd, recvBuffer, BUFSIZE, 0, (struct sockaddr*)&newServAddr, &newLen) < 0)
		{
			perror("RecvFrom error :");
			exit(1);
		}
		printf("Port received from server : %d\n", atoi(recvBuffer));
		
	}
	else
	{
		goto send_filename;
	}
	
	changeServAddr.sin_family = AF_INET;
	changeServAddr.sin_addr = newServAddr.sin_addr;
	changeServAddr.sin_port = htons(atoi(recvBuffer));

	//Step - Forgotten
	//Connect to new socket with the received port number
	if(connect(sockfd,(struct sockaddr*)&changeServAddr,sizeof(struct sockaddr))< 0)
	{
		perror("Server Connection Failed:");
		exit(1);
	}
	struct sockaddr_in newServSock;
	socklen_t newSocklen;
	bzero(&newServSock, sizeof(newServSock));
	newSocklen = sizeof(newServSock);
	
	if(getpeername(sockfd, (struct sockaddr*)&newServSock, &newSocklen) < 0)
	{
		perror("Getpeername failed :");
		exit(1);
	}
	
	printf("New Server address %s:%u\n",inet_ntoa(newServSock.sin_addr), ntohs(newServSock.sin_port));


	//Step - To send the ACK for the port number received (Send the message window)

	FD_ZERO(&rset);
	bzero(&newServAddr, sizeof(newServAddr));
	retransmitCounter=0;

send_ACK_for_port:
	
	retransmitCounter++;
	if(retransmitCounter > 3)
	{
		printf("Server never sent a ACK for my message window\n");
		printf("Now will terminate the client\n");
		exit(1);
	}

	time_count.tv_sec = 3;
	time_count.tv_usec = 0;
	memset(buffer, 0, BUFSIZE);
	sprintf(buffer,"%d",c_arguments->window_size);
	//if(sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&changeServAddr, sizeof(struct sockaddr)) < 0)
	if(write(sockfd, buffer, strlen(buffer)) < 0)
	{
		perror("SendTo failed:");
		exit(1);
	}

	

	FD_SET(sockfd, &rset);
	maxfdp1=sockfd+1;

	if(select(maxfdp1, &rset, NULL, NULL, &time_count) < 0)
	{
		if(errno == EINTR)
			goto send_ACK_for_port;
		else
		{
			perror("Select Error :");
			exit(1);
		}
	}
	if(FD_ISSET(sockfd, &rset))
	{
		//printf("Hi I'm here");
		newLen = sizeof(newServAddr);
		memset(recvBuffer, 0, BUFSIZE);
		if(recvfrom(sockfd, recvBuffer, BUFSIZE, 0, (struct sockaddr*)&newServAddr, &newLen) < 0)
		{
			perror("RecvFrom error :");
			if(errno == 111)
			{
				printf("Server never sent an ACK for my window buffer message\n");
				printf("Now terminating the client\n");
			}			
			exit(1);
		}
		//printf("	received this : %s\n", recvBuffer);
		if(strcmp(recvBuffer, "ACK") == 0)
		{	
			//printf("Yes\n");
			goto send_ACK_for_port;
		}
		goto send_File;
		
	}
	else
	{
		goto send_ACK_for_port;
	}

send_File:
	recieveFile(sockfd, c_arguments->file_name, c_arguments->window_size, c_arguments->seed, c_arguments->probability, c_arguments->mean_time);
	return;
}

int main(int argc, char **argv)
{
	//int sockfd;
	int flag_local =0;
	int flag_same =0;
	struct ifi_info *ifi;
	struct ifi_info *ifiClient;
	struct sockaddr_in *sin, *mask, *locAddr, *tempAddr;
	struct sockaddr_in servaddr, cliaddr;
	char serverStr[INET_ADDRSTRLEN], clientStr[INET_ADDRSTRLEN];	

	struct sockaddr *sa;


	bzero(&servaddr, sizeof(servaddr));
	bzero(&cliaddr, sizeof(cliaddr));
	memset(serverStr, 0, INET_ADDRSTRLEN);
	memset(clientStr, 0, INET_ADDRSTRLEN);
	
	
	client_args c_arguments;

	memset(&c_arguments, 0, sizeof(c_arguments));

	if(RET_FAILURE == read_client_input_file(&c_arguments))
	{
		exit(RET_FAILURE);
	}

#ifdef DEBUG
	printf("IP address: %s\n", c_arguments.ipAddress);		
	printf("Port number: %d\n", c_arguments.port_number);
	printf("File to send: %s\n", c_arguments.file_name);	
	printf("Window size: %d\n", c_arguments.window_size);
	printf("Seed: %d\n", c_arguments.seed);
	printf("Probability: %f\n", c_arguments.probability);
	printf("Mean time: %d\n", c_arguments.mean_time);
#endif


	

	unsigned long serverAddress=0;
	unsigned long temp=0, tempMask=0;
	unsigned long longestNetMask=0, longestNetMaskDiff =0; //Diff for server if on different network

	char* serverIPAddress = c_arguments.ipAddress;
	
	char* loopBack = "127.0.0.1";
	unsigned long loopBackAddress =0;
	inet_pton(AF_INET, loopBack, &loopBackAddress);
	inet_pton(AF_INET, serverIPAddress, &servaddr.sin_addr);
	
	
	/*
	**	Step 1
	**	To check whether the server is local or not
	*/
	for(ifi = get_ifi_info_plus(AF_INET, 1);  ifi != NULL; ifi=ifi->ifi_next)
	{
		mask = ((struct sockaddr_in *)ifi->ifi_ntmaddr);
		locAddr = ((struct sockaddr_in *)ifi->ifi_addr);
		
		if((servaddr.sin_addr.s_addr & mask->sin_addr.s_addr) == (locAddr->sin_addr.s_addr & mask->sin_addr.s_addr))
		{
			
			//To check Whether the server is on same network, if yes choose the highest precision netmask address
			flag_local = 1;
			temp = mask->sin_addr.s_addr;
			printf("MAsk of the interface : %ld\n", temp);
			if(temp > longestNetMask)
			{
				longestNetMask = temp;
				ifiClient = ifi;
			}
			//printf("Server is on the host itself \n");
			if(servaddr.sin_addr.s_addr == locAddr->sin_addr.s_addr)
			{
				flag_same = 1;
			}
		}
		else
		{
			tempMask = mask->sin_addr.s_addr;
			if(tempMask > longestNetMaskDiff)
			{
				longestNetMaskDiff = temp;
				ifiClient = ifi;
			}
		}
		printf("Information for an interface\n");
		printf("Name : %s\n", ifi->ifi_name);
		printf("Address : %s\n", my_sock_ntop(ifi->ifi_addr, sizeof(struct sockaddr)));
		printf("NetWork Mask : %s\n",my_sock_ntop(ifi->ifi_ntmaddr, sizeof(struct sockaddr)));
		printf("\n");

	}

	
	if(flag_same == 1)
	{
		servaddr.sin_addr.s_addr = loopBackAddress;
		locAddr->sin_addr.s_addr = loopBackAddress;
		cliaddr.sin_addr.s_addr = loopBackAddress;
	}	
	else if(flag_local == 1)
		{
			locAddr = ((struct sockaddr_in *)ifiClient->ifi_addr);
			cliaddr.sin_addr = locAddr->sin_addr;
		}
		else
		{
			locAddr = ((struct sockaddr_in *)ifiClient->ifi_addr);
			cliaddr.sin_addr = locAddr->sin_addr;
		}

	//printf("IS server on same network = %d\n Is server Local =%d \n", flag_same, flag_local);

	if(inet_ntop(AF_INET,&servaddr.sin_addr, serverStr, sizeof(serverStr)) <= 0)
	{
		perror("INET_PTON error:");
	}
	/*if(inet_ntop(AF_INET,&locAddr->sin_addr, clientStr, sizeof(clientStr)) <= 0)
	{
		perror("INET_PTON error:");
	}*/
	if(inet_ntop(AF_INET,&cliaddr.sin_addr, clientStr, sizeof(clientStr)) <= 0)
	{
		perror("INET_PTON error:");
	}
	
	//printf("Server address: %s\n",serverStr);
	//printf("Client Address: %s\n", clientStr);


	/*
	** 	Step 2
	**	Bind the UDP Socket to that client and server Address
	*/
	
	int sockfd;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		perror("Error while creating UDP Socket:");
		exit(1);
	}	
	
	if(flag_same == 1)
	{
		int optval =1;
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_DONTROUTE, &optval, sizeof optval);
	}

	cliaddr.sin_family	= AF_INET;
	cliaddr.sin_port        = htons(0);
	
	if(bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0)
	{
		perror("Bind error:");
		exit(1);
	}

	struct sockaddr_in cliSock;
	socklen_t len;
	bzero(&cliSock, sizeof(cliSock));
	len = sizeof(cliSock);
	if(getsockname(sockfd, (struct sockaddr *) &cliSock, &len) < 0)
	{
		perror("Get Socket Name error:");
		exit(1);
	}	

	printf("Client Binded to %s:%u\n",inet_ntoa(cliSock.sin_addr), ntohs(cliSock.sin_port));

	servaddr.sin_family      = AF_INET;
	servaddr.sin_port        = htons(c_arguments.port_number);
        //printf("Sever port : %d %d\n", servaddr.sin_port,MY_SERV_PORT);
	
	if(connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr))< 0)
	{
		perror("Server Connection Failed:");
		exit(1);
	}
	
	struct sockaddr_in servSock;
	//socklen_t len;
	bzero(&servSock, sizeof(servSock));
	len = sizeof(servSock);
	
	if(getpeername(sockfd, (struct sockaddr*)&servSock, &len) < 0)
	{
		perror("Getpeername failed :");
		exit(1);
	}
	
	printf("Server address %s:%u\n",inet_ntoa(servSock.sin_addr), ntohs(servSock.sin_port));

	/*
	**	Step 3
	**	Start acknowledgement with the server
	*/
	
	fileClient(sockfd, &servaddr, sizeof(servaddr), &c_arguments);


}
