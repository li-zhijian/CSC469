/*
 *   CSC469 Fall 2010 A3
 *  
 *      File:      client_main.c 
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
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*************** GLOBAL VARIABLES ******************/

static char *option_string = "h:t:u:n:";

/* For communication with chat server */
/* These variables provide some extra clues about what you will need to
 * implement.
 */
char server_host_name[MAX_HOST_NAME_LEN];

/* For control messages */
u_int16_t server_tcp_port;
struct addrinfo *server_tcp_info;

/* For chat messages */
u_int16_t server_udp_port;
struct addrinfo *server_udp_info;
int udp_socket_fd;

/* Needed for REGISTER_REQUEST */
char member_name[MAX_MEMBER_NAME_LEN];
u_int16_t client_udp_port; 

/* Initialize with value returned in REGISTER_SUCC response */
u_int16_t member_id = 0;

/* For communication with receiver process */
pid_t receiver_pid;
char ctrl2rcvr_fname[MAX_FILE_NAME_LEN];
int ctrl2rcvr_qid;
int receiver_created = 0;

/* MAX_MSG_LEN is maximum size of a message, including header+body.
 * We define the maximum size of the msgdata field based on this.
 */
#define MAX_MSGDATA (MAX_MSG_LEN - sizeof(struct chat_msghdr))


/************* FUNCTION DEFINITIONS ***********/

static void usage(char **argv) {

    printf("usage:\n");

#ifdef USE_LOCN_SERVER
    printf("%s -n <client member name>\n",argv[0]);
#else 
    printf("%s -h <server host name> -t <server tcp port> -u <server udp port> -n <client member name>\n",argv[0]);
#endif /* USE_LOCN_SERVER */

    exit(1);
}

/* Function to be executed when control message received */
typedef void (*ControlAction)(char *, struct control_msghdr *);

void shutdown_clean() {
    /* Function to clean up after ourselves on exit, freeing any 
     * used resources 
     */
    if(server_tcp_info != NULL) freeaddrinfo(server_tcp_info);
    if(server_udp_info != NULL) freeaddrinfo(server_udp_info);

    /* Add to this function to clean up any additional resources that you
     * might allocate.
     */

    msg_t msg;

    /* 1. Send message to receiver to quit */
    msg.mtype = RECV_TYPE;
    msg.body.status = CHAT_QUIT;
    msgsnd(ctrl2rcvr_qid, &msg, sizeof(struct body_s), 0);

    /* 2. Close open fd's */
    close(udp_socket_fd);

    /* 3. Wait for receiver to exit */
    waitpid(receiver_pid, 0, 0);

    /* 4. Destroy message channel */
    unlink(ctrl2rcvr_fname);
    if (msgctl(ctrl2rcvr_qid, IPC_RMID, NULL)) {
        perror("cleanup - msgctl removal failed");
    }

    exit(0);
}



int initialize_client_only_channel(int *qid)
{
    /* Create IPC message queue for communication with receiver process */

    int msg_fd;
    int msg_key;

    /* 1. Create file for message channels */

    snprintf(ctrl2rcvr_fname,MAX_FILE_NAME_LEN,"/tmp/ctrl2rcvr_channel.XXXXXX");
    msg_fd = mkstemp(ctrl2rcvr_fname);

    if (msg_fd  < 0) {
        perror("Could not create file for communication channel");
        return -1;
    }

    close(msg_fd);

    /* 2. Create message channel... if it already exists, delete it and try again */

    msg_key = ftok(ctrl2rcvr_fname, 42);

    if ( (*qid = msgget(msg_key, IPC_CREAT|IPC_EXCL|S_IREAD|S_IWRITE)) < 0) {
        if (errno == EEXIST) {
            if ( (*qid = msgget(msg_key, S_IREAD|S_IWRITE)) < 0) {
                perror("First try said queue existed. Second try can't get it");
                unlink(ctrl2rcvr_fname);
                return -1;
            }
            if (msgctl(*qid, IPC_RMID, NULL)) {
                perror("msgctl removal failed. Giving up");
                unlink(ctrl2rcvr_fname);
                return -1;
            } else {
                /* Removed... try opening again */
                if ( (*qid = msgget(msg_key, IPC_CREAT|IPC_EXCL|S_IREAD|S_IWRITE)) < 0) {
                    perror("Removed queue, but create still fails. Giving up");
                    unlink(ctrl2rcvr_fname);
                    return -1;
                }
            }

        } else {
            perror("Could not create message queue for client control <--> receiver");
            unlink(ctrl2rcvr_fname);
            return -1;
        }
    
    }

    return 0;
}



