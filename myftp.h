#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include "unp.h"

typedef unsigned short mybool;

#define myTRUE 1
#define myFALSE 0

#define FILEBUFSIZE 512-sizeof(struct header)
#define RECVBUFSIZE 512

#define AdditiveIncrease    2
#define MultiplicativeIncrease     1

struct header{
	int seq;
	uint32_t ts;
	mybool isACK;
	mybool isLast;
	int availWindow;
};

struct myList{
	struct iovec *iv;
	struct myList* next;
};



struct congestion{
    int ssthresh;
    int maxWindowSize;
    int cliWindowSize;
    int currWindowSize;
    int state;
};


void sendFile(int, char* ,struct sockaddr*, socklen_t, int ,int );
