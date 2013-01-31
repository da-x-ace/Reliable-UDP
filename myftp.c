#include <setjmp.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <signal.h>
#include "udp_utils.h"
#include "myftp.h"
#include "myrtt.h"

static struct my_rtt_info rttinfo;
static int rttinit=0;


struct header sendhdr, recvhdr;

static sigjmp_buf jmpbuf;

struct myList *head=NULL;

#define FILEBUFSIZE 512-sizeof(struct header)
#define RECVBUFSIZE 512

//typedef struct sockaddr* mysockaddr;

static void sig_alarm(int signo)
{
	siglongjmp(jmpbuf,1);
}


void mySwap(int* a, int* b)
{
	int temp;
	temp = *a;
	*a = *b;
	*b = temp;
}

int myMin(int a , int b)
{
	if( a >= b)
		return b;
	else 
		return a;
}


int myMax(int a , int b)
{
	if( a >= b)
		return a;
	else 
		return b;
}

void copyNode(struct myList* node, struct iovec* iv)
{
	//printf("Inside copyData\n");
	node->iv = calloc(2,sizeof(struct iovec));
	//memset(head->iv, 0, sizeof(head->iv));
	struct header* thishdr = (struct header*)iv[0].iov_base;
	struct header* temphdr = malloc(sizeof(struct header));
	memset(temphdr, 0, sizeof(struct header));
	temphdr->seq = thishdr->seq;
	temphdr->ts = thishdr->ts;
	temphdr->isACK = thishdr->isACK;
	temphdr->isLast = thishdr->isLast;
	temphdr->availWindow = thishdr->availWindow;

	node->iv[0].iov_base = (void*)temphdr;
	node->iv[0].iov_len  = iv[0].iov_len;
//	node->iv[1].iov_base = strdup((char*)iv[1].iov_base);
	char *tempBuf = (char*)malloc(sizeof(char)*FILEBUFSIZE);
	memset(tempBuf, 0, FILEBUFSIZE);
	memcpy(tempBuf, (char*)iv[1].iov_base, FILEBUFSIZE);
#ifdef DEBUG1
	printf("Temp buf : %s\n", tempBuf);
#endif
	node->iv[1].iov_base = tempBuf;

	node->iv[1].iov_len  = iv[1].iov_len;

#ifdef DEBUG1
	struct header* tempp = (struct header*) node->iv[0].iov_base;
	printf("Seq No. : %d\n", tempp->seq);
	printf("Data : %s\n",((char*)(node->iv[1].iov_base)));
#endif
}

void deallocateNode(struct myList *node)
{
	free(node->iv[0].iov_base);
	free(node->iv[1].iov_base);
	free(node->iv);
	free(node);
}


struct myList* addToList(struct myList* head, struct iovec* iv)
{
	struct myList* node;
	
	if(head == NULL)
	{
		//printf("\n In here\n");
		node=(struct myList *)malloc(sizeof(struct myList));
		//memset(head, 0, sizeof(struct myList));
		copyNode(node, iv);
		//printf(" Data : %s\n",(char*)(head->iv[1].iov_base));
		node->next = NULL;
		head=node;
		return head;
	}
	else
	{
		int checkSeq = ((struct header*)(iv[0].iov_base))->seq;
		struct myList *temp = head;
		int tempSeq  = ((struct header*)(temp->iv[0].iov_base))->seq;
		if(checkSeq <= tempSeq)
		{
			node=(struct myList *)malloc(sizeof(struct myList));
			copyNode(node, iv);
			//printf(" Data : %s\n",(char*)(head->iv[1].iov_base));
			node->next = temp;
			head=node;
			return head;
		}
		else
		{
			while(temp->next != NULL)
			{
				tempSeq = ((struct header*)(temp->next->iv[0].iov_base))->seq;
				if(checkSeq <= tempSeq)
				{
					node=(struct myList *)malloc(sizeof(struct myList));
					copyNode(node, iv);
					//printf(" Data : %s\n",(char*)(head->iv[1].iov_base));
					node->next = temp->next;
					temp->next=node;
					return head;
				}
				temp = temp->next;
			}
			
			node=(struct myList *)malloc(sizeof(struct myList));
			copyNode(node, iv);
			node->next = NULL;
			temp->next = node;
			return head;
		}
	}
}

