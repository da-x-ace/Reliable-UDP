CC = gcc

LIBS = -lresolv -lsocket -lnsl -lm -lpthread\
         /home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: client server

client: client.o get_ifi_info_plus.o client_ftp.o myftp.o myrtt.o
	${CC} ${FLAGS} -o client client.o get_ifi_info_plus.o client_ftp.o myftp.o myrtt.o ${LIBS}    

client.o: client.c
	${CC} ${CFLAGS} -c client.c

server: server.o get_ifi_info_plus.o linked_list.o myrtt.o myftp.o
	${CC} ${FLAGS} -o server server.o get_ifi_info_plus.o linked_list.o myrtt.o myftp.o ${LIBS}

server.o: server.c
	${CC} ${CFLAGS} -c server.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

linked_list.o: linked_list.c
	${CC} ${CFLAGS} -c linked_list.c

myrtt.o: myrtt.c
	${CC} ${CFLAGS} -c myrtt.c 

myftp.o: myftp.c
	${CC} ${CFLAGS} -c myftp.c

client_ftp.o: client_ftp.c
	${CC} ${CFLAGS} -c client_ftp.c

clean:
	rm  client client.o server server.o get_ifi_info_plus.o linked_list.o myrtt.o myftp.o client_ftp.o
