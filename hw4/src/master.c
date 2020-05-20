#include <stdlib.h>

#include "debug.h"
#include "polya.h"

#include "csapp.h"

//Array used to store the current state of the workers
sig_atomic_t volatile worker_state[MAX_WORKERS];

//Array used to store PIDs of worker proccesses
pid_t worker_pids[MAX_WORKERS];

//Flag to express that there are idle workers available
sig_atomic_t volatile are_idle_workers;

//Flag to express that there are stopped workers that where the result could be read from
sig_atomic_t volatile are_stopped_workers;

//The master signal handler
void master_signal_handler(int sig);

//Function that returns the index in the array of worker pids of the given pid
int get_pid_index(pid_t worker_pid);

/*
 * master
 *
 * @brief  Main function for the master process.
 * @details  Performs required initialization, creates a specified number of
 * worker processes and enters a main loop that repeatedly assigns problems
 * to workers and posts results received from workers, until all of the worker
 * processes are idle and a NULL return from get_problem_variant indicates that
 * there are no further problems to be solved.  Once this occurs, each worker
 * process is sent a SIGTERM signal, which they should handle by terminating
 * normally.  When all worker processes have terminated, the master process
 * itself terminates.
 * @param workers  If positive, then the number of worker processes to create.
 * Otherwise, the default of one worker process is used.
 * @return  A value to be returned as the exit status of the master process.
 * This value should be EXIT_SUCCESS if all worker processes have terminated
 * normally, otherwise EXIT_FAILURE.
 */