struct myList* getNode(struct myList* head, int seqNo)
{
	struct myList* temp=head;
	while(temp != NULL)
	{
		if(((struct header*)temp->iv[0].iov_base)->seq == seqNo)
			return temp;
		temp= temp->next;
	}
	return NULL;
}

int getLength(struct myList *head)
{
    struct myList *temp = head;
    int length = 0;
    while(temp != NULL)
    {
        length++;
        temp = temp->next;
    }
    return length;
}

struct myList* deleteNodes(struct myList* head, int seqNo)
{
	struct myList* temp=head;
	struct myList* toDelete = NULL;

	if(getLength(head) <= 0)
		return NULL;

	while(temp != NULL)
	{
		if(((struct header *)(temp->iv[0].iov_base))->seq <= seqNo)
		{
			toDelete = temp;
			temp = temp->next;
			toDelete->next = NULL;
			deallocateNode(toDelete);
		}
		else
			break;
	}
	return temp;
}


struct myList* deleteFromList(struct myList* head, int seqNo)
{
	struct myList* temp=head;
	struct myList* toDelete = NULL;
	if(((struct header*)temp->iv[0].iov_base)->seq == seqNo)
	{
		if(temp->next != NULL)
			head= temp->next;
		else
			head = NULL;
		temp->next = NULL;
		deallocateNode(temp);
		return head;
	}
	while(temp->next!= NULL)
	{
		if(((struct header*)temp->next->iv[0].iov_base)->seq == seqNo)
		{
			toDelete = temp->next;
			temp->next = toDelete->next;
			toDelete->next = NULL;
			deallocateNode(toDelete);
		}
		temp = temp->next;
	}
	return head;
}


void printList(struct myList* head)
{
	struct myList *temp = head;
	struct header *temphdr;
	printf("________Printing the Sliding WIndow Buffer List________\n");
	while(temp != NULL)
	{
		temphdr = (struct header *)temp->iv[0].iov_base;
		printf("Seq No. : %d\n", temphdr->seq);
		printf("Data : %s\n",(char*)(temp->iv[1].iov_base));
		temp = temp->next;
	}
	printf("________End Printing the Sliding WIndow Buffer List________\n");
}

void congestionValues(struct congestion* myCongestion, int flag)
{	
	if(flag == 1)
	{
		myCongestion->currWindowSize = myCongestion->currWindowSize * 2;
	}
	else if(flag == 2)
		{
			myCongestion->currWindowSize = myCongestion->currWindowSize + 1;
		}
	
	
	if(myCongestion->ssthresh > myCongestion->maxWindowSize)
		myCongestion->ssthresh = myCongestion->maxWindowSize;
	if(myCongestion->currWindowSize > myCongestion->maxWindowSize)
			myCongestion->currWindowSize = myCongestion->maxWindowSize;

	if(myCongestion->currWindowSize < myCongestion->ssthresh)
	{
		myCongestion->state = MultiplicativeIncrease;
	}
	else
	{
		myCongestion->state = AdditiveIncrease;
	}
	printf("Current Window Size = %d, ssthreshold = %d, state = %d\n", myCongestion->currWindowSize, myCongestion->ssthresh, myCongestion->state);
}