int create_receiver()
{
    /* Only create receiver once */
    if(receiver_created) {
        TRACE("Receiver already created");
        return 0;
    }

    /* Create the receiver process using fork/exec and get the port number
     * that it is receiving chat messages on.
     */

    int retries = 20;
    int numtries = 0;

    /* 1. Set up message channel for use by control and receiver processes */

    if (initialize_client_only_channel(&ctrl2rcvr_qid) < 0) {
        return -1;
    }

    /* 2. fork/exec xterm */

    receiver_pid = fork();

    if (receiver_pid < 0) {
        fprintf(stderr,"Could not fork child for receiver\n");
        return -1;
    }

    if ( receiver_pid == 0) {
        /* this is the child. Exec receiver */
        char *argv[] = {"xterm",
                "-e",
                "./receiver",
                "-f",
                ctrl2rcvr_fname,
                0
        };

        execvp("xterm", argv);
        printf("Child: exec returned. that can't be good.\n");
        exit(1);
    } 

    /* This is the parent */

    /* 3. Read message queue and find out what port client receiver is using */

    while ( numtries < retries ) {
        int result;
        msg_t msg;
        result = msgrcv(ctrl2rcvr_qid, &msg, sizeof(struct body_s), CTRL_TYPE, IPC_NOWAIT);
        if (result == -1 && errno == ENOMSG) {
            usleep(500 * 1000); /* Sleep 500 milliseconds and retry */
            numtries++;
        } else if (result > 0) {
            if (msg.body.status == RECV_READY) {
                printf("Start of receiver successful, port %u\n",msg.body.value);
                client_udp_port = msg.body.value;
            } else {
                printf("start of receiver failed with code %u\n",msg.body.value);
                return -1;
            }
            break;
        } else {
            perror("msgrcv");
        }
    
    }

    if (numtries == retries) {
        /* give up.  wait for receiver to exit so we get an exit code at least */
        int exitcode;
        printf("Gave up waiting for msg.  Waiting for receiver to exit now\n");
        waitpid(receiver_pid, &exitcode, 0);
        printf("start of receiver failed, exited with code %d\n",exitcode);
    }

    /* Mark receiver as created */
    receiver_created = 1;

    return 0;
}


/*********************************************************************/

/* We define one handle_XXX_req() function for each type of 
 * control message request from the chat client to the chat server.
 * These functions should return 0 on success, and a negative number 
 * on error.
 */

/**
 * Send a control message to server with specified data. Returns the number
 * of bytes read from the server and puts them in result.
 *
 * Note: char *result must be of at least MAX_MSG_LEN
 */
int send_control_msg(uint16_t msg_type, char *data, uint16_t data_len, char *result) {
    int status;

    /* Initialize buffer */
    char buf[MAX_MSG_LEN];
    memset(buf, 0, MAX_MSG_LEN);

    /* Initialize TCP socket */
    int tcp_socket_fd = socket(server_tcp_info->ai_family, server_tcp_info->ai_socktype, server_tcp_info->ai_protocol);

    /* Connect to server */
    if((status = connect(tcp_socket_fd, server_tcp_info->ai_addr, server_tcp_info->ai_addrlen)) != 0) {
        perror("Error when connecting to chat server");
        return -1;
    }

    /* Initialize headr */
    struct control_msghdr *hdr = (struct control_msghdr *) buf;
    /* DO NOT convert byte order for this value, server uses it directly */
    hdr->msg_type = msg_type;
    hdr->member_id = member_id;
    hdr->msg_len = sizeof (struct control_msghdr) + data_len;

    /* Copy over data to packet */
    memcpy(hdr->msgdata, data, data_len);

    /* Send packet to server */
    send(tcp_socket_fd, buf, hdr->msg_len, 0);

    /* Clear return buffer */
    memset(result, 0, MAX_MSG_LEN);

    /* Read result from server */
    status = recv(tcp_socket_fd, result, MAX_MSG_LEN, 0);

    /* Close TCP Connection to server */
    close(tcp_socket_fd);

    return status;
}

int handle_register_req()
{
    /* Initialize buffer */
    char buf[MAX_MSG_LEN], res[MAX_MSG_LEN];
    memset(buf, 0, MAX_MSG_LEN);

    struct register_msgdata *data = (struct register_msgdata *) buf;
    /* DO convert this value as it is used directly in network structs */
    data->udp_port = htons(client_udp_port);
    strcpy((char *) data->member_name, member_name);

    /* Calculate length */
    uint16_t data_len = sizeof (struct register_msgdata) + strlen(member_name) + 1;

    if(send_control_msg(REGISTER_REQUEST, buf, data_len, res) <= 0) {
        return NETWORK_ERROR;
    }

    struct control_msghdr *hdr = (struct control_msghdr *) res;

    if(hdr->msg_type == REGISTER_SUCC) {
        printf("Successfully registered with server.");
        member_id = hdr->member_id;
    } else {
        printf("Could not register with server: %s", (char *) hdr->msgdata);
        return INVALID_REQUEST;
    }

    return 0;
}

int handle_room_list_req()
{
    char res[MAX_MSG_LEN];

    if(send_control_msg(ROOM_LIST_REQUEST, NULL, 0, res) <= 0) {
        return NETWORK_ERROR;
    }

    struct control_msghdr *hdr = (struct control_msghdr *) res;

    if(hdr->msg_type == ROOM_LIST_SUCC) {
        printf("%s", (char *) hdr->msgdata);
    } else {
        printf("Room list request failed: %s", (char *) hdr->msgdata);
        return INVALID_REQUEST;
    }

    return 0;
}

int handle_room_request(char *room_name, uint16_t msg_type,
        uint16_t succ_type,
        ControlAction succ, ControlAction fail) {
    char res[MAX_MSG_LEN];

    if(send_control_msg(msg_type,
            room_name, strlen(room_name) + 1, res) <= 0) {
        return NETWORK_ERROR;
    }

    struct control_msghdr *hdr = (struct control_msghdr *) res;

    if(hdr->msg_type == succ_type) {
        succ(room_name, hdr);
    } else {
        if(fail) {
            fail(room_name, hdr);
        } else {
            printf("Request failed: %s", (char *) hdr->msgdata);
        }

        return INVALID_REQUEST;
    }

    return 0;
}

void member_list_succ(char *room_name, struct control_msghdr *hdr) {
    printf("Members in room %s: %s", room_name, (char *) hdr->msgdata);
}

int handle_member_list_req(char *room_name) {
    return handle_room_request(room_name, MEMBER_LIST_REQUEST, MEMBER_LIST_SUCC, member_list_succ, NULL);
}

void switch_room_succ(char *room_name, struct control_msghdr *hdr) {
    printf("Switched to room: %s", room_name);
}

int handle_switch_room_req(char *room_name) {
    return handle_room_request(room_name, SWITCH_ROOM_REQUEST, SWITCH_ROOM_SUCC, switch_room_succ, NULL);
}

void create_room_succ(char *room_name, struct control_msghdr *hdr) {
    printf("Created room: %s", room_name);
}

int handle_create_room_req(char *room_name) {
    return handle_room_request(room_name, CREATE_ROOM_REQUEST, CREATE_ROOM_SUCC, create_room_succ, NULL);
}

int handle_heartbeat_req() {
    char res[MAX_MSG_LEN];

    /**
     * Send a heartbeat to the server at a fixed interval to make sure
     * we can still connect and communicate.
     */
    if(send_control_msg(MEMBER_KEEP_ALIVE, NULL, 0, res) < 0) {
        return NETWORK_ERROR;
    }

    /* Heartbeat successful */
    return 0;
}


int handle_quit_req()
{
    /* Send quit message to server */
    char res[MAX_MSG_LEN];
    send_control_msg(QUIT_REQUEST, NULL, 0, res);

    /* Don't care about return value from server, go ahead and quit */
    shutdown_clean();
    return 0;
}