int master(int workers) {
    //Master process is starting execution, so use this function
    sf_start();

    //installing the master signal handler to handle SIGCHLD signals
    Signal(SIGCHLD, master_signal_handler);

    //Array used to store PIDs of worker proccesses. (This array will only be used by the main master process)
    pid_t local_worker_pids[workers];

    //Write to pipes
    int fd_write_to[workers][2];

    //The files that the main process will write to
    FILE *write_to_file[workers];

    //Read from pipes
    int fd_read_from[workers][2];

    //The files that the main process will read from
    FILE *read_from_file[workers];

    //Creating a bit vector mask and saving the previous one
    sigset_t mask, prev_mask1, prev_mask2;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGPIPE);

    /* Block SIGPIPE and save previous blocked set */
    Sigprocmask(SIG_BLOCK, &mask, &prev_mask1);

    //Initializing the worker processes
    for(int i = 0; i < workers; i++)
    {
        //Processing the fds that the main process will write to
        if(pipe(fd_write_to[i]) < 0)
        {
            unix_error("Pipe error");
        }

        //Processing the fds that the main process will read from
        if(pipe(fd_read_from[i]) < 0)
        {
            unix_error("Pipe error");
        }

        //Here I want to block the signal handler because I am using the global array with worker pids that is used by signal handler
        //Adding the SIGCHLD signal to the set of desired signals to be blocked
        Sigaddset(&mask, SIGCHLD);
        //Block SIGCHLD and save previous blocked set
        Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

        //Forking and creating the new process
        pid_t temp_pid = Fork();

        //Child process
        if(temp_pid == 0)
        {
            //Unblock SIGCHLD (IN THE CHILD PROCESS)
            Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
            //Removing the block of the SIGCHLD signal from the current mask
            Sigdelset(&mask, SIGCHLD);

            //Changing stdin of the child process to read from what parent will write to
            Dup2(fd_write_to[i][0], 0);

            //Changing stdin of the child process to write to what parent will read from
            Dup2(fd_read_from[i][1], 1);

            //closing the corresponding end of the pipe that the child process will not write to
            if(close(fd_write_to[i][1]) < 0)
            {
                unix_error("Pipe error");
            }

            //closing the corresponding end of the pipe that the child process will not read from
            if(close(fd_read_from[i][0]) < 0)
            {
                unix_error("Pipe error");
            }

            //Starting the new program and setiing args and envp to NULL
            Execve("bin/polya_worker", NULL, NULL);
        }
        //Parent process, which is storing child PID into array
        else
        {
            //Store the worker process's PID in the pid array
            worker_pids[i] = temp_pid;

            //Store the worker process's PID in the pid array that will only be used by the main master process
            local_worker_pids[i] = temp_pid;

            //State the state change of the worker to worker started
            worker_state[i] = WORKER_STARTED;

            //debug("[%d:Master] Changing state of worker %d from WORKER_INIT to WORKER_STARTED", getpid(), i);

            //Report the change of state in the child process
            sf_change_state(local_worker_pids[i], 0, WORKER_STARTED);

            //Wrap the file descriptors in files so that they could be read from by the main process
            //Files that will be written to
            write_to_file[i] = Fdopen(fd_write_to[i][1], "w");

            //Files that will be read from
            read_from_file[i] = Fdopen(fd_read_from[i][0], "r");
        }

        //Unblock SIGCHLD (IN THE PARENT PROCESS)
        Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
        //Removing the block of the SIGCHLD signal from the current mask
        Sigdelset(&mask, SIGCHLD);
    }

    //Flag to specify if no more problems are to be returned
    int no_more_problems = 0;

    //Flag to specify that all workers are idle
    int all_workers_idle = 0;

    //problem storage for the current problem that the worker is solving
    struct problem *worker_problem_storage[workers];

    //The "main loop" that will keep running until there are no more problems and all workers are idle
    while((no_more_problems == 0) || (all_workers_idle == 0))
    {
        //The pause that will occur until a SIGCHLD is sent
        while((are_stopped_workers == 0) && (are_idle_workers == 0))
        {
            Sigsuspend(&mask);
        }

        //Adding the SIGCHLD signal to the set of desired signals to be blocked
        Sigaddset(&mask, SIGCHLD);
        //Block SIGCHLD and save previous blocked set
        Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

        //Array of current worker states
        int temp_worker_state[workers];

        //debug("[%d:Master] Populating local state array ", getpid());

        //populating temporary state array with the current worker states
        for(int i = 0; i < workers; i++)
        {
            temp_worker_state[i] = worker_state[i];
        }

        //Setting the flags that tell me if I have idle to 0 because I have recorded it
        are_idle_workers = 0;

        //Unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
        //Removing the block of the SIGCHLD signal from the current mask
        Sigdelset(&mask, SIGCHLD);

        //result found flag (It is set to -1 so that current solutions are not canceled)
        int result_found_flag = -1;

        //IDLE workers will only be sent problems if there are anymore problems
        if(no_more_problems == 0)
        {
            //Loop that iterates through worker states and sends problems to idles workers
            for(int i = 0; i < workers; i++)
            {
                //condition that works only if the worker is idle
                if(temp_worker_state[i] == WORKER_IDLE)
                {
                    //Acquring the problem variant that needs to be solved
                    struct problem *problem_variant_to_be_solved = get_problem_variant(workers, i);

                    //If the return value of get_problem_variant() is null set no_more_problems to 1 and break
                    if(problem_variant_to_be_solved == NULL)
                    {
                        //Set no_more_problems = 1 and break
                        no_more_problems = 1;
                        break;
                    }

                    //I want to block SIGCHLD signals here because I want the worker to be in the CONTINUED state before the RUNNING state
                    //I am also manipulating the global state array used by the signal handler
                    //Adding the SIGCHLD signal to the set of desired signals to be blocked
                    Sigaddset(&mask, SIGCHLD);
                    //Block SIGCHLD and save previous blocked set
                    Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

                    //debug("[%d:Master] Sending SIGCONT signal to worker %d", getpid(), i);

                    //Send SIGCONT signal to idle workers
                    Kill(local_worker_pids[i], SIGCONT);

                    //Set the global array state to worker continued
                    worker_state[i] = WORKER_CONTINUED;

                    //Set the local array state to worker continued
                    temp_worker_state[i] = WORKER_CONTINUED;

                    //debug("[%d:Master] Changing state of worker %d from WORKER_IDLE to WORKER_CONTINUED", getpid(), i);

                    //Report the change of state in the child process
                    sf_change_state(local_worker_pids[i], WORKER_IDLE, WORKER_CONTINUED);

                    //Unblock SIGCHLD
                    Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
                    //Removing the block of the SIGCHLD signal from the current mask
                    Sigdelset(&mask, SIGCHLD);

                    //Write problem to the respective file of the worker process
                    //Calculating the total number of bytes to write to stdout
                    int entire_problem_size = (problem_variant_to_be_solved->size);

                    //Cast problem to char pointer to be written into the file
                    char *problem_storage = (char *)problem_variant_to_be_solved;

                    //Storing the problem in the assocaited index of the worker problem storage array
                    worker_problem_storage[i] = problem_variant_to_be_solved;

                    //debug("[%d:Master] Writing problem to worker %d", getpid(), i);

                    //Letting the system know that this problem is going to be sent
                    sf_send_problem(local_worker_pids[i], problem_variant_to_be_solved);

                    //loop to write problem structure to process file
                    for(int j = 0; j < entire_problem_size; j++)
                    {
                        //getting the return value of fputc
                        int return_value_fputc = fputc((*problem_storage), write_to_file[i]);
                        if(return_value_fputc == EOF)
                        {
                            unix_error("File write error");
                        }

                        //Incrementing the problem_storage byte pointer
                        problem_storage++;
                    }

                    //debug("[%d:Master] Flushing problem contents to worker %d", getpid(), i);

                    //Flushing contents
                    fflush(write_to_file[i]);
                }
            }
        }

        //Adding the SIGCHLD signal to the set of desired signals to be blocked
        Sigaddset(&mask, SIGCHLD);
        //Block SIGCHLD and save previous blocked set
        Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

        //debug("[%d:Master] Populating local state array ", getpid());

        //populating temporary state array with the current worker states
        for(int i = 0; i < workers; i++)
        {
            temp_worker_state[i] = worker_state[i];
        }

        //Setting the flags that tell me if I have stopped to 0 because I have recorded it
        are_stopped_workers = 0;

        //Unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
        //Removing the block of the SIGCHLD signal from the current mask
        Sigdelset(&mask, SIGCHLD);

        //Loop that iterates through worker states and reads the result of workers in the STOPPED state
        for(int i = 0; i < workers; i++)
        {
            //condition that works only if the worker is idle
            if(temp_worker_state[i] == WORKER_STOPPED)
            {
                //Creating a temporary result struct to hold the original block
                struct result temp_result;
                char *original_result_storage = (char *)&temp_result;

                //reading sizeof(struct result) bytes and storing them into original result storage
                for(int j = 0; j < sizeof(struct result); j++)
                {
                    int readbyte = fgetc(read_from_file[i]);
                    if((readbyte == EOF) && (ferror(read_from_file[i]) != 0))
                    {
                        unix_error("File read error");
                    }

                    //storing and incrementing the byte
                    (*original_result_storage) = readbyte;
                    original_result_storage++;
                }

                //Resetting the original result storage pointer back to temp result
                original_result_storage = (char *)&temp_result;

                //calculating the remaining number of bytes to read from stdin
                int entire_result_size = temp_result.size;
                int remaining_bytes_to_read = temp_result.size - sizeof(struct result);

                //debug("[%d:Master] CALLING MALLOC FOR RESULT FROM WORKER %d", getpid(), i);

                //Calling malloc to allocate new space for the result
                char *new_result_storage = Malloc(entire_result_size);
                struct result *new_result = (struct result *)new_result_storage;

                //Copying the contents of the old result to the new result
                for(int j = 0; j < sizeof(struct result); j++)
                {
                    (*new_result_storage) = (*original_result_storage);
                    new_result_storage++;
                    original_result_storage++;
                }

                //Acquiring the remaining bytes to be read in the file
                for(int j = 0; j < remaining_bytes_to_read; j++)
                {
                    int readbyte = fgetc(read_from_file[i]);
                    if((readbyte == EOF) && (ferror(read_from_file[i]) != 0))
                    {
                        unix_error("File read error");
                    }

                    //Storing and incrementing the new result storage
                    (*new_result_storage) = readbyte;
                    new_result_storage++;
                }

                //debug("[%d:Master] recieved result from worker %d", getpid(), i);

                //Notify the system that a result has been recieved
                sf_recv_result(local_worker_pids[i], new_result);

                //The return value of post_result will be stored here
                int post_result_return = -1;

                //debug("[%d:Master] resulted will be posted from worker %d", getpid(), i);

                //Now get the problem that the result originally got
                //Temporary problem that might need to be passed
                struct problem temp_passed_problem = {.size = sizeof(struct problem), .id = new_result->id};
                //This condition is if the result has no associated problem
                if(worker_problem_storage[i] == NULL)
                {
                    //Setting the result to failed and so I now recieve a failed post result return
                    new_result->failed = 1;
                    post_result_return = post_result(new_result, &temp_passed_problem);
                }
                else
                {
                    //post_result and it should return 0 if the result is correct
                    post_result_return = post_result(new_result, worker_problem_storage[i]);
                }

                //If the solution is already correct, get the return value of the most recently returned post_result
                if(result_found_flag != 0)
                {
                    result_found_flag = post_result_return;
                }

                //debug("[%d:Master] resulted posted from worker %d", getpid(), i);

                //Now if the result returned flag is 0 then set all the other problems being worked on to NULL
                if(post_result_return == 0)
                {
                    for(int j = 0; j < workers; j++)
                    {
                        worker_problem_storage[j] = NULL;
                    }
                }

                //Free the result that has malloc'd (allocated)
                Free(new_result);

                //I am manipulating the global state array used by the signal handler
                //Adding the SIGCHLD signal to the set of desired signals to be blocked
                Sigaddset(&mask, SIGCHLD);
                //Block SIGCHLD and save previous blocked set
                Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

                //Set the global array state to worker idle
                worker_state[i] = WORKER_IDLE;

                //Set the local array state to worker idle
                temp_worker_state[i] = WORKER_IDLE;

                //Report the change of state in the child process
                sf_change_state(local_worker_pids[i], WORKER_STOPPED, WORKER_IDLE);

                //Set the are idle flags to 1 because the worker that was just processed is now idle
                are_idle_workers = 1;

                //Unblock SIGCHLD
                Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
                //Removing the block of the SIGCHLD signal from the current mask
                Sigdelset(&mask, SIGCHLD);
            }
        }

        //Here, I want to send SIGHUP SIGNALS ONLY TO THE WORKERS THAT ARE CURRENTLY IN RUNNING OF CONTINUED STATE
        //Adding the SIGCHLD signal to the set of desired signals to be blocked
        Sigaddset(&mask, SIGCHLD);
        //Block SIGCHLD and save previous blocked set
        Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

        //If the solution to a problem was correct, tell all of the other workers to cancel their jobs
        if(result_found_flag == 0)
        {
            for(int i = 0; i < workers; i++)
            {
                //Check to see which workers are running and continued
                if((worker_state[i] == WORKER_RUNNING) || (worker_state[i] == WORKER_CONTINUED))
                {
                    //Use the sf_cancel function to show that worker is canceled
                    sf_cancel(local_worker_pids[i]);

                    //Send those workers a SIGHUP signal
                    Kill(local_worker_pids[i], SIGHUP);
                }
            }
        }

        //Unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
        //Removing the block of the SIGCHLD signal from the current mask
        Sigdelset(&mask, SIGCHLD);

        //Iterating through all of the states to check if the workers have aborted or (primarily to check that they are idle)
        for(int i = 0; i < workers; i++)
        {
            //if true then set the flag to 1
            if((temp_worker_state[i] == WORKER_IDLE) || (temp_worker_state[i] == WORKER_ABORTED))
            {
                all_workers_idle = 1;
            }
            //else set the flag to 0 an break (No point in continuing)
            else
            {
                all_workers_idle = 0;
                break;
            }
        }

        //Now iterating through array to see if it should set idle flag (I am going to block the signal right now as well)
        //Adding the SIGCHLD signal to the set of desired signals to be blocked
        Sigaddset(&mask, SIGCHLD);
        //Block SIGCHLD and save previous blocked set
        Sigprocmask(SIG_BLOCK, &mask, &prev_mask2);

        //Iterating through all of the states to check if the workers have become IDLE
        for(int i = 0; i < workers; i++)
        {
            //check to see if there are availble IDLE workers
            if(worker_state[i] == WORKER_IDLE)
            {
                //Set the are idle flags to 1 because there has been an IDLE worker spotted
                are_idle_workers = 1;
            }
        }

        //Iterating to see if all workers have ABORTED
        int num_aborted_workers = 0;
        for(int i = 0; i < workers; i++)
        {
            //check to see if there are availble IDLE workers
            if(worker_state[i] == WORKER_ABORTED)
            {
                //Increment the number of ABORTED workers
                num_aborted_workers++;
            }
        }

        //Checking if all of the workers have aborted, if so end the while loop
        if(num_aborted_workers == workers)
        {
            //Set the flags to end the while loop
            no_more_problems = 1;
            all_workers_idle = 1;
        }

        //Unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &prev_mask2, NULL);
        //Removing the block of the SIGCHLD signal from the current mask
        Sigdelset(&mask, SIGCHLD);
    }

    //Close all of the file descriptors
    for(int i = 0; i < workers; i++)
    {
        //close the other end of the pipe
        if(close(fd_write_to[i][0]) < 0)
        {
            unix_error("Pipe error");
        }

        //close the other end of the pipe
        if(close(fd_read_from[i][1]) < 0)
        {
            unix_error("Pipe error");
        }

        //Now I am closing the file descriptors for no memory leaks
        fclose(read_from_file[i]);
        fclose(write_to_file[i]);
    }

    //Restore the original bit signal block mask
    Sigprocmask(SIG_SETMASK, &prev_mask1, NULL);

    //Restoring the default signal handler
    Signal(SIGCHLD, SIG_DFL);

    //worker has aborted flag
    int worker_aborted = 0;

    //Number of sigterms sent
    int num_sigterms_sent = 0;

    //Sending the worker processes SIGTERM signals
    for(int i = 0; i < workers; i++)
    {
        //If worker has aborted set aborted flag
        if(worker_state[i] == WORKER_ABORTED)
        {
            worker_aborted = 1;
        }
        //else send the worker a SIGTERM signal
        else if(worker_state[i] == WORKER_IDLE)
        {
            num_sigterms_sent++;

            //Send a SIGTERM to end
            Kill(local_worker_pids[i], SIGTERM);
        }
    }

    //Loop SIGCONT signal sender
    for(int i = 0; i < workers; i++)
    {
        //If worker has aborted set aborted flag
        if(worker_state[i] == WORKER_ABORTED)
        {
            worker_aborted = 1;
        }
        //else send the worker a SIGTERM signal
        else if(worker_state[i] == WORKER_IDLE)
        {
            //Change state of worker form idle to running
            sf_change_state(worker_pids[i], WORKER_IDLE, WORKER_RUNNING);

            //Send a SIGCONT to continue
            Kill(local_worker_pids[i], SIGCONT);
        }
    }

    pid_t process_pid;
    int child_status = 0;

    //Now reap the number of processes that had SIGCONTS sent to them
    for(int i = 0; i < num_sigterms_sent; i++)
    {
        //Get the pid of the reaped child process
        process_pid = Waitpid(-1, &child_status, 0);

        //If terminates unsuccessfully
        if(child_status != 0)
        {
            //Update aborted flag
            worker_aborted = 1;

            //Change state of worker form running to aborted
            sf_change_state(process_pid, WORKER_RUNNING, WORKER_ABORTED);
        }
        else
        {
            //Change state of worker form running to exited
            sf_change_state(process_pid, WORKER_RUNNING, WORKER_EXITED);
        }
    }

    //Master process is ending execution, so use this function
    sf_end();

    //If at least one worker has aborted then exit unsuccessfully
    if(worker_aborted == 1)
    {
        return EXIT_FAILURE;
    }

    //else exit successfully
    return EXIT_SUCCESS;
}

