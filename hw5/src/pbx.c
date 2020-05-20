#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

//pbx struct containing the registry of clients and a mutex for it. It also contains a flag for notifications of shutdown
struct pbx {
    TU *tu_registry[PBX_MAX_EXTENSIONS];
    sem_t pbx_mutex;

    //variables for shutdown
    int shutdown_mode;
    sem_t pbx_shutdown_mutex;
};

/* tu struct containing an extension number, file descriptor for its
 * client connection, current state, and mutex
 */
struct tu {
    int extension_number;
    int file_descriptor;
    int current_state;
    int connected_tu_extension_number;
    sem_t tu_mutex;
};

/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init()
{
    //Allocating space on the heap for a pbx struct, and zeroing out data
    PBX *pbx_instance = Calloc(sizeof(struct pbx), sizeof(struct pbx));

    //Initializing the pbx mutex
    int sem_return_value = sem_init(&(pbx_instance->pbx_mutex), 0, 1);

    //Associating the global pbx instance with the allocated one
    pbx = pbx_instance;

    (pbx_instance->shutdown_mode) = 0;

    sem_init(&(pbx_instance->pbx_shutdown_mutex), 0, 0);

    //if an error has occured with initialization, return -1
    if(sem_return_value == -1)
    {
        return NULL;
    }

    //return pbx instance
    return pbx_instance;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed.  The PBX object itself is freed, and should not be used again.
 *
 * @param pbx  The PBX to be shut down.
 */
void pbx_shutdown(PBX *pbx)
{
    //Locking access to the pbx_mutex
    P(&(pbx->pbx_mutex));

    //setting shut_down_mode to 1
    (pbx->shutdown_mode) = 1;

    //number of TUs to shutdown
    int num_tus_to_shut = 0;

    //iterating through the registry to see which threads to terminate
    for(int i = 0; i < PBX_MAX_EXTENSIONS; i++)
    {
        //if tu_registry is still active, shut it down
        if((pbx->tu_registry)[i] != NULL)
        {
            //shutting down the connection only for reading
            shutdown(((pbx->tu_registry)[i])->file_descriptor, SHUT_RD);

            //incrementing num_tus_to_shut
            num_tus_to_shut++;
        }
    }

    //Unlocking access to the pbx_mutex
    V(&(pbx->pbx_mutex));

    //iterating through the registry to see which threads to terminate
    for(int i = 0; i < num_tus_to_shut; i++)
    {
        //blocking while waiting for tu to shutdown
        P(&(pbx->pbx_shutdown_mutex));
    }

    //Now free the pbx server
    Free(pbx);
}

/*
 * Register a TU client with a PBX.
 * This amounts to "plugging a telephone unit into the PBX".
 * The TU is assigned an extension number and it is initialized to the TU_ON_HOOK state.
 * A notification of the assigned extension number is sent to the underlying network
 * client.
 *
 * @param pbx  The PBX.
 * @param fd  File descriptor providing access to the underlying network client.
 * @return A TU object representing the client TU, if registration succeeds, otherwise NULL.
 * The caller is responsible for eventually calling pbx_unregister to free the TU object
 * that was returned.
 */
TU *pbx_register(PBX *pbx, int fd)
{
    //Error checking beforehand
    if(pbx == NULL)
    {
        return NULL;
    }

    //Lock pbx access to register one TU at a time
    P(&(pbx->pbx_mutex));

    //if shutdown mode is enabled, do not register anymore TUs or perform anymore actions of or between TUs
    if((pbx->shutdown_mode) == 1)
    {
        //unblock access to pbx
        V(&(pbx->pbx_mutex));
        return NULL;
    }

    //Array counter
    int i;

    //registry slot available
    int tu_slot_found = 0;

    //iterating through registry to find empty slot
    for(i = 0; i < PBX_MAX_EXTENSIONS; i++)
    {
        //if spot is equal to NULL, set flag and return
        if((pbx->tu_registry)[i] == NULL)
        {
            tu_slot_found = 1;
            break;
        }
    }

    //no TU slot was found so return NULL
    if(tu_slot_found != 1)
    {
        //Unlock pbx access before returning
        V(&(pbx->pbx_mutex));
        return NULL;
    }

    //Allocate space for a TU
    (pbx->tu_registry)[i] = Malloc(sizeof(struct tu));

    //Now set TU values
    ((pbx->tu_registry)[i])->extension_number = i;
    ((pbx->tu_registry)[i])->file_descriptor = fd;
    ((pbx->tu_registry)[i])->current_state = TU_ON_HOOK;

    //initializing the TU mutex
    int sem_return_value = sem_init(&(((pbx->tu_registry)[i])->tu_mutex), 0, 1);

    //Writing to the network connection that it is on a hook
    dprintf(fd, "%s %d\r\n", tu_state_names[TU_ON_HOOK], i);

    //Unlock pbx access
    V(&(pbx->pbx_mutex));

    //if sem_return_value is -1 return NULL
    if(sem_return_value == -1)
    {
        return NULL;
    }

    //return the TU in the registry
    return ((pbx->tu_registry)[i]);
}

/*
 * Unregister a TU from a PBX.
 * This amounts to "unplugging a telephone unit from the PBX".
 *
 * @param pbx  The PBX.
 * @param tu  The TU to be unregistered.
 * This object is freed as a result of the call and must not be used again.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu)
{
    //Lock pbx access to unregister one TU at a time, and allow no more receptions
    P(&(pbx->pbx_mutex));

    //If either is NULL return -1
    if((pbx == NULL) || (tu == NULL))
    {
        //Unlock pbx access before returning
        V(&(pbx->pbx_mutex));
        return -1;
    }

    //Set the TU pointer to NULL in the registry
    ((pbx->tu_registry)[(tu->extension_number)]) = NULL;

    //Free the allocated resources for the TU
    Free(tu);

    //if pbx shutdown mode is enabled, then unblock waiting master
    if((pbx->shutdown_mode) == 1)
    {
        //unblock shutdown mutex (waiting master)
        V(&(pbx->pbx_shutdown_mutex));
    }

    //Unlock pbx access
    V(&(pbx->pbx_mutex));

    //return success
    return 0;
}

/*
 * Get the file descriptor for the network connection underlying a TU.
 * This file descriptor should only be used by a server to read input from
 * the connection.  Output to the connection must only be performed within
 * the PBX functions.
 *
 * @param tu
 * @return the underlying file descriptor, if any, otherwise -1.
 */
int tu_fileno(TU *tu)
{
    //If tu is NULL return error
    if(tu == NULL)
    {
        return -1;
    }

    return tu->file_descriptor;
}

/*
 * Get the extension number for a TU.
 * This extension number is assigned by the PBX when a TU is registered
 * and it is used to identify a particular TU in calls to tu_dial().
 * The value returned might be the same as the value returned by tu_fileno(),
 * but is not necessarily so.
 *
 * @param tu
 * @return the extension number, if any, otherwise -1.
 */
int tu_extension(TU *tu)
{
    //If tu is NULL return error
    if(tu == NULL)
    {
        return -1;
    }

    return tu->extension_number;
}

/*
 * Take a TU receiver off-hook (i.e. pick up the handset).
 *
 *   If the TU was in the TU_ON_HOOK state, it goes to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RINGING state, it goes to the TU_CONNECTED state,
 *     reflecting an answered call.  In this case, the calling TU simultaneously
 *     also transitions to the TU_CONNECTED state.
 *   If the TU was in any other state, then it remains in that state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU. In addition, if the new state is TU_CONNECTED, then the
 * calling TU is also notified of its new state.
 *
 * @param tu  The TU that is to be taken off-hook.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_pickup(TU *tu)
{
    //Error checking beforehand
    if(tu == NULL)
    {
        return -1;
    }

    //Lock pbx access to avoid deadlocks
    P(&(pbx->pbx_mutex));

    //if shutdown mode is enabled, do not register anymore TUs or perform anymore actions of or between TUs
    if((pbx->shutdown_mode) == 1)
    {
        //unblock access to pbx
        V(&(pbx->pbx_mutex));
        return -1;
    }

    //Lock TU access to change state
    P(&(tu->tu_mutex));

    //if tu is ON_HOOK, switch to ON_DIAL, and send message
    if((tu->current_state) == TU_ON_HOOK)
    {
        //change TUs current state
        (tu->current_state) = TU_DIAL_TONE;

        //Writing to the network connection that it is on dial tone
        dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[TU_DIAL_TONE]);
    }
    //if tu was ringing, switch to connected
    else if((tu->current_state) == TU_RINGING)
    {
        //Block the connected to say that it is now connected
        P(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));

        //check if the TU has a connected TU and that it is in the RINGBACK state
        if(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state == TU_RING_BACK)
        {
            //Change state of connected TU
            (((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state) = TU_CONNECTED;

            //Writing to the connected TU network connection that it is now connected
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "%s %d\r\n", tu_state_names[TU_CONNECTED], (tu->extension_number));

            //change TUs current state
            (tu->current_state) = TU_CONNECTED;

            //Writing to the network connection that it is now connected
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[TU_CONNECTED], (tu->connected_tu_extension_number));
        }

        //Unblock connected extension
        V(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));
    }
    //else keep the state change the same and report that it is unchanged
    else
    {
        //Writing for the connected state which would be different
        if((tu->current_state) == TU_CONNECTED)
        {
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[(tu->current_state)], (tu->connected_tu_extension_number));
        }
        else
        {
            //Writing to the network connection of its current state
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[(tu->current_state)]);
        }
    }

    //Unlock TU access
    V(&(tu->tu_mutex));

    //Unlock pbx access to avoid deadlocks
    V(&(pbx->pbx_mutex));

    //return success
    return 0;
}

/*
 * Hang up a TU (i.e. replace the handset on the switchhook).
 *
 *   If the TU was in the TU_CONNECTED state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the peer TU (the one to which the call is currently
 *     connected) simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RING_BACK state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the calling TU (which is in the TU_RINGING state)
 *     simultaneously transitions to the TU_ON_HOOK state.
 *   If the TU was in the TU_RINGING state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the called TU (which is in the TU_RING_BACK state)
 *     simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_DIAL_TONE, TU_BUSY_SIGNAL, or TU_ERROR state,
 *     then it goes to the TU_ON_HOOK state.
 *   If the TU was in any other state, then there is no change of state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.  In addition, if the previous state was TU_CONNECTED,
 * TU_RING_BACK, or TU_RINGING, then the peer, called, or calling TU is also
 * notified of its new state.
 *
 * @param tu  The tu that is to be hung up.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_hangup(TU *tu)
{
    //Error checking beforehand
    if(tu == NULL)
    {
        return -1;
    }

    //Lock pbx access to avoid deadlocks
    P(&(pbx->pbx_mutex));

    //Lock TU access to change state
    P(&(tu->tu_mutex));

    //if tu was CONNECTED, switch to ON_HOOK
    if((tu->current_state) == TU_CONNECTED)
    {
        //Block the connected to say that it is now connected
        P(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));

        //check if the TU has a connected TU and that it is in the RINGBACK state
        if(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state == TU_CONNECTED)
        {
            //Change state of connected TU to DIAL_TONE
            (((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state) = TU_DIAL_TONE;

            //Writing to the connected TU network connection that it is now ON_DIAL
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "%s\r\n", tu_state_names[TU_DIAL_TONE]);

            //change TUs current state
            (tu->current_state) = TU_ON_HOOK;

            //only send my peer messages if I am in shutdown mode
            if((pbx->shutdown_mode) != 1)
            {
                //Writing to the network connection that it is now on hook
                dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[TU_ON_HOOK], (tu->extension_number));
            }
        }

        //Unblock connected extension
        V(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));
    }
    //else if tu was RING_BACK, switch to ON_HOOK, and change other other to ON_HOOK
    else if((tu->current_state) == TU_RING_BACK)
    {
        //Block the connected to say that it is now on_hook
        P(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));

        //check if the TU has a connected TU and that it is in the RINGING state
        if(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state == TU_RINGING)
        {
            //Change state of connected TU to ON_HOOK
            (((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state) = TU_ON_HOOK;

            //Writing to the connected TU network connection that it is now ON_HOOK
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "%s %d\r\n", tu_state_names[TU_ON_HOOK], (tu->connected_tu_extension_number));

            //change TUs current state
            (tu->current_state) = TU_ON_HOOK;

            //only send my peer messages if I am in shutdown moder
            if((pbx->shutdown_mode) != 1)
            {
                //Writing to the network connection that it is now on hook
                dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[TU_ON_HOOK], (tu->extension_number));
            }
        }

        //Unblock connected extension
        V(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));
    }
    //else if tu was RINGING, switch to ON_HOOK, and change other other to DIAL_TONE
    else if((tu->current_state) == TU_RINGING)
    {
        //Block the connected to say that it is now on_hook
        P(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));

        //check if the TU has a connected TU and that it is in the RINGBACK state
        if(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state == TU_RING_BACK)
        {
            //Change state of connected TU to ON_HOOK
            (((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state) = TU_DIAL_TONE;

            //Writing to the connected TU network connection that it is now ON_HOOK
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "%s\r\n", tu_state_names[TU_DIAL_TONE]);

            //change TUs current state
            (tu->current_state) = TU_ON_HOOK;

            //only send my peer messages if I am in shutdown moder
            if((pbx->shutdown_mode) != 1)
            {
                //Writing to the network connection that it is now on hook
                dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[TU_ON_HOOK], (tu->extension_number));
            }
        }

        //Unblock connected extension
        V(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));
    }
    //else if the TU was in any of these states, change them to ON_HOOK (do this as well if the current state is ON_HOOK)
    else if(((tu->current_state) == TU_DIAL_TONE) || ((tu->current_state) == TU_BUSY_SIGNAL) || ((tu->current_state) == TU_ERROR) || ((tu->current_state) == TU_ON_HOOK))
    {
        //change TUs current state
        (tu->current_state) = TU_ON_HOOK;

        //only send my peer messages if I am in shutdown moder
        if((pbx->shutdown_mode) != 1)
        {
            //Writing to the network connection that it is now on hook
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[TU_ON_HOOK], (tu->extension_number));
        }
    }
    //else keep the state change the same and report that it is unchanged
    else
    {
        //only send my peer messages if I am in shutdown moder
        if((pbx->shutdown_mode) != 1)
        {
            //Writing to the network connection that it is now connected
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[(tu->current_state)]);
        }
    }

    //Unlock TU access
    V(&(tu->tu_mutex));

    //Unlock pbx access to avoid deadlocks
    V(&(pbx->pbx_mutex));

    //return success
    return 0;
}

/*
 * Dial an extension on a TU.
 *
 *   If the specified extension number does not refer to any currently registered
 *     extension, then the TU transitions to the TU_ERROR state.
 *   Otherwise, if the TU was in the TU_DIAL_TONE state, then what happens depends
 *     on the current state of the dialed extension:
 *       If the dialed extension was in the TU_ON_HOOK state, then the calling TU
 *         transitions to the TU_RING_BACK state and the dialed TU simultaneously
 *         transitions to the TU_RINGING state.
 *       If the dialed extension was not in the TU_ON_HOOK state, then the calling
 *         TU transitions to the TU_BUSY_SIGNAL state and there is no change to the
 *         state of the dialed extension.
 *   If the TU was in any state other than TU_DIAL_TONE, then there is no state change.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.  In addition, if the new state is TU_RING_BACK, then the
 * called extension is also notified of its new state (i.e. TU_RINGING).
 *
 * @param tu  The tu on which the dialing operation is to be performed.
 * @param ext  The extension to be dialed.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_dial(TU *tu, int ext)
{
    //Error checking beforehand
    if((tu == NULL) || ((ext >= PBX_MAX_EXTENSIONS) || (ext < 0)))
    {
        return -1;
    }

    //Lock pbx access to make sure the pointer doesn't become NULL In the middle of the connection
    P(&(pbx->pbx_mutex));

    //if shutdown mode is enabled, do not register anymore TUs or perform anymore actions of or between TUs
    if((pbx->shutdown_mode) == 1)
    {
        //unblock access to pbx
        V(&(pbx->pbx_mutex));
        return -1;
    }

    //Lock TU access to change state
    P(&(tu->tu_mutex));

    //if tu was on Dial tone, switch to ring back
    if((tu->current_state) == TU_DIAL_TONE)
    {
        //if dialed TU does not exist, go to error state
        if((pbx->tu_registry)[ext] == NULL)
        {
            //change TUs current state
            (tu->current_state) = TU_ERROR;

            //Writing to the network connection of an error
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[TU_ERROR]);
        }
        //dialed TU extension is itself (for race condition)
        else if((tu->extension_number) == ext)
        {
            //change TUs current state
            (tu->current_state) = TU_BUSY_SIGNAL;

            //Writing to the network connection of busy signal to itself
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[TU_BUSY_SIGNAL]);
        }
        else
        {
            //Lock dialed TU to confirm state
            P(&(((pbx->tu_registry)[ext])->tu_mutex));

            //check if the dialed TU is in the ON_HOOK state
            if(((pbx->tu_registry)[ext])->current_state == TU_ON_HOOK)
            {
                //Change state of dialed TU
                (((pbx->tu_registry)[ext])->current_state) = TU_RINGING;

                //Writing to the connected TU network connection that it is now ringing
                dprintf((((pbx->tu_registry)[ext])->file_descriptor), "%s\r\n", tu_state_names[TU_RINGING]);

                //change TUs current state
                (tu->current_state) = TU_RING_BACK;

                //Writing to the network connection that it is now ringing back
                dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[TU_RING_BACK]);

                //now connecting the TUs by writing extension numbers on both TUs, first on the connected TU
                (((pbx->tu_registry)[ext])->connected_tu_extension_number) = (tu->extension_number);

                //then on the current TU
                (tu->connected_tu_extension_number) = ext;
            }
            //if the other one is not on the on_hook state, change the current tu to busy signal
            else
            {
                //change TUs current state to BUSY SIGNAL
                (tu->current_state) = TU_BUSY_SIGNAL;

                //Writing to the network connection that it is on busy signal
                dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[TU_BUSY_SIGNAL]);
            }

            //Unlock dialed TU
            V(&(((pbx->tu_registry)[ext])->tu_mutex));
        }
    }
    //else keep the state change the same and report that it is unchanged
    else
    {
        //if tu was on ON_HOOK, write a different message
        if((tu->current_state) == TU_ON_HOOK)
        {
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[(tu->current_state)], (tu->extension_number));
        }
        //Writing for the connected state which would be different
        else if((tu->current_state) == TU_CONNECTED)
        {
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[(tu->current_state)], (tu->connected_tu_extension_number));
        }
        //else keep the state the same and send the message
        else
        {
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[(tu->current_state)]);
        }
    }

    //Unlock TU access
    V(&(tu->tu_mutex));

    //Unlock pbx access to avoid deadlocks
    V(&(pbx->pbx_mutex));

    //return success
    return 0;
}

/*
 * "Chat" over a connection.
 *
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged and a notification containing
 * the current state is sent to the TU sending the chat.
 *
 * @param tu  The tu sending the chat.
 * @param msg  The message to be sent.
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg)
{
    //Error checking beforehand
    if((tu == NULL) || (msg == NULL))
    {
        return -1;
    }

    //Lock pbx to avoid deadlocks
    P(&(pbx->pbx_mutex));

    //if shutdown mode is enabled, do not register anymore TUs or perform anymore actions of or between TUs
    if((pbx->shutdown_mode) == 1)
    {
        //unblock access to pbx
        V(&(pbx->pbx_mutex));
        return -1;
    }

    //Lock TU access to change state
    P(&(tu->tu_mutex));

    //if the tu is in the connected state
    if((tu->current_state) == TU_CONNECTED)
    {
        //Block peer connection mutex to send messages correctly
        P(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));

        //check if the connected TU is in the connected state
        if(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->current_state == TU_CONNECTED)
        {
            //Writing the message passed message to the connected client
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "CHAT ");
            //loop counter
            int i = 0;
            //Use a loop to print out everything until the null terminator
            while((msg[i] != '\0') && (msg[i] != '\r'))
            {
                //print out each letter
                dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "%c", msg[i]);
                //increment counter
                i++;
            }
            //Finally print out the \r\n to the client
            dprintf((((pbx->tu_registry)[(tu->connected_tu_extension_number)])->file_descriptor), "\r\n");

            //Writing to the current tu that it is in the connected state
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[(tu->current_state)], (tu->connected_tu_extension_number));
        }

        //Unblock connected peer extension
        V(&(((pbx->tu_registry)[(tu->connected_tu_extension_number)])->tu_mutex));
    }
    //if the tu is not in the connected state, unlock mutex, send message, and return error
    else
    {
        //if tu was on ON_HOOK, write a different message
        if((tu->current_state) == TU_ON_HOOK)
        {
            dprintf((tu->file_descriptor), "%s %d\r\n", tu_state_names[(tu->current_state)], (tu->extension_number));
        }
        //else keep the state the same and send the message
        else
        {
            dprintf((tu->file_descriptor), "%s\r\n", tu_state_names[(tu->current_state)]);
        }

        //Unlock TU access
        V(&(tu->tu_mutex));

        //Unlock pbx to avoid deadlocks
        V(&(pbx->pbx_mutex));

        return -1;
    }

    //Unlock TU access
    V(&(tu->tu_mutex));

    //Unlock pbx to avoid deadlocks
    V(&(pbx->pbx_mutex));

    //return success
    return 0;
}