int init_client()
{
    /* Initialize client so that it is ready to start exchanging messages
     * with the chat server.
     */
    int status;
    struct addrinfo tcp_hints, udp_hints;

    /* Make sure hints structs are empty */
    memset(&tcp_hints, 0, sizeof (tcp_hints));
    memset(&udp_hints, 0, sizeof (udp_hints));

    /* Initialize TCP Hints */
    tcp_hints.ai_family = AF_INET; /* Only communicate over IPv4 */
    tcp_hints.ai_socktype = SOCK_STREAM;

    /* Initialize UDP Hints */
    udp_hints.ai_family = AF_INET; /* Only communicate over IPv4 */
    udp_hints.ai_socktype = SOCK_DGRAM;

#ifdef USE_LOCN_SERVER

    /* 0. Get server host name, port numbers from location server.
     *    See retrieve_chatserver_info() in client_util.c
     */
    if(retrieve_chatserver_info(server_host_name, &server_tcp_port, &server_udp_port) == -1) {
        return NETWORK_ERROR;
    }

#endif
 
    /* 1. initialization to allow TCP-based control messages to chat server */
    char tcp_port[MAX_HOST_NAME_LEN];
    sprintf(tcp_port, "%d", server_tcp_port);

    /* Get TCP server info */
    if((status = getaddrinfo(server_host_name, tcp_port, &tcp_hints, &server_tcp_info)) != 0) {
        perror("Error when getting TCP server info");
        return NETWORK_ERROR;
    }

    /* 2. initialization to allow UDP-based chat messages to chat server */
    char udp_port[MAX_HOST_NAME_LEN];
    sprintf(udp_port, "%d", server_udp_port);

    /* Get UDP server info */
    if((status = getaddrinfo(server_host_name, udp_port, &udp_hints, &server_udp_info)) != 0) {
        perror("Error when getting UDP server info");
        return NETWORK_ERROR;
    }

    /* Initialize UDP socket */
    udp_socket_fd = socket(server_udp_info->ai_family, server_udp_info->ai_socktype, server_udp_info->ai_protocol);

    /* 3. spawn receiver process - see create_receiver() in this file. */

    if((status = create_receiver()) != 0) {
        return NETWORK_ERROR;
    }

    /* 4. register with chat server */
    if((status = handle_register_req()) != 0) {
        return status;
    }

    return status;
}



void handle_chatmsg_input(char *inputdata)
{
    /* inputdata is a pointer to the message that the user typed in.
     * This function should package it into the msgdata field of a chat_msghdr
     * struct and send the chat message to the chat server.
     */

    char buf[MAX_MSG_LEN];
    memset(buf, 0, MAX_MSG_LEN);

    struct chat_msghdr *hdr = (struct chat_msghdr *) buf;

    /* Initialize header */
    hdr->sender.member_id = member_id;
    hdr->msg_len = sizeof (struct chat_msghdr) + strlen(inputdata) + 1;
    strcpy((char *) hdr->msgdata, inputdata);

    /* Send packet to server */
    sendto(udp_socket_fd, buf, hdr->msg_len, 0, server_udp_info->ai_addr, server_udp_info->ai_addrlen);
}


/* This should be called with the leading "!" stripped off the original
 * input line.
 * 
 * Returns 0 if successful otherwise error code.
 *
 */
int handle_command_input(char *line)
{
    char cmd = line[0]; /* single character identifying which command */
    int len = 0;
    int goodlen = 0;
    int result = 0;

    line++; /* skip cmd char */

    /* 1. Simple format check */

    switch(cmd) {

    case 'r':
    case 'q':
        if (strlen(line) != 0) {
            printf("Error in command format: !%c should not be followed by anything.\n",cmd);
            return INVALID_COMMAND;
        }
        break;

    case 'c':
    case 'm':
    case 's':
        {
            int allowed_len = MAX_ROOM_NAME_LEN;

            if (line[0] != ' ') {
                printf("Error in command format: !%c should be followed by a space and a room name.\n",cmd);
                return INVALID_COMMAND;
            }
            line++; /* skip space before room name */

            len = strlen(line);
            goodlen = strcspn(line, " \t\n"); /* Any more whitespace in line? */
            if (len != goodlen) {
                printf("Error in command format: line contains extra whitespace (space, tab or carriage return)\n");
                return INVALID_COMMAND;
            }
            if (len > allowed_len) {
                printf("Error in command format: name must not exceed %d characters.\n",allowed_len);
                return INVALID_COMMAND;
            }
        }
        break;

    default:
        printf("Error: unrecognized command !%c\n",cmd);
        return INVALID_COMMAND;
        break;
    }

    /* 2. Passed format checks.  Handle the command */

    switch(cmd) {

    case 'r':
        result = handle_room_list_req();
        break;

    case 'c':
        result = handle_create_room_req(line);
        break;

    case 'm':
        result = handle_member_list_req(line);
        break;

    case 's':
        result = handle_switch_room_req(line);
        break;

    case 'q':
        result = handle_quit_req(); // does not return. Exits.
        break;

    default:
        printf("Error !%c is not a recognized command.\n",cmd);
        break;
    }

    /* Currently, we ignore the result of command handling.
     * You may want to change that.
     */

    return result;
}

