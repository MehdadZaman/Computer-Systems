#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <signal.h>
#include "const.h"

#define TEST_TIMEOUT 15

#define STUDENT_OUTPUT "student_output"
#define TEST_INPUT "tests/inputs"

#define ASSERT_RETURN do { \
    cr_assert_eq(ret, exp_ret, \
		 "Wrong return value from function: expected %d, was %d\n", \
                 exp_ret, ret); \
  } while(0)

#define ASSERT_GLOBAL_OPTIONS do { \
    cr_assert_eq(global_options, exp_global_opt, \
		 "Wrong value for global_options: expected 0x%x, was 0x%x\n", \
		 exp_global_opt, global_options); \
  } while(0)

#define ASSERT_SYMBOL_STRUCT do { \
    cr_assert_eq(ret_symbol->value, exp_symbol.value, "returned symbol has incorrect value field! Got: %d | Exp: %d", ret_symbol->value, exp_symbol.value); \
    cr_assert_eq(ret_symbol->rule, exp_symbol.rule, "returned symbol has incorrect rule field! Got: %p | Exp: %p", ret_symbol->rule, exp_symbol.rule); \
    cr_assert_eq(ret_symbol->refcnt, exp_symbol.refcnt, "returned symbol has incorrect refcnt field! Got: %d | Exp: %d", ret_symbol->refcnt, exp_symbol.refcnt); \
    cr_assert_eq(ret_symbol->next, exp_symbol.next, "returned symbol has incorrect next field! Got: %p | Exp: %p", ret_symbol->next, exp_symbol.next); \
    cr_assert_eq(ret_symbol->prev, exp_symbol.prev, "returned symbol has incorrect prev field! Got: %p | Exp: %p", ret_symbol->prev, exp_symbol.prev); \
    cr_assert_eq(ret_symbol->nextr, exp_symbol.nextr, "returned symbol has incorrect nextr field! Got: %p | Exp: %p", ret_symbol->nextr, exp_symbol.nextr); \
    cr_assert_eq(ret_symbol->prevr, exp_symbol.prevr, "returned symbol has incorrect prevr field! Got: %p | Exp: %p", ret_symbol->prevr, exp_symbol.prev); \
} while(0)


#define COMPARE_OUTPUT(output, reference, exp_ret)				\
    run_with_system("cmp "STUDENT_OUTPUT"/"output" "TEST_INPUT"/"reference, exp_ret);


/* Imported from cse320archivef2019-hw1 */
void assert_no_diffs(char *student, char *ref, int recursive) {
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "diff %s %s %s > /dev/null", recursive ? "-r" : "", ref, student);
    int status = system(cmd);
    cr_assert_eq(WEXITSTATUS(status), EXIT_SUCCESS,
		 "Diff did not report same contents (status %d)\n"
                 "for '%s' (student) and '%s' (reference)\n",
		 WEXITSTATUS(status), student, ref);
}

/* Imported from cse320archivef2019-hw1 */
void run_with_system(char *cmd, int exp_status) {
    int status = system(cmd);
    cr_assert(WIFEXITED(status), "The program did not exit normally.\n");
    cr_assert_eq(WEXITSTATUS(status), exp_status,
		 "The exit status was wrong: expected %d, was %d\n",
		 exp_status, WEXITSTATUS(status));
}

void init_output() {
    mkdir(STUDENT_OUTPUT, 0700);
}


/**
 * ================================
 * PART I
 * validargs(argc, argv) unit-tests
 * ================================
 *
 */


/**
 * validargs_1
 * Tests if all flags after -h should be ignored, thus global_opts == 1
 * @brief bin/sequitur -h -c -d
 */