void sendFile(int sockfd, char* fileName, struct sockaddr* cliAddr, socklen_t cliAddrLen, int cliWindow, int servWindow)
{
	struct sockaddr_in *destAddr;

	//bzero(&destAddr, sizeof(struct sockaddr));
	destAddr= (struct sockaddr_in *)cliAddr;
	printf("New Client address %s:%u\n",inet_ntoa(destAddr->sin_addr), ntohs(destAddr->sin_port));

	printf("Sockfd = %d\n", sockfd);

	fd_set rset;
	int maxfdp1;
	FD_ZERO(&rset);

	fd_set rset_probe;
	int maxfdp1_probe;
	FD_ZERO(&rset_probe);

	//Signals Initialization

	sigset_t mySignalSet;	

	if(sigemptyset(&mySignalSet) < 0)
	{
		perror("Sigemptyset error:");
		exit(1);
	}

	if(sigaddset(&mySignalSet, SIGALRM), 0)
	{
		perror("Sigaddset error");
		exit(1);
	} 

	signal(SIGALRM, sig_alarm);

	//End signal initialization

	//Congestion Control
	struct congestion myCongestion;
	myCongestion.ssthresh = myMin(cliWindow, servWindow);
	myCongestion.maxWindowSize = myMin(cliWindow, servWindow);
	myCongestion.cliWindowSize = cliWindow;
	myCongestion.currWindowSize = 1;
	myCongestion.state = MultiplicativeIncrease;
	
	int congestionFlag = MultiplicativeIncrease;
	int firstPacket = 1;

	//End Congestion control

	

	struct itimerval value, ovalue, pvalue, zeroValue;
	int which = ITIMER_REAL;
	int tempTime, tempSec, tempMSec;

	struct iovec iovsend[2], iovrecv[2];
	struct iovec *iovsendDup;
	
	struct myList* packet = NULL;	

	if (rttinit == 0) {
		my_rtt_init(&rttinfo);		/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

	mybool flag = 1;
	mybool probe = 2;
	
	//int fin;
	FILE *fin;
	fin = fopen(fileName,"r");
	if(fin == NULL) {
		perror("Error opening a file:");
		exit(1);
	}
	
	//printf("Sizeof myheader = %d\n", sizeof(struct header));
	//printf("Sizeof file buffer = %d\n", FILEBUFSIZE);
	
	int currentTime=0;

	int countDup=0;			//For counting the duplicates Acks
	int sequenceNumber=0;		//The sequence number used while sending
	int tempClientWindow = cliWindow;
	int tempCounter=0;
	//int sequenceACK=1;		//Gives me the sequence number of the packet recently ACKed
	int firstSequence = 1;
	int firstSequenceSent=0;	//Gives me the sequence number of first packet sent
	int lastSequenceSent=0;		//Gives me the sequence number of 
	//int lastACKRecv =0;
	int receivedACK=0;		//Gives me the sequence number of the packet recently AC
	int expectedACK =0;
	int isLast = 0;

	int recvCliWindow=0;

	int sendAgain = 0;

	int readBytes =0;
	char buf[FILEBUFSIZE];
	memset(buf, 0, FILEBUFSIZE);
	char recvBuf[RECVBUFSIZE];
	memset(recvBuf, 0, RECVBUFSIZE);

	//printf("Hi I'm here \n");
	int packetSend =0;
	//int tempWindow = cliWindow;	
	int tempWindow = 1;

	int retransmitCounter = 0;
	struct timeval time_count;
	recvCliWindow = cliWindow;

	tempCounter =0;
start_sending:
	//tempCounter =0;
	//packetSend++;	
	
	//tempWindow = cliWindow;

	printf("Recieved Window = %d\n", recvCliWindow);

	if(sigprocmask(SIG_BLOCK, &mySignalSet, NULL) < 0)
	{
		perror("sigprocmask block error:");
		exit(1);
	}

	// Persistant timer code for probe packet
	FD_ZERO(&rset_probe);
	retransmitCounter = 0;
probe_check:
	if(recvCliWindow == 0)
	{
		printf("---------Sending Probe Packet-------------\n");
		retransmitCounter++;
		if(retransmitCounter > 12)
		{
			printf("Client window size = 0 after trying for 12 times\n");
			printf("Now will terminate the server\n");
			exit(1);
		}
		time_count.tv_sec = 3;
		time_count.tv_usec = 0;
		memset((void *)&sendhdr, 0, sizeof(struct header));
		memset(buf, 0, FILEBUFSIZE);
		
		sendhdr.seq = 0;
		sendhdr.isLast = probe;
		iovsend[0].iov_base = (void*)&sendhdr;
		iovsend[0].iov_len = sizeof(struct header);
		iovsend[1].iov_base = (void*)&buf;
		iovsend[1].iov_len = sizeof(buf);

		sleep(5);
		if(writev(sockfd, iovsend, 2) < 0)
		{
			perror("Writev failed:");
			exit(1);
		}

		FD_SET(sockfd, &rset_probe);
		maxfdp1_probe=sockfd+1;

		if(select(maxfdp1_probe, &rset_probe, NULL, NULL, &time_count) < 0)
		{
			if(errno == EINTR)
				goto check_window;
			else
			{
				perror("Select Error :");
				exit(1);
			}
		}
		if(FD_ISSET(sockfd, &rset_probe))
		{
			//printf("Hi I'm here");
			memset((void *)&recvhdr, 0, sizeof(struct header));
			memset(recvBuf, 0, RECVBUFSIZE);
			iovrecv[0].iov_base = (void*)&recvhdr;
			iovrecv[0].iov_len = sizeof(recvhdr);
			iovrecv[1].iov_base = recvBuf;
			iovrecv[1].iov_len = sizeof(recvBuf);
			if(readv(sockfd, iovrecv, 2) < 0)
			{
				perror("Error while reading the ACK:");
				exit(1);
			}
			if(recvhdr.isLast == 3)
			{
				recvCliWindow = recvhdr.availWindow;
				printf("Received Reply for Probe Packet : Window Size = %d\n", recvCliWindow);
			}
		
		}
		else
		{
			goto check_window;
		}

check_window:
		if(recvCliWindow <= 0)
			goto probe_check;

	}
	

	//Persistant timer code end

	printf("Will Send packets :\n");

	if(firstPacket != 1)
	{
		printf("Update congestion values\n");
		myCongestion.maxWindowSize = recvCliWindow;
		congestionValues(&myCongestion, myCongestion.state);
		tempWindow = myCongestion.currWindowSize;
		
	}
	printf("Current Window Size = %d, ssthreshold = %d, state = %d\n", myCongestion.currWindowSize, myCongestion.ssthresh, myCongestion.state);
	firstPacket = 0;
	//currWindowSize stores the info about the packets which is to be sent
	//printf("Hi before already packets\n");
	if((getLength(head) >= myCongestion.currWindowSize) &&  recvCliWindow > 0)
	{
		tempWindow = myCongestion.currWindowSize;
		printf("_____Packets already in Server Window_______\n");

		packet = head;
		my_rtt_newpack(&rttinfo);

		while(tempWindow > 0)
		{
			
			printf("Sending packet with sequence number = %d\n", ((struct header *)packet->iv[0].iov_base)->seq);
			iovsendDup = packet->iv;
			if(writev(sockfd, iovsendDup, 2) < 0)
			{
				perror("Writev failed:");
				exit(1);
			}
			lastSequenceSent = ((struct header *)packet->iv[0].iov_base)->seq;
			packet = packet->next;
			tempWindow--;
		}
		//goto wait_timer;
		
	}

	//printf("Hi after already packets\n");
	if(getLength(head) > 0 && (getLength(head) < myCongestion.currWindowSize) &&  recvCliWindow > 0)
	{
		tempWindow = myCongestion.currWindowSize;
		packet = head;
		my_rtt_newpack(&rttinfo);
		while(tempWindow > 0 && packet != NULL)
		{
			printf("Sending packet with sequence number = %d\n", ((struct header *)packet->iv[0].iov_base)->seq);
			iovsendDup = packet->iv;
			if(writev(sockfd, iovsendDup, 2) < 0)
			{
				perror("Writev failed:");
				exit(1);
			}
			lastSequenceSent = ((struct header *)packet->iv[0].iov_base)->seq;
			packet = packet->next;
			tempWindow--;
		}
	}

	//printf("Hi before adding to linked list\n");
	printf("TempWindow = %d\n", tempWindow);
	//while(tempCounter < packetSend && tempCounter < 6)
	while(tempWindow > 0 && recvCliWindow > 0)
	{
		
		memset((void *)&sendhdr, 0, sizeof(struct header));
		memset(buf, 0, FILEBUFSIZE);
		if((readBytes = fread(buf, FILEBUFSIZE, sizeof(char), fin)) < 0)
		{
			perror("fread Error:");
			exit(1);
		}
#ifdef DEBUG1
		printf("Read buffer : %s\n", buf);
#endif
		sendhdr.seq = ++sequenceNumber;
		
		if(feof(fin))
		{	
			printf("Last packet \n");
			sendhdr.isLast = flag;
			isLast = 1;
		}
		else
		{
			//sendhdr.isLast = 0;
		}
		printf("\nSequence number : %d\n", sendhdr.seq);
		iovsend[0].iov_base = (void*)&sendhdr;
		iovsend[0].iov_len = sizeof(struct header);
		iovsend[1].iov_base = (void*)&buf;
		iovsend[1].iov_len = sizeof(buf);
			
		
		head = addToList(head, iovsend);
#ifdef DEBUG1
		printf("Sequence check = %d\n", ((struct header *)head->iv[0].iov_base)->seq);
		printf("Data check = %s\n", (char*)head->iv[1].iov_base);
#endif
		//printList(head);
		
		my_rtt_newpack(&rttinfo);
		sendhdr.ts = my_rtt_ts(&rttinfo);		

		if(writev(sockfd, iovsend, 2) < 0)
		{
			perror("Writev failed:");
			exit(1);
		}

		//sendhdr.ts = my_rtt_ts(&rttinfo);
		printf("Sending packet with sequence number = %d\n", sendhdr.seq);
		lastSequenceSent = sendhdr.seq;
 		//tempCounter++;
		tempWindow--;
		if(sendhdr.isLast == myTRUE)
			tempWindow=0;
	}
	

common_all:

	printf("Window contains %d to %d sequence Numbers\n", ((struct header *)head->iv[0].iov_base)->seq , lastSequenceSent);
	
	sendAgain = 0;
	
	if(sigprocmask(SIG_UNBLOCK, &mySignalSet, NULL) < 0)
	{
		perror("sigprocmask unblock error:");
		exit(1);
	}

wait_timer:

	expectedACK = ((struct header *)head->iv[0].iov_base)->seq;

	if(sendAgain == 1)
	{
		if((packet = getNode(head, expectedACK)) != NULL)
		{	
			printf("Retransmission for packet with sequence number = %d after timeout\n", expectedACK);
			iovsendDup = packet->iv;
			if(writev(sockfd, iovsendDup, 2) < 0)
			{
				perror("Writev failed:");
				exit(1);
			}
		//setitimer(ITIMER_REAL, &zeroValue,0);
		//my_rtt_newpack(&rttinfo);
		}
	}

	
	// Timeout for next ACK
	// if(Current Time - Previous time) < RTO, goto select and wait for the remaining time

	countDup =0;

	//currentTime = my_rtt_ts(&rttinfo);
	
	//tempTime = (int32_t)(rttinfo.rtt_rto - (currentTime - ((struct header *)head->iv[0].iov_base)->ts));


	tempTime = my_rtt_start(&rttinfo);
	//tempTime = ((struct header *)head->iv[0].iov_base)->ts;
        if (tempTime >= 1000) {
                tempSec = tempTime / 1000;
        }
        tempMSec = (tempTime % 1000)*1000;

    	value.it_interval.tv_sec = 0;
    	value.it_interval.tv_usec = 0;
    	value.it_value.tv_sec = tempSec;
        value.it_value.tv_usec = tempMSec;

	zeroValue.it_interval.tv_sec = 0;
    	zeroValue.it_interval.tv_usec = 0;
    	zeroValue.it_value.tv_sec = 0;
        zeroValue.it_value.tv_usec = 0;
   
    	printf("\nTIMER Value: %d %d\n", (int)value.it_value.tv_sec,(int)value.it_value.tv_usec);

	setitimer(ITIMER_REAL, &zeroValue,0);
    	setitimer(ITIMER_REAL, &value,0);

	if (sigsetjmp(jmpbuf, 1) != 0) 
	{
		if (my_rtt_timeout(&rttinfo) < 0) {
			printf("Network conditions pretty bad.\n");
			rttinit = 0;	/* reinit in case we're called again */
			errno = ETIMEDOUT;
			exit(1);
			//return(-1);
		}
#ifdef	DEBUG
		err_msg("Timeout, retransmitting\n");
#endif
		sendAgain =1;

		myCongestion.ssthresh = myMax(myCongestion.ssthresh/2, 2);
		myCongestion.currWindowSize = 1;
		myCongestion.state = MultiplicativeIncrease;
		//congestionValues(&myCongestion, MultiplicativeIncrease);
		printf("Current Window Size = %d, ssthreshold = %d, state = %d\n", myCongestion.currWindowSize, myCongestion.ssthresh, myCongestion.state);
		//goto start_sending;		

		goto wait_timer;
	}
	
	
	FD_ZERO(&rset);

wait_ACK:

	FD_SET(sockfd, &rset);
	maxfdp1=sockfd+1;

	if(select(maxfdp1, &rset, NULL, NULL, NULL) < 0)
	{
		if(errno == EINTR)
			goto wait_ACK;
		else
		{
			perror("Select Error :");
			exit(1);
		}
	}
	if(FD_ISSET(sockfd, &rset))
	{
		memset((void *)&recvhdr, 0, sizeof(struct header));
		iovrecv[0].iov_base = (void*)&recvhdr;
		iovrecv[0].iov_len = sizeof(recvhdr);
		iovrecv[1].iov_base = recvBuf;
		iovrecv[1].iov_len = sizeof(recvBuf);
		if(readv(sockfd, iovrecv, 2) < 0)
		{
			perror("Error while reading the ACK:");
			exit(1);
		}
		
		//expectedACK = ((struct header *)head->iv[0].iov_base)->seq;
		//printf("Expected ACK = %d\n", expectedACK);
		printf("Window Size Available  = %d\n", recvhdr.availWindow);
		if(recvhdr.isACK ==1)
		{
			printf("Window Size Available  = %d\n", recvhdr.availWindow);
			recvCliWindow = recvhdr.availWindow;

			receivedACK = recvhdr.seq;
			printf("Received ACK for packet : %d\n", recvhdr.seq);
			
			packet = getNode(head, recvhdr.seq);
	                if(packet != NULL)
			{
				if(recvhdr.ts == ((struct header*)(packet->iv[0].iov_base))->ts)
	                        {
					my_rtt_stop(&rttinfo, my_rtt_ts(&rttinfo) - ((struct header*)(head->iv[0].iov_base))->ts);
					//my_rtt_stop(&rttinfo, my_rtt_ts(&rttinfo) - recvhdr.ts);
					printf("RTO updated to: %d millisec\n", rttinfo.rtt_rto);
	                        }
				my_rtt_newpack(&rttinfo);
			}
	
			printf("ReceivedACK =%d ExpectedACK = %d last sequence = %d\n",receivedACK, expectedACK, lastSequenceSent);
			if(receivedACK == expectedACK)
			{
				expectedACK++;
			}
			
			//printf("Length of the linked list = %d\n",getLength(head));
			//head = deleteFromList(head, receivedACK);
			head = deleteNodes(head, receivedACK);
			//printf("Length of the linked list = %d\n",getLength(head));

			if(isLast == 1)
			{
				if(receivedACK == lastSequenceSent)
				{
					printf("File transfer completed successfully \n");
					goto close_file;
				}
			}

			if(receivedACK == lastSequenceSent)
			{
				printf("Received the required sequence number before timeout\n");
				setitimer(ITIMER_REAL, &zeroValue,0);
				
				goto start_sending;			
			}
			
			if(receivedACK < expectedACK)
			{
				if((packet = getNode(head, receivedACK)) == NULL)
				{
					countDup++;
				}
				if(countDup == 2)
				{
					if((packet = getNode(head, receivedACK+1)) != NULL)					
					{
						//Fast Retransmit the packet with sequence number (receivedACK+1)
						printf("Fast retransmission for packet with sequence number = %d\n", receivedACK+1);
						iovsendDup = packet->iv;
						if(writev(sockfd, iovsendDup, 2) < 0)
						{
							perror("Writev failed:");
							exit(1);
						}
						setitimer(ITIMER_REAL, &zeroValue,0);
						my_rtt_newpack(&rttinfo);
						
						printf("Fast recovery Happened here\n");
						myCongestion.ssthresh = myMax(myCongestion.ssthresh/2, 2);
						myCongestion.currWindowSize = myCongestion.ssthresh;
						myCongestion.state = AdditiveIncrease;
						//congestionValues(&myCongestion, AdditiveIncrease);
						printf("Current Window Size = %d, ssthreshold = %d, state = %d\n", myCongestion.currWindowSize, myCongestion.ssthresh, myCongestion.state);

						goto wait_timer;
					}
				}
			}
			if(receivedACK > expectedACK)
			{
				expectedACK = receivedACK+1;
			}
		}
	}
	else
	{
		/*printf("Timeout occured\n");
		int willRetry =0;
		if(lastACKRecv == 0)
		{
			//Have to retransmit packet 1
			willRetry = my_rtt_timeout(&rttinfo);
			if(willRetry < 0)
			{
				printf("Network conditions pretty bad.\n");
				rttinit = 0;					
				errno = ETIMEDOUT;
				exit(1);
				//return(-1);
			}
			packet = getNode(head, firstSequence);
			if(packet != NULL)
			{
				((struct header*)(packet->iv[0].iov_base))->ts = my_rtt_ts(&rttinfo);
				printf("Retransmission of packet with seqNo = : %d\n", firstSequence);
				//writev(sockfd, packet->iv, 2);
			}
		}*/
		
		goto wait_ACK;
	}	
	
goto wait_ACK;

	

close_file:

	fclose(fin);
	
}




