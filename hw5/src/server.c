#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

//trim function for chat option
char *trim_string(char *str);

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This variable must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives messages from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  The service loop ends when the network connection
 * shuts down and EOF is seen.  This could occur either as a result of the
 * client explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *pbx_client_service(void *arg)
{
    //Acquiring the file descriptor that has been allocated
    int *arg_ptr = arg;
    int clientfd = (*arg_ptr);

    //Freeing the pointer that has been retrieved
    Free(arg);

    //Detaching thread, so that it does not need to be reaped
    Pthread_detach(pthread_self());

    //Registering client file descriptor with PBX module
    TU *registered_TU = pbx_register(pbx, clientfd);

    //if the tu cannot be registed, just end the session
    if(registered_TU == NULL)
    {
        //close connection
        shutdown(clientfd, SHUT_RDWR);

        //free associated file descriptor
        Close(clientfd);
        return NULL;
    }

    //File that will be used to re bytes from
    FILE *client_file = fdopen(clientfd, "w+");

    //Flag recognizing that EOF has been sent (connection terminated)
    int eof_sent = 0;

    //service loop
    while(1)
    {
        //Original storage for the client's message
        char *client_message = Malloc(1);

        //Flag recognizing that '\r' character was read over the connection
        int carrige_sent = 0;

        //Original message size
        int message_size = 1;

        //Iteration counter
        int iteration = 0;

        //Loop to read read (consume message from file descriptor)
        while(1)
        {
            int readbyte = fgetc(client_file);

            //If connection has terminated, "EOF" has been sent, so mark flag
            if(readbyte == EOF)
            {
                //Mark that the EOF flag has been sent and break reading loop
                eof_sent = 1;
                break;
            }

            //If '\r' was read, set the carrige flag
            if(readbyte == '\r')
            {
                carrige_sent = 1;
            }
            //'\r' was the byte previously read, and '\n' was read so message complete
            else if((readbyte == '\n') && (carrige_sent == 1))
            {
                break;
            }
            //If none of these conditions are true, set carrige flag to 0
            else
            {
                carrige_sent = 0;
            }

            //Reallocate more space for byte
            client_message = Realloc(client_message, message_size);

            //storing the byte in the original address plus iteration count
            *(client_message + iteration) = readbyte;

            //Increment the message size and reallocate new space for the newly read byte
            message_size++;

            //Increment iteration
            iteration++;
        }

        //If EOF was sent, break the connection and free message storage
        if(eof_sent == 1)
        {
            Free(client_message);
            break;
        }

        //Creating buffer to store the sent message ('\r'should be character at index message_size)
        char message_buffer[iteration];

        //Copy the memory from the heap, last '\r' character does not need to be read
        strncpy(message_buffer, client_message, (iteration - 1));

        //Now free the allocated message storage
        Free(client_message);

        //Place a null terminator at end of the string
        message_buffer[iteration - 1] = '\0';

        //Check case pickup
        if(message_buffer[0] == 'p')
        {
            //Check string length and compare string to array string
            if((strlen(message_buffer) == 6) && (strcmp(message_buffer, tu_command_names[TU_PICKUP_CMD]) == 0))
            {
                //If so, call pickup
                tu_pickup(registered_TU);
            }
        }

        //Check case hangup
        if(message_buffer[0] == 'h')
        {
            //Check string length and compare string to array string
            if((strlen(message_buffer) == 6) && (strcmp(message_buffer, tu_command_names[TU_HANGUP_CMD]) == 0))
            {
                //If so, call hangup
                tu_hangup(registered_TU);
            }
        }

        //Check case dial
        if(message_buffer[0] == 'd')
        {
            //Check string length and compare string to array string
            if(strncmp(message_buffer, tu_command_names[TU_DIAL_CMD], 4) == 0)
            {
                //Perform operation only if message buffer length is greater than 5
                if(strlen(message_buffer) > 5)
                {
                    //using safe strtol function to acquire extension number
                    int dial_number = (int)strtol((message_buffer + 5), NULL, 10);
                    if(errno != EINVAL)
                    {
                        //If so, call dial
                        tu_dial(registered_TU, dial_number);
                    }
                }
            }
        }

        //Check case chat
        if(message_buffer[0] == 'c')
        {
            //Check string length and compare string to array string
            if(strncmp(message_buffer, tu_command_names[TU_CHAT_CMD], 4) == 0)
            {
                //using trim_string to send a trimmed message
                char *chat_message = trim_string(message_buffer + 4);

                //send chat message
                tu_chat(registered_TU, chat_message);
            }
        }
    }

    //Hanging up the TU after the connection has been terminated
    tu_hangup(registered_TU);

    //closing the file (to free up space)
    fclose(client_file);

    //unregister the TU, freeing its resources
    pbx_unregister(pbx, registered_TU);

    //return NULL
    return NULL;
}

//trim function for chat option
char *trim_string(char *str)
{
    //end of string pointer
    char *str_end;

    //increment string pointer while == ' '
    while((*str) == ' ')
    {
        str++;
    }

    //locate end of string
    str_end = str + strlen(str) - 1;

    ////decrement string pointer while == ' '
    while ((str_end > str) && ((*str_end) == ' '))
    {
        str_end--;
    }

    //putting a null terminator at the end
    *(str_end + 1) = '\0';
    return str;
}