//SIGNAL Handler for SIGCHLD
void master_signal_handler(int sig)
{
    int olderrno = errno;

    pid_t pid;
    int child_status;

    //Loop to wait for next waiting signaled child
    while ((pid = Waitpid(-1, &child_status, (WNOHANG | WUNTRACED | WCONTINUED))) != 0)
    {
        //find the index of the assocaited pid
        int child_index = get_pid_index(pid);

        //These are error signals that are sent
        if(WIFSIGNALED(child_status))
        {
            if((WTERMSIG(child_status) == SIGABRT) || (WTERMSIG(child_status) == SIGSEGV))
            {
                sf_change_state(pid, worker_state[child_index], WORKER_ABORTED);

                worker_state[child_index] = WORKER_ABORTED;

                //Saying that there are aborted workers because one aborted
                are_idle_workers = 1;

                //restore old value of errno and return
                errno = olderrno;
                return;
            }
        }

        //started -> idle
        if(worker_state[child_index] == WORKER_STARTED)
        {
            //debug("[%d:Master] Changing state of worker %d from WORKER_STARTED to WORKER_IDLE", getpid(), child_index);

            sf_change_state(pid, WORKER_STARTED, WORKER_IDLE);

            worker_state[child_index] = WORKER_IDLE;

            //update the idle workers flag because there are now idle workers
            are_idle_workers = 1;
        }

        //continued -> running
        else if(worker_state[child_index] == WORKER_CONTINUED)
        {
            //debug("[%d:Master] Changing state of worker %d from WORKER_CONTINUED to WORKER_RUNNING", getpid(), child_index);

            sf_change_state(pid, WORKER_CONTINUED, WORKER_RUNNING);

            worker_state[child_index] = WORKER_RUNNING;

            //THIS IS PRIMARILY FOR THE RACE CONDITION IF A SIGSTOP HAS OVERRIDEN THE SIGCONT
            if(WIFSTOPPED(child_status))
            {
                sf_change_state(pid, WORKER_RUNNING, WORKER_STOPPED);

                worker_state[child_index] = WORKER_STOPPED;

                //update stopped worker flags because there are now results to be read
                are_stopped_workers = 1;
            }
        }

        //running->stopped
        else if(worker_state[child_index] == WORKER_RUNNING)
        {
            //debug("[%d:Master] Changing state of worker %d from WORKER_RUNNING to WORKER_STOPPED", getpid(), child_index);

            sf_change_state(pid, WORKER_RUNNING, WORKER_STOPPED);

            worker_state[child_index] = WORKER_STOPPED;

            //update stopped worker flags because there are now results to be read
            are_stopped_workers = 1;
        }
    }

    //restore old value of errno
    errno = olderrno;
}

//Function that iterates through the array of worker PIDS to find the index of the pid
int get_pid_index(pid_t worker_pid)
{
    for(int i = 0; i < MAX_WORKERS; i++)
    {
        if(worker_pids[i] == worker_pid)
        {
            //return the associated index
            return i;
        }
    }

    //if nothing else return -1
    return -1;
}