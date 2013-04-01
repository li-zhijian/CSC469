/*
 *   CSC469 Fall 2010 A3
 *  
 *      File:      client.h 
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

#include <netinet/in.h>
#include <netdb.h>

#include "client.h"

#ifdef DEBUG
#define LOCATION_SERVER "localhost"
#else
#define LOCATION_SERVER "www.cdf.toronto.edu"
#endif

static void build_req(char *buf)
{
	/* Write an HTTP GET request for the chatserver.txt file into buf */

	int nextpos;

	sprintf(buf,"GET /~csc469h/winter/chatserver.txt HTTP/1.0\r\n");

	nextpos = strlen(buf);
	sprintf(&buf[nextpos],"\r\n");
}

static char *skip_http_headers(char *buf)
{
	/* Given a pointer to a buffer which contains an HTTP reply,
	 * skip lines until we find a blank, and then return a pointer
	 * to the start of the next line, which is the reply body.
	 * 
	 * DO NOT call this function if buf does not contain an HTTP
	 * reply message.  The termination condition on the while loop 
	 * is ill-defined for arbitrary character arrays, and may lead 
	 * to bad things(TM). 
	 *
	 * Feel free to improve on this.
	 */

	char *curpos;
	int n;
	char line[256];

	curpos = buf;

	while ( sscanf(curpos,"%256[^\n]%n",line,&n) > 0) {
		if (strlen(line) == 1) { /* Just the \r was consumed */
			/* Found our blank */
			curpos += n+1; /* skip line and \n at end */
			break;
		}
		curpos += n+1;
	}

	return curpos;
}


int retrieve_chatserver_info(char *chatserver_name, u_int16_t *tcp_port, u_int16_t *udp_port)
{
	int locn_socket_fd;
	int buflen;
	int code;
	int  n;

	/* Initialize locnserver_addr. 
	 * We use a text file at a web server for location info
	 * so this is just contacting the CDF web server 
	 */
	TRACE("Location server: %s", LOCATION_SERVER);

	/* 
	 * 1. Set up TCP connection to web server "www.cdf.toronto.edu", 
	 *    port 80 
	 */
	int status;
	struct addrinfo hints;
	struct addrinfo *location_server_info;

	/* Make sure hints are empty */
	memset(&hints, 0, sizeof (struct addrinfo));

	/* Initialize hints */
	hints.ai_family = AF_INET; /* Only use IPv4 */
	hints.ai_socktype = SOCK_STREAM; /* TCP Connection */

	/* Get location server info */
	if((status = getaddrinfo(LOCATION_SERVER, "http", &hints, &location_server_info)) != 0) {
        perror("Error when getting location server info");
        return -1;
    }

	/* Initialize TCP Socket */
	locn_socket_fd = socket(location_server_info->ai_family,
	        location_server_info->ai_socktype,
	        location_server_info->ai_protocol);

	/* Connect to location server */
	if((status = connect(locn_socket_fd, location_server_info->ai_addr, location_server_info->ai_addrlen)) != 0) {
	    perror("Error when connecting to location server");
	    return -1;
	}

	/* The code you write should initialize locn_socket_fd so that
	 * it is valid for the write() in the next step.
	 */

	/* 2. write HTTP GET request to socket */

	char buf[MAX_MSG_LEN];
	memset(buf, 0, MAX_MSG_LEN);
	build_req(buf);
	buflen = strlen(buf);

	write(locn_socket_fd, buf, buflen);

	/* Clean up buffer before read */
	memset(buf, 0, MAX_MSG_LEN);

	/* 3. Read reply from web server */
	status = read(locn_socket_fd, buf, MAX_MSG_LEN);
	if(status <= 0) {
	    printf("Error when reading from location server.\n");
	    return -1;
	}

	/* Close socket */
	close(locn_socket_fd);

	/* 
	 * 4. Check if request succeeded.  If so, skip headers and initialize
	 *    server parameters with body of message.  If not, print the 
	 *    STATUS-CODE and STATUS-TEXT and return -1.
	 */

	/* Ignore version, read STATUS-CODE into variable 'code' , and record
	 * the number of characters scanned from buf into variable 'n'
	 */
	sscanf(buf, "%*s %d%n", &code, &n);

	if(code != 200) {
	    printf("HTTP request failed: %d\n", code);
	}

	char *body = skip_http_headers(buf);

	/* 5. Clean up after ourselves and return. */
	if(sscanf(body, "%s %d %d", chatserver_name, tcp_port, udp_port) < 3) {
	    printf("Error when parsing location server response\n");
	    return -1;
	}

	/* Output location server result */
	printf("Socket Server: %s, TCP Port: %d, UDP Port: %d\n", chatserver_name, *tcp_port, *udp_port);

	return 0;
}
