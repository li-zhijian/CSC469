/*
 *   CSC469 Fall 2010 A3
 *  
 *      File:      client_recv.c 
 *      Author:    Angela Demke Brown
 *      Version:   1.0.0
 *      Date:      17/11/2010
 *   
 * Please report bugs/comments to demke@cs.toronto.edu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "client.h"

static char *option_string = "f:";

/* For communication with chat client control process */
int ctrl2rcvr_qid;
char ctrl2rcvr_fname[MAX_FILE_NAME_LEN];

/* Socket to listen for messages */
u_int16_t client_udp_port;
struct addrinfo *client_udp_info;
int udp_socket_fd;

/* Macro for queue operations */
#define ERROR(code) do { \
    send_error(ctrl2rcvr_qid, (code)); \
    exit(-1); \
} while(0)

#define OK(code) send_ok(ctrl2rcvr_qid, (code))

void usage(char **argv) {
	printf("usage:\n");
	printf("%s -f <msg queue file name>\n",argv[0]);
	exit(1);
}


void open_client_channel(int *qid) {

	/* Get messsage channel */
	key_t key = ftok(ctrl2rcvr_fname, 42);

	if ((*qid = msgget(key, 0400)) < 0) {
		perror("open_channel - msgget failed");
		fprintf(stderr,"for message channel ./msg_channel\n");

		/* No way to tell parent about our troubles, unless/until it 
		 * wait's for us.  Quit now.
		 */
		exit(1);
	}

	return;
}

void send_error(int qid, u_int16_t code)
{
	/* Send an error result over the message channel to client control process */
	msg_t msg;

	msg.mtype = CTRL_TYPE;
	msg.body.status = RECV_NOTREADY;
	msg.body.value = code;

	if (msgsnd(qid, &msg, sizeof(struct body_s), 0) < 0) {
		perror("send_error msgsnd");
	}
							 
}

void send_ok(int qid, u_int16_t port)
{
	/* Send "success" result over the message channel to client control process */
	msg_t msg;

	msg.mtype = CTRL_TYPE;
	msg.body.status = RECV_READY;
	msg.body.value = port;

	if (msgsnd(qid, &msg, sizeof(struct body_s), 0) < 0) {
		perror("send_ok msgsnd");
	} 

}

void init_receiver()
{

	/* 1. Make sure we can talk to parent (client control process) */
	printf("Trying to open client channel\n");

	open_client_channel(&ctrl2rcvr_qid);

	/**** YOUR CODE TO FILL IMPLEMENT STEPS 2 AND 3 ****/

	/* 2. Initialize UDP socket for receiving chat messages. */
	int status;
	struct addrinfo hints;

	/* Make sure hints struct is empty */
	memset(&hints, 0, sizeof (struct addrinfo));

	/* Initialize hints */
	hints.ai_family = AF_INET; /* Only communicate over IPv4 */
	hints.ai_socktype = SOCK_DGRAM; /* Listen for UDP packets */
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV; /* Fill in my IP */

	/* Get our address info */
	if((status = getaddrinfo(NULL, "0", &hints, &client_udp_info)) != 0) {
	    ERROR(NAME_FAILED);
	}

	/* Initialize Socket */
	if((udp_socket_fd = socket(client_udp_info->ai_family, client_udp_info->ai_socktype, client_udp_info->ai_protocol)) == -1) {
	    ERROR(SOCKET_FAILED);
	}

	/* Bind socket to any available port */
	if(bind(udp_socket_fd, client_udp_info->ai_addr, client_udp_info->ai_addrlen) == -1) {
	    ERROR(BIND_FAILED);
	}

	/* Get Socket Info */
	if(getsockname(udp_socket_fd, client_udp_info->ai_addr, &(client_udp_info->ai_addrlen)) != 0) {
	    ERROR(NAME_FAILED);
	}

	/* 3. Tell parent the port number if successful, or failure code if not. 
	 *    Use the send_error and send_ok functions
	 */
	client_udp_port = ntohs(((struct sockaddr_in *)client_udp_info->ai_addr)->sin_port);
	OK(client_udp_port);
}




/* Function to deal with a single message from the chat server */

void handle_received_msg(char *buf)
{
    struct chat_msghdr *hdr = (struct chat_msghdr *) buf;
    printf("%s: %s", hdr->sender.member_name, (char *) hdr->msgdata);
}



/* Main function to receive and deal with messages from chat server
 * and client control process.  
 *
 * You may wish to refer to server_main.c for an example of the main 
 * server loop that receives messages, but remember that the client 
 * receiver will be receiving (1) connection-less UDP messages from the 
 * chat server and (2) IPC messages on the from the client control process
 * which cannot be handled with the same select()/FD_ISSET strategy used 
 * for file or socket fd's.
 */
void receive_msgs()
{
	char buf[MAX_MSG_LEN];

	/* Initialize file descriptor sets for select */
	fd_set rset;
	fd_set allset;

	/*
	 * Should really use POSIX Message Queus so that we can monitor both
	 * Socket and Queue using select. But since makefile doesn't link
	 * against librt it is not possible to do so for this assignment.
	 */
	FD_ZERO(&allset); /* Clear Sets */
	FD_ZERO(&rset);
	FD_SET(udp_socket_fd, &allset);

	int maxfd = udp_socket_fd;

	while(1) {
	    /* Initialize timeout */
	    struct timeval tv;
	    tv.tv_sec = 0;
	    tv.tv_usec = 100 * 1000; /* 100 Milliseconds */

	    /* Wait for one of the fd's to become active or timeout */
	    rset = allset;
	    if(select(maxfd + 1, &rset, NULL, NULL, &tv) == -1) {
            perror("select");
            exit(1);
        }

	    /* Check UDP Socket */
	    if(FD_ISSET(udp_socket_fd, &rset)) {
	        memset(buf, 0, MAX_MSG_LEN);
	        recvfrom(udp_socket_fd, buf, MAX_MSG_LEN, 0, NULL, NULL);
	        handle_received_msg(buf);
	    }

	    /* Check Message Queue */
        int result;
        msg_t msg;
        result = msgrcv(ctrl2rcvr_qid, &msg, sizeof(struct body_s), RECV_TYPE, IPC_NOWAIT);
        if (result == -1 && errno == ENOMSG) {
            continue;
        } else if (result > 0) {
            if (msg.body.status == CHAT_QUIT) {
                printf("Quitting chat\n");
                close(udp_socket_fd);

                #ifdef DEBUG
                /* Let user know we're quitting */
                fflush(stdout);
                usleep(250 * 1000);
                #endif

                exit(0);
            }
        } else {
            perror("msgrcv");
        }
	}

	/* Cleanup */
	free(buf);
	return;
}


int main(int argc, char **argv) {
	char option;

	printf("RECEIVER alive: parsing options! (argc = %d)\n",argc);

	while((option = getopt(argc, argv, option_string)) != -1) {
		switch(option) {
		case 'f':
			strncpy(ctrl2rcvr_fname, optarg, MAX_FILE_NAME_LEN);
			break;
		default:
			printf("invalid option %c\n",option);
			usage(argv);
			break;
		}
	}

	if(strlen(ctrl2rcvr_fname) == 0) {
		usage(argv);
	}

	printf("Receiver options ok... initializing\n");

	init_receiver();

	receive_msgs();

	return 0;
}
