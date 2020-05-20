#include <stdlib.h>

#include "debug.h"
#include "polya.h"

#include "csapp.h"

#include <string.h>

//signal flags
sig_atomic_t volatile stop_flag = 0;
sig_atomic_t volatile terminate_flag = 0;

//Worker signal handler prototype
void worker_signal_handler(int sig);

/*
 * worker
 *
 * @brief  Main function for a worker process.
 * @details  Performs required initialization, then enters a main loop that
 * repeatedly receives from the master process a problem to be solved, attempts
 * to solve that problem, and returns the result of the solution attempt to
 * the master process.  Signals SIGSTOP and SIGCONT are used to stop and start
 * the worker process as described in the homework document.
 * If a SIGHUP signal is received by a worker while it is trying to solve a problem,
 * the worker process will abandon the current solution attempt, send a "failed" result
 * back to the master process, and stop, awaiting a new problem to be sent by the master.
 * If a SIGTERM signal is received by a worker, it will terminate normally with
 * exit status EXIT_SUCCESS.
 * @return EXIT_SUCCESS upon receipt of SIGTERM.
 */
int worker(void) {
    //Opening the file that will be read from
    FILE *problem_file = stdin;

    //Opening file that will be written to
    FILE *result_file = stdout;

    //Initialializing the signal handler to accept SIGHUP, SIGTERM, and SIGCONT signals
    Signal(SIGHUP, worker_signal_handler);
    Signal(SIGTERM, worker_signal_handler);
    Signal(SIGCONT, worker_signal_handler);

    //debug("[%d:Worker] sending SIGSTOP to itself-1", getpid());

    //Setting the stop flag to 1 and sending the SIGSTOP to itself
    stop_flag = 1;
    Kill(getpid(), SIGSTOP);

    while(terminate_flag == 0)
    {
        //Creating a temporary problem struct to hold the original block
        struct problem temp_problem;
        char *original_problem_storage = (char *)&temp_problem;

        //debug("[%d:Worker] reading struct problem part 1", getpid());

        //reading sizeof(struct problem) bytes and storing them into original problem storage
        for(int i = 0; i < sizeof(struct problem); i++)
        {
            int readbyte = fgetc(problem_file);
            if((readbyte == EOF) && (ferror(problem_file) != 0))
            {
                unix_error("File read error");
            }

            //storing and incrementing the byte
            (*original_problem_storage) = readbyte;
            original_problem_storage++;
        }

        //Resetting the original problem storage pointer back to temp problem
        original_problem_storage = (char *)&temp_problem;

        //calculating the remaining number of bytes to read from stdin
        int entire_problem_size = temp_problem.size;
        int remaining_bytes_to_read = temp_problem.size - sizeof(struct problem);

        //Calling malloc to allocate new space for the problem
        char *new_problem_storage = Malloc(entire_problem_size);
        struct problem *new_problem = (struct problem *)new_problem_storage;

        //Copying the contents of the old problem to the new problem
        for(int i = 0; i < sizeof(struct problem); i++)
        {
            (*new_problem_storage) = (*original_problem_storage);
            new_problem_storage++;
            original_problem_storage++;
        }

        //debug("[%d:Worker] reading struct problem part 2-- reading data of problem", getpid());

        //Acquiring the remaining bytes to be read in the file
        for(int i = 0; i < remaining_bytes_to_read; i++)
        {
            int readbyte = fgetc(problem_file);
            if((readbyte == EOF) && (ferror(problem_file) != 0))
            {
                unix_error("File read error");
            }

            //Storing and incrementing the new problem storage
            (*new_problem_storage) = readbyte;
            new_problem_storage++;
        }

        //debug("[%d:Worker] solving problem", getpid());

        //new_problem is the pointer to the new problem
        //Attempt to solve the problem and pass in SIGHUP SIGNAL flag
        struct result *problem_result = (solvers[(new_problem->type)].solve)(new_problem, &stop_flag);

        //debug("[%d:Worker] problem solved", getpid());

        //If problem_result is NULL, I malloc a new one
        if(problem_result == NULL)
        {
            //calling malloc to create new result if it is NULL
            problem_result = Malloc(sizeof(struct result));

            //Fixing up the result block
            (problem_result->size) = sizeof(struct result);
            (problem_result->id) = (new_problem->id);
            (problem_result->failed) = 1;
        }

        //Calculating the total number of bytes to write to stdout
        int entire_result_size = (problem_result->size);

        //casting it to a char pointer
        char *result_storage = (char *)problem_result;

        //debug("[%d:Worker] writing result to pipe to send to master", getpid());

        //Will only write to file in the SIGTERM SIGNAL was NOT sent and problem_result is not equal to NULL
        if(terminate_flag == 0)
        {
            //loop to write result structure to stdout file
            for(int i = 0; i < entire_result_size; i++)
            {
                int return_value_fputc = fputc((*result_storage), result_file);
                if(return_value_fputc == EOF)
                {
                    unix_error("File write error");
                }

                //Incrementing the result_storage byte pointer
                result_storage++;
            }

            //Flushing contents
            fflush(result_file);
        }

        //Free the result returned by the solver
        Free(problem_result);

        //Free the problem that was originally allocated by Malloc
        Free(new_problem);

        //debug("[%d:Worker] sending SIGSTOP to itself-22222222", getpid());

        //Setting the stop flag to 1 and sending the SIGSTOP to itself
        stop_flag = 1;
        Kill(getpid(), SIGSTOP);
    }

    //debug("[%d:Worker] terminating", getpid());

    //Closing the end of the pipes that the process is working with
    Close(STDIN_FILENO);
    Close(STDOUT_FILENO);

    //The process will exit successfully only if SIGTERM was sent
    if(terminate_flag == 1)
    {
        exit(EXIT_SUCCESS);
    }

    //else return with exit failure
    return EXIT_FAILURE;
}

//worker signal handler
void worker_signal_handler(int sig)
{
    if(sig == SIGHUP)
    {
        //debug("[%d:Worker] solution to be canceled", getpid());

        stop_flag = 1;
    }
    else if(sig == SIGCONT)
    {
        //debug("[%d:Worker] to continue", getpid());

        stop_flag = 0;
    }
    else if(sig == SIGTERM)
    {
        //debug("[%d:Worker] will terminate", getpid());

        //This sets both of the flags to 1
        stop_flag = 1;
        terminate_flag = 1;
    }
}