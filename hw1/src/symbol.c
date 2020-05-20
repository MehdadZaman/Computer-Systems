#include "const.h"
#include "sequitur.h"

/*
 * Symbol management.
 *
 * The functions here manage a statically allocated array of SYMBOL structures,
 * together with a stack of "recycled" symbols.
 */

/*
 * Initialization of this global variable that could not be performed in the header file.
 */
int next_nonterminal_value = FIRST_NONTERMINAL;

/*
 * The first node in the list of recycled nodes.
 */
SYMBOL *recycled_list_head = NULL;

/**
 * Initialize the symbols module.
 * Frees all symbols, setting num_symbols to 0, and resets next_nonterminal_value
 * to FIRST_NONTERMINAL;
 */
void init_symbols(void) {
    num_symbols = 0;
    next_nonterminal_value = FIRST_NONTERMINAL;

    recycled_list_head = NULL;
}

/**
 * Get a new symbol.
 *
 * @param value  The value to be used for the symbol.  Whether the symbol is a terminal
 * symbol or a non-terminal symbol is determined by its value: terminal symbols have
 * "small" values (i.e. < FIRST_NONTERMINAL), and nonterminal symbols have "large" values
 * (i.e. >= FIRST_NONTERMINAL).
 * @param rule  For a terminal symbol, this parameter should be NULL.  For a nonterminal
 * symbol, this parameter can be used to specify a rule having that nonterminal at its head.
 * In that case, the reference count of the rule is increased by one and a pointer to the rule
 * is stored in the symbol.  This parameter can also be NULL for a nonterminal symbol if the
 * associated rule is not currently known and will be assigned later.
 * @return  A pointer to the new symbol, whose value and rule fields have been initialized
 * according to the parameters passed, and with other fields zeroed.  If the symbol storage
 * is exhausted and a new symbol cannot be created, then a message is printed to stderr and
 * abort() is called.
 *
 * When this function is called, if there are any recycled symbols, then one of those is removed
 * from the recycling list and used to satisfy the request.
 * Otherwise, if there currently are no recycled symbols, then a new symbol is allocated from
 * the main symbol_storage array and the num_symbols variable is incremented to record the
 * allocation.
 */

SYMBOL *new_symbol(int value, SYMBOL *rule) {

    SYMBOL tempSymbol;

    if(recycled_list_head != NULL)
    {
        SYMBOL *temp_symbol_ptr = recycled_list_head;

        if((value < FIRST_NONTERMINAL) && (value >= 0))
        {
            recycled_list_head = ((recycled_list_head)->next);

            (temp_symbol_ptr->value) = value;
            (temp_symbol_ptr->refcnt) = 0;
            (temp_symbol_ptr->rule) = NULL;
            (temp_symbol_ptr->next) = NULL;
            (temp_symbol_ptr->prev) = NULL;
            (temp_symbol_ptr->nextr) = NULL;
            (temp_symbol_ptr->prevr) = NULL;

            return temp_symbol_ptr;
        }
        else if((value >= FIRST_NONTERMINAL) && (value <= SYMBOL_VALUE_MAX))
        {
            recycled_list_head = ((recycled_list_head)->next);
            if(rule != NULL)
            {
                ((rule)->refcnt)++;
            }
            (temp_symbol_ptr->value) = value;
            (temp_symbol_ptr->refcnt) = 0;
            (temp_symbol_ptr->rule) = rule;
            (temp_symbol_ptr->next) = NULL;
            (temp_symbol_ptr->prev) = NULL;
            (temp_symbol_ptr->nextr) = NULL;
            (temp_symbol_ptr->prevr) = NULL;

            return temp_symbol_ptr;
        }
        else
        {
            return NULL;
        }
    }

    if(num_symbols >= MAX_SYMBOLS)
    {
        fprintf(stderr, "%s\n", "The current number of symbols has met its maximum capacity.");
        abort();
    }

    if((value < FIRST_NONTERMINAL) && (value >= 0))
    {
         tempSymbol = (SYMBOL) {
             .value = value,
             .refcnt = 0,
             .rule = NULL,
             .next = NULL,
             .prev = NULL,
             .nextr = NULL,
             .prevr = NULL};

        (*(symbol_storage + num_symbols)) = tempSymbol;
        num_symbols++;
        return (symbol_storage + (num_symbols - 1));
    }
    else if((value >= FIRST_NONTERMINAL) && (value <= SYMBOL_VALUE_MAX))
    {
        if(rule != NULL)
        {
            ((rule)->refcnt)++;
        }

        tempSymbol = (SYMBOL) {
        .value = value,
        .refcnt = 0,
        .rule = rule,
        .next = NULL,
        .prev = NULL,
        .nextr = NULL,
        .prevr = NULL};

        (*(symbol_storage + num_symbols)) = tempSymbol;
        num_symbols++;
        return (symbol_storage + (num_symbols - 1));
    }
    else
    {
        return NULL;
    }
}

/**
 * Recycle a symbol that is no longer being used.
 *
 * @param s  The symbol to be recycled.  The caller must not use this symbol any more
 * once it has been recycled.
 *
 * Symbols being recycled are added to the recycled_symbols list, where they will
 * be made available for re-allocation by a subsequent call to new_symbol.
 * The recycled_symbols list is managed as a LIFO list (i.e. a stack), using the
 * next field of the SYMBOL structure to chain together the entries.
 */
void recycle_symbol(SYMBOL *s) {
    if(s == NULL)
    {
        return;
    }
    else
    {
        ((s)-> next) = recycled_list_head;
        recycled_list_head = s;
    }
}