#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

int main_listenfd;

static void terminate(int status);

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    //flag for checking p option
    int required_p_option = 0;
    char *port = NULL;

    int option = 0;

    //getopt() while loop for parsing command line arguments
    while ((option = getopt(argc, argv, "p:")) != -1)
    {
        switch(option)
        {
            //check case p
            case 'p':

                //set required_p_option to found
                required_p_option = 1;

                //set port to the argument that was passed in, increment, and break
                port = optarg;
                optarg++;
                break;
        }
    }

    //if p flag was not found, print usage and exit failure
    if(required_p_option != 1)
    {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Perform required initialization of the PBX module.
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    //Setting up the passive listening file descriptor
    int listenfd = Open_listenfd(port);

    //setting global file descriptor to the one acquired by main_listenfd
    main_listenfd = listenfd;

    //Installation of SIGHUP handler (terminate)
    Signal(SIGHUP, terminate);

    //Ignore SIGPIPE to prevent disruptions
    Signal(SIGPIPE, SIG_IGN);

    //connection fd, and arguments for accept method
    int connfd = 0;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //loop to accept connections on socket
    while(1)
    {
        //creating argument for clientlen
        clientlen = sizeof(struct sockaddr_storage);

        //calling Accept method for file descriptor
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        //creating the variable for the thread_id, which I will not use
        pthread_t temp_thread_id;

        //Allocating storage for the file descriptor
        int *heap_fd_storage = Malloc(sizeof(int));

        //Storing file descriptor in the heap
        (*heap_fd_storage) = connfd;

        //creating the heap, passing in the method and the arguments
        Pthread_create(&temp_thread_id, NULL, pbx_client_service, heap_fd_storage);
    }

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {

    //Shutting down pbx server
    pbx_shutdown(pbx);

    //Closing server file descriptor to close port
    Close(main_listenfd);

    //Exiting with the given status
    exit(status);
}