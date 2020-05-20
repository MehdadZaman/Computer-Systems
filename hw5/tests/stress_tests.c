#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <criterion/criterion.h>
#include <pthread.h>

#include "__test_includes.h"
#include "excludes.h"

/*
 * These tests attempt to heavily exercise the student's server in a
 * "black box" configuration.  They fail unless the student has provided
 * implementations for all the modules.  The idea of the test is to
 * start the server, create several processes running a random tester
 * program, and let them pound on the server concurrently for a significant
 * number of operations.
 *
 * These tests current have to be run sequentially (-j1) because they
 * each start their own server, but share the global server_pid variable.
 */
#define SUITE stress_suite

#define SERVER_PORT_NO 10000
#define SERVER_PORT "10000"

static int server_pid;

static void wait_for_server() {
    int ret;
    int i = 0;
    do {
        fprintf(stderr, "Waiting for server to start (i = %d)\n", i);
	ret = system("netstat -an | grep 'LISTEN[ ]*$' | grep ':"SERVER_PORT"'");
	sleep(SERVER_STARTUP_SLEEP);
    } while(++i < 30 && WEXITSTATUS(ret));
}

static void wait_for_no_server() {
    int ret;
    do {
	ret = system("netstat -an | grep 'LISTEN[ ]*$' | grep ':"SERVER_PORT"'");
	if(WEXITSTATUS(ret) == 0) {
	    fprintf(stderr, "Waiting for server port to clear...\n");
	    system("killall -s KILL pbx > /dev/null");
	    sleep(1);
	} else {
	    break;
	}
    } while(1);
}

static void init() {
#if defined(NO_SERVER) || defined(NO_PBX)
  cr_assert_fail("Not all the modules were implemented\n");
#endif
    server_pid = 0;
    wait_for_no_server();
    fprintf(stderr, "***Starting server...");
    if((server_pid = fork()) == 0) {
	// No valgrind for these tests.
	execlp("timeout", "timeout", "--preserve-status", "--signal=TERM", "--kill-after=5", "60",
	       "bin/pbx", "-p", SERVER_PORT, NULL);
	fprintf(stderr, "Failed to exec server\n");
	abort();
    }
    fprintf(stderr, "pid = %d\n", server_pid);
    // Wait for server to start before returning
    wait_for_server();
}

static void fini(int chk) {
    int ret;
    cr_assert(server_pid != 0, "No server was started!\n");
    fprintf(stderr, "***Sending SIGHUP to server pid %d\n", server_pid);
    kill(server_pid, SIGHUP);
    sleep(SERVER_SHUTDOWN_SLEEP);
    kill(server_pid, SIGKILL);
    wait(&ret);
    fprintf(stderr, "***Server wait() returned = 0x%x\n", ret);
    if(chk) {
      if(WIFSIGNALED(ret))
	  cr_assert_fail("***Server terminated ungracefully with signal %d\n", WTERMSIG(ret));
      cr_assert_eq(WEXITSTATUS(ret), 0, "Server exit status was not 0");
    }
}

static void killall() {
    system("killall -s KILL tester pbx /usr/lib/valgrind/memcheck-amd64-linux > /dev/null 2>&1");
}

#define NUM_TESTERS 10
#define TEST_LENGTH "10000"

#if defined(TEST_CONFIG_D)
#define TEST_NAME stress_test
Test(SUITE, TEST_NAME, .init = init, .fini = killall, .timeout = 120)
{
    char *name = QUOTE(SUITE)"/"QUOTE(TEST_NAME);
    int pids[NUM_TESTERS];

    fprintf(stderr, "Running test %s\n", name);

    // Create tester processes.
    for(int i = 0; i < NUM_TESTERS; i++) {
	if((pids[i] = fork()) < 0)
	    cr_assert_fail("Failed to fork tester %d\n", i);
	if(pids[i] == 0) { // Child
	    if(freopen("stress_test.out", "w", stderr) == NULL) {
	      fprintf(stderr, "Failed to redirect stderr for tester %d\n", i);
	      abort();
	    }
	    if(execl("util/tester", "tester", "-p", SERVER_PORT, "-l", TEST_LENGTH,
		     "-x", "0", "-y", "15", "-d", "0", NULL) < 0) {
	        fprintf(stderr, "Failed to exec tester %d\n", i);
		abort();
	    }
	}
	fprintf(stderr, "Forked tester %d (pid %d)\n", i, pids[i]);
    }

    // Wait for tester processes to finish.
    for(int i = 0; i < NUM_TESTERS; i++) {
	int s, pid;
	if((pid = waitpid(pids[i], &s, 0)) < 0)
	    cr_assert_fail("Waitpid failed before all testers reaped??\n");
	fprintf(stderr, "Tester pid %d terminated with status 0x%x\n", pid, s);
	cr_assert(!WIFSIGNALED(s), "Tester pid %d terminated with signal %d\n", pid, WTERMSIG(s));
	cr_assert_eq(WEXITSTATUS(s), 0, "Tester pid %d exit status was not 0\n", pid);
    }
    fprintf(stderr, "All testers terminated\n");
    fini(0);
}
#undef TEST_NAME
#endif

#undef SUITE