void reconnect_client() {
    int status = 0;

    do {
        printf("\nReestablishing connection to server...\n");

        status = init_client();

        if(status == NETWORK_ERROR) {
            printf("Waiting 5 seconds before next retry\n");
            sleep(5);
        } else if(status == INVALID_REQUEST) {
            printf("Quitting because name already taken on new server\n");
            shutdown_clean();
        }
    } while (status != 0);
}

void get_user_input()
{
    char buf[MAX_MSGDATA];
    char *result_str;

    /* Set up file descriptor sets to monitor */
    fd_set rset;
    fd_set allset;

    /* Initialize sets */
    FD_ZERO(&allset);
    FD_ZERO(&rset);
    FD_SET(STDIN_FILENO, &allset);

    /* Disable buffering on stdout */
    setbuf(stdout, NULL);

    /* Print out initial Prompt */
    printf("\n[%s]>  ", member_name);

    while(TRUE) {
        /* Initialize timeout */
        struct timeval tv;
        tv.tv_sec = 5; /* 5 Seconds */
        tv.tv_usec = 0;

        /* Get input or timeout */
        rset = allset;
        if(select(STDIN_FILENO + 1, &rset, NULL, NULL, &tv) == -1) {
            perror("select");
            return;
        }

        if(FD_ISSET(STDIN_FILENO, &rset)) {
            /* Clear buffer */
            memset(buf, 0, MAX_MSGDATA);

            result_str = fgets(buf,MAX_MSGDATA,stdin);

            if (result_str == NULL) {
                printf("Error or EOF while reading user input.  Guess we're done.\n");
                break;
            }

            /* Check if control message or chat message */

            if (buf[0] == '!') {
                /* buf probably ends with newline.  If so, get rid of it. */
                int len = strlen(buf);
                if (buf[len-1] == '\n') {
                    buf[len-1] = '\0';
                }

                if(handle_command_input(&buf[1]) == NETWORK_ERROR) {
                    reconnect_client();
                }
            } else {
                handle_chatmsg_input(buf);
            }

            /* Print out prompt */
            printf("\n[%s]>  ", member_name);
        } else {
            /* Timed out, send heartbeat */
            if(handle_heartbeat_req() == NETWORK_ERROR) {
                reconnect_client();

                /* Print out prompt */
                printf("\n[%s]>  ", member_name);
            }
        }
    }
}


int main(int argc, char **argv)
{
    char option;
 
    while((option = getopt(argc, argv, option_string)) != -1) {
        switch(option) {
        case 'h':
            strncpy(server_host_name, optarg, MAX_HOST_NAME_LEN);
            break;
        case 't':
            server_tcp_port = atoi(optarg);
            break;
        case 'u':
            server_udp_port = atoi(optarg);
            break;
        case 'n':
            strncpy(member_name, optarg, MAX_MEMBER_NAME_LEN);
            break;
        default:
            printf("invalid option %c\n",option);
            usage(argv);
            break;
        }
    }

#ifdef USE_LOCN_SERVER

    printf("Using location server to retrieve chatserver information\n");

    if (strlen(member_name) == 0) {
        usage(argv);
    }

#else

    if(server_tcp_port == 0 || server_udp_port == 0 ||
       strlen(server_host_name) == 0 || strlen(member_name) == 0) {
        usage(argv);
    }

#endif /* USE_LOCN_SERVER */

    if(init_client() < 0) {
        if(receiver_created) {
            shutdown_clean();
        }

        exit(-1);
    }

    get_user_input();

    return 0;
}
