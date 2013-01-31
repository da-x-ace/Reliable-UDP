#include "udp_utils.h"
#include "myftp.h"
#include <math.h>

typedef struct 
{
	int sock_fd;
	char fileName[BUFFER_SIZE];
	int windowSize;
	int seed;
	float probability;
	int meanTime;

}producer_args;

producer_args p_args;

struct myList *client_head=NULL;
static int sequence_tracker = 0;
static int transfer_completed = 0;

int list_count = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t count_max = PTHREAD_COND_INITIALIZER;

int length_of_list;

void* print_consumer(void *args);
pthread_t consumer_thread; 

int generate_random_number()
{
	return (-1) * log((float)drand48()) * p_args.meanTime;
}

void recieveFile(int sock_fd, char* fileName, int windowSize, int seed, float probability, int meanTime)
{
	struct iovec recv_packet[2], send_packet[2], iovrecv[2];
	int nbytes;
	struct timeval tv;
	struct myList *to_send_ts;
	struct header sendhdr, recvhdr;
	char recieved_buffer[FILEBUFSIZE];
	fd_set service_set, rset_last;
	int finCounter = 0;
	int finStart = -1;

	p_args.sock_fd = sock_fd;
	p_args.windowSize = windowSize;
	memset(p_args.fileName, 0, BUFFER_SIZE);
	int length = strlen(fileName);
	memcpy(p_args.fileName, fileName, length);
	p_args.seed = seed;
	p_args.probability = probability;
	p_args.meanTime = meanTime;

#if 1
	printf("Seed: %d\n", p_args.seed);
	printf("Probability: %f\n", p_args.probability);
	printf("Mean-Time: %d\n", p_args.meanTime);
#endif

	srand48(seed);

	pthread_create(&consumer_thread,NULL,print_consumer,NULL);

//////////////////////////////////////////////////////////////////////////
	while (1) 
	{
    	FD_ZERO(&service_set);
		FD_SET(p_args.sock_fd ,&service_set);

		tv.tv_sec = 5;
		tv.tv_usec = 0;
                        
		if(select(p_args.sock_fd + 1, &service_set, NULL, NULL, &tv) < 0)
		{
			if (errno == EINTR)
				continue;
		}

		if(FD_ISSET(p_args.sock_fd, &service_set))
		{
			memset((void*)&recvhdr, 0, sizeof(struct header));
			memset(recieved_buffer, 0, FILEBUFSIZE);
			recv_packet[0].iov_base = (void*)&recvhdr;
			recv_packet[0].iov_len = sizeof(struct header);
			recv_packet[1].iov_base = recieved_buffer;
			recv_packet[1].iov_len = sizeof(recieved_buffer);
				
			nbytes = readv(p_args.sock_fd, recv_packet,2);
							
			if(nbytes <= 0)
			{
				printf("ERROR: Server terminated. Quitting. \n ");
				goto done_with_this;
			}
///// Probing packet
			if (recvhdr.isLast == 2)
			{
				memset((void*)&sendhdr, 0, sizeof(struct header));
				sendhdr.isLast = 3;//myTRUE;
				/*lock the variable*/
				pthread_mutex_lock(&count_mutex);			
				sendhdr.availWindow = p_args.windowSize - getLength(client_head);
				pthread_cond_signal(&count_max);
				/*unlock the variable*/
				pthread_mutex_unlock(&count_mutex);

				send_packet[0].iov_base = (void*)&sendhdr;
				send_packet[0].iov_len = sizeof(struct header);
				send_packet[1].iov_base = NULL;
				send_packet[1].iov_len = 0;

				if(writev(p_args.sock_fd, send_packet, 2) < 0)
				{
					perror("Writev failed:");
					exit(1);
				}
				printf("Sent ACK for PROBING PACKET with window size %d and isLast as %d\n", sendhdr.availWindow, sendhdr.isLast);
				continue;
			}
///// Probing packet
		   float random_dropper = (float)drand48();
		   printf("Random for dropping packets = %f\n", random_dropper);
		   if(random_dropper >= p_args.probability)
		   {
			/* lock the variable */
			pthread_mutex_lock(&count_mutex);
///////////////// CRITICAL SECTION
			int to_find = recvhdr.seq;
			//printf("List length = %d\n", getLength(client_head));
			if (NULL == (struct myList*)getNode(client_head, to_find) &&
				getLength(client_head) < p_args.windowSize && to_find > sequence_tracker)
			{
				client_head = (struct myList*) addToList(client_head, recv_packet);
				length_of_list = getLength(client_head);
				printf("Producer adding packet with sequence number: %d \n", recvhdr.seq);
				to_send_ts = (struct myList*)getNode(client_head, recvhdr.seq);
				printf("%d has ts of %d\n", recvhdr.seq, recvhdr.ts);
				if(recvhdr.isLast == myTRUE)
				{
					printf("**********LAST Packet added to the list\n");
					finStart = 1;
				}

			}

			if(recvhdr.seq == sequence_tracker + 1)
			{
				do
				{
					sequence_tracker++;

				}while(getNode(client_head, sequence_tracker + 1));
			}

			printf("Sequences in order till now : %d\n", sequence_tracker);

			memset((void*)&sendhdr, 0, sizeof(struct header));
			sendhdr.seq = sequence_tracker;
			sendhdr.isACK = 1;//myTRUE;
			if(to_send_ts != NULL)
				sendhdr.ts = ((struct header*)to_send_ts->iv[0].iov_base)->ts;
			sendhdr.availWindow = p_args.windowSize - getLength(client_head);
///////////////// CRITICAL SECTION	
			pthread_cond_signal(&count_max);
			/*unlock the variable*/
			pthread_mutex_unlock(&count_mutex);

			send_packet[0].iov_base = (void*)&sendhdr;
			send_packet[0].iov_len = sizeof(struct header);
			send_packet[1].iov_base = NULL;
			send_packet[1].iov_len = 0;

			if(writev(p_args.sock_fd, send_packet, 2) < 0)
			{
				perror("Writev failed:");
				exit(1);
			}
			printf("Sending ACK for sequence number = %d with window size = %d, ts = %d\n", sendhdr.seq, sendhdr.availWindow, sendhdr.ts);

			if (finStart == 1)
				goto retransmit_fin;
		   }
		   else
		   {
				printf("Dropping packet %d since %f < %f\n", recvhdr.seq, random_dropper, p_args.probability);
				continue;
		   }
		}
	}

		struct timeval time_count;
		FD_ZERO(&rset_last);
		int retransmitCounter = 0;

retransmit_fin:
	
		retransmitCounter++;
		if(retransmitCounter > 5)
		{
			goto done_with_this;
		}

 		FD_SET(p_args.sock_fd, &rset_last);
        time_count.tv_sec = 3;
        time_count.tv_usec = 0;

        if(select(p_args.sock_fd+1, &rset_last, NULL, NULL, &time_count) < 0)
        {
            if(errno == EINTR)
                goto retransmit_fin;
            else
            {
                perror("Select Error :");
                exit(1);
            }
        }
        if(FD_ISSET(p_args.sock_fd, &rset_last))
        {
            //printf("Hi I'm here");
            memset((void *)&recvhdr, 0, sizeof(struct header));
            memset(recieved_buffer, 0, FILEBUFSIZE);
            iovrecv[0].iov_base = (void*)&recvhdr;
            iovrecv[0].iov_len = sizeof(recvhdr);
            iovrecv[1].iov_base = recieved_buffer;
            iovrecv[1].iov_len = sizeof(recieved_buffer);
            if(readv(p_args.sock_fd, iovrecv, 2) < 0)
            {
                perror("Error while reading the ACK:");
                exit(1);
            }
            if(recvhdr.isLast == 1)
            {
            	printf("Received Last packet Again \n");
            	if(writev(p_args.sock_fd, send_packet, 2) < 0)
				{
					perror("Writev failed:");
					exit(1);
				}
            }
        }
        else
        {
            goto retransmit_fin;
        }


done_with_this:
	printf("Exit producer \n");
	pthread_exit(NULL);

	//child thread termination
	pthread_join(consumer_thread, NULL);
	pthread_mutex_destroy(&count_mutex);
	pthread_cond_destroy(&count_max);
	printf("\n In TIME_WAIT state.. Waiting for 30 seconds\n\n");
	sleep(30);

//////////////////////////////////////////////////////////////////////////
   
}