Test(validargs_suite, validargs_1, .timeout=TEST_TIMEOUT) {
    int argc = 5;
    char *argv[] = {"bin/sequitur", "-h", "-c", "-d", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = 0;
    int exp_global_opt = 0x1;

    ASSERT_RETURN;
}

/**
 * validargs_2
 * Tests if default block size is 1024
 * @brief bin/sequitur -c
 */
Test(validargs_suite, validargs_2, .timeout=TEST_TIMEOUT) {
    int argc = 2;
    char *argv[] = {"bin/sequitur", "-c", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = 0;
    int exp_global_opt = 1024 << 16 | 0x2;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_3
 * Tests proper blocksize bit mask
 * @brief bin/sequitur -c -b 100
 */
Test(validargs_suite, validargs_3, .timeout=TEST_TIMEOUT) {
    int argc = 4;
    char *argv[] = {"bin/sequitur", "-c", "-b", "100", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = 0;
    int exp_global_opt = 100 << 16 | 0x2;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}


/**
 * validargs_4
 * Tests validargs behavior when block size is out of bounds
 * @brief bin/sequitur -c -b 0
 */
Test(validargs_suite, validargs_4, .timeout=TEST_TIMEOUT) {
    int argc = 4;
    char *argv[] = {"bin/sequitur", "-c", "-b", "0", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_5
 * Tests missing argument for -b flag
 * @brief bin/sequitur -c -b
 */
Test(validargs_suite, validargs_5, .timeout=TEST_TIMEOUT) {
    int argc = 3;
    char *argv[] = {"bin/sequitur", "-c", "-b", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_6
 * Tests proper compress flag set
 * @brief bin/sequitur -c
 */
Test(validargs_suite, validargs_6, .timeout=TEST_TIMEOUT) {
    int argc = 2;
    char *argv[] = {"bin/sequitur", "-c", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = 0;
    int exp_global_opt = 1024 << 16 | 0x2;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_7
 * Tests proper decompress flag set
 * @brief bin/sequitur -d
 */
Test(validargs_suite, validargs_7, .timeout=TEST_TIMEOUT) {
    int argc = 2;
    char *argv[] = {"bin/sequitur", "-d", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = 0;

    ASSERT_RETURN;
    // Only check if the bits other than block size matches 0x4
    cr_assert_eq(global_options & 0xFFFF, 0x4, "failed to set -d flag in global options. Got 0x%x", global_options);
}

 /**
  * validargs_8
  * Tests behavior when no flag is passed
  * @brief bin/sequitur
  */
Test(validargs_suite, validargs_8, .timeout=TEST_TIMEOUT) {
    int argc = 1;
    char *argv[] = {"bin/sequitur", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_9
 * Tests behavior when erroneous flags are passed
 * @brief bin/sequitur -a
 */
Test(validargs_suite, validargs_9, .timeout=TEST_TIMEOUT) {
    int argc = 2;
    char *argv[] = {"bin/sequitur", "-a", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_10
 * Tests behavior when -b flag is passed with -d
 * @brief bin/sequitur -d -b 20
 */
Test(validargs_suite, validargs_10, .timeout=TEST_TIMEOUT) {
    int argc = 4;
    char *argv[] = {"bin/sequitur", "-d", "-b", "20", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_11
 * Tests behavior when -c and -d are both passed
 * @brief bin/sequitur -c -d
 */
Test(validargs_suite, validargs_11, .timeout=TEST_TIMEOUT) {
    int argc = 3;
    char *argv[] = {"bin/sequitur", "-c", "-d", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * validargs_12
 * Tests behavior when -b flag is passed before -c
 * @brief bin/sequitur -b 20 -c
 */
Test(validargs_suite, validargs_12, .timeout=TEST_TIMEOUT) {
    int argc = 4;
    char *argv[] = {"bin/sequitur", "-b", "20", "-c", NULL};

    int ret = validargs(argc, argv);

    int exp_ret = -1;
    int exp_global_opt = 0;

    ASSERT_RETURN;
    ASSERT_GLOBAL_OPTIONS;
}

/**
 * ================================
 * PART II
 * low-level unit-tests
 * ================================
 *
 */

/**
 * init_symbols
 * @brief checks if init_symbols resets num_symbols and next_nonterminal_value variables
 */
Test(symbols_suite, init_symbols, .timeout=TEST_TIMEOUT) {
    num_symbols = 320;
    next_nonterminal_value = FIRST_NONTERMINAL + num_symbols;

    init_symbols();

    cr_assert(num_symbols == 0, "num_symbols not set to 0!");
    cr_assert_eq(next_nonterminal_value, FIRST_NONTERMINAL, "next_nonterminal_value not set to FIRST_NONTERMINAL!");
}

/**
 * init_rules
 * @brief checks if init_rules properly nulls out the main_rule and resets rule_map
 */
Test(rules_suite, init_rules, .timeout=TEST_TIMEOUT) {
    SYMBOL temp = {0};
    main_rule = &temp;
    memset(rule_map, 'A', MAX_SYMBOLS * sizeof(SYMBOL *));

    init_rules();

    cr_assert_null(main_rule, "main_rule was not set to NULL!");
    int i;
    for(i = 0; i < MAX_SYMBOLS; i++) {
        cr_assert_null(rule_map[i], "rule_map at index %d not NULL!", i);
    }
}

/**
 * new_symbol_1
 * @brief checks proper symbol creation for a terminal symbol
 */
Test(symbols_suite, new_symbol_1, .timeout=TEST_TIMEOUT) {
    int exp_val = 10;
    int exp_numsymb = num_symbols + 1;
    SYMBOL *exp_addr = &symbol_storage[num_symbols];

    SYMBOL *ret_symbol = new_symbol(exp_val, NULL);
    SYMBOL exp_symbol = {0};
    exp_symbol.value = exp_val;

    cr_assert_eq(ret_symbol, exp_addr, "returned symbol was not properly assigned in symbol storage!");
    cr_assert_eq(num_symbols, exp_numsymb, "num_symbols was not incremented!");
    ASSERT_SYMBOL_STRUCT;
}

/**
 * new_symbol_2
 * @brief checks proper symbol creation for a nonterminal symbol with a rule
 */
Test(symbols_suite, new_symbol_2, .timeout=TEST_TIMEOUT) {
    int exp_val = 320;
    int exp_numsymb = num_symbols + 1;
    SYMBOL *exp_addr = &symbol_storage[num_symbols];

    SYMBOL exp_symbol = {0};
    exp_symbol.value = exp_val;
    exp_symbol.rule = &exp_symbol;
    SYMBOL *ret_symbol = new_symbol(exp_val, &exp_symbol);

    cr_assert_eq(ret_symbol, exp_addr, "returned symbol was not properly assigned in symbol storage!");
    cr_assert_eq(num_symbols, exp_numsymb, "num_symbols was not incremented!");
    cr_assert_eq(exp_symbol.refcnt, 1, "rule passed did not have refcnt incremented!");
    exp_symbol.refcnt = 0;
    ASSERT_SYMBOL_STRUCT;
}

/**
 * new_symbol_3
 * @brief checks for proper abort when symbol_storage is exhausted
 */
Test(symbols_suite, new_symbol_3,  .signal=SIGABRT, .timeout=TEST_TIMEOUT) {
    num_symbols = MAX_SYMBOLS;
    new_symbol(320, NULL);

    cr_assert(1 == 0, "failed to abort when symbol_storage was full!");
}

/**
 * new_rule_1
 * @brief checks for proper value in a new rule
 */
Test(rules_suite, new_rule_1, .timeout=TEST_TIMEOUT) {
    unsigned int exp_value = FIRST_NONTERMINAL + 5;  // Rules cannot have terminals as their heads.
    SYMBOL *ret_rule = new_rule(exp_value);

    cr_assert_not_null(ret_rule, "returned rule was NULL");
    cr_assert_eq(exp_value, ret_rule->value, "new rule's 'value' field didn't match passed value!");
}

/**
 * new_rule_2
 * @brief checks for proper prev, next, and rule fields in a new rule
 */
Test(rules_suite, new_rule_2, .timeout=TEST_TIMEOUT) {
    unsigned int exp_value = FIRST_NONTERMINAL + 5;  // Rules cannot have terminals as their heads.
    SYMBOL *ret_rule = new_rule(exp_value);

    cr_assert_not_null(ret_rule, "returned rule was NULL");
    cr_assert_eq(ret_rule, ret_rule->rule, "new rule didn't initialize 'rule' field to point back to itself!");
    cr_assert_eq(ret_rule, ret_rule->prev, "new rule didn't initialize 'prev' field to point back to itself!");
    cr_assert_eq(ret_rule, ret_rule->next, "new rule didn't initialize 'next' field to point back to itself!");
}

/**
 * add_rule_1
 * @brief check to see if adding a rule functions correctly when main_rule is NULL
 */
Test(rules_suite, add_rule_1, .timeout=TEST_TIMEOUT) {
    SYMBOL temp_rule = {0};
    main_rule = NULL;
    add_rule(&temp_rule);

    cr_assert_eq(&temp_rule, main_rule, "originally NULL main_rule didn't change to added rule");
    cr_assert_eq(&temp_rule, main_rule->prevr, "new main rule's 'prevr' field doesn't point back to itself!");
    cr_assert_eq(&temp_rule, main_rule->nextr, "new main rule's 'nextr' field doesn't point back to itself!");
}

/**
 * init_digram_hash_1
 * @brief check to see if the digram_table was initialized to NULL
 */
Test(digram_suite, init_digram_hash_1, .timeout=TEST_TIMEOUT) {
    /* Fill the table with something that isn't NULL*/
    for(int i = 0; i < MAX_DIGRAMS; i++){
        digram_table[i] = TOMBSTONE;
    }

    /* The table should be initialized to NULL after running this */
    init_digram_hash();

    /* Verify that that's the case */
    for(int i = 0; i < MAX_DIGRAMS; i++){
        cr_assert_null(digram_table[i], "the digram table wasn't successfully initialized to NULL!");
    }
}

/**
 * digram_get_1
 * @brief check to see if getting an existing digram succeeds indexing via DIGRAM_HASH (no collision)
 */
Test(digram_suite, digram_get_1, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = &s1;

    SYMBOL *ret_symbol = digram_get(v1, v2);
    cr_assert_eq(&s1, ret_symbol, "failed to return existing digram from digram_table (no collision)");
}

/**
 * digram_get_2
 * @brief check to see if getting an existing digram succeeds indexing via DIGRAM_HASH (with collision)
 */
Test(digram_suite, digram_get_2, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = &s2; // Make sure the space in between isn't NULL
    digram_table[digram_table_index+1] = &s1;

    SYMBOL *ret_symbol = digram_get(v1, v2);
    cr_assert_eq(&s1, ret_symbol, "failed to return existing digram from digram_table (with collision)");
}

/**
 * digram_get_3
 * @brief check to see if digram_get handles nonexistent digrams
 */
Test(digram_suite, digram_get_3, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = NULL;

    SYMBOL *ret_symbol = digram_get(v1, v2);
    cr_assert_null(ret_symbol, "failed to return NULL for a nonexistent digram");
}

/**
 * digram_get_4
 * @brief check to see if digram_get handles TOMBSTONEs and looping around
 */
Test(digram_suite, digram_get_4, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    for (int i=0; i<MAX_DIGRAMS; i++) {
        digram_table[i] = TOMBSTONE;  // Leave a trail of TOMBSTONEs that wraps around the digram_table
    }
    digram_table[0] = &s1;  // The digram to be looked up resides immediately on the other side

    SYMBOL *ret_symbol = digram_get(v1, v2);
    cr_assert_eq(&s1, ret_symbol, "failed lookup on an existing digram (wrapping around the table with TOMBSTONEs)");
}

/**
 * digram_delete_1
 * @brief check to see if deleting an existing digram succeeds and returns 0
 */
Test(digram_suite, digram_delete_1, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = &s1;

    int retval = digram_delete(&s1);  // Attempt to delete s1
    // Since s1 exists in digram_table, we expect return value 0
    cr_assert_eq(0, retval, "expected return value 0 when deleting an existing digram");
    // Check that s1 was replaced with a TOMBSTONE
    cr_assert_eq(TOMBSTONE, digram_table[digram_table_index], "expected deleted digram to be replaced with TOMBSTONE");
}

/**
 * digram_delete_2
 * @brief check to see if deleting a nonexistent digram fails and returns -1
 */
Test(digram_suite, digram_delete_2, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = NULL;

    int retval = digram_delete(&s1);  // Attempt to delete s1
    // Since s1 doesn't exist in digram_table, we expect return value 0
    cr_assert_eq(-1, retval, "expected return value of -1 when attempting to delete a nonexistent digram");
}

/**
 * digram_delete_3
 * @brief check to see that delete_digram will only delete the specific digram that is passed as the
 * argument, not some other matching digram that happens to be in the table
 */
Test(digram_suite, digram_delete_3, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1, s2;  // Digram 1 (existing in table)
    SYMBOL s3, s4;  // Digram 2 with same value of Digram 1, (existing after Digram 1 in the table)

    /* Assemble Digram 1 */
    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    /* Assemble Digram 2 */
    s3.value = v1;
    s4.value = v2;
    s3.next = &s4;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = &s1;  // Insert Digram 1
    digram_table[digram_table_index+1] = &s3; // Insert Digram 2

    int retval = digram_delete(&s3);  // Attempt to delete Digram 2. Hopefully, Digram 1 isn't affected and Digram 2 is deleted
    cr_assert_eq(&s1, digram_table[digram_table_index], "attempting to delete a digram shouldn't affect other digrams with the same value");
    cr_assert_eq(TOMBSTONE, digram_table[digram_table_index+1], "expected digram to be deleted and replaced by a TOMBSTONE");
    cr_assert_eq(0, retval, "expected return value of 0 when attempting to delete an existing digram");
}

/**
 * digram_put_1
 * @brief check to see if inserting a unique digram into the table succeeds ands returns 0
 */
Test(digram_suite, digram_put_1, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = NULL;

    int retval = digram_put(&s1);  // Attempt to insert s1
    cr_assert_eq(0, retval, "expected return value of 0 when attempting to insert a new, unique digram");
    cr_assert_eq(&s1, digram_table[digram_table_index], "new, unique digram wasn't inserted correctly into digram_table");
}

/**
 * digram_put_2
 * @brief check to see if inserting a non-unique digram into the table changes nothing and returns 1
 */
Test(digram_suite, digram_put_2, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary

    /* Test Digram 1: a digram already in the table */
    SYMBOL s1;
    SYMBOL s2;
    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    /* Test Digram 2: a digram with the same value as digram 1, to be "inserted" */
    SYMBOL s3;
    SYMBOL s4;
    s3.value = v1;
    s4.value = v2;
    s3.next = &s4;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = &s1;

    int retval = digram_put(&s3);  // Attempt to insert s2
    cr_assert_eq(1, retval, "expected return value of 1 when attempting to insert a non-unique digram");
    cr_assert_eq(&s1, digram_table[digram_table_index], "digram table changed despite returning 1 after digram_put");
}

/**
 * digram_put_3
 * @brief check to see if erroneous conditions result in digram_put returning -1
 * (lvelikov) I can't come up with a way to accurately test inserting into a full digram_table,
 * as some implementations (like the solution code) depend on a custom "number of entries" counter
 */
Test(digram_suite, digram_put_3, .timeout=TEST_TIMEOUT) {
    /* Test Digram: a digram with no second symbol */
    SYMBOL s1;
    s1.value = 5;
    s1.next = NULL;

    /* Check that you can't insert a digram with no second symbol */
    int retval = digram_put(&s1);
    cr_assert_eq(-1, retval, "expected return value of -1 when attempting to insert a malformed digram");
}

/**
 * digram_put_4
 * @brief check to see if inserting a unique digram into the table replaces a TOMBSTONE and returns 0
 */
Test(digram_suite, digram_put_4, .timeout=TEST_TIMEOUT) {
    int v1 = 5, v2 = 6; // Arbitrary
    SYMBOL s1;
    SYMBOL s2;

    s1.value = v1;
    s2.value = v2;
    s1.next = &s2;

    int digram_table_index = DIGRAM_HASH(v1, v2);
    digram_table[digram_table_index] = TOMBSTONE;  // digram_put should replace this with &s1

    int retval = digram_put(&s1);  // Attempt to insert s1
    cr_assert_eq(0, retval, "expected return value of 0 when attempting to insert a new, unique digram");
    cr_assert_eq(&s1, digram_table[digram_table_index], "inserted digram didn't replace TOMBSTONE");
}

/**
 * ================================
 * PART III
 * tests on decompress
 * ================================
 *
 */

/**
 * decomp_empty
 * @brief test decompress on empty file input
 *              expect return EOF
 * in: TEST_INPUT/empty
 * out: /dev/null
 */
Test(decompress_suite, decomp_empty, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/empty", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = EOF;
    ASSERT_RETURN;
}

/**
 * decomp_no_eot
 * @brief test decompress on a file missing the EOT
 * in: TEST_INPUT/missing_eot.txt.seq
 * out: /dev/null
 */
Test(decompress_suite, decomp_no_eot, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/missing_eot.txt.seq", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = EOF;
    ASSERT_RETURN;
}

/**
 * decomp_unexp_sob
 * @brief test decompress on a file with an unexpected SOB
 * in: TEST_INPUT/unexpected_sob.txt.seq
 * out: /dev/null
 */
Test(decompress_suite, decomp_unexp_sob, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/unexpected_sob.txt.seq", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = EOF;
    ASSERT_RETURN;
}

/**
 * decomp_trunc
 * @brief test decompress on a file that is truncated at the end of a block,
 * tests return value.
 * in: TEST_INPUT/truncated
 * out: /dev/null
 */
Test(decompress_suite, decomp_trunc, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/truncated.seq", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = EOF;
    ASSERT_RETURN;
}

/**
 * decomp_trunc_compare
 * @brief test decompress on a file that is truncated at the end of a block,
 * tests the partial output for comparison with what is expected with expected output.
 * in: TEST_INPUT/truncated
 * out: /dev/null
 */
Test(decompress_suite, decomp_trunc_compare, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/truncated.seq", "r");
    FILE *out = fopen(STUDENT_OUTPUT"/truncated.txt", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }
    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    COMPARE_OUTPUT("truncated.txt", "truncated.txt", 0);
}

/**
 * decomp_valid_compare
 * @brief a positive test for decompress on a valid compressed input.
 * with expected output.
 * in: TEST_INPUT/truncated
 * out: /dev/null
 */
Test(decompress_suite, decomp_valid_compare, .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/jingle_bells.txt.seq", "r");
    FILE *out = fopen(STUDENT_OUTPUT"/jingle_bells.txt", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }
    int ret = decompress(in, out);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    COMPARE_OUTPUT("jingle_bells.txt", "jingle_bells.txt", 0);
}

/**
 * ================================
 * PART IV
 * tests on compress
 * ================================
 *
 */

/**
 * compress_empty
 * @brief test compress on empty file input
 *              expect return EOF
 * in: TEST_INPUT/empty
 * out: /dev/null
 */
Test(compress_suite, compress_empty , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/empty", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = compress(in, out, 1024 << 10);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = 2;
    ASSERT_RETURN;
}

/**
 * compress_2mb
 * @brief test compress on 2mb text file with default blocksize
 * in: TEST_INPUT/2mb_text_1024.txt
 * out: STUDENT_OUTPUT/2mb_text_1024.txt.seq
 */
Test(compress_suite, compress_2mb , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/2mb_text_1024.txt", "r");
    FILE *out = fopen(STUDENT_OUTPUT"/2mb_text_1024.txt.seq", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int ret = compress(in, out, 1024 << 10);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    cr_assert(ret != EOF, "Returned EOF when not expected");
    run_with_system("./tests/seqcheck < "STUDENT_OUTPUT"/2mb_text_1024.txt.seq", 0);
}

/**
 * inverse_1
 * @brief Checks if student compress/decompress are inverses of each other
 * in: TEST_INPUT/jingle_bells.txt
 * out: STUDENT_OUTPUT/inverse_1.txt.seq
 */
Test(compress_suite, inverse_1 , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/jingle_bells.txt", "r");
    FILE *reversed = fopen(STUDENT_OUTPUT"/inverse_1.txt", "w");
    FILE *out = fopen(STUDENT_OUTPUT"/inverse_1.txt.seq", "w");

    if (in == NULL || out == NULL || reversed == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int cret = compress(in, out, 1024 << 10);

    if (fclose(out) == EOF){
      cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    }

    out = fopen(STUDENT_OUTPUT"/inverse_1.txt.seq", "r");

    if (out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int dret = decompress(out, reversed);

    if (fclose(in) == EOF || fclose(out) == EOF || fclose(reversed) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    COMPARE_OUTPUT("inverse_1.txt", "jingle_bells.txt", 0);
}

/**
 * inverse_2
 * @brief Checks if student compress/decompress are inverses of each other.
 * The input is a binary file.
 * in: TEST_INPUT/binary_input
 * out: STUDENT_OUTPUT/inverse_2
 */
Test(compress_suite, inverse_2 , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/binary_input", "r");
    FILE *reversed = fopen(STUDENT_OUTPUT"/inverse_2.txt", "w");
    FILE *out = fopen(STUDENT_OUTPUT"/inverse_2.txt.seq", "w");

    if (in == NULL || out == NULL || reversed == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int cret = compress(in, out, 1024 << 10);

    if (fclose(out) == EOF){
      cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    }

    out = fopen(STUDENT_OUTPUT"/inverse_2.txt.seq", "r");

    if (out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }

    int dret = decompress(out, reversed);

    if (fclose(in) == EOF || fclose(out) == EOF || fclose(reversed) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    COMPARE_OUTPUT("inverse_2.txt", "binary_input", 0);
}

/**
 * comp_ret_1
 * @brief Tests to see if student returns correct bytes written in compress
 * in: TEST_INPUT/jingle_bells.txt
 * out: STUDENT_OUTPUT/comp_ret_1
 */
Test(compress_suite, comp_ret_1 , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/jingle_bells.txt", "r");
    FILE *out = fopen(STUDENT_OUTPUT"/comp_ret_1", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }
    int ret = compress(in, out, 1024 << 10);
    if (fclose(in) == EOF || fclose(out) == EOF)
            cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = 766;
    ASSERT_RETURN;
}

/**
 * comp_ret_2
 * @brief Tests to see if student returns correct bytes written in compress. This time
 * the input is a binary file.
 * in: TEST_INPUT/binary_input
 * out: /dev/null
 */
Test(compress_suite, comp_ret_2 , .init=init_output, .timeout=TEST_TIMEOUT) {
    FILE *in = fopen(TEST_INPUT"/binary_input", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }
    int ret = compress(in, out, 1024);
    if (fclose(in) == EOF || fclose(out) == EOF)
        cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = 478;
    ASSERT_RETURN;
}

/**
 * comp_ret_3
 * @brief Tests to see if student returns correct bytes written in compress. The block
 * size used to compress is 8192.
 * in: TEST_INPUT/binary_input
 * out: /dev/null
 */
Test(compress_suite, comp_ret_3 , .init=init_output, .timeout= 20) {
    FILE *in = fopen(TEST_INPUT"/2mb_text_1024.txt", "r");
    FILE *out = fopen("/dev/null", "w");
    if (in == NULL || out == NULL) {
        cr_log_error("%s: FAILED TO OPEN FILE", __func__);
    }
    int ret = compress(in, out, 8192);
    if (fclose(in) == EOF || fclose(out) == EOF)
            cr_log_warn("%s: FAILED TO CLOSE FD", __func__);
    int exp_ret = 2284648;
    ASSERT_RETURN;
}

/**
 * ================================
 * PART V
 * black-box tests on program as a whole
 * ================================
 *
 */


/**
 * blackbox_pipecomp_1
 * @brief Performs blackbox compression test on 2mb text file through piping and checks using seqcheck.
 * Synopsis: cat TEST_INPUT/jingle_bells.txt | ./bin sequitur -c | ./tests/seqcheck
 */
Test(blackbox_suite, blackbox_pipecomp_1, .init=init_output, .timeout=TEST_TIMEOUT) {
    run_with_system("cat "TEST_INPUT"/jingle_bells.txt | timeout -sKILL 10 ./bin/sequitur -c | ./tests/seqcheck", 0);
}

/**
 * blackbox_pipedecomp_1
 * @brief Performs blackbox decomp test on 2mb text file through piping and checks output with original.
 * Synopsis: cat TEST_INPUT/jingle_bells.txt | ./bin sequitur -c | cat > "STUDENT_OUTPUT"/blackbox_pipedecomptest_1.txt
 */
Test(blackbox_suite, blackbox_pipedecomp_1, .init=init_output, .timeout=TEST_TIMEOUT) {
    run_with_system("cat "TEST_INPUT"/jingle_bells.txt.seq | timeout -sKILL 10 ./bin/sequitur -d | cat > "STUDENT_OUTPUT"/blackbox_pipedecomp_1.txt", 0);
    COMPARE_OUTPUT("blackbox_pipedecomp_1.txt", "jingle_bells.txt", 0);
}
