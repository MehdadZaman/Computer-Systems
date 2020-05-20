/*
 * This problem will hang while solving unless it is the last variant of problem.
 * Based off of trivial solver with modifications to the solver method.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "debug.h"
#include "polya.h"

static struct problem *cancel_construct();
static void cancel_vary(struct problem *prob, int var);
static struct result *cancel_solve(struct problem *aprob, volatile sig_atomic_t *canceledp);
static int cancel_check(struct result *result, struct problem *prob);

/*
 * Initialize the cancel test solver.
 */
struct solver_methods cancel_solver_methods = {
   cancel_construct, cancel_vary, cancel_solve, cancel_check
};

void cancel_solver_init(void) {
    solvers[CANCEL_PROBLEM_TYPE] = cancel_solver_methods;
}

/*
 * Construct a trivial problem.
 *
 * @param id     The problem ID.
 * @param nvars  The number of possible variants of the problem.
 */
static struct problem *cancel_construct(int id, int nvars) {
    struct problem *prob = malloc(sizeof(struct problem));
    if(prob == NULL)
	return NULL;
    prob->size = sizeof(struct problem);
    prob->type = CANCEL_PROBLEM_TYPE;
    prob->nvars = nvars;
    prob->id = id;
    return prob;
}

/*
 * Create a variant of a trivial problem.
 * This function does nothing.
 *
 * @param prob  The problem to be modified.
 * @param var  Integer in the range [0, nvars) specifying the particular variant
 * form to be created.
 */
static void cancel_vary(struct problem *prob, int var) {
    prob->var = var;
}

/*
 * Solve a "problem", returning a result.
 * All variants except the last are unsolvable and result in an infinite loop.
 *
 * @param aprob  Pointer to a structure describing the problem to be solved.
 * @param canceledp  Pointer to a flag indicating whether the current solution attempt
 * is to be canceled.
 * @return  Either a result structure with a "failed" field equal to zero is returned,
 * or NULL is returned if such a structure could not be created.  The caller is
 * responsible for freeing any non-NULL pointer that is returned.
 */
static struct result *cancel_solve(struct problem *aprob, volatile sig_atomic_t *canceledp) {
    debug("[%d:Worker] Cancel solver (id = %d)", getpid(), aprob->id);

    if (aprob->var == aprob->nvars - 1) {
	// Introduce a delay here to try to make it likely that workers working on the
	// unsolvable variant get started before the solution is obtained.
	sleep(1);
        struct result *result = calloc(sizeof(struct result), 1);
        if (result == NULL) {
            debug("[%d:Worker] cancel prob ran out of memeory? \n", getpid());
            return NULL;
        }
        result->size = sizeof(*result);
        result->id = aprob->id;

        debug("[%d:Worker] Returning result (size = %ld, failed = %d)",
	        getpid(), result->size, result->failed);
        return result;
    } else {
        // If it is not the last variant, infinite loop until canceled.
        while (*canceledp != 1)
	    ;
        return NULL;
    }
}

/*
 * Check a solution to a trivial problem.
 *
 * @param result  The result to be checked.
 * @param prob  The problem the result is supposed to solve.
 * @return -1 if the result is marked "failed", otherwise 0.
 */
static int cancel_check(struct result *result, struct problem *prob) {
    if(result->failed)
	return -1;
    return 0;
}