void* print_consumer(void *args)
{
	int i, rc;
	struct myList * temp_node;
	FILE *fp = NULL;
	int finished_doing = -1;
	int time_to_sleep = -1;

	printf("Print Consumer : \n");

	fp = fopen("recvd.txt", "w");
   	if(fp == NULL) 
	{
	    printf("ERROR! Unable to open file. \n");
	    return;
   	}

	for (;;) 
	{
/*		struct timeval tv;
		struct timespec ts;
		if (gettimeofday(&tv, NULL) < 0)
			 err_sys("gettimeofday error");
		ts.tv_sec = tv.tv_sec + 10;     
		ts.tv_nsec = tv.tv_usec * 1000; 

print:*/
		Pthread_mutex_lock(&count_mutex);
		/*rc = pthread_cond_timedwait(&count_max, &count_mutex,&ts);       
		if (rc == ETIMEDOUT) {
			Pthread_mutex_unlock(&count_mutex);
			goto print;
		}*/

		if (client_head != NULL)									
		{
			int toStart = ((struct header*)client_head->iv[0].iov_base)->seq;
			for(i = toStart; i<=sequence_tracker; i++)
			{
				temp_node = (struct myList*) getNode(client_head, i);

				if (temp_node != NULL)
				{
					
					printf("_________________PACKET DATA START_______________________\n");
					printf("*************************Printing packet with sequence number %d\n", ((struct header*)temp_node->iv[0].iov_base)->seq);
					//printf("Data bytes = %d\n", temp_node->iv[1].iov_len);
					printf("Data = \n%s\n", (char *)temp_node->iv[1].iov_base);
					printf("_________________PACKET DATA END_______________________\n");

					if (((struct header*)temp_node->iv[0].iov_base)->isLast == 1)
					{
						finished_doing = 0;
						//fputs((char *)temp_node->iv[1].iov_base, fp);
						printf("Length of data in packet = %d\n", strlen((char *)temp_node->iv[1].iov_base));
						fwrite ((char *)temp_node->iv[1].iov_base , 1 , strlen((char *)temp_node->iv[1].iov_base) , fp);
						printf("****************Last packet printed **************************************\n");
					}
					else
					{

						//fputs((char *)temp_node->iv[1].iov_base, fp);
						printf("Length of data in packet = %d\n", temp_node->iv[1].iov_len);
						fwrite ((char *)temp_node->iv[1].iov_base , 1 , temp_node->iv[1].iov_len , fp);
						printf("****************Last %d**************************************\n", ((struct header*)temp_node->iv[0].iov_base)->isLast);

					}
					client_head = (struct myList*) deleteFromList(client_head, i);						

				}

				if( finished_doing == 0)
				{
					pthread_mutex_unlock(&count_mutex);
					goto lets_go_home;
				}
			}
		}
		/*unlock the variable*/
		pthread_mutex_unlock(&count_mutex);
		time_to_sleep = generate_random_number();
		printf("*********************************************************\n");
		printf("Print Consumer sleeping for %d milliseconds.\n", time_to_sleep);
		printf("*********************************************************\n");
		usleep(time_to_sleep * 1000);
	}

lets_go_home:
	fclose(fp);
	printf("Exit Consumer\n");

	pthread_exit(NULL);	
